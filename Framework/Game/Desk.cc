#include <sys/time.h>
#include <stdarg.h>
#include <memory>
#include <atomic>

#include <boost/bind.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>


#include <muduo/base/Logging.h>

#include "public/SubNetIP.h"

//#include "json/json.h"
//#include "crypto/crypto.h"

//#include "public/Global.h"
#include "GameDefine.h"

#include "IDesk.h"
#include "IDeskDelegate.h"
#include "IPlayer.h"
#include "IRobotDelegate.h"
#include "IReplayRecord.h"

#include "Desk.h"
#include "DeskMgr.h"
#include "Player.h"
#include "PlayerMgr.h"
#include "Robot.h"
#include "RobotMgr.h"

#include "public/MongoDB/MongoDBClient.h"
#include "public/RedisClient/RedisClient.h"

#include "proto/GameServer.Message.pb.h"
#include "public/TraceMsg/TraceMsg.h"
#include "public/StdRandom.h"
//#include "public/TaskService.h"

extern int g_bisDebug;
extern int g_bisDisEncrypt;


atomic_llong  CDesk::curStorage_(0);
double        CDesk::lowStorage_(0);
double        CDesk::highStorage_(0);

int           CDesk::sysKillAllRatio_(0);
int           CDesk::sysReduceRatio_(0);
int           CDesk::sysChangeCardRatio_(0);


CDesk::CDesk()
        : deskDelegate_(NULL)
        , gameInfo_(NULL)
        , roomInfo_(NULL)
        , logicThread_(NULL)
        , status_(GAME_STATUS_INIT)
        , loop_(NULL) {
    players_.clear();
    memset(&deskState_,0,sizeof(deskState_));
}

CDesk::~CDesk() {
    players_.clear();
}

/// <summary>
/// 桌子初始化
/// </summary>
/// <param name="deskDelegate">子游戏桌子代理</param>
/// <param name="deskState">桌子特殊状态</param>
/// <param name="gameInfo">游戏类型</param>
/// <param name="roomInfo">游戏房间</param>
/// <param name="threadTimer">桌子定时器</param>
/// <param name="logicThread">桌子逻辑线程</param>
void CDesk::Init(
    muduo::net::EventLoop* loop,
    std::shared_ptr<IDeskDelegate>& deskDelegate,
    DeskState& deskState,
    GameInfo* gameInfo,
    RoomInfo* roomInfo,
    std::shared_ptr<muduo::net::EventLoopThread>& threadTimer,
    ILogicThread* logicThread) {
    loop_ = loop;
	deskDelegate_ = deskDelegate;
	gameInfo_ = gameInfo;
	roomInfo_ = roomInfo;
	logicThread_ = logicThread;
	threadTimer_ = threadTimer;
	deskState_ = deskState;
	players_.resize(roomInfo_->maxPlayerNum);
    //初始化玩家动态数组 players[chairid]
	for (int i = 0; i < roomInfo_->maxPlayerNum; ++i) {
		players_[i] = std::shared_ptr<IPlayer>();
        freevec_.push_back(i);
	}
}

/// <summary>
/// 桌子牌局编号
/// </summary>
/// <returns></returns>
std::string CDesk::NewRoundID() {
    //roomid-timestamp-pid-deskid-rand
    std::string strRoundId = to_string(roomInfo_->roomId) + "-";
    int64_t seconds_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    strRoundId += std::to_string(seconds_since_epoch) + "-";
    strRoundId += std::to_string(::getpid()) + "-";
    strRoundId += std::to_string(GetDeskID())+"-";
    strRoundId += std::to_string(rand()%10);
    return strRoundId;
}

/// <summary>
/// 机器人入桌
/// </summary>
/// <returns></returns>
int CDesk::checkRobotEnter() {
    
    assert(loop_);
    assert(loop_->assertInLoopThread());

    RobotStrategyParam& robotStrategy = CRobotMgr::get_mutable_instance().robotStrategy_;
	uint32_t maxNum = roomInfo_->maxPlayerNum;
	int32_t maxRobot = roomInfo_->androidCount;
	
    uint32_t realNum = 0, robotNum = 0;
	GetPlayerCount(realNum, robotNum);
	uint32_t curNum = realNum + robotNum;
	uint32_t deskID = GetDeskID();
    //百人场
	if (gameInfo_->gameType == GameType_BaiRen) {
        //隔段时间计算机器人波动系数
		static double_t robotEnterPercentage = 0;
		static time_t last = 0;
		time_t now = time(NULL);
		if (now - last > 3600) {
			struct tm* local = localtime(&now);
			uint8_t hour = (int)local->tm_hour;
			float ra = ((random() % 10) + 95) * 0.01;//随机浮动一定比例0.9~1.1
			robotEnterPercentage = roomInfo_->enterAndroidPercentage[hour] ?
				roomInfo_->enterAndroidPercentage[hour] * (ra) : 0.5 * (ra);
			last = now;
		}
		//机器人数随时间变化
		maxRobot *= robotEnterPercentage;
		//进入桌子的真人越多，允许进的机器人就越少
		if (roomInfo_->realChangeAndroid > 0) {
			maxRobot -= (int)realNum / roomInfo_->realChangeAndroid;
		}
		if (curNum < maxNum && robotNum < maxRobot) {
			std::shared_ptr<IPlayer> pUser = std::make_shared<CRobot>();
			if (!CanUserEnter(pUser)) {
				return 1;
			}
			int32_t needRobot = maxRobot - robotNum;
			//每次最多进5个
			needRobot = needRobot > 5 ? 5 : needRobot;
			for (int i = 0; i < needRobot; ++i) {
				//取出一个机器人
				std::shared_ptr<IPlayer> robot = CRobotMgr::get_mutable_instance().Pick();
				if (!robot) {
					LOG_ERROR << __FUNCTION__ << " 机器人没有库存了";
					return -1;
				}
				int weight[robotStrategy.areas.size()];
				for (int j = 0; j < robotStrategy.areas.size(); ++j) {
					weight[j] = robotStrategy.areas[j].weight;
				}
                weight_.init(weight, robotStrategy.areas.size());
                weight_.shuffle();
				int x = weight_.getResult();
				int64_t minScore = std::dynamic_pointer_cast<CRobot>(robot)->GetTakeMinScore();
				int64_t minLine = robotStrategy.areas[x].lowTimes * minScore / 100;
				int64_t highLine = robotStrategy.areas[x].highTimes * minScore / 100;
				int64_t score = weight_.rand_.betweenInt64(minLine, highLine).randInt64_mt();
				robot->SetScore(score);
				std::shared_ptr<IRobotDelegate> robotDelegate = robot->GetDelegate();
				if (robotDelegate) {
                    std::shared_ptr<IDesk> pDesk = shared_from_this();
				    robotDelegate->SetDesk(pDesk);
				}
				//机器人入桌
                OnUserEnterAction(muduo::net::WeakTcpConnectionPtr(), 0, robot, false);
				//让每次都只进一个机器人
				//break;
			}
		}
	}
	else {
		//对战类
		if (realNum > 0 && curNum < maxNum) {
			//非百人游戏匹配前3.6秒都必须等待玩家加入桌子，禁入机器人，定时器到时空缺的机器人一次性填补
			//如果定时器触发前，真实玩家都齐了，秒开
			//处理逻辑在各个子游戏模块 CanUserLeave 中处理
			std::shared_ptr<IPlayer> pUser = std::make_shared<CRobot>();
			if (!CanUserEnter(pUser)) {
				return 1;
			}
			//每次最多进1个
			uint32_t needNum = maxNum - curNum;
			needNum = needNum > 1 ? 1 : needNum;
			for (int i = 0; i < needNum; ++i) {
				//取出一个机器人
				std::shared_ptr<IPlayer> robot = CRobotMgr::get_mutable_instance().Pick();
				if (!robot) {
					LOG_ERROR << __FUNCTION__ << " 机器人没有库存了";
					return -1;
				}
				int weight[robotStrategy.areas.size()];
				for (int j = 0; j < robotStrategy.areas.size(); ++j) {
					weight[j] = robotStrategy.areas[j].weight;
				}
                weight_.init(weight, robotStrategy.areas.size());
                weight_.shuffle();
				int x = weight_.getResult();
				int64_t minScore = std::dynamic_pointer_cast<CRobot>(robot)->GetTakeMinScore();
				int64_t minLine = robotStrategy.areas[x].lowTimes * minScore / 100;
				int64_t highLine = robotStrategy.areas[x].highTimes * minScore / 100;
				int64_t score = weight_.rand_.betweenInt64(minLine, highLine).randInt64_mt();
				robot->SetScore(score);
				std::shared_ptr<IRobotDelegate> robotDelegate = robot->GetDelegate();
				if (robotDelegate) {
					std::shared_ptr<IDesk> pDesk = shared_from_this();
					robotDelegate->SetDesk(pDesk);
				}
				//机器人入桌
                OnUserEnterAction(muduo::net::WeakTcpConnectionPtr(), 0, robot, false);
				//让每次都只进一个机器人
				break;
			}
		}
	}
    return 0;
}

bool CDesk::sendData(std::shared_ptr<IPlayer>& pPlayer, uint8_t mainId, uint8_t subId,
    std::vector<uint8_t> &data, packet::header_t const *header, int enctype) {
    return false;
}

bool CDesk::SendData(uint32_t chairID, uint8_t subid, const uint8_t* data, int size, bool bRecord /* = true */)
{
    return false;
}

bool CDesk::SendData(std::shared_ptr<IPlayer>& pPlayer, uint8_t subid, const uint8_t* data, int datasize, bool isRecord)
{
    return false;
}

bool CDesk::SendGameMessage(uint32_t chairID, const std::string& szMessage, uint8_t msgType, int64_t score) {
    return false;
}

void CDesk::GetDeskInfo(DeskState& deskState)
{
    deskState = deskState_;
}

uint32_t CDesk::GetDeskID()
{
    return deskState_.deskID;
}

std::shared_ptr<muduo::net::EventLoopThread> CDesk::GetLoopThread() {
    return threadTimer_;
}

bool CDesk::DismissGame()
{
   deskDelegate_->OnGameConclude(INVALID_CHAIR, GER_DISMISS);
   return true;
}

bool CDesk::ConcludeGame(uint8_t gameStatus)
{
    status_ = gameStatus;
    if( gameInfo_->matchforbids[MTH_PLAYER_CNT] )
    {
        bool onlyOneGuy = GetPlayerCount(false) == 1;
        uint32_t maxPlayerNum = roomInfo_->maxPlayerNum;
        for(uint32_t i = 0; i < maxPlayerNum -1; ++i)
        {
            if(players_[i] && !players_[i]->IsRobot())
            {
                if(onlyOneGuy)
                {
                    REDISCLIENT.AddToMatchedUser(players_[i]->GetUserID(), 0);
                    break;
                }
                for(uint32_t j = i + 1; j < maxPlayerNum; ++j)
                {
                    if(players_[j] && !players_[j]->IsRobot())
                    {
                        REDISCLIENT.AddToMatchedUser(players_[i]->GetUserID(), players_[j]->GetUserID());
                    }
                }
            }
        }
    }
    /*
    // add by James 180907-clear table user on conclude game.
    if (m_pRoomKindInfo->bisQipai)
    {
        word wMaxPlayer = m_pRoomKindInfo->nStartMaxPlayer;
        for (int i = 0; i < wMaxPlayer; ++i)
        {
            CPlayer* pPlayer = players_[i];
            if (pPlayer)
            {
                if (pPlayer->GetStatus() == sOffline)
                {
                    ClearPlayer(pPlayer->GetChairID());
                }
                else
                {
                    // check if auto ready content now.
                    if (m_pRoomKindInfo->bisAutoReady)
                    {
                        // reset the user state to sit down now.
                        pPlayer->SetStatus(sReady);
                    }   else
                    {
                        // reset the user state to sit state.
                        pPlayer->SetStatus(sSit);
                        LOG_ERROR << "Sit2";

                    }
                }
            }
        }
    }
    */

    // modify by James end.
    if (deskDelegate_)
    {
        deskDelegate_->Reposition();
    }

    return !logicThread_->IsServerStoped();
}

int64_t CDesk::CalculateRevenue(int64_t score)
{
    return score * gameInfo_->revenueRatio / 100;
}

std::shared_ptr<IPlayer> CDesk::GetPlayer(uint32_t chairID)
{
    std::shared_ptr<IPlayer> pPlayer;
//    pPlayer.reset();
    if(chairID < roomInfo_->maxPlayerNum)
    {
//        READ_LOCK(m_list_mutex);
        pPlayer = players_[chairID];
    }
    return pPlayer;
}

std::shared_ptr<IPlayer> CDesk::GetPlayerBy(int64_t userID)
{
    std::shared_ptr<IPlayer> pPlayer;
//    pPlayer.reset();

//    READ_LOCK(m_list_mutex);
    for (int i = 0; i < roomInfo_->maxPlayerNum; ++i)
    {
        // check the special user item data value now.
        std::shared_ptr<IPlayer> pITempUserItem = players_[i];
        if(pITempUserItem && pITempUserItem->GetUserID() == userID)
        {
            return pITempUserItem;
        }
    }
    // LOG_DEBUG << "GetPlayerBy userid:" << userid << ", pPlayer:" << pPlayer;
    return pPlayer;
}

bool CDesk::IsExist(uint32_t chairID)
{
    bool bExist = false;
    do
    {
        if ((chairID < 0) || (chairID >= roomInfo_->maxPlayerNum))
            break;

        std::shared_ptr<IPlayer> pPlayer;
        {
//            READ_LOCK(m_list_mutex);
            pPlayer = players_[chairID];
        }

        if ((pPlayer) && (pPlayer->GetUserID() > 0))
        {
            bExist = true;
        }

    }while(0);
    return bExist;
}

void CDesk::SetGameStatus(uint8_t cbStatus)
{
	//LOG_DEBUG << "--- *** " << (int)cbStatus;
   status_ = cbStatus;
}

// get the special game status now.
uint8_t CDesk::GetGameStatus()
{
    return status_;
}

bool CDesk::CanUserEnter(std::shared_ptr<IPlayer>& pUser)
{
    return deskDelegate_->CanUserEnter(pUser);
}

bool CDesk::CanUserLeave(int64_t userID)
{
    return deskDelegate_->CanUserLeave(userID);
}

void CDesk::SetUserTrustee(uint32_t chairID, bool bTrustee)
{
    if(chairID < roomInfo_->maxPlayerNum)
    {
        std::shared_ptr<IPlayer> pPlayer;
        {
//            READ_LOCK(m_list_mutex);
            pPlayer = players_[chairID];
        }
        if (pPlayer)
        {
            pPlayer->SetTrustee(bTrustee);
        }
    }
    return;
}

bool CDesk::GetUserTrustee(uint32_t chairID)
{
    bool bTrustee = false;

    if(chairID < roomInfo_->maxPlayerNum)
    {
        std::shared_ptr<IPlayer> pPlayer;
        {
//            READ_LOCK(m_list_mutex);
            pPlayer = players_[chairID];
        }
        if(pPlayer)
        {
            bTrustee = pPlayer->GetTrustee();
        }
    }
    return bTrustee;
}

bool CDesk::OnUserLeave(std::shared_ptr<IPlayer>& pPlayer, bool bSendToSelf, bool bForceLeave)
{
    bool ret = false;
    if(pPlayer)
    {
        LOG_DEBUG << __FILE__ << __FUNCTION__ << "CDesk::OnUserLeft"<<pPlayer->GetUserID();
//        if(gameInfo_->gameType == GameType_BaiRen)
        {
            // 离线
            pPlayer->SetStatus(sOffline);
            // 托管
            pPlayer->SetTrustee(true);
//            BroadcastUserStatus(pPlayer, bSendToSelf);
        }
        ret = deskDelegate_->OnUserLeave(pPlayer->GetUserID(), false);
    }
    return ret;
}

bool CDesk::OnUserOffline(std::shared_ptr<IPlayer>& pPlayer, bool bLeaveGs)
{
    bool ret = false;
    if(pPlayer)
    {
        LOG_DEBUG << __FILE__ << __FUNCTION__ << "CDesk::OnUserOffline"<<pPlayer->GetUserID();
        {
            pPlayer->SetStatus(sOffline);
            pPlayer->SetTrustee(true);
//            BroadcastUserStatus(pPlayer, false);
        }
        ret = deskDelegate_->OnUserLeave(pPlayer->GetUserID(), false);
    }
    return ret;
}

uint32_t CDesk::GetPlayerCount(bool bIncludeRobot)
{
    uint32_t count = 0;

//    READ_LOCK(m_list_mutex);
    for (int i = 0; i < roomInfo_->maxPlayerNum; ++i)
    {
        std::shared_ptr<IPlayer> pPlayer = players_[i];

        if(pPlayer)
        {
            if (pPlayer->IsRobot())
            {
                if(bIncludeRobot)
                {
                    ++count;
                }
            }else
            {
                 ++count;
            }
        }
    }
    return (count);
}

uint32_t CDesk::GetRobotCount()
{
    uint32_t count = 0;

//    READ_LOCK(m_list_mutex);
    for (int i = 0; i < roomInfo_->maxPlayerNum; ++i)
    {
        std::shared_ptr<IPlayer> pPlayer = players_[i];

        // check user item now.
        if(pPlayer  && pPlayer->IsRobot())
        {
            ++count;
        }
    }
    return (count);
}

uint32_t CDesk::GetMaxPlayerCount()
{
    return roomInfo_->maxPlayerNum;
}

void CDesk::GetPlayerCount(uint32_t &nRealCount, uint32_t &nRobotCount)
{
    nRealCount = 0;
    nRobotCount = 0;

//    READ_LOCK(m_list_mutex);
    for (int i =0 ; i < roomInfo_->maxPlayerNum; ++i)
    {
        std::shared_ptr<IPlayer> pPlayer = players_[i];

        // check if the validate.
        if(pPlayer)
        {
            // try to check if include android now.
            if(pPlayer->IsRobot())
                ++nRobotCount;
            else
                ++nRealCount;
        }
    }
    return;
}

RoomInfo* CDesk::GetGameRoomInfo()
{
    return roomInfo_;
}


void CDesk::ClearPlayer(uint32_t chairID, bool bSendState, bool bSendToSelf, uint8_t cbSendErrorCode)
{

}

/// <summary>
/// 是否机器人
/// </summary>
/// <param name="chairID">座位号</param>
/// <returns></returns>
bool CDesk::IsRobot(uint32_t chairID) {
	if (chairID < roomInfo_->maxPlayerNum) {
		std::shared_ptr<IPlayer> pPlayer;
		{
			//READ_LOCK(mutex_);
			pPlayer = players_[chairID];
		}
		if (pPlayer) {
			return pPlayer->IsRobot();
		}
	}
	return false;
}

/// <summary>
/// 子游戏桌子代理接收消息
/// </summary>
/// <param name="chairID">消息来自椅子ID</param>
/// <param name="subId">子命令ID</param>
/// <param name="data"></param>
/// <param name="size"></param>
/// <returns></returns>
bool CDesk::OnGameEvent(uint32_t chairID, uint8_t subId, const uint8_t *data, int size)
{
    if (deskDelegate_) {
        return deskDelegate_->OnGameMessage(chairID, subId, data, size);
    }
    return false;
}

bool CDesk::OnUserEnterAction(
	const muduo::net::WeakTcpConnectionPtr& weakConn,
	BufferPtr buf,
    std::shared_ptr<IPlayer> &pPlayer, bool bDistribute) {
    
    assert(loop_);
    assert(loop_->assertInLoopThread());

	assert(pPlayer && pPlayer->IsValid());
	muduo::net::TcpConnectionPtr conn(weakConn.lock());
	if (conn) {
		int chairID = allocChairID();
		assert(chairID != INVALID_CHAIR);
		pPlayer->SetDeskID(deskState_.deskID);
		pPlayer->SetChairID(chairID);
		players_[chairID] = pPlayer;
		pPlayer->SetStatus(sSit);
		if (!pPlayer->IsRobot()) {
			//应答
			GameServer::MSG_S2C_UserEnterMessageResponse rspdata;
			rspdata.mutable_header()->set_sign(HEADER_SIGN);
			rspdata.set_retcode(0);
			rspdata.set_errormsg("Enter Room OK.");

			packet::internal_prev_header_t const* pre_header_ = packet::get_pre_header(buf);
			packet::header_t const* header_ = packet::get_header(buf);

			size_t len = rspdata.ByteSizeLong();
			std::vector<uint8_t> data(packet::kPrevHeaderLen + packet::kHeaderLen + len);
			if (rspdata.SerializeToArray(&data[packet::kPrevHeaderLen + packet::kHeaderLen], len)) {

				packet::internal_prev_header_t* pre_header = (packet::internal_prev_header_t*)&data[0];
				packet::header_t* header = (packet::header_t*)(&data[0] + packet::kPrevHeaderLen);

				memcpy(pre_header, pre_header_, packet::kPrevHeaderLen);
				memcpy(header, header_, packet::kHeaderLen);

				header->mainID = ::Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_SERVER;
				header->subID = ::GameServer::SUB_S2C_ENTER_ROOM_RES;
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
	//BroadcastUserInfoToOther(pIServerUserItem);
	//SendAllOtherUserInfoToUser(pIServerUserItem);
	//BroadcastUserStatus(pIServerUserItem, true);
	deskDelegate_->OnUserEnter(pPlayer->GetUserID(), pPlayer->IsLookon());
}

bool CDesk::OnUserStandup(std::shared_ptr<IPlayer>& pPlayer, bool bSendState, bool bSendToSelf)
{
    return false;
}


void CDesk::BroadcastUserToOthers(std::shared_ptr<IPlayer> & pPlayer)
{
}

void CDesk::SendOthersToUser(std::shared_ptr<IPlayer>& pPlayer)
{
 
}

void CDesk::SendOtherToUser(std::shared_ptr<IPlayer>& pUser, UserInfo &userInfo)
{

}

void CDesk::BroadcastUserScore(std::shared_ptr<IPlayer>& pPlayer)
{
}

void CDesk::BroadcastUserStatus(std::shared_ptr<IPlayer> & pIUserItem, bool bSendTySelf)
{
 
}


/*
写玩家分
@param pScoreInfo 玩家分数结构体数组
@param nCount  写玩家个数
@param strRound 
*/
bool CDesk::WriteUserScore(UserScoreInfo* pScoreInfo, uint32_t nCount, string &strRound)
{
    return true;
}

bool CDesk::WriteBlacklistLog(std::shared_ptr<IPlayer>& pPlayer, int status)
{
    return true;
}

bool CDesk::WriteSpecialUserScore(SpecialScoreInfo* pSpecialScoreInfo, uint32_t nCount, string &strRound)
{
    return true;
}

bool CDesk::UpdateUserScoreToDB(int64_t userID, UserScoreInfo* pScoreInfo)
{
    return false;
}

bool CDesk::UpdateUserScoreToDB(int64_t userID, SpecialScoreInfo* pScoreInfo)
{
    return false;
}

bool CDesk::AddUserGameInfoToDB(UserBaseInfo &userBaseInfo, UserScoreInfo *scoreInfo, string &strRoundId, int32_t userCount, bool bAndroidUser)
{
 
}

bool CDesk::AddUserGameInfoToDB(SpecialScoreInfo *scoreInfo, string &strRoundId, int32_t userCount, bool bAndroidUser)
{

}

bool CDesk::AddScoreChangeRecordToDB(UserBaseInfo &userBaseInfo, int64_t sourceScore, int64_t addScore, int64_t targetScore)
{
    return false;
}

bool CDesk::AddScoreChangeRecordToDB(SpecialScoreInfo *scoreInfo)
{
    return false;
}

bool CDesk::AddUserGameLogToDB(UserBaseInfo &userBaseInfo, UserScoreInfo *scoreInfo, string &strRoundId)
{
    return false;
}

bool CDesk::AddUserGameLogToDB(SpecialScoreInfo *scoreInfo, string &strRoundId)
{
    return false;
}

int CDesk::UpdateStorageScore(int64_t changeStockScore)
{
    static int count = 0;
    curStorage_ += changeStockScore;
    if(++count > 0)
    {
        WriteGameChangeStorage(changeStockScore);
        count = 0;
    }
    return count;
}

bool CDesk::GetStorageScore(StorageInfo &storageInfo)
{
    storageInfo.lLowlimit           = roomInfo_->totalStockLowerLimit;;
    storageInfo.lUplimit            = roomInfo_->totalStockHighLimit;
    storageInfo.lEndStorage         = roomInfo_->totalStock;
    storageInfo.lSysAllKillRatio    = roomInfo_->systemKillAllRatio;
    storageInfo.lReduceUnit         = roomInfo_->systemReduceRatio;
    storageInfo.lSysChangeCardRatio = roomInfo_->changeCardRatio;

    return true;
}

bool CDesk::ReadStorageScore()
{
    curStorage_ = roomInfo_->totalStock;
    lowStorage_ = roomInfo_->totalStockLowerLimit;
    highStorage_ = roomInfo_->totalStockHighLimit;


    sysKillAllRatio_ = roomInfo_->systemKillAllRatio;
    sysReduceRatio_ = roomInfo_->systemReduceRatio;
    sysChangeCardRatio_ = roomInfo_->changeCardRatio;
    return true;
}

bool CDesk::WriteGameChangeStorage(int64_t changeStockScore)
{
    mongocxx::collection coll = MONGODBCLIENT["gameconfig"]["game_kind"];
    coll.update_one(document{} << "rooms.roomid" << (int32_t)roomInfo_->roomId << finalize,
                    document{} << "$inc" << open_document
                    <<"rooms.$.totalstock" << changeStockScore << close_document
                    << finalize);
    int64_t newStock;
    bool res = REDISCLIENT.hincrby(REDIS_CUR_STOCKS,to_string(roomInfo_->roomId),changeStockScore,&newStock);
    if(res)
    {
        //LOG_INFO << "Stock Changed From : "<< changeStockScore <<" + " << (int64_t)roomInfo_->totalStock << "=" << (int64_t)newStock;
        roomInfo_->totalStock = newStock;
        curStorage_ = newStock;
    }
    return true;
}

bool CDesk::SaveReplay(GameReplay& replay) {
    return replay.saveAsStream ? 
        SaveReplayDetailBlob(replay) :
        SaveReplayDetailJson(replay);
}

bool CDesk::SaveReplayDetailBlob(GameReplay& replay)
{
    if(replay.players.size() == 0 || !replay.players[0].flag)
    {
        return  false;
    }
	bsoncxx::builder::stream::document builder{};
	auto insert_value = builder
        //<< "gameid" << replay.gameid
		<< "gameinfoid" << replay.gameinfoid
		<< "timestamp" << bsoncxx::types::b_date{ chrono::system_clock::now() }
		<< "roomname" << replay.roomname
		<< "cellscore" << replay.cellscore
        << "detail" << bsoncxx::types::b_binary{ bsoncxx::binary_sub_type::k_binary, uint32_t(replay.detailsData.size()), reinterpret_cast<uint8_t const*>(replay.detailsData.data()) }
        << "players" << open_array;

    for(tagReplayPlayer &player : replay.players)
    {
        if(player.flag)
        {
            insert_value = insert_value << bsoncxx::builder::stream::open_document
                                        << "userid" << player.userid
                                        << "account" << player.accout
                                        << "score" << player.score
                                        << "chairid" << player.chairid
                                        << bsoncxx::builder::stream::close_document;
        }
    }
    insert_value << close_array << "steps" << open_array;
    for(tagReplayStep &step : replay.steps)
    {
        if(step.flag)
        {
            insert_value = insert_value << bsoncxx::builder::stream::open_document
                                        << "bet" << step.bet
                                        << "time" << step.time
                                        << "ty" << step.ty
                                        << "round" << step.round
                                        << "chairid" << step.chairID
                                        << "pos" << step.pos
                                        << bsoncxx::builder::stream::close_document;
        }
    }

    insert_value << close_array << "results" << open_array;

    for(tagReplayResult &result : replay.results)
    {
        if(result.flag)
        {
            insert_value = insert_value << bsoncxx::builder::stream::open_document
                                        << "chairid" << result.chairID
                                        << "bet" << result.bet
                                        << "pos" << result.pos
                                        << "win" << result.win
                                        << "cardtype" << result.cardtype
                                        << "isbanker" << result.isbanker
                                        << bsoncxx::builder::stream::close_document;
        }
    }
    auto after_array = insert_value << close_array;

    auto doc = after_array << bsoncxx::builder::stream::finalize;

    mongocxx::collection coll = MONGODBCLIENT["gamelog"]["game_replay"];
    bsoncxx::stdx::optional<mongocxx::result::insert_one> result = coll.insert_one(doc.view());
    return true;
}

bool CDesk::SaveReplayDetailJson(GameReplay& replay)
{
    return false;
}

void CDesk::DeleteUserToProxy(std::shared_ptr<IPlayer>& pPlayer, int32_t nKickType)
{
   
}

bool CDesk::DelUserOnlineInfo(int64_t userID, bool bonlyExpired)
{
    return false;
}

void CDesk::dumpUserList() {
    LOG_DEBUG << __FUNCTION__ << " dump user list:";
    int size = players_.size();
    for (int i = 0; i < size; ++i) {
        int64_t userID = 0;
        std::shared_ptr<IPlayer> pPlayer = players_[i];
        if (pPlayer != NULL) {
            userID = pPlayer->GetUserID();
        }
        LOG_DEBUG << " tableid[" << deskState_.deskID << "] chairid[" << i
                  << "] userid[" << userID << "]";
    }
}
