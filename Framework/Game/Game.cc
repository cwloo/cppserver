#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include "muduo/net/TcpClient.h"
#include "muduo/base/Thread.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"

#include <utility>
#include <map>

#include <stdio.h>
#include <unistd.h>
#include <memory>

#include <boost/algorithm/algorithm.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "public/SubNetIP.h"
#include "public/NetCardIP.h"
#include "public/Utils.h"

#include "public/codec/aes.h"
#include "public/codec/mymd5.h"
#include "public/codec/base64.h"
#include "public/codec/htmlcodec.h"
#include "public/codec/urlcodec.h"

#include "GameDefine.h"
#include "IDesk.h"
#include "IDeskDelegate.h"
#include "IPlayer.h"
#include "IRobotDelegate.h"
#include "IReplayRecord.h"

#include "Player.h"
#include "PlayerMgr.h"
#include "Robot.h"
#include "RobotMgr.h"
#include "Desk.h"
#include "DeskMgr.h"

#include "public/Global.h"
#include "Game.h"

GameSrv::GameSrv(muduo::net::EventLoop* loop,
    const muduo::net::InetAddress& listenAddr) :
          server_(loop, listenAddr, "GameSrv")
        , threadTimer_(new muduo::net::EventLoopThread(muduo::net::EventLoopThread::ThreadInitCallback(), "EventLoopThreadTimer"))
        , ipFinder_("qqwry.dat") {
    initHandlers();

	//网络I/O线程池，I/O收发读写 recv(read)/send(write)
	muduo::net::ReactorSingleton::inst(loop, "RWIOThreadPool");

    //大厅服[S]端 <- 网关服[C]端
    server_.setConnectionCallback(
        std::bind(&GameSrv::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&GameSrv::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	//公共定时器
	threadTimer_->startLoop();
}

GameSrv::~GameSrv() {
    quit();
}

void GameSrv::quit() {
	for (size_t i = 0; i < threadPool_.size(); ++i) {
		threadPool_[i]->stop();
	}
	if (zkclient_) {
		zkclient_->closeServer();
	}
	if (redisClient_) {
		redisClient_->unsubscribe();
	}
}

void GameSrv::initHandlers() {
	//PING心跳
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER,
		::Game::Common::MESSAGE_CLIENT_TO_SERVER_SUBID::KEEP_ALIVE_REQ)]
		= std::bind(&GameSrv::cmd_keep_alive_ping, this,
			std::placeholders::_1, std::placeholders::_2);
	//进入桌子
	handlers_[packet::enword(
		::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER,
		::GameServer::SUBID::SUB_C2S_ENTER_ROOM_REQ)]
		= std::bind(&GameSrv::cmd_enter_room, this,
			std::placeholders::_1, std::placeholders::_2);
	//玩家准备
	handlers_[packet::enword(
		::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER ,
		::GameServer::SUBID::SUB_C2S_USER_READY_REQ)]
		= std::bind(&GameSrv::cmd_user_ready, this,
			std::placeholders::_1, std::placeholders::_2);
	//离开桌子
	handlers_[packet::enword(
		::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER,
		::GameServer::SUBID::SUB_C2S_USER_LEFT_REQ)]
		= std::bind(&GameSrv::cmd_user_left, this,
			std::placeholders::_1, placeholders::_2);
	//玩家离线
	handlers_[packet::enword(
		::Game::Common::MAIN_MESSAGE_PROXY_TO_GAME_SERVER,
		::Game::Common::MESSAGE_PROXY_TO_GAME_SERVER_SUBID::GAME_SERVER_ON_USER_OFFLINE)]
		= std::bind(&GameSrv::cmd_user_offline, this,
			std::placeholders::_1, std::placeholders::_2);
	//逻辑消息
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_GAME_LOGIC, 0)]
		= std::bind(&GameSrv::cmd_game_message, this,
			std::placeholders::_1, std::placeholders::_2);
	//节点维护
	handlers_[packet::enword(
		::Game::Common::MAIN_MESSAGE_HTTP_TO_SERVER,
		::Game::Common::MESSAGE_HTTP_TO_SERVER_SUBID::MESSAGE_NOTIFY_REPAIR_SERVER)]
		= std::bind(&GameSrv::cmd_notifyRepairServerResp, this,
			std::placeholders::_1, std::placeholders::_2);
}

//db加载game_kind信息
bool GameSrv::db_load_room_kind_info(uint32_t gameid, uint32_t roomid) {
	bool bok = false;
	try{
		mongocxx::collection kindCollection = MONGODBCLIENT["gameconfig"]["game_kind"];
		bsoncxx::document::value query_value = document{} << "gameid" << (int32_t)gameid << finalize;
		bsoncxx::stdx::optional<bsoncxx::document::value> result = kindCollection.find_one(query_value.view());
		if (result) {
			LOG_DEBUG << __FUNCTION__ << " QueryResult: " << bsoncxx::to_json(result->view());
			bsoncxx::document::view view = result->view();
			//int32_t gameid_ = view["gameid"].get_int32();
			std::string gamename = view["gamename"].get_utf8().value.to_string();
			int32_t sortid = view["sort"].get_int32();
			std::string serviceName = view["servicename"].get_utf8().value.to_string();
			int32_t revenueRatio = view["revenueRatio"].get_int32();
			int32_t gametype = view["type"].get_int32();
			int32_t ishot = view["ishot"].get_int32();
			int32_t status_ = view["status"].get_int32();
			int32_t updateNumTimes = view["updatePlayerNum"].get_int32();
			int32_t matchMask = view["matchmask"].get_int32();//0b000010;
			auto rooms = view["rooms"].get_array();
			for (auto& doc : rooms.value) {
				if (doc["roomid"].get_int32() == roomid) {
					std::string roomname = doc["roomname"].get_utf8().value.to_string();
					int32_t deskcount = doc["tablecount"].get_int32();
					int64_t floorScore = doc["floorscore"].get_int64();
					int64_t ceilScore = doc["ceilscore"].get_int64();
					int64_t enterMinScore = doc["enterminscore"].get_int64();
					int64_t enterMaxScore = doc["entermaxscore"].get_int64();
					int32_t minPlayerNum = doc["minplayernum"].get_int32();
					int32_t maxPlayerNum = doc["maxplayernum"].get_int32();
					int64_t broadcastScore = doc["broadcastscore"].get_int64();
					int32_t enableRobot = doc["enableandroid"].get_int32();
					int32_t maxRobotNum = doc["androidcount"].get_int32();//每张桌子最大机器人数
					int64_t maxJettonScore = doc["maxjettonscore"].get_int64();
					int64_t totalStock = doc["totalstock"].get_int64();
					int64_t totalStockLowerLimit = doc["totalstocklowerlimit"].get_int64();
					int64_t totalStockHighLimit = doc["totalstockhighlimit"].get_int64();
					int32_t sysKillAllRatio = doc["systemkillallratio"].get_int32();
					int32_t systemReduceRatio = doc["systemreduceratio"].get_int32();
					int32_t changeCardRatio = doc["changecardratio"].get_int32();
					int32_t roomstatus = doc["status"].get_int32();
					int32_t realChangeRobot = 0;
					std::vector<int64_t> jettonsVec;
					bsoncxx::types::b_array jettons;
					bsoncxx::document::element elem = doc["jettons"];
					if (elem.type() == bsoncxx::type::k_array)
						jettons = elem.get_array();
					else
						jettons = doc["jettons"]["_v"].get_array();
					for (auto& jetton : jettons.value) {
						jettonsVec.push_back(jetton.get_int64());
					}
					//百人场
					if (gametype == GameType_BaiRen) {
						realChangeRobot = doc["realChangeAndroid"].get_int32();
						bsoncxx::types::b_array robotPercentage = doc["androidPercentage"]["_v"].get_array();
						for (auto& percent : robotPercentage.value) {
							roomInfo_.enterAndroidPercentage.push_back(percent.get_double());
						}
					}
					gameInfo_.gameId = gameid;
					gameInfo_.gameName = gamename;
					gameInfo_.sortId = sortid;
					gameInfo_.gameServiceName = serviceName;
					gameInfo_.revenueRatio = revenueRatio;
					gameInfo_.gameType = gametype;
					for (int i = 0; i < MTH_MAX; ++i) {
						gameInfo_.matchforbids[i] = ((matchMask & (1 << i)) != 0);
					}
					roomInfo_.gameId = gameid;
					roomInfo_.roomId = roomid;
					roomInfo_.roomName = roomname;
					roomInfo_.deskCount = deskcount;
					roomInfo_.floorScore = floorScore;
					roomInfo_.ceilScore = ceilScore;
					roomInfo_.enterMinScore = enterMinScore;
					roomInfo_.enterMaxScore = enterMaxScore;
					roomInfo_.minPlayerNum = minPlayerNum;
					roomInfo_.maxPlayerNum = maxPlayerNum;
					roomInfo_.broadcastScore = broadcastScore;
					roomInfo_.maxJettonScore = maxJettonScore;
					roomInfo_.androidCount = maxRobotNum;
					roomInfo_.totalStock = totalStock;
					roomInfo_.totalStockLowerLimit = totalStockLowerLimit;
					roomInfo_.totalStockHighLimit = totalStockHighLimit;
					roomInfo_.systemKillAllRatio = sysKillAllRatio;
					roomInfo_.systemReduceRatio = systemReduceRatio;
					roomInfo_.changeCardRatio = changeCardRatio;
					roomInfo_.serverStatus = roomstatus;
					roomInfo_.realChangeAndroid = realChangeRobot;
					roomInfo_.bEnableAndroid = enableRobot;
					roomInfo_.jettons = jettonsVec;
					roomInfo_.updatePlayerNumTimes = updateNumTimes;
					bok = true;
					break;
				}
			}
		}
	}
	catch (std::exception& e) {
		LOG_ERROR << __FUNCTION__ << " exception: " << e.what();
	}
	return bok;
}

/// <summary>
/// 游戏初始化，创建桌子，机器人
/// </summary>
/// <param name="gameid"></param>
/// <param name="roomid"></param>
/// <returns></returns>
bool GameSrv::init_server(uint32_t gameid, uint32_t roomid) {
	//db加载游戏房间配置数据
	if (db_load_room_kind_info(gameid, roomid)) {
		//玩家管理器初始化
		CPlayerMgr::get_mutable_instance().Init(&roomInfo_);
		//桌子管理器初始化，创建指定数量桌子
		CDeskMgr::get_mutable_instance().Init(&gameInfo_, &roomInfo_, threadTimer_, 0, deskThreadPool_);
		if (roomInfo_.bEnableAndroid) {
			//机器人管理器初始化，创建指定数量机器人
			CRobotMgr::get_mutable_instance().Init(&gameInfo_, &roomInfo_, 0);
			//机器人定时轮询每张桌子
			std::vector<muduo::net::EventLoop*> loops = deskThreadPool_->getAllLoops();
			for (size_t i = 0; i < loops.size(); ++i) {
				if (gameInfo_.gameType == GameType_BaiRen) {
					loops[i]->runEvery(3.0,
						std::bind(&CRobotMgr::OnTimerRobotEnter, &CRobotMgr::get_mutable_instance(), loops[i]));
					//百人场开辟一张桌子出来让机器人进入
					std::shared_ptr<IPlayer> player = std::shared_ptr<CPlayer>();
					CDeskMgr::get_mutable_instance().FindSuitDesk(player);
				}
				else {
					loops[i]->runEvery(0.1f,
						std::bind(&CRobotMgr::OnTimerRobotEnter, &CRobotMgr::get_mutable_instance(), loops[i]));
				}
			}
		}
		else {
			LOG_WARN << __FUNCTION__ << " roomid = " << roomid
				<< " Robot Enabled  = " << roomInfo_.bEnableAndroid << " maxRobotNum = " << roomInfo_.androidCount;
		}
	}
}

/// <summary>
/// PING心跳
/// </summary>
/// <param name="conn"></param>
/// <param name="msg"></param>
/// <param name="msgLen"></param>
/// <param name="header_"></param>
/// <param name="pre_header_"></param>
void GameSrv::cmd_keep_alive_ping(
	const muduo::net::TcpConnectionPtr& conn,
	BufferPtr buf) {
	packet::internal_prev_header_t const* pre_header_ = packet::get_pre_header(buf);
	packet::header_t const* header_ = packet::get_header(buf);
	uint8_t const* msg = packet::get_msg(header_);
	size_t msgLen = packet::get_msglen(header_);
	//心跳请求
	::Game::Common::KeepAliveMessage reqdata;
	if (reqdata.ParseFromArray(msg, msgLen)) {
		//心跳应答
		::Game::Common::KeepAliveMessageResponse rspdata;
		rspdata.mutable_header()->CopyFrom(reqdata.header());
		rspdata.set_retcode(1);
		rspdata.set_errormsg("User LoginInfo TimeOut, Restart Login.");
		//用户登陆token
		std::string const& token = reqdata.session();
		int64_t userid = 0;
		uint32_t agentid = 0;
		std::string account;
		if (redis_get_token_info(token, userid, account, agentid)) {
			//redis更新userid游戏中状态过期时间
			if (REDISCLIENT.ResetExpiredUserOnlineInfo(userid)) {
				rspdata.set_retcode(0);
				rspdata.set_errormsg("KEEP ALIVE PING OK.");
			}
			else {
				rspdata.set_retcode(2);
				rspdata.set_errormsg("KEEP ALIVE PING Error UserId Not Find!");
			}
		}
		else {
			rspdata.set_retcode(1);
			rspdata.set_errormsg("KEEP ALIVE PING Error Session Not Find!");
		}
		size_t len = rspdata.ByteSizeLong();
		std::vector<uint8_t> data(packet::kPrevHeaderLen + packet::kHeaderLen + len);
		if (rspdata.SerializeToArray(&data[packet::kPrevHeaderLen + packet::kHeaderLen], len)) {
			
			packet::internal_prev_header_t* pre_header =  (packet::internal_prev_header_t *)&data[0];
			packet::header_t* header = (packet::header_t*)(&data[0] + packet::kPrevHeaderLen);

			memcpy(pre_header, pre_header_, packet::kPrevHeaderLen);
			memcpy(header, header_, packet::kHeaderLen);

			header->subID = ::Game::Common::MESSAGE_CLIENT_TO_SERVER_SUBID::KEEP_ALIVE_RES;
			header->len = packet::kHeaderLen + len;
			header->realsize = len;
			header->crc = packet::getCheckSum((uint8_t const*)&header->ver, header->len - 4);

			pre_header->len = packet::kPrevHeaderLen + packet::kHeaderLen + len;
			packet::checkCheckSum(pre_header);

			TraceMessageID(header->mainID, header->subID);
			conn->send(&data[0], data.size());
		}
	}
}

bool GameSrv::SendGameErrorCode(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t mainid, uint8_t subid,
	uint32_t errcode, std::string errmsg,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
	::GameServer::MSG_S2C_UserEnterMessageResponse rspdata;
	rspdata.mutable_header()->set_sign(HEADER_SIGN);
	rspdata.set_retcode(errcode);
	rspdata.set_errormsg(errmsg);
	size_t len = rspdata.ByteSizeLong();
	std::vector<uint8_t> data(packet::kPrevHeaderLen + packet::kHeaderLen + len);
	if (rspdata.SerializeToArray(&data[packet::kPrevHeaderLen + packet::kHeaderLen], len)) {

		packet::internal_prev_header_t* pre_header = (packet::internal_prev_header_t*)&data[0];
		packet::header_t* header = (packet::header_t*)(&data[0] + packet::kPrevHeaderLen);

		memcpy(pre_header, pre_header_, packet::kPrevHeaderLen);
		memcpy(header, header_, packet::kHeaderLen);

		header->subID = mainid;
		header->subID = subid;
		header->len = packet::kHeaderLen + len;
		header->realsize = len;
		header->crc = packet::getCheckSum((uint8_t const*)&header->ver, header->len - 4);

		pre_header->len = packet::kPrevHeaderLen + packet::kHeaderLen + len;
		packet::checkCheckSum(pre_header);

		TraceMessageID(header->mainID, header->subID);
		conn->send(&data[0], data.size());
	}
}

bool GameSrv::GetUserBaseInfo(int64_t userid, UserBaseInfo& baseInfo) {
	try {
		mongocxx::collection userCollection = MONGODBCLIENT["gamemain"]["game_user"];
		auto query_value = document{} << "userid" << userid << finalize;
		bsoncxx::stdx::optional<bsoncxx::document::value> result = userCollection.find_one(query_value.view());
		if (result) {
			bsoncxx::document::view view = result->view();
			baseInfo.userID = view["userid"].get_int64();
			baseInfo.account = view["account"].get_utf8().value.to_string();
			baseInfo.agentid = view["agentid"].get_int32();
			baseInfo.lineCode = view["linecode"].get_utf8().value.to_string();
			baseInfo.headId = view["headindex"].get_int32();
			baseInfo.nickName = view["nickname"].get_utf8().value.to_string();
			baseInfo.userScore = view["score"].get_int64();
			baseInfo.status = view["status"].get_int32();
			int64_t totalAddScore = view["alladdscore"].get_int64();
			int64_t totalSubScore = view["allsubscore"].get_int64();
			int64_t totalWinScore = view["winorlosescore"].get_int64();
			int64_t totalBet = totalAddScore - totalSubScore;
			return true;
		}
	}
	catch (std::exception& e) {
		LOG_ERROR << __FUNCTION__ << " exception: " << e.what();
	}
	return (false);
}

/// <summary>
/// 进入桌子
/// </summary>
/// <param name="conn"></param>
/// <param name="msg"></param>
/// <param name="msgLen"></param>
/// <param name="header_"></param>
/// <param name="pre_header_"></param>
void GameSrv::cmd_enter_room(
	const muduo::net::TcpConnectionPtr& conn, BufferPtr buf) {
	packet::internal_prev_header_t const* pre_header_ = packet::get_pre_header(buf);
	packet::header_t const* header_ = packet::get_header(buf);
	uint8_t const* msg = packet::get_msg(header_);
	size_t msgLen = packet::get_msglen(header_);
	//请求
	::GameServer::MSG_C2S_UserEnterMessage reqdata;
	if (reqdata.ParseFromArray(msg, msgLen)) {
		//应答
		::GameServer::MSG_S2C_UserEnterMessageResponse rspdata;
		rspdata.mutable_header()->CopyFrom(reqdata.header());
		rspdata.set_retcode(0);
		rspdata.set_errormsg("Unknown Error");
		int64_t userid = pre_header_->userID;
		uint32_t clientip = pre_header_->ipaddr;
		uint32_t gameid = reqdata.gameid();
		uint32_t roomid = reqdata.roomid();
		assert(gameid == gameInfo_.gameId && roomid == roomInfo_.roomId);

		std::string clientpwd = reqdata.dynamicpassword();

		//判断玩家是否在上分
		//Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER
		//::GameServer::SUB_S2C_ENTER_ROOM_RES
		//ERROR_ENTERROOM_USER_ORDER_SCORE
		//需要先加锁禁止玩家上分操作，然后继续

		//redis已标记玩家游戏中
		if (REDISCLIENT.ExistsUserOnlineInfo(userid)) {
			std::string uuid;
			if (REDISCLIENT.GetUserLoginInfo(userid, "dynamicPassword", uuid)) {
				std::string passwd = buffer2HexStr((unsigned char const*)uuid.c_str(), uuid.size());
				if (clientpwd == passwd) {
					{
						//muduo::net::TcpConnectionPtr conn(weakConn.lock());

						//std::shared_ptr<packet::internal_prev_header_t> shared_iternal(new packet::internal_prev_header_t());
						//memcpy(shared_iternal.get(), pre_header_, packet::kPrevHeaderLen);
						//std::shared_ptr<UserProxyConnectionInfo>  userProxyConnInfo(new UserProxyConnectionInfo());
						// userProxyConnInfo->strProxyIP = conn->peerAddress().toIp();
						//userProxyConnInfo->strProxyIP = conn->peerAddress().toIpPort();
						//userProxyConnInfo->internal_header = shared_iternal;
						//{
						//	WRITE_LOCK(m_UserIdProxyConnectionInfoMapMutex);
						//	m_UserIdProxyConnectionInfoMap[userId] = userProxyConnInfo;
						//}
						entities_.add(userid, muduo::net::WeakTcpConnectionPtr(conn));
					}
				}
				else {
					//桌子密码错误
					SendGameErrorCode(conn,
						::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER,
						::GameServer::SUB_S2C_ENTER_ROOM_RES,
						ERROR_ENTERROOM_PASSWORD_ERROR, "", header_, pre_header_);
					return;
				}
			}
			else {
				//会话不存在
				SendGameErrorCode(conn,
					::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER,
					::GameServer::SUB_S2C_ENTER_ROOM_RES,
					ERROR_ENTERROOM_NOSESSION, "", header_, pre_header_);
				return;
			}
		}
		else {
			//游戏已结束
			SendGameErrorCode(conn,
				::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER,
				::GameServer::SUB_S2C_ENTER_ROOM_RES,
				ERROR_ENTERROOM_GAME_IS_END, "ERROR_ENTERROOM_GAME_IS_END", header_, pre_header_);
			return;
		}
		std::shared_ptr<IPlayer> pPlayer = CPlayerMgr::get_mutable_instance().Get(userid);
		if (pPlayer && pPlayer->IsValid()) {
			//断线重连进入游戏判断
			std::shared_ptr<IDesk> pDesk = CDeskMgr::get_mutable_instance().GetDesk(pPlayer->GetDeskID());
			if (pDesk && pDesk->CanUserEnter(pPlayer)) {
				pPlayer->SetTrustee(false);
				//用户进入桌子动作
				RunInLoop(pDesk->GetLoop(),
					std::bind(&CDesk::OnUserEnterAction,
						std::dynamic_pointer_cast<CDesk>(pDesk).get(),
						muduo::net::WeakTcpConnectionPtr(conn), buf, pPlayer, false));
			}
			else {
				REDISCLIENT.DelUserOnlineInfo(userid);
				//游戏已经结束
				SendGameErrorCode(conn,
					::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER,
					::GameServer::SUB_S2C_ENTER_ROOM_RES,
					ERROR_ENTERROOM_GAME_IS_END, "ERROR_ENTERROOM_GAME_IS_END", header_, pre_header_);
			}
			return;
		}
		//判断在其他游戏中
		uint32_t gameid_ = 0, roomid_ = 0;
		if (REDISCLIENT.GetUserOnlineInfo(userid, gameid_, roomid_)) {
			if (gameInfo_.gameId != gameid || roomInfo_.roomId != roomid_) {
				SendGameErrorCode(conn,
					::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER,
					::GameServer::SUB_S2C_PLAY_IN_OTHERROOM,
					ERROR_ENTERROOM_USERINGAME, "ERROR_ENTERROOM_USERINGAME", header_, pre_header_);
				return;
			}
		}
		UserBaseInfo userInfo;
		if (conn) {
			if (!GetUserBaseInfo(userid, userInfo)) {
				REDISCLIENT.DelUserOnlineInfo(userid);
				SendGameErrorCode(conn,
					::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER,
					::GameServer::SUB_S2C_ENTER_ROOM_RES, ERROR_ENTERROOM_USERNOTEXIST,
					"ERROR_ENTERROOM_USERNOTEXIST", header_, pre_header_);
				return;
			}
		}
		else {
			REDISCLIENT.DelUserOnlineInfo(userid);
			SendGameErrorCode(conn,
				::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER,
				::GameServer::SUB_S2C_ENTER_ROOM_RES, ERROR_ENTERROOM_NOSESSION,
				"ERROR_ENTERROOM_NOSESSION", header_, pre_header_);
			return;
		}
		//最小进入条件
		if (roomInfo_.enterMinScore > 0 &&
			userInfo.userScore < roomInfo_.enterMinScore) {
			REDISCLIENT.DelUserOnlineInfo(userid);
			SendGameErrorCode(conn,
				::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER,
				::GameServer::SUB_S2C_ENTER_ROOM_RES,
				ERROR_ENTERROOM_SCORENOENOUGH,
				"ERROR_ENTERROOM_SCORENOENOUGH", header_, pre_header_);
			return;
		}
		//最大进入条件
		if (roomInfo_.enterMaxScore > 0 &&
			userInfo.userScore > roomInfo_.enterMaxScore) {
			REDISCLIENT.DelUserOnlineInfo(userid);
			SendGameErrorCode(conn,
				::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER,
				::GameServer::SUB_S2C_ENTER_ROOM_RES,
				ERROR_ENTERROOM_SCORELIMIT,
				"ERROR_ENTERROOM_SCORELIMIT", header_, pre_header_);
			return;
		}
		{
			userInfo.ip = pre_header_->ipaddr;
			pPlayer->SetUserBaseInfo(userInfo);
			std::shared_ptr<IDesk> pDesk = CDeskMgr::get_mutable_instance().FindSuitDesk(pPlayer, INVALID_DESK);
			if (!pDesk) {
				REDISCLIENT.DelUserOnlineInfo(userid);
				SendGameErrorCode(conn,
					::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER,
					::GameServer::SUB_S2C_ENTER_ROOM_RES,
					ERROR_ENTERROOM_TABLE_FULL,
					"ERROR_ENTERROOM_TABLE_FULL", header_, pre_header_);
				return;
			}
			RunInLoop(pDesk->GetLoop(),
				std::bind(&CDesk::OnUserEnterAction,
					std::dynamic_pointer_cast<CDesk>(pDesk).get(),
					muduo::net::WeakTcpConnectionPtr(conn), buf, pPlayer, false));
		}
	}
}

/// <summary>
/// 玩家准备
/// </summary>
/// <param name="conn"></param>
/// <param name="msg"></param>
/// <param name="msgLen"></param>
/// <param name="header_"></param>
/// <param name="pre_header_"></param>
void GameSrv::cmd_user_ready(
	const muduo::net::TcpConnectionPtr& conn, BufferPtr buf) {
}

/// <summary>
/// 离开桌子
/// </summary>
/// <param name="conn"></param>
/// <param name="msg"></param>
/// <param name="msgLen"></param>
/// <param name="header_"></param>
/// <param name="pre_header_"></param>
void GameSrv::cmd_user_left(
	const muduo::net::TcpConnectionPtr& conn, BufferPtr buf) {
}

/// <summary>
/// 玩家离线
/// </summary>
/// <param name="conn"></param>
/// <param name="msg"></param>
/// <param name="msgLen"></param>
/// <param name="header_"></param>
/// <param name="pre_header_"></param>
void GameSrv::cmd_user_offline(
	const muduo::net::TcpConnectionPtr& conn, BufferPtr buf) {
}

/// <summary>
/// 逻辑消息
/// </summary>
/// <param name="conn"></param>
/// <param name="msg"></param>
/// <param name="msgLen"></param>
/// <param name="header_"></param>
/// <param name="pre_header_"></param>
void GameSrv::cmd_game_message(
	const muduo::net::TcpConnectionPtr& conn, BufferPtr buf) {
}

/// <summary>
/// 节点维护
/// </summary>
/// <param name="conn"></param>
/// <param name="msg"></param>
/// <param name="msgLen"></param>
/// <param name="header_"></param>
/// <param name="pre_header_"></param>
void GameSrv::cmd_notifyRepairServerResp(
	const muduo::net::TcpConnectionPtr& conn, BufferPtr buf) {
}



//db刷新所有游戏房间信息
void GameSrv::db_refresh_game_room_info() {
	
}


//redis刷新所有房间游戏人数
void GameSrv::redis_refresh_room_player_nums() {

}

void GameSrv::redis_update_room_player_nums() {

}

//redis通知刷新游戏房间配置
void GameSrv::on_refresh_game_config(std::string msg) {
	db_refresh_game_room_info();
}


//redis查询token，判断是否过期
bool GameSrv::redis_get_token_info(
	std::string const& token,
	int64_t& userid, std::string& account, uint32_t& agentid) {
	try {
		std::string value;
		if (REDISCLIENT.get(token, value)) {
			boost::property_tree::ptree root;
			std::stringstream s(value);
			boost::property_tree::read_json(s, root);
			userid = root.get<int64_t>("userid");
			account = root.get<std::string>("account");
			agentid = root.get<uint32_t>("agentid");
			return userid > 0;
		}
	}
	catch (std::exception& e) {
		LOG_ERROR << __FUNCTION__ << " exception: " << e.what();
	}
	return false;
}
//db更新用户在线状态
bool GameSrv::db_update_online_status(int64_t userid, int32_t status) {
	bool bok = false;
	try {
		//////////////////////////////////////////////////////////////////////////
		//玩家登陆网关服信息
		//使用hash	h.usr:proxy[1001] = session|ip:port:port:pid<弃用>
		//使用set	s.uid:1001:proxy = session|ip:port:port:pid<使用>
		//网关服ID格式：session|ip:port:port:pid
		//第一个ip:port是网关服监听客户端的标识
		//第二个ip:port是网关服监听订单服的标识
		//pid标识网关服进程id
		//////////////////////////////////////////////////////////////////////////
		REDISCLIENT.del("s.uid:" + to_string(userid) + ":proxy");
		bsoncxx::document::value query_value = document{} << "userid" << userid << finalize;
		mongocxx::collection userCollection = MONGODBCLIENT["gamemain"]["game_user"];
		bsoncxx::document::value update_value = document{}
			<< "$set" << open_document
			<< "onlinestatus" << bsoncxx::types::b_int32{ status }
			<< close_document
			<< finalize;
		userCollection.update_one(query_value.view(), update_value.view());
		bok = true;
	}
	catch (std::exception& e) {
		LOG_ERROR << __FUNCTION__ << " exception: " << e.what();
	}
	return bok;
}

//zookeeper
bool GameSrv::initZookeeper(std::string const& ipaddr) {
	zkclient_.reset(new ZookeeperClient(ipaddr));
	zkclient_->SetConnectedWatcherHandler(
		std::bind(&GameSrv::zookeeperConnectedHandler, this));
	if (!zkclient_->connectServer()) {
		LOG_FATAL << __FUNCTION__ << " --- *** " << "initZookeeper error";
		abort();
		return false;
	}
	//自注册zookeeper节点检查
	threadTimer_->getLoop()->runAfter(5.0f, std::bind(&GameSrv::onTimerCheckSelf, this));
	return true;
}

//RedisCluster
bool GameSrv::initRedisCluster(std::string const& ipaddr, std::string const& passwd)
{
	redisClient_.reset(new RedisClient());
	if (!redisClient_->initRedisCluster(ipaddr, passwd)) {
		LOG_FATAL << __FUNCTION__ << " --- *** " << "initRedisCluster error";
		abort();
		return false;
	}
	redisIpaddr_ = ipaddr;
	redisPasswd_ = passwd;
    //刷新配置通知
    redisClient_->subscribeRefreshConfigMessage(
		std::bind(&GameSrv::on_refresh_game_config, this, std::placeholders::_1));
    redisClient_->startSubThread();

    return true;
}

//RedisCluster
bool GameSrv::initRedisCluster() {

	if (!REDISCLIENT.initRedisCluster(redisIpaddr_, redisPasswd_)) {
		LOG_FATAL << __FUNCTION__ << " --- *** " << "initRedisCluster error";
		abort();
		return false;
	}
	return true;
}

//RedisLock
bool GameSrv::initRedisLock() {
	for (std::vector<std::string>::const_iterator it = redlockVec_.begin();
		it != redlockVec_.end(); ++it) {
		std::vector<std::string> vec;
		boost::algorithm::split(vec, *it, boost::is_any_of(":"));
		LOG_INFO << __FUNCTION__ << " --- *** " << "\nredisLock " << vec[0].c_str() << ":" << vec[1].c_str();
		REDISLOCK.AddServerUrl(vec[0].c_str(), atol(vec[1].c_str()));
	}
	return true;
}

//MongoDB
bool GameSrv::initMongoDB(std::string const& url) {
	//http://mongocxx.org/mongocxx-v3/tutorial/
	LOG_INFO << __FUNCTION__ << " --- *** " << url;
	mongocxx::instance instance{};
	//mongoDBUrl_ = url;
	//http://mongocxx.org/mongocxx-v3/tutorial/
	//mongodb://admin:6pd1SieBLfOAr5Po@192.168.0.171:37017,192.168.0.172:37017,192.168.0.173:37017/?connect=replicaSet;slaveOk=true&w=1&readpreference=secondaryPreferred&maxPoolSize=50000&waitQueueMultiple=5
	MongoDBClient::ThreadLocalSingleton::setUri(url);
	return true;
}

static __thread mongocxx::database* dbgamemain_;

//MongoDB
bool GameSrv::initMongoDB() {
	static __thread mongocxx::database db = MONGODBCLIENT["gamemain"];
	dbgamemain_ = &db;
	return true;
}

void GameSrv::zookeeperConnectedHandler() {
	if (ZNONODE == zkclient_->existsNode("/GAME"))
		zkclient_->createNode("/GAME", "GAME"/*, true*/);
    //网关服务
	//if (ZNONODE == zkclient_->existsNode("/GAME/ProxyServers"))
	//	zkclient_->createNode("/GAME/ProxyServers", "ProxyServers"/*, true*/);
    //大厅服务
	//if (ZNONODE == zkclient_->existsNode("/GAME/HallServers"))
	//	zkclient_->createNode("/GAME/HallServers", "HallServers"/*, true*/);
	//大厅维护
	//if (ZNONODE == zkclient_->existsNode("/GAME/HallServersInvalid"))
	//	zkclient_->createNode("/GAME/HallServersInvalid", "HallServersInvalid", true);
    //游戏服务
	if (ZNONODE == zkclient_->existsNode("/GAME/GameServers"))
		zkclient_->createNode("/GAME/GameServers", "GameServers"/*, true*/);
    //游戏维护
	//if (ZNONODE == zkclient_->existsNode("/GAME/GameServersInvalid"))
	//	zkclient_->createNode("/GAME/GameServersInvalid", "GameServersInvalid", true);
    {
		//指定网卡 ip:port
		std::vector<std::string> vec;
        //server_ ip:port
		boost::algorithm::split(vec, server_.ipPort(), boost::is_any_of(":"));
		// roomid:ip:port
        nodeValue_ = std::to_string(roomInfo_.roomId) + strIpAddr_ + ":" + vec[1];
        nodePath_ = "/GAME/GameServers/" + nodeValue_;
        //启动时自注册自身节点
        zkclient_->createNode(nodePath_, nodeValue_, true);
        //挂维护中的节点
        invalidNodePath_ = "/GAME/GameServersInvalid/" + nodeValue_;
    }
}

//自注册zookeeper节点检查
void GameSrv::onTimerCheckSelf() {
	if (ZNONODE == zkclient_->existsNode("/GAME"))
		zkclient_->createNode("/GAME", "GAME"/*, true*/);
	if (ZNONODE == zkclient_->existsNode("/GAME/GameServers"))
		zkclient_->createNode("/GAME/GameServers", "GameServers"/*, true*/);
	if (ZNONODE == zkclient_->existsNode(nodePath_)) {
		LOG_ERROR << __FUNCTION__ << " " << nodePath_;
		zkclient_->createNode(nodePath_, nodeValue_, true);
	}
	//自注册zookeeper节点检查
	threadTimer_->getLoop()->runAfter(5.0f, std::bind(&GameSrv::onTimerCheckSelf, this));
}

//MongoDB/RedisCluster/RedisLock
void GameSrv::threadInit()
{
	initRedisCluster();
	initMongoDB();
	initRedisLock();
}

//启动worker线程
//启动TCP监听网关客户端
void GameSrv::start(int numThreads, int numWorkerThreads, int maxSize)
{
	//网络I/O线程数
	numThreads_ = numThreads;
	muduo::net::ReactorSingleton::setThreadNum(numThreads);
	//启动网络I/O线程池，I/O收发读写 recv(read)/send(write)
	muduo::net::ReactorSingleton::start();

	//worker线程数，最好 numWorkerThreads = n * numThreads
	numWorkerThreads_ = numWorkerThreads;

	//创建若干worker线程，启动worker线程池
	for (int i = 0; i < numWorkerThreads; ++i) {
		std::shared_ptr<muduo::ThreadPool> threadPool = std::make_shared<muduo::ThreadPool>("ThreadPool:" + std::to_string(i));
		threadPool->setThreadInitCallback(std::bind(&GameSrv::threadInit, this));
		threadPool->setMaxQueueSize(maxSize);
		threadPool->start(1);
		threadPool_.push_back(threadPool);
	}
	//创建若干桌子worker线程，启动桌子线程池
	deskThreadPool_->setThreadNum(numWorkerThreads);
	deskThreadPool_->start(std::bind(&GameSrv::threadInit, this));

	LOG_INFO << __FUNCTION__ << " --- *** "
		<< "\GameSrv = " << server_.ipPort()
		<< " 网络I/O线程数 = " << numThreads
		<< " worker线程数 = " << numWorkerThreads;

	//启动TCP监听客户端
	//使用ET模式accept/read/write
	server_.start(true);

	//获取网络I/O模型EventLoop池
	//std::shared_ptr<muduo::net::EventLoopThreadPool> threadPool =
	//	/*server_.*/muduo::net::ReactorSingleton::threadPool();
	//std::vector<muduo::net::EventLoop*> loops = threadPool->getAllLoops();

	//为各网络I/O线程创建Context
	//for (size_t index = 0; index < loops.size(); ++index) {
	//	loops[index]->setContext(EventLoopContextPtr(new EventLoopContext(index)));
	//}

	//为每个网络I/O线程绑定若干worker线程(均匀分配)
	//{
	//	int next = 0;
	//	for (size_t index = 0; index < threadPool_.size(); ++index) {
	//		EventLoopContextPtr context = boost::any_cast<EventLoopContextPtr>(loops[next]->getContext());
	//		assert(context);
	//		context->addWorkerIndex(index);
	//		if (++next >= loops.size()) {
	//			next = 0;
	//		}
	//	}
	//}
	//
	//db刷新所有游戏房间信息
	threadTimer_->getLoop()->runAfter(3.0f, std::bind(&GameSrv::db_refresh_game_room_info, this));
	//redis刷新所有房间游戏人数
	threadTimer_->getLoop()->runAfter(30, std::bind(&GameSrv::redis_refresh_room_player_nums, this));
}

//游戏服[S]端 <- 网关服[C]端
void GameSrv::onConnection(const muduo::net::TcpConnectionPtr& conn) {
    
    conn->getLoop()->assertInLoopThread();
    
    if (conn->connected()) {
		int32_t num = numConnected_.incrementAndGet();
		LOG_INFO << __FUNCTION__ << " --- *** " << "大厅服[" << conn->localAddress().toIpPort() << "] <- 网关服["
			<< conn->peerAddress().toIpPort() << "] "
			<< (conn->connected() ? "UP" : "DOWN") << " " << num;

    }
    else {
		int32_t num = numConnected_.decrementAndGet();
		LOG_INFO << __FUNCTION__ << " --- *** " << "大厅服[" << conn->localAddress().toIpPort() << "] <- 网关服["
			<< conn->peerAddress().toIpPort() << "] "
			<< (conn->connected() ? "UP" : "DOWN") << " " << num;
    }
}

//游戏服[S]端 <- 网关服[C]端
void GameSrv::onMessage(
    const muduo::net::TcpConnectionPtr& conn,
               muduo::net::Buffer* buf,
               muduo::Timestamp receiveTime) {

	conn->getLoop()->assertInLoopThread();

    //解析TCP数据包，先解析包头(header)，再解析包体(body)，避免粘包出现
    while (buf->readableBytes() >= packet::kMinPacketSZ) {
       
        // FIXME: use Buffer::peekInt32()
        const uint16_t len = buf->peekInt16();

		//数据包太大或太小
		if (/*likely*/(len > packet::kMaxPacketSZ ||
			       len < packet::kPrevHeaderLen + packet::kHeaderLen)) {
			if (conn) {
#if 0
				//不再发送数据
				conn->shutdown();
#else
				//直接强制关闭连接
				conn->forceClose();
#endif
			}
			break;
		}
		else if (/*likely*/(len <= buf->readableBytes())) {
			BufferPtr buffer(new muduo::net::Buffer(len));
			buffer->append(buf->peek(), static_cast<size_t>(len));
			buf->retrieve(len);
			packet::internal_prev_header_t* pre_header = (packet::internal_prev_header_t*)buffer->peek();
			assert(packet::checkCheckSum(pre_header));
			std::string session((char const*)pre_header->session, sizeof(pre_header->session));
			assert(!session.empty() && session.size() == packet::kSessionSZ);
			//session -> hash(session) -> index
			//int index = hash_session_(session) % threadPool_.size();
			//threadPool_[index]->run(
			//	std::bind(
			//		&GameSrv::asyncClientHandler,
			//		this,
			//		muduo::net::WeakTcpConnectionPtr(conn), buffer, receiveTime));
			RunInLoop(threadTimer_->getLoop(),
				std::bind(
					&GameSrv::asyncClientHandler,
					this,
					muduo::net::WeakTcpConnectionPtr(conn), buffer, receiveTime));
		}
		//数据包不足够解析，等待下次接收再解析
		else /*if (likely(len > buf->readableBytes()))*/ {
			break;
		}
    }
}

//游戏服[S]端 <- 网关服[C]端
void GameSrv::asyncClientHandler(
    muduo::net::WeakTcpConnectionPtr const& weakConn,
    BufferPtr &buf,
    muduo::Timestamp receiveTime) {
	muduo::net::TcpConnectionPtr peer(weakConn.lock());
	if (!peer) {
		return;
	}
	//内部消息头internal_prev_header_t + 命令消息头header_t
    if(buf->readableBytes() < packet::kPrevHeaderLen + packet::kHeaderLen) {
        return;
    }
	//内部消息头internal_prev_header_t
	packet::internal_prev_header_t* pre_header = (packet::internal_prev_header_t *)buf->peek();
	//session
	std::string session((char const*)pre_header->session, sizeof(pre_header->session));
	assert(!session.empty() && session.size() == packet::kSessionSZ);
	//userid
	int64_t userid = pre_header->userID;
	//命令消息头header_t
	packet::header_t* header = (packet::header_t *)(buf->peek() + packet::kPrevHeaderLen);
	//校验CRC header->len = packet::kHeaderLen + len
	//header.len uint16_t
	//header.crc uint16_t
	//header.ver ~ header.realsize + protobuf
	uint16_t crc = packet::getCheckSum((uint8_t const*)&header->ver, header->len - 4);
	assert(header->crc == crc);
	size_t len = buf->readableBytes();
	if( header->len == len - packet::kPrevHeaderLen &&
		header->crc == crc &&
		header->ver == 1 &&
		header->sign == HEADER_SIGN) {
		//mainID
        switch(header->mainID) {
		case Game::Common::MAINID::MAIN_MESSAGE_PROXY_TO_GAME_SERVER:
		case Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER:
		case Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_GAME_LOGIC:
		case Game::Common::MAINID::MAIN_MESSAGE_HTTP_TO_SERVER: {
                switch(header->enctype) {
				case packet::PUBENC_PROTOBUF_NONE: {
					//NONE
					TraceMessageID(header->mainID, header->subID);
					int cmd = packet::enword(header->mainID, header->subID);
					CmdCallbacks::const_iterator it = handlers_.find(cmd);
					if (it != handlers_.end()) {
                            
						CmdCallback const& handler = it->second;
#if 0
						handler(peer,
							(uint8_t const*)header + packet::kHeaderLen,
							header->len - packet::kHeaderLen,
							header, pre_header);
#else
						handler(peer, buf);
#endif
                    }
					else {
                    }
                    break;
                }
				case packet::PUBENC_PROTOBUF_RSA: { 
					//RSA
					TraceMessageID(header->mainID, header->subID);
					break;
                }
				case packet::PUBENC_PROTOBUF_AES: {
					//AES
                    break;
                }
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }
    }
	else {
    }
}

//发送消息
void sendMessage(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* data, size_t len, packet::header_t const* header,
	packet::internal_prev_header_t const* pre_header) {

}