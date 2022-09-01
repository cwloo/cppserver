#include <muduo/base/Logging.h>

#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>

#include <boost/filesystem.hpp>

#include "public/MongoDB/MongoDBClient.h"
#include "public/RedisClient/RedisClient.h"

#include "IDesk.h"
#include "IDeskDelegate.h"
#include "IPlayer.h"
#include "IRobotDelegate.h"
#include "IReplayRecord.h"

#include "Player.h"
#include "Robot.h"
#include "RobotMgr.h"
#include "Desk.h"
#include "DeskMgr.h"

#include "public/Utils.h"
#include "public/StdRandom.h"
#include "public/Global.h"

#include "proto/GameServer.Message.pb.h"

CRobotMgr::CRobotMgr() :
    gameInfo_(NULL)
    , roomInfo_(NULL)
    , logicThread_(NULL) {
}

CRobotMgr::~CRobotMgr()
{
    usedRobots_.clear();
    freeRobots_.clear();
    
    gameInfo_ = NULL;
    roomInfo_ = NULL;
    logicThread_ = NULL;
}
/// <summary>
/// 机器人初始化
/// </summary>
/// <param name="gameInfo">游戏类型</param>
/// <param name="roomInfo">房间信息</param>
/// <param name="logicThread">逻辑线程</param>
void CRobotMgr::Init(GameInfo* gameInfo, RoomInfo * roomInfo, ILogicThread* logicThread) {
    gameInfo_  = gameInfo;//游戏类型
    roomInfo_  = roomInfo;//房间信息
    logicThread_  = logicThread;//逻辑线程
	assert(gameInfo && roomInfo && logicThread);
    //加载子游戏机器人代理so库模块
    std::string strDllPath = boost::filesystem::initial_path<boost::filesystem::path>().string();
    strDllPath.append("/");
    //./libGame_zjh_score_android.so
    std::string strDllName = clearDllPrefix(gameInfo->gameServiceName);
    strDllName.append("_score_android");
    strDllName.insert(0, "./lib");
    strDllName.append(".so");
    strDllName.insert(0, strDllPath);
	LOG_DEBUG << __FUNCTION__ << " >>> Load " << strDllName;
	getchar();
    void* dlhandle = dlopen(strDllName.c_str(), RTLD_LAZY);
    if(!dlhandle) {
        char buf[BUFSIZ] = { 0 };
        snprintf(buf, BUFSIZ, " Can't Open %s, %s", strDllName.c_str(), dlerror());
        LOG_ERROR << __FUNCTION__ << buf;
        exit(0);
    }
	RobotDelegateCreator createRobotDelegate = (RobotDelegateCreator)dlsym(dlhandle, FuncNameCreateRobot);
	if (!createRobotDelegate) {
        dlclose(dlhandle);
		char buf[BUFSIZ] = { 0 };
		snprintf(buf, BUFSIZ, " Can't Find %s, %s", FuncNameCreateRobot, dlerror());
        LOG_ERROR << __FUNCTION__ << buf;
		exit(0);
	}
	//db加载机器人
	try {
		//android_strategy 机器人策略
		mongocxx::collection androidStrategy = MONGODBCLIENT["gameconfig"]["android_strategy"];
		bsoncxx::document::value query_value = document{} << "gameid" << (int32_t)roomInfo_->gameId << "roomid" << (int32_t)roomInfo_->roomId << finalize;
		bsoncxx::stdx::optional<bsoncxx::document::value> result = androidStrategy.find_one(query_value.view());
		if (result) {
			bsoncxx::document::view view = result->view();
			LOG_DEBUG << "Query Android Strategy:" << bsoncxx::to_json(view);
			robotStrategy_.gameId = view["gameid"].get_int32();
			robotStrategy_.roomId = view["roomid"].get_int32();
			robotStrategy_.exitLowScore = view["exitLowScore"].get_int64();
			robotStrategy_.exitHighScore = view["exitHighScore"].get_int64();
			robotStrategy_.minScore = view["minScore"].get_int64();
			robotStrategy_.maxScore = view["maxScore"].get_int64();
			auto arr = view["areas"].get_array();
			for (auto& elem : arr.value) {
				RobotStrategyArea area;
				area.weight = elem["weight"].get_int32();
				area.lowTimes = elem["lowTimes"].get_int32();
				area.highTimes = elem["highTimes"].get_int32();
				robotStrategy_.areas.emplace_back(area);
			}
		}
		uint32_t robotCount = 0;
		//房间桌子数 * 每张桌子最大机器人数
		uint32_t maxRobottCount = roomInfo_->deskCount * roomInfo_->androidCount;
		//android_user 机器人账号
		mongocxx::collection coll = MONGODBCLIENT["gameconfig"]["android_user"];
		mongocxx::cursor cursor = coll.find({});
		for (auto& doc : cursor) {
			LOG_DEBUG << "QueryResult:" << bsoncxx::to_json(doc);
			
			UserBaseInfo userInfo;
			userInfo.userID = doc["userId"].get_int64();
			userInfo.account = doc["account"].get_utf8().value.to_string();
			userInfo.headId = doc["headId"].get_int32();
			userInfo.nickName = doc["nickName"].get_utf8().value.to_string();
			userInfo.takeMinScore = robotStrategy_.minScore;
			userInfo.takeMaxScore = robotStrategy_.maxScore;
			userInfo.userScore = weight_.rand_.betweenInt64(robotStrategy_.minScore, robotStrategy_.maxScore).randInt64_mt();//随机机器人携带积分
			userInfo.agentid = 0;
			userInfo.status = 0;
			userInfo.location = doc["location"].get_utf8().value.to_string();
			//创建机器人
			std::shared_ptr<IPlayer> robot(new CRobot());
			//创建子游戏机器人代理
			std::shared_ptr<IRobotDelegate> robotDelegate = createRobotDelegate();
			if (!robot || !robotDelegate) {
				LOG_ERROR << __FUNCTION__ << " create robot userid = " << userInfo.userID << " Failed!";
				break;
			}
			std::dynamic_pointer_cast<CRobot>(robot)->SetDelegate(robotDelegate);
			std::dynamic_pointer_cast<CRobot>(robot)->SetUserBaseInfo(userInfo);
			robotDelegate->SetPlayer(robot);
			robotDelegate->SetRobotStrategy(&robotStrategy_);
			{
				//WRITE_LOCK(mutex_);
				freeRobots_.push_back(robot);
			}
			if (++robotCount >= maxRobottCount) {
				break;
			}
			LOG_DEBUG << __FUNCTION__ << " Preload Robot, userID:" << userInfo.userID << ", score:" << userInfo.userScore;
		}
		LOG_INFO << __FUNCTION__ << " Preload Robot count: " << robotCount << " maxCount: " << maxRobottCount;
	}
	catch (std::exception& e) {
		LOG_ERROR << __FUNCTION__ << " exception:" << e.what();
	}
}

/// <summary>
/// 取出一个机器人
/// </summary>
/// <returns></returns>
std::shared_ptr<IPlayer> CRobotMgr::Pick() {
	std::shared_ptr<IPlayer> robot;
	WRITE_LOCK(mutex_);
	if (!freeRobots_.empty()) {
		robot = freeRobots_.front();
		freeRobots_.pop_front();
		robot->Reset();
		usedRobots_.push_back(robot);
	}
// 	else {
// 		try {
// 			robot = std::shared_ptr<IPlayer>(new CRobot());
// 		}
// 		catch (...) {
// 			assert(false);
// 		}
// 	}
// 	{
// 		//WRITE_LOCK(mutex_);
// 		usedRobots_.push_back(robot);
// 	}
	return robot;
}

/// <summary>
/// 回收机器人对象
/// </summary>
/// <param name="userID">用户ID</param>
/// <returns></returns>
bool CRobotMgr::Delete(int64_t userID) {
	WRITE_LOCK(mutex_);
	for (auto it = usedRobots_.begin(); it != usedRobots_.end(); ++it) {
		if ((*it)->GetUserID() == userID) {
			std::shared_ptr<IPlayer> robot = *it;
			usedRobots_.erase(it);
			std::shared_ptr<IRobotDelegate> robotDelegate =
				std::dynamic_pointer_cast<CRobot>(robot)->GetDelegate();
			if (robotDelegate) {
				robotDelegate->Reposition();
			}
			freeRobots_.push_back(robot);
			return true;
		}
	}
	return false;
}

/// <summary>
/// 机器人库存
/// </summary>
/// <returns></returns>
bool CRobotMgr::Empty() {
	READ_LOCK(mutex_);
	return freeRobots_.empty();
}

/// <summary>
/// 机器人定时入桌
/// </summary>
void CRobotMgr::OnTimerRobotEnter(muduo::net::EventLoop* loop) {
	if (roomInfo_->serverStatus == SERVER_STOPPED) {
		LOG_WARN << __FUNCTION__ << " SERVER IS STOPPING...";
		return;
	}
	if (!roomInfo_->bEnableAndroid) {
		LOG_ERROR << __FUNCTION__ << " 机器人被禁用了";
		return;
	}
	if (Empty()) {
		LOG_ERROR << __FUNCTION__ << " 机器人没有库存了";
		return;
	}
	std::list<std::shared_ptr<IDesk>> usedDesks;
	CDeskMgr::get_mutable_instance().GetUsedDesks(loop, usedDesks);
	for (auto it : usedDesks) {
		std::shared_ptr<CDesk> pDesk = std::dynamic_pointer_cast<CDesk>(it);
		if (pDesk) {
			if (pDesk->checkRobotEnter() < 0) {
				break;
			}
		}
		else {
			assert(false);
		}
	}
}
