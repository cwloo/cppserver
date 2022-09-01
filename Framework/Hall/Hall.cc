

#include <muduo/base/Logging.h>

#include <boost/algorithm/algorithm.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "public/Global.h"
#include "public/SubNetIP.h"
#include "public/NetCardIP.h"
#include "public/Utils.h"

#include "public/codec/aes.h"
#include "public/codec/mymd5.h"
#include "public/codec/base64.h"
#include "public/codec/htmlcodec.h"
#include "public/codec/urlcodec.h"

#include "public/Global.h"
#include "GameDefine.h"
#include "Hall.h"

HallSrv::HallSrv(muduo::net::EventLoop* loop,
    const muduo::net::InetAddress& listenAddr) :
          server_(loop, listenAddr, "HallSrv")
        , threadTimer_(new muduo::net::EventLoopThread(muduo::net::EventLoopThread::ThreadInitCallback(), "EventLoopThreadTimer"))
        , ipFinder_("qqwry.dat") {
    initHandlers();

	//网络I/O线程池，I/O收发读写 recv(read)/send(write)
	muduo::net::ReactorSingleton::inst(loop, "RWIOThreadPool");

    //大厅服[S]端 <- 网关服[C]端
    server_.setConnectionCallback(
        std::bind(&HallSrv::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HallSrv::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	//公共定时器
	threadTimer_->startLoop();
}

HallSrv::~HallSrv() {
    quit();
}

void HallSrv::quit() {
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

void HallSrv::initHandlers() {
	//PING心跳
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL,
		::Game::Common::MESSAGE_CLIENT_TO_SERVER_SUBID::KEEP_ALIVE_REQ)]
		= std::bind(&HallSrv::cmd_keep_alive_ping, this,
			std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//玩家登陆
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL,
		::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_LOGIN_MESSAGE_REQ)]
		= std::bind(&HallSrv::cmd_login_servers, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//玩家离线
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_PROXY_TO_HALL,
		::Game::Common::MESSAGE_PROXY_TO_HALL_SUBID::HALL_ON_USER_OFFLINE)]
		= std::bind(&HallSrv::cmd_on_user_offline, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//获取game_kind信息(所有游戏房间信息)
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL,
		::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_GET_GAME_ROOM_INFO_REQ)]
		= std::bind(&HallSrv::cmd_get_game_info, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//获取玩家游戏情况(断线重连)
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL,
		::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_GET_PLAYING_GAME_INFO_MESSAGE_REQ)]
		= std::bind(&HallSrv::cmd_get_playing_game_info, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//查询指定类型游戏节点
	handlers_[packet::enword(::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL,
		::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_GET_GAME_SERVER_MESSAGE_REQ)]
		= std::bind(&HallSrv::cmd_get_game_server_message, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//获取房间人数
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL,
		::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_GET_ROOM_PLAYER_NUM_REQ)]
		= std::bind(&HallSrv::cmd_get_room_player_nums, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//修改玩家头像
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL,
		::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_SET_HEAD_MESSAGE_REQ)]
		= std::bind(&HallSrv::cmd_set_headid, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//修改玩家昵称
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL << 8,
		::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_SET_NICKNAME_MESSAGE_REQ)]
		= std::bind(&HallSrv::cmd_set_nickname, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//查询玩家积分
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL,
		::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_GET_USER_SCORE_MESSAGE_REQ)]
		= std::bind(&HallSrv::cmd_get_userscore, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//玩家游戏记录
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL,
		::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_GET_PLAY_RECORD_REQ)]
		= std::bind(&HallSrv::cmd_get_play_record, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//游戏记录详情
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL,
		::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_GET_RECORD_DETAIL_REQ)]
		= std::bind(&HallSrv::cmd_get_play_record_detail, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//大厅维护接口
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_HTTP_TO_SERVER,
		::Game::Common::MESSAGE_HTTP_TO_SERVER_SUBID::MESSAGE_NOTIFY_REPAIR_SERVER)]
		= std::bind(&HallSrv::cmd_repair_hallserver, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//获取任务列表
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL,
		::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_GET_TASK_LIST_REQ)]
		= std::bind(&HallSrv::cmd_get_task_list, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	//获取任务奖励
	handlers_[packet::enword(
		::Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL,
		::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_GET_AWARDS_REQ)]
		= std::bind(&HallSrv::cmd_get_task_award, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
}

/// <summary>
/// PING心跳
/// </summary>
/// <param name="conn"></param>
/// <param name="msg"></param>
/// <param name="msgLen"></param>
/// <param name="header_"></param>
/// <param name="pre_header_"></param>
void HallSrv::cmd_keep_alive_ping(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
	//心跳请求
	::Game::Common::KeepAliveMessage reqdata;
	if (reqdata.ParseFromArray(msg, msgLen)) {
		//心跳应答
		::Game::Common::KeepAliveMessageResponse rspdata;
		rspdata.mutable_header()->CopyFrom(reqdata.header());
		rspdata.set_retcode(0);
		rspdata.set_errormsg("KEEP ALIVE PING OK.");
		//用户登陆token
		std::string const& token = reqdata.session();
		//redis更新token过期时间
		REDISCLIENT.resetExpired(token);
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

/// <summary>
/// 玩家登录
/// </summary>
/// <param name="conn"></param>
/// <param name="msg"></param>
/// <param name="msgLen"></param>
/// <param name="header_"></param>
/// <param name="pre_header_"></param>
void HallSrv::cmd_login_servers(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
	//请求
	::HallServer::LoginMessage reqdata;
	if (reqdata.ParseFromArray(msg, msgLen)) {
		//应答
		::HallServer::LoginMessageResponse rspdata;
		rspdata.mutable_header()->CopyFrom(reqdata.header());
		rspdata.set_retcode(-1);
		rspdata.set_errormsg("Unkwown Error");
		int64_t userid = 0;
		uint32_t agentid = 0;
		std::string account;
		//用户登陆token
		std::string const& token = reqdata.session();
		try {
			//登陆IP
			uint32_t ipaddr = pre_header_->ipaddr;
			//查询IP所在国家/地区
			std::string country, location;
			ipFinder_.GetAddressByIp(ntohl(ipaddr), location, country);
			std::string loginIp = Inet2Ipstr(ipaddr);
			//不能频繁登陆操作(间隔5s)
			std::string key = REDIS_LOGIN_3S_CHECK + token;
			if (REDISCLIENT.exists(key)) {
				LOG_ERROR << __FUNCTION__ << " token = " << token
					<< " IP = " << loginIp << " " << location << " 登陆太频繁了";
				return;
			}
			else {
				REDISCLIENT.set(key, key, 5);
			}
			//系统当前时间
			std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
			//redis查询token，判断是否过期
			if (redis_get_token_info(token, userid, account, agentid)) {
				mongocxx::collection coll = MONGODBCLIENT["gamemain"]["game_user"];
				bsoncxx::document::value query_value = document{} << "userid" << userid << finalize;
				bsoncxx::stdx::optional<bsoncxx::document::value> result = coll.find_one(query_value.view());
				if (result) {
					//查询用户数据
					LOG_DEBUG << __FUNCTION__ << " Query result: " << bsoncxx::to_json(*result);
					bsoncxx::document::view view = result->view();
					std::string account_ = view["account"].get_utf8().value.to_string();
					int32_t agentid_ = view["agentid"].get_int32();
					int32_t headid = view["headindex"].get_int32();
					std::string nickname = view["nickname"].get_utf8().value.to_string();
					int64_t score = view["score"].get_int64();
					int32_t status_ = view["status"].get_int32();
					std::string lastLoginIp = view["lastloginip"].get_utf8().value.to_string();//lastLoginIp
					std::chrono::system_clock::time_point lastLoginTime = view["lastlogintime"].get_date();//lastLoginTime
					//userid(account,agentid)必须一致
					if (account == account_ && agentid == agentid_) {
						//////////////////////////////////////////////////////////////////////////
						//玩家登陆网关服信息
						//使用hash	h.usr:proxy[1001] = session|ip:port:port:pid<弃用>
						//使用set	s.uid:1001:proxy = session|ip:port:port:pid<使用>
						//网关服ID格式：session|ip:port:port:pid
						//第一个ip:port是网关服监听客户端的标识
						//第二个ip:port是网关服监听订单服的标识
						//pid标识网关服进程id
						//std::string proxyinfo = string(internal_header->session) + "|" + string(internal_header->servid);
						//REDISCLIENT.set("s.uid:" + to_string(userId) + ":proxy", proxyinfo);
						if (!status_) {
							rspdata.set_retcode(::HallServer::LoginMessageResponse::LOGIN_SEAL_ACCOUNTS);
							rspdata.set_errormsg("对不起，您的账号已冻结，请联系客服。");
							db_add_login_logger(userid, loginIp, location, now, 1, agentid);
							goto end;
						}
						//全服通知到各网关服顶号处理
						std::string session((char const*)pre_header_->session, sizeof(pre_header_->session));
						boost::property_tree::ptree root;
						std::stringstream s;
						root.put("userid", userid);
						root.put("session", session);
						boost::property_tree::write_json(s, root, false);
						std::string msg = s.str();
						REDISCLIENT.publishUserLoginMessage(msg);
						
						std::string uuid = createUUID();
						std::string passwd = buffer2HexStr((unsigned char const*)uuid.c_str(), uuid.size());
						REDISCLIENT.SetUserLoginInfo(userid, "dynamicPassword", passwd);
						
						rspdata.set_userid(userid);
						rspdata.set_account(account);
						rspdata.set_agentid(agentid);
						rspdata.set_nickname(nickname);
						rspdata.set_headid(headid);
						rspdata.set_score(score);
						rspdata.set_gamepass(passwd);
						rspdata.set_retcode(::HallServer::LoginMessageResponse::LOGIN_OK);
						rspdata.set_errormsg("User Login OK.");

						//通知网关服登陆成功
						const_cast<packet::internal_prev_header_t*>(pre_header_)->userID = userid;
						const_cast<packet::internal_prev_header_t*>(pre_header_)->ok = 1;

						//db更新用户登陆信息
						db_update_login_info(userid, loginIp, lastLoginTime, now);
						//db添加用户登陆日志
						db_add_login_logger(userid, loginIp, location, now, 0, agentid);
						//redis更新登陆时间
						REDISCLIENT.SetUserLoginInfo(userid, "lastlogintime", std::to_string(chrono::system_clock::to_time_t(now)));
						//redis更新token过期时间
						REDISCLIENT.resetExpired(token);
						LOG_ERROR << __FUNCTION__ << " LOGIN SERVER OK!";
					}
					else {
						//账号不一致
						LOG_ERROR << __FUNCTION__ << " token = " << token << " userid = " << userid
							<< "\naccount = " << account << " agentid = " << agentid
							<< "\naccount = " << account_ << " agentid = " << agentid_
							<< "\nNot Same Error.";
						rspdata.set_retcode(::HallServer::LoginMessageResponse::LOGIN_ACCOUNTS_NOT_EXIST);
						rspdata.set_errormsg("account/agentid Not Exist Error.");
						db_add_login_logger(userid, loginIp, location, now, 2, agentid);
					}
				}
				else {
					//账号不存在
					LOG_ERROR << __FUNCTION__ << " token = " << token << " userid = " << userid << " Not Exist Error.";
					rspdata.set_retcode(::HallServer::LoginMessageResponse::LOGIN_ACCOUNTS_NOT_EXIST);
					rspdata.set_errormsg("userid Not Exist Error.");
					db_add_login_logger(userid, loginIp, location, now, 3, agentid);
				}
			}
			else {
				//token不存在或已过期
				LOG_ERROR << __FUNCTION__ << " token = " << token << " Not Exist Error.";
				rspdata.set_retcode(::HallServer::LoginMessageResponse::LOGIN_ACCOUNTS_NOT_EXIST);
				rspdata.set_errormsg("Session Not Exist Error.");
				db_add_login_logger(userid, loginIp, location, now, 4, agentid);
			}
		}
		catch (std::exception& e) {
			LOG_ERROR << __FUNCTION__ << " exception: " << e.what();
			rspdata.set_retcode(::HallServer::LoginMessageResponse::LOGIN_NETBREAK);
			rspdata.set_errormsg("Database/redis Update Error.");
		}
	end:
		size_t len = rspdata.ByteSizeLong();
		std::vector<uint8_t> data(packet::kPrevHeaderLen + packet::kHeaderLen + len);
		if (rspdata.SerializeToArray(&data[packet::kPrevHeaderLen + packet::kHeaderLen], len)) {

			packet::internal_prev_header_t* pre_header = (packet::internal_prev_header_t*)&data[0];
			packet::header_t* header = (packet::header_t*)(&data[0] + packet::kPrevHeaderLen);

			memcpy(pre_header, pre_header_, packet::kPrevHeaderLen);
			memcpy(header, header_, packet::kHeaderLen);

			header->subID = ::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_LOGIN_MESSAGE_RES;
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

/// <summary>
/// 玩家离线
/// </summary>
/// <param name="conn"></param>
/// <param name="msg"></param>
/// <param name="msgLen"></param>
/// <param name="header_"></param>
/// <param name="pre_header_"></param>
void HallSrv::cmd_on_user_offline(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
	//userid
	int64_t userid = pre_header_->userID;
	//db更新用户离线状态
	db_update_online_status(userid, 0);
	std::string lastLoginTime;
	//redis查询用户登陆时间
	if (REDISCLIENT.GetUserLoginInfo(userid, "lastlogintime", lastLoginTime)) {
		std::chrono::system_clock::time_point loginTime = std::chrono::system_clock::from_time_t(stoull(lastLoginTime));
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		//db添加用户登出日志
		db_add_logout_logger(userid, loginTime, now);
	}
}

/// <summary>
/// 获取game_kind信息(所有游戏房间信息)
/// </summary>
/// <param name="conn"></param>
/// <param name="msg"></param>
/// <param name="msgLen"></param>
/// <param name="header_"></param>
/// <param name="pre_header_"></param>
void HallSrv::cmd_get_game_info(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
	//请求
	::HallServer::GetGameMessage reqdata;
	if (reqdata.ParseFromArray(msg, msgLen)) {
		//应答
		::HallServer::GetGameMessageResponse rspdata;
		rspdata.mutable_header()->CopyFrom(reqdata.header());
		rspdata.set_retcode(0);
		rspdata.set_errormsg("Get Game Message OK!");
		{
			READ_LOCK(gameinfo_mutex_);
			rspdata.CopyFrom(gameinfo_);
		}
		size_t len = rspdata.ByteSizeLong();
		std::vector<uint8_t> data(packet::kPrevHeaderLen + packet::kHeaderLen + len);
		if (rspdata.SerializeToArray(&data[packet::kPrevHeaderLen + packet::kHeaderLen], len)) {

			packet::internal_prev_header_t* pre_header = (packet::internal_prev_header_t*)&data[0];
			packet::header_t* header = (packet::header_t*)(&data[0] + packet::kPrevHeaderLen);

			memcpy(pre_header, pre_header_, packet::kPrevHeaderLen);
			memcpy(header, header_, packet::kHeaderLen);

			header->subID = ::Game::Common::CLIENT_TO_HALL_GET_GAME_ROOM_INFO_RES;
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
//db刷新所有游戏房间信息
void HallSrv::db_refresh_game_room_info() {
	static STD::Random r(0, threadPool_.size() - 1);
	int index = r.randInt_re();
	assert(index >= 0 && index < threadPool_.size());
	threadPool_[index]->run(std::bind(&HallSrv::db_update_game_room_info, this));
}

void HallSrv::db_update_game_room_info() {
	WRITE_LOCK(gameinfo_mutex_);
	gameinfo_.clear_header();
	gameinfo_.clear_gamemessage();
	try {
		//游戏类型，抽水率，排序id，游戏类型，热度，维护状态，游戏名称
		uint32_t gameid = 0, revenueratio = 0, gamesortid = 0, gametype = 0, gameishot = 0;
		std::string gamename;
		int32_t gamestatus = 0;
		//房间号，桌子数量，最小/最多游戏人数，房间状态，房间名称
		uint32_t roomid = 0, tableCount = 0, minPlayerNum = 0, maxPlayerNum = 0, roomstatus = 0;
		std::string roomname;
		//房间底注，房间顶注，最小/最大准入分，区域限红
		int64_t floorscore, ceilscore, enterMinScore, enterMaxScore, maxJettonScore;

		mongocxx::collection kindCollection = MONGODBCLIENT["gameconfig"]["game_kind"];
		mongocxx::cursor cursor = kindCollection.find({});
		for (auto& doc : cursor) {
			LOG_DEBUG << __FUNCTION__ << " QueryResult: " << bsoncxx::to_json(doc);
			gameid = doc["gameid"].get_int32();						//游戏ID
			gamename = doc["gamename"].get_utf8().value.to_string();//游戏名称
			gamesortid = doc["sort"].get_int32();					//游戏排序0 1 2 3 4
			gametype = doc["type"].get_int32();						//0-百人场  1-对战类
			gameishot = doc["ishot"].get_int32();					//0-正常 1-火爆 2-新
			gamestatus = doc["status"].get_int32();					//-1:关停 0:暂未开放 1：正常开启  2：敬请期待
			::HallServer::GameMessage* gameMsg = gameinfo_.add_gamemessage();
			gameMsg->set_gameid(gameid);
			gameMsg->set_gamename(gamename);
			gameMsg->set_gamesortid(gamesortid);
			gameMsg->set_gametype(gametype);
			gameMsg->set_gameishot(gameishot);
			gameMsg->set_gamestatus(gamestatus);
			//各游戏房间
			auto rooms = doc["rooms"].get_array();
			for (auto& doc_ : rooms.value)
			{
				roomid = doc_["roomid"].get_int32();						//房间类型 初 中 高 房间
				roomname = doc_["roomname"].get_utf8().value.to_string();//类型名称 初 中 高
				tableCount = doc_["tablecount"].get_int32();				//桌子数量 有几桌游戏开启中
				floorscore = doc_["floorscore"].get_int64();				//房间底注
				ceilscore = doc_["ceilscore"].get_int64();				//房间顶注
				enterMinScore = doc_["enterminscore"].get_int64();		//进游戏最低分(最小准入分)
				enterMaxScore = doc_["entermaxscore"].get_int64();		//进游戏最大分(最大准入分)
				minPlayerNum = doc_["minplayernum"].get_int32();			//最少游戏人数(至少几人局)
				maxPlayerNum = doc_["maxplayernum"].get_int32();			//最多游戏人数(最大几人局)
				maxJettonScore = doc_["maxjettonscore"].get_int64();		//各区域最大下注(区域限红)
				roomstatus = doc_["status"].get_int32();					//-1:关停 0:暂未开放 1：正常开启  2：敬请期待

				::HallServer::GameRoomMessage* roomMsg = gameMsg->add_gameroommsg();
				roomMsg->set_roomid(roomid);
				roomMsg->set_roomname(roomname);
				roomMsg->set_tablecount(tableCount);
				roomMsg->set_floorscore(floorscore);
				roomMsg->set_ceilscore(ceilscore);
				roomMsg->set_enterminscore(enterMinScore);
				roomMsg->set_entermaxscore(enterMaxScore);
				roomMsg->set_minplayernum(minPlayerNum);
				roomMsg->set_maxplayernum(maxPlayerNum);
				roomMsg->set_maxjettonscore(maxJettonScore);
				roomMsg->set_status(roomstatus);
				//配置可下注筹码数组
				bsoncxx::types::b_array jettons;
				bsoncxx::document::element elem = doc_["jettons"];
				if (elem.type() == bsoncxx::type::k_array)
					jettons = elem.get_array();
				else
					jettons = doc_["jettons"]["_v"].get_array();
				for (auto& jetton : jettons.value) {
					roomMsg->add_jettons(jetton.get_int64());
				}
				//房间在游戏中人数
				roomMsg->set_playernum(0);
			}
		}
		redis_update_room_player_nums();
	}
	catch (exception& e) {
		LOG_ERROR << "MongoDBException: " << e.what();
	}
}

//redis刷新所有房间游戏人数
void HallSrv::redis_refresh_room_player_nums() {
	static STD::Random r(0, threadPool_.size() - 1);
	int index = r.randInt_re();
	assert(index >= 0 && index < threadPool_.size());
	threadPool_[index]->run(std::bind(&HallSrv::redis_update_room_player_nums, this));
	threadTimer_->getLoop()->runAfter(30, std::bind(&HallSrv::redis_refresh_room_player_nums, this));
}

void HallSrv::redis_update_room_player_nums() {
	WRITE_LOCK(room_playernums_mutex_);
	room_playernums_.clear_header();
	room_playernums_.clear_gameplayernummessage();
	auto& gameMessage = gameinfo_.gamemessage();
	try {
		//各个子游戏
		for (auto& gameinfo : gameMessage) {
			::HallServer::SimpleGameMessage* simpleMessage = room_playernums_.add_gameplayernummessage();
			uint32_t gameid = gameinfo.gameid();
			simpleMessage->set_gameid(gameid);
			auto& gameroommsg = gameinfo.gameroommsg();
			//各个子游戏房间
			for (auto& roominfo : gameroommsg) {
				::HallServer::RoomPlayerNum* roomPlayerNum = simpleMessage->add_roomplayernum();
				uint32_t roomid = roominfo.roomid();
				//redis获取房间人数
				uint64_t playerNums = 0;
				if (room_servers_.find(roomid) != room_servers_.end()) {
					REDISCLIENT.GetGameServerplayerNum(room_servers_[roomid], playerNums);
				}
				//更新房间游戏人数
				roomPlayerNum->set_roomid(roomid);
				roomPlayerNum->set_playernum(playerNums);
				const_cast<::HallServer::GameRoomMessage&>(roominfo).set_playernum(playerNums);
				LOG_ERROR << __FUNCTION__ << " roomid = " << roomid << " playerNums = " << playerNums;
			}
		}
	}
	catch (std::exception& e) {
		LOG_ERROR << __FUNCTION__ << " exception: " << e.what();
	}
}

//redis通知刷新游戏房间配置
void HallSrv::on_refresh_game_config(std::string msg) {
	db_refresh_game_room_info();
}

/// <summary>
/// 获取玩家游戏情况(断线重连) 
/// </summary>
/// <param name="conn"></param>
/// <param name="msg"></param>
/// <param name="msgLen"></param>
/// <param name="header_"></param>
/// <param name="pre_header_"></param>
void HallSrv::cmd_get_playing_game_info(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
	//请求
	::HallServer::GetPlayingGameInfoMessage reqdata;
	if (reqdata.ParseFromArray(msg, msgLen)) {
		//应答
		::HallServer::GetPlayingGameInfoMessageResponse rspdata;
		rspdata.mutable_header()->CopyFrom(reqdata.header());
		//userid
		int64_t userid = pre_header_->userID;
		//判断玩家是否游戏中
		uint32_t gameid = 0, roomid = 0;
		if (REDISCLIENT.GetUserOnlineInfo(userid, gameid, roomid)) {
			//游戏中
			rspdata.set_gameid(gameid);
			rspdata.set_roomid(roomid);
			rspdata.set_retcode(0);
			rspdata.set_errormsg("Get Playing Game Info OK.");
		}
		else {
			//没有玩了
			rspdata.set_retcode(1);
			rspdata.set_errormsg("This User Not In Playing Game!");
		}
		size_t len = rspdata.ByteSizeLong();
		std::vector<uint8_t> data(packet::kPrevHeaderLen + packet::kHeaderLen + len);
		if (rspdata.SerializeToArray(&data[packet::kPrevHeaderLen + packet::kHeaderLen], len)) {

			packet::internal_prev_header_t* pre_header = (packet::internal_prev_header_t*)&data[0];
			packet::header_t* header = (packet::header_t*)(&data[0] + packet::kPrevHeaderLen);

			memcpy(pre_header, pre_header_, packet::kPrevHeaderLen);
			memcpy(header, header_, packet::kHeaderLen);

			header->subID = ::Game::Common::CLIENT_TO_HALL_GET_PLAYING_GAME_INFO_MESSAGE_RES;
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

/// <summary>
/// 查询指定类型游戏节点 
/// </summary>
/// <param name="conn"></param>
/// <param name="msg"></param>
/// <param name="msgLen"></param>
/// <param name="header_"></param>
/// <param name="pre_header_"></param>
void HallSrv::cmd_get_game_server_message(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
	//请求
	::HallServer::GetGameServerMessage reqdata;
	if (reqdata.ParseFromArray(msg, msgLen)) {
		//应答
		::HallServer::GetGameServerMessageResponse rspdata;
		rspdata.mutable_header()->CopyFrom(reqdata.header());
		rspdata.set_retcode(1);
		rspdata.set_errormsg("Unknown error.");
		uint32_t gameid = reqdata.gameid();
		uint32_t roomid = reqdata.roomid();
		//userid
		int64_t userid = pre_header_->userID;
		//判断玩家是否游戏中
		uint32_t gameid_ = 0, roomid_ = 0;
		if (REDISCLIENT.GetUserOnlineInfo(userid, gameid_, roomid_)) {
			if (gameid != gameid_ || roomid != roomid_) {
				//在别的游戏中
				rspdata.set_retcode(ERROR_ENTERROOM_USERINGAME);
				rspdata.set_errormsg("user in other game.");
			}
			else {
				//通知网关服成功
				const_cast<packet::internal_prev_header_t*>(pre_header_)->ok = 1;
				rspdata.set_retcode(0);
				rspdata.set_errormsg("Get Game Server IP OK.");
			}
		}
		else {
			//随机一个指定类型游戏节点
			std::string ipport;
			if ((roomid - gameid * 10) < 20) {
				random_game_server_ipport(roomid, ipport);
			}
			else {
			}
			//可能ipport节点不可用，要求zk实时监控
			if (!ipport.empty()) {
				//redis更新玩家游戏节点
				REDISCLIENT.SetUserOnlineInfoIP(userid, ipport);
				//redis更新玩家游戏中
				REDISCLIENT.SetUserOnlineInfo(userid, gameid, roomid);
				rspdata.set_retcode(0);
				rspdata.set_errormsg("Get Game Server IP OK.");
				//通知网关服成功
				const_cast<packet::internal_prev_header_t*>(pre_header_)->ok = 1;
			}
			else {
				//分配失败，清除游戏中状态
				REDISCLIENT.DelUserOnlineInfo(userid);
				rspdata.set_retcode(ERROR_ENTERROOM_GAMENOTEXIST);
				rspdata.set_errormsg("Game Server Not found!!!");
			}
		}

		size_t len = rspdata.ByteSizeLong();
		std::vector<uint8_t> data(packet::kPrevHeaderLen + packet::kHeaderLen + len);
		if (rspdata.SerializeToArray(&data[packet::kPrevHeaderLen + packet::kHeaderLen], len)) {

			packet::internal_prev_header_t* pre_header = (packet::internal_prev_header_t*)&data[0];
			packet::header_t* header = (packet::header_t*)(&data[0] + packet::kPrevHeaderLen);

			memcpy(pre_header, pre_header_, packet::kPrevHeaderLen);
			memcpy(header, header_, packet::kHeaderLen);

			header->subID = ::Game::Common::CLIENT_TO_HALL_GET_GAME_SERVER_MESSAGE_RES;
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

//获取房间人数
void HallSrv::cmd_get_room_player_nums(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
	//请求
	::HallServer::GetServerPlayerNum reqdata;
	if (reqdata.ParseFromArray(msg, msgLen)) {
		//应答
		::HallServer::GetServerPlayerNumResponse rspdata;
		rspdata.mutable_header()->CopyFrom(reqdata.header());
		rspdata.set_retcode(0);
		rspdata.set_errormsg("Get ServerPlayerNum Message OK!");
		{
			READ_LOCK(room_playernums_mutex_);
			rspdata.CopyFrom(room_playernums_);
		}
		size_t len = rspdata.ByteSizeLong();
		std::vector<uint8_t> data(packet::kPrevHeaderLen + packet::kHeaderLen + len);
		if (rspdata.SerializeToArray(&data[packet::kPrevHeaderLen + packet::kHeaderLen], len)) {

			packet::internal_prev_header_t* pre_header = (packet::internal_prev_header_t*)&data[0];
			packet::header_t* header = (packet::header_t*)(&data[0] + packet::kPrevHeaderLen);

			memcpy(pre_header, pre_header_, packet::kPrevHeaderLen);
			memcpy(header, header_, packet::kHeaderLen);

			header->subID = ::Game::Common::CLIENT_TO_HALL_GET_ROOM_PLAYER_NUM_RES;
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

//修改玩家头像
void HallSrv::cmd_set_headid(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
	//请求
	::HallServer::SetHeadIdMessage reqdata;
	if (reqdata.ParseFromArray(msg, msgLen)) {
		//应答
		::HallServer::SetHeadIdMessageResponse rspdata;
		rspdata.mutable_header()->CopyFrom(reqdata.header());
		rspdata.set_retcode(0);
		rspdata.set_errormsg("OK.");
		//userid
		int64_t userid = pre_header_->userID;
		int32_t headid = reqdata.headid();
		try {
			//db修改
			mongocxx::collection userCollection = MONGODBCLIENT["gamemain"]["game_user"];
			userCollection.update_one(document{} << "userid" << userid << finalize,
				document{} << "$set" << open_document <<
				"headindex" << headid << close_document << finalize);
			rspdata.set_userid(userid);
			rspdata.set_headid(headid);
		}
		catch (std::exception& e) {
			LOG_ERROR << __FUNCTION__ << " exception: " << e.what();
			rspdata.set_retcode(1);
			rspdata.set_errormsg("Database  Error.");
		}
		size_t len = rspdata.ByteSizeLong();
		std::vector<uint8_t> data(packet::kPrevHeaderLen + packet::kHeaderLen + len);
		if (rspdata.SerializeToArray(&data[packet::kPrevHeaderLen + packet::kHeaderLen], len)) {

			packet::internal_prev_header_t* pre_header = (packet::internal_prev_header_t*)&data[0];
			packet::header_t* header = (packet::header_t*)(&data[0] + packet::kPrevHeaderLen);

			memcpy(pre_header, pre_header_, packet::kPrevHeaderLen);
			memcpy(header, header_, packet::kHeaderLen);

			header->subID = ::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_SET_HEAD_MESSAGE_RES;
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

//修改玩家昵称
void HallSrv::cmd_set_nickname(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
}

//查询玩家积分
void HallSrv::cmd_get_userscore(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
	//请求
	::HallServer::GetUserScoreMessage reqdata;
	if (reqdata.ParseFromArray(msg, msgLen)) {
		//应答
		::HallServer::GetUserScoreMessageResponse rspdata;
		rspdata.mutable_header()->CopyFrom(reqdata.header());
		//userid
		int64_t userid = pre_header_->userID;
		int64_t score = 0;
		rspdata.set_userid(userid);
		try {
			mongocxx::options::find opts = mongocxx::options::find{};
			opts.projection(document{} << "score" << 1 << finalize);
			mongocxx::collection userCollection = MONGODBCLIENT["gamemain"]["game_user"];
			bsoncxx::stdx::optional<bsoncxx::document::value> result =
				userCollection.find_one(document{} << "userid" << userid << finalize, opts);
			if (result) {
				bsoncxx::document::view view = result->view();
				score = view["score"].get_int64();
				rspdata.set_score(score);
				rspdata.set_retcode(0);
				rspdata.set_errormsg("CMD GET USER SCORE OK.");
			}
			else {
				rspdata.set_retcode(1);
				rspdata.set_errormsg("CMD GET USER SCORE OK.");
			}
		}
		catch (std::exception& e) {
			LOG_ERROR << __FUNCTION__ << " exception: " << e.what();
			rspdata.set_retcode(2);
			rspdata.set_errormsg("Database  Error.");
		}
		size_t len = rspdata.ByteSizeLong();
		std::vector<uint8_t> data(packet::kPrevHeaderLen + packet::kHeaderLen + len);
		if (rspdata.SerializeToArray(&data[packet::kPrevHeaderLen + packet::kHeaderLen], len)) {

			packet::internal_prev_header_t* pre_header = (packet::internal_prev_header_t*)&data[0];
			packet::header_t* header = (packet::header_t*)(&data[0] + packet::kPrevHeaderLen);

			memcpy(pre_header, pre_header_, packet::kPrevHeaderLen);
			memcpy(header, header_, packet::kHeaderLen);

			header->subID = ::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_GET_USER_SCORE_MESSAGE_RES;
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

//玩家游戏记录
void HallSrv::cmd_get_play_record(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
	//请求
	::HallServer::GetPlayRecordMessage reqdata;
	if (reqdata.ParseFromArray(msg, msgLen)) {
		//应答
		::HallServer::GetPlayRecordMessageResponse rspdata;
		rspdata.mutable_header()->CopyFrom(reqdata.header());
		//userid
		int64_t userid = pre_header_->userID;
		int32_t gameid = reqdata.gameid();
		rspdata.set_gameid(gameid);
		try {
			std::string roundid;//牌局编号
			int32_t roomid = 0;//房间号
			int64_t winscore = 0;//输赢分
			std::chrono::system_clock::time_point endTime;//结束时间
			int64_t endTimestamp = 0;//结束时间戳
			//(userid, gameid)
			mongocxx::options::find opts = mongocxx::options::find{};
			opts.projection(document{} << "gameinfoid" << 1 << "roomid" << 1 << "winscore" << 1 << "gameendtime" << 1 << finalize);
			opts.sort(document{} << "_id" << -1 << finalize);//"_id"字段排序
			opts.limit(10); //显示最新10条
			mongocxx::collection playCollection = MONGODBCLIENT["gamemain"]["play_record"];
			mongocxx::cursor cursor = playCollection.find(document{} << "userid" << userid << "gameid" << gameid << finalize, opts);
			for (auto& doc : cursor) {
				roundid = doc["gameinfoid"].get_utf8().value.to_string();	//牌局编号
				roomid = doc["roomid"].get_int32();							//房间号
				winscore = doc["winscore"].get_int64();						//输赢分
				endTime = doc["gameendtime"].get_date();					//结束时间
				endTimestamp = (std::chrono::duration_cast<std::chrono::seconds>(endTime.time_since_epoch())).count();

				::HallServer::GameRecordInfo* gameRecordInfo = rspdata.add_detailinfo();
				gameRecordInfo->set_gameroundno(roundid);
				gameRecordInfo->set_roomid(roomid);
				gameRecordInfo->set_winlosescore(winscore);
				gameRecordInfo->set_gameendtime(endTimestamp);
			}
			rspdata.set_retcode(0);
			rspdata.set_errormsg("CMD GET USER SCORE OK.");
		}
		catch (std::exception& e) {
			LOG_ERROR << __FUNCTION__ << " exception: " << e.what();
			rspdata.set_retcode(1);
			rspdata.set_errormsg("MongoDB  Error.");
		}
		size_t len = rspdata.ByteSizeLong();
		std::vector<uint8_t> data(packet::kPrevHeaderLen + packet::kHeaderLen + len);
		if (rspdata.SerializeToArray(&data[packet::kPrevHeaderLen + packet::kHeaderLen], len)) {

			packet::internal_prev_header_t* pre_header = (packet::internal_prev_header_t*)&data[0];
			packet::header_t* header = (packet::header_t*)(&data[0] + packet::kPrevHeaderLen);

			memcpy(pre_header, pre_header_, packet::kPrevHeaderLen);
			memcpy(header, header_, packet::kHeaderLen);

			header->subID = ::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_GET_PLAY_RECORD_RES;
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

//游戏记录详情
void HallSrv::cmd_get_play_record_detail(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
	//请求
	::HallServer::GetRecordDetailMessage reqdata;
	if (reqdata.ParseFromArray(msg, msgLen)) {
		//应答
		::HallServer::GetRecordDetailResponse rspdata;
		rspdata.mutable_header()->CopyFrom(reqdata.header());
		std::string const& roundid = reqdata.gameroundno();
		rspdata.set_gameroundno(roundid);
		rspdata.set_retcode(-1);
		rspdata.set_errormsg("Not Exist.");
		//userid
		int64_t userid = pre_header_->userID;
		try {
			std::string jsondata;
			mongocxx::collection replayCollection = MONGODBCLIENT["gamelog"]["game_replay"];
			mongocxx::options::find opts = mongocxx::options::find{};
			opts.projection(document{} << "detail" << 1 << finalize);
			bsoncxx::stdx::optional<bsoncxx::document::value> result = 
				replayCollection.find_one(document{} << "gameinfoid" << roundid << finalize, opts);
			if (result) {
				bsoncxx::document::view view = result->view();
				if (view["detail"]) {
					switch (view["detail"].type()) {
					case bsoncxx::type::k_null:
						break;
					case bsoncxx::type::k_utf8:
						jsondata = view["detail"].get_utf8().value.to_string();
						LOG_ERROR << __FUNCTION__ << "\n" << jsondata;
						rspdata.set_detailinfo(jsondata);
						break;
					case bsoncxx::type::k_binary:
						rspdata.set_detailinfo(view["detail"].get_binary().bytes, view["detail"].get_binary().size);
						break;
					}
				}
				rspdata.set_retcode(0);
				rspdata.set_errormsg("CMD GET GAME RECORD DETAIL OK.");
			}
		}
		catch (std::exception& e) {
			LOG_ERROR << __FUNCTION__ << " exception: " << e.what();
			rspdata.set_retcode(1);
			rspdata.set_errormsg("MongoDB Error.");
		}
		size_t len = rspdata.ByteSizeLong();
		std::vector<uint8_t> data(packet::kPrevHeaderLen + packet::kHeaderLen + len);
		if (rspdata.SerializeToArray(&data[packet::kPrevHeaderLen + packet::kHeaderLen], len)) {

			packet::internal_prev_header_t* pre_header = (packet::internal_prev_header_t*)&data[0];
			packet::header_t* header = (packet::header_t*)(&data[0] + packet::kPrevHeaderLen);

			memcpy(pre_header, pre_header_, packet::kPrevHeaderLen);
			memcpy(header, header_, packet::kHeaderLen);

			header->subID = ::Game::Common::MESSAGE_CLIENT_TO_HALL_SUBID::CLIENT_TO_HALL_GET_RECORD_DETAIL_RES;
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

//大厅维护接口
void HallSrv::cmd_repair_hallserver(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
}

//获取任务列表
void HallSrv::cmd_get_task_list(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
}

//获取任务奖励
void HallSrv::cmd_get_task_award(
	const muduo::net::TcpConnectionPtr& conn,
	uint8_t const* msg, size_t msgLen,
	packet::header_t const* header_,
	packet::internal_prev_header_t const* pre_header_) {
}

/// <summary>
/// 随机一个指定类型游戏节点
/// </summary>
/// <param name="roomid"></param>
/// <param name="ipport"></param>
void HallSrv::random_game_server_ipport(uint32_t roomid, std::string& ipport) {
	ipport.clear();
	static STD::Random r;
	READ_LOCK(room_servers_mutex_);
	std::map<int, std::vector<std::string>>::iterator it = room_servers_.find(roomid);
	if (it != room_servers_.end()) {
		std::vector<std::string>& rooms = it->second;
		if (rooms.size() > 0) {
			int index = r.betweenInt(0, rooms.size() - 1).randFloat_mt();
			ipport = rooms[index];
		}
	}
}

//redis查询token，判断是否过期
bool HallSrv::redis_get_token_info(
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

//db更新用户登陆信息(登陆IP，时间)
bool HallSrv::db_update_login_info(
	int64_t userid,
	std::string const& loginIp,
	std::chrono::system_clock::time_point& lastLoginTime,
	std::chrono::system_clock::time_point& now) {
	bool bok = false;
	int64_t days = (std::chrono::duration_cast<std::chrono::seconds>(lastLoginTime.time_since_epoch())).count() / 3600 / 24;
	int64_t nowDays = (std::chrono::duration_cast<chrono::seconds>(now.time_since_epoch())).count() / 3600 / 24;
	try {
		bsoncxx::document::value query_value = document{} << "userid" << userid << finalize;
		mongocxx::collection userCollection = MONGODBCLIENT["gamemain"]["game_user"];
		if (days + 1 == nowDays) {
			bsoncxx::document::value update_value = document{}
				<< "$set" << open_document
				<< "lastlogintime" << bsoncxx::types::b_date(now)
				<< "lastloginip" << loginIp
				<< "onlinestatus" << bsoncxx::types::b_int32{ 1 }
				<< close_document
				<< "$inc" << open_document
				<< "activedays" << 1								//活跃天数+1
				<< "keeplogindays" << 1 << close_document			//连续登陆天数+1
				<< finalize;
			userCollection.update_one(query_value.view(), update_value.view());
		}
		else if (days == nowDays) {
			bsoncxx::document::value update_value = document{}
				<< "$set" << open_document
				<< "lastlogintime" << bsoncxx::types::b_date(now)
				<< "lastloginip" << loginIp
				<< "onlinestatus" << bsoncxx::types::b_int32{ 1 }
				<< close_document
				<< finalize;
			userCollection.update_one(query_value.view(), update_value.view());
		}
		else {
			bsoncxx::document::value update_value = document{}
				<< "$set" << open_document
				<< "lastlogintime" << bsoncxx::types::b_date(now)
				<< "lastloginip" << loginIp
				<< "keeplogindays" << bsoncxx::types::b_int32{ 1 }	//连续登陆天数1
				<< "onlinestatus" << bsoncxx::types::b_int32{ 1 }
				<< close_document
				<< "$inc" << open_document
				<< "activedays" << 1 << close_document				//活跃天数+1
				<< finalize;
			userCollection.update_one(query_value.view(), update_value.view());
		}
		bok = true;
	}
	catch (std::exception& e) {
		LOG_ERROR << __FUNCTION__ << " exception: " << e.what();
	}
	return bok;
}

//db更新用户在线状态
bool HallSrv::db_update_online_status(int64_t userid, int32_t status) {
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

//db添加用户登陆日志
bool HallSrv::db_add_login_logger(
	int64_t userid,
	std::string const& loginIp,
	std::string const& location,
	std::chrono::system_clock::time_point& now,
	uint32_t status, uint32_t agentid) {
	bool bok = false;
	try {
		mongocxx::collection loginLogCollection = MONGODBCLIENT["gamemain"]["login_log"];
		bsoncxx::document::value insert_value = bsoncxx::builder::stream::document{}
			<< "userid" << userid
			<< "loginip" << loginIp
			<< "address" << location
			<< "status" << (int32_t)status //错误码
			<< "agentid" << (int32_t)agentid
			<< "logintime" << bsoncxx::types::b_date(now)
			<< bsoncxx::builder::stream::finalize;

		LOG_DEBUG << __FUNCTION__ << " Insert Document:" << bsoncxx::to_json(insert_value);

		bsoncxx::stdx::optional<mongocxx::result::insert_one> result = loginLogCollection.insert_one(insert_value.view());
		bok = true;
	}
	catch (std::exception& e) {
		LOG_ERROR << __FUNCTION__ << " exception: " << e.what();
	}
	return bok;
}

//db添加用户登出日志
bool HallSrv::db_add_logout_logger(
	int64_t userid,
	std::chrono::system_clock::time_point& loginTime,
	std::chrono::system_clock::time_point& now) {
	bool bok = false;
	try {

		int32_t agentid = 0;
		mongocxx::options::find opts = mongocxx::options::find{};
		opts.projection(document{} << "agentid" << 1 << finalize);
		bsoncxx::document::value query_value = document{} << "userid" << userid << finalize;
		mongocxx::collection userCollection = MONGODBCLIENT["gamemain"]["game_user"];
		bsoncxx::stdx::optional<bsoncxx::document::value> result = userCollection.find_one(query_value.view(), opts);
		bsoncxx::document::view view = result->view();
		agentid = view["agentid"].get_int32();
		//在线时长
		chrono::seconds durationTime = chrono::duration_cast<chrono::seconds>(now - loginTime);
		mongocxx::collection logoutLogCollection = MONGODBCLIENT["gamemain"]["logout_log"];
		bsoncxx::document::value insert_value = document{}
			<< "userid" << userid
			<< "logintime" << bsoncxx::types::b_date(loginTime)					//登陆时间
			<< "logouttime" << bsoncxx::types::b_date(now)						//离线时间
			<< "agentid" << agentid
			<< "playseconds" << bsoncxx::types::b_int64{ durationTime.count() } //在线时长
			<< finalize;
		/*bsoncxx::stdx::optional<mongocxx::result::insert_one> result = */logoutLogCollection.insert_one(insert_value.view());
		bok = true;
	}
	catch (std::exception& e) {
		LOG_ERROR << __FUNCTION__ << " exception: " << e.what();
	}
	return bok;
}

//zookeeper
bool HallSrv::initZookeeper(std::string const& ipaddr) {
	zkclient_.reset(new ZookeeperClient(ipaddr));
	zkclient_->SetConnectedWatcherHandler(
		std::bind(&HallSrv::zookeeperConnectedHandler, this));
	if (!zkclient_->connectServer()) {
		LOG_FATAL << __FUNCTION__ << " --- *** " << "initZookeeper error";
		abort();
		return false;
	}
	//自注册zookeeper节点检查
	threadTimer_->getLoop()->runAfter(5.0f, std::bind(&HallSrv::onTimerCheckSelf, this));
	return true;
}

//RedisCluster
bool HallSrv::initRedisCluster(std::string const& ipaddr, std::string const& passwd)
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
		std::bind(&HallSrv::on_refresh_game_config, this, std::placeholders::_1));
    redisClient_->startSubThread();

    return true;
}

//RedisCluster
bool HallSrv::initRedisCluster() {

	if (!REDISCLIENT.initRedisCluster(redisIpaddr_, redisPasswd_)) {
		LOG_FATAL << __FUNCTION__ << " --- *** " << "initRedisCluster error";
		abort();
		return false;
	}
	return true;
}

//RedisLock
bool HallSrv::initRedisLock() {
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
bool HallSrv::initMongoDB(std::string const& url) {
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
bool HallSrv::initMongoDB() {
	static __thread mongocxx::database db = MONGODBCLIENT["gamemain"];
	dbgamemain_ = &db;
	return true;
}

void HallSrv::zookeeperConnectedHandler() {
	if (ZNONODE == zkclient_->existsNode("/GAME"))
		zkclient_->createNode("/GAME", "GAME"/*, true*/);
    //网关服务
	if (ZNONODE == zkclient_->existsNode("/GAME/ProxyServers"))
		zkclient_->createNode("/GAME/ProxyServers", "ProxyServers"/*, true*/);
    //大厅自己
	if (ZNONODE == zkclient_->existsNode("/GAME/HallServers"))
		zkclient_->createNode("/GAME/HallServers", "HallServers"/*, true*/);
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
        nodeValue_ = strIpAddr_ + ":" + vec[1];
        nodePath_ = "/GAME/HallServers/" + nodeValue_;
        //启动时自注册自身节点
        zkclient_->createNode(nodePath_, nodeValue_, true);
        //挂维护中的节点
        invalidNodePath_ = "/GAME/HallServersInvalid/" + nodeValue_;
    }
	{
        //网关服 ip:port
		std::vector<std::string> names;
		if (ZOK == zkclient_->getClildren(
            "/GAME/ProxyServers",
            names,
			std::bind(
                &HallSrv::onGatewayWatcherHandler, this,
				placeholders::_1, std::placeholders::_2,
				placeholders::_3, std::placeholders::_4,
				placeholders::_5), this)) {
            printf("--- ********************* 可用网关服列表 \n");
            for (std::string const& name : names) {
                printf("%s\n",name.c_str());
            }
		}
	}
	{
        //游戏服 roomid:ip:port
		std::vector<std::string> names;
		if (ZOK == zkclient_->getClildren(
            "/GAME/GameServers",
            names,
            std::bind(
                &HallSrv::onGameWatcherHandler, this,
			placeholders::_1, std::placeholders::_2,
			placeholders::_3, std::placeholders::_4,
			placeholders::_5), this)) {
			
            WRITE_LOCK(room_servers_mutex_);
            printf("--- ********************* 可用游戏服列表 \n");
			for (std::string const& name : names) {
				printf("%s\n", name.c_str());
				std::vector<string> vec;
				boost::algorithm::split(vec, name, boost::is_any_of(":"));
                std::vector<std::string>& room = room_servers_[stoi(vec[0])];
                room.emplace_back(name);
			}
		}
	}
}

void HallSrv::onGatewayWatcherHandler(int type, int state,
    const std::shared_ptr<ZookeeperClient>& zkClientPtr,
	const std::string& path, void* context) {
    //网关服 ip:port
    std::vector<std::string> names;
	if (ZOK == zkclient_->getClildren(
        "/GAME/ProxyServers",
        names,
        std::bind(
            &HallSrv::onGatewayWatcherHandler, this,
		placeholders::_1, std::placeholders::_2,
		placeholders::_3, std::placeholders::_4,
		placeholders::_5), this)) {
		printf("--- ********************* 可用网关服列表 \n");
		for (std::string const& name : names) {
			printf("%s\n", name.c_str());
		}
	}
}

void HallSrv::onGameWatcherHandler(int type, int state,
    const std::shared_ptr<ZookeeperClient>& zkClientPtr,
	const std::string& path, void* context) {
    //游戏服 roomid:ip:port
	std::vector<std::string> names;
	if (ZOK == zkclient_->getClildren(
        "/GAME/GameServers",
        names,
        std::bind(
		&HallSrv::onGameWatcherHandler, this,
		placeholders::_1, std::placeholders::_2,
		placeholders::_3, std::placeholders::_4,
		placeholders::_5), this)) {
		WRITE_LOCK(room_servers_mutex_);
		printf("--- ********************* 可用游戏服列表 \n");
		for (std::string const& name : names) {
			printf("%s\n", name.c_str());
			std::vector<string> vec;
			boost::algorithm::split(vec, name, boost::is_any_of(":"));
			std::vector<std::string>& room = room_servers_[stoi(vec[0])];
			room.emplace_back(name);
		}
	}
}

//自注册zookeeper节点检查
void HallSrv::onTimerCheckSelf() {
	if (ZNONODE == zkclient_->existsNode("/GAME"))
		zkclient_->createNode("/GAME", "GAME"/*, true*/);
	if (ZNONODE == zkclient_->existsNode("/GAME/HallServers"))
		zkclient_->createNode("/GAME/HallServers", "HallServers"/*, true*/);
	if (ZNONODE == zkclient_->existsNode(nodePath_)) {
		LOG_ERROR << __FUNCTION__ << " " << nodePath_;
		zkclient_->createNode(nodePath_, nodeValue_, true);
	}
	//自注册zookeeper节点检查
	threadTimer_->getLoop()->runAfter(5.0f, std::bind(&HallSrv::onTimerCheckSelf, this));
}

//MongoDB/RedisCluster/RedisLock
void HallSrv::threadInit()
{
	initRedisCluster();
	initMongoDB();
	initRedisLock();
}

//启动worker线程
//启动TCP监听网关客户端
void HallSrv::start(int numThreads, int numWorkerThreads, int maxSize)
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
		threadPool->setThreadInitCallback(std::bind(&HallSrv::threadInit, this));
		threadPool->setMaxQueueSize(maxSize);
		threadPool->start(1);
		threadPool_.push_back(threadPool);
	}

	LOG_INFO << __FUNCTION__ << " --- *** "
		<< "\HallSrv = " << server_.ipPort()
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
	threadTimer_->getLoop()->runAfter(3.0f, std::bind(&HallSrv::db_refresh_game_room_info, this));
	//redis刷新所有房间游戏人数
	threadTimer_->getLoop()->runAfter(30, std::bind(&HallSrv::redis_refresh_room_player_nums, this));
}

//大厅服[S]端 <- 网关服[C]端
void HallSrv::onConnection(const muduo::net::TcpConnectionPtr& conn) {
    
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

//大厅服[S]端 <- 网关服[C]端
void HallSrv::onMessage(
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
			int index = hash_session_(session) % threadPool_.size();
			threadPool_[index]->run(
				std::bind(
					&HallSrv::asyncClientHandler,
					this,
					muduo::net::WeakTcpConnectionPtr(conn), buffer, receiveTime));
		}
		//数据包不足够解析，等待下次接收再解析
		else /*if (likely(len > buf->readableBytes()))*/ {
			break;
		}
    }
}

//大厅服[S]端 <- 网关服[C]端
void HallSrv::asyncClientHandler(
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
		case Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_HALL:
		case Game::Common::MAINID::MAIN_MESSAGE_PROXY_TO_HALL:
		case Game::Common::MAINID::MAIN_MESSAGE_HTTP_TO_SERVER: {
                switch(header->enctype) {
				case packet::PUBENC_PROTOBUF_NONE: {
					//NONE
					TraceMessageID(header->mainID, header->subID);
					int cmd = packet::enword(header->mainID, header->subID);
					CmdCallbacks::const_iterator it = handlers_.find(cmd);
					if (it != handlers_.end()) {
                            
						CmdCallback const& handler = it->second;
						handler(peer,
							(uint8_t const*)header + packet::kHeaderLen,
							header->len - packet::kHeaderLen,
							header, pre_header);
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
            case Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_PROXY:
            case Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER:
            case Game::Common::MAINID::MAIN_MESSAGE_CLIENT_TO_GAME_LOGIC:
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