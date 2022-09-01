#include <muduo/base/Logging.h>

#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <memory>

#include <boost/filesystem.hpp>

#include "public/MongoDB/MongoDBClient.h"
#include "public/RedisClient/RedisClient.h"

#include "IDesk.h"
#include "IDeskDelegate.h"
#include "IPlayer.h"
#include "IRobotDelegate.h"
#include "IReplayRecord.h"

#include "Desk.h"
#include "DeskMgr.h"

#include "public/Utils.h"
#include "public/StdRandom.h"
#include "public/Global.h"

CDeskMgr::CDeskMgr()
	:roomInfo_(NULL)
	, gameInfo_(NULL) {
}

CDeskMgr::~CDeskMgr() {
	Clear();
}

void CDeskMgr::Clear() {
	desks_.clear();
	freeDesks_.clear();
	usedDesks_.clear();
}

/// <summary>
/// 初始化
/// </summary>
/// <param name="gameInfo">游戏类型</param>
/// <param name="roomInfo">房间信息</param>
/// <param name="threadTimer">桌子定时器</param>
/// <param name="logicThread">桌子逻辑线程</param>
void CDeskMgr::Init(
    GameInfo* gameInfo,
    RoomInfo* gameRoomInfo,
    std::shared_ptr<muduo::net::EventLoopThread> &threadTimer,
    ILogicThread* logicThread,
	std::shared_ptr<muduo::net::EventLoopThreadPool>& logicThreads) {
    gameInfo_ = gameInfo;
    roomInfo_ = gameRoomInfo;
	assert(gameInfo && gameRoomInfo && logicThread);
	//加载子游戏桌子代理so库模块
	std::string strDllPath = boost::filesystem::initial_path<boost::filesystem::path>().string();
	strDllPath.append("/");
	//./libGame_zjh.so
	std::string strDllName = clearDllPrefix(gameInfo_->gameServiceName);
	strDllName.insert(0, "./lib");
	strDllName.append(".so");
	strDllName.insert(0, strDllPath);
	LOG_DEBUG << __FUNCTION__ << " >>> Loading " << strDllName;
	getchar();
	void* dlhandle = dlopen(strDllName.c_str(), RTLD_LAZY);
	if (!dlhandle) {
		char buf[BUFSIZ] = { 0 };
		snprintf(buf, BUFSIZ, " Can't Open %s, %s", strDllName.c_str(), dlerror());
		LOG_ERROR << __FUNCTION__ << buf;
		exit(0);
	}
	 DeskDelegateCreator createDeskDelegate = (DeskDelegateCreator)dlsym(dlhandle, FuncNameCreateDesk);
	if (!createDeskDelegate) {
		dlclose(dlhandle);
		char buf[BUFSIZ] = { 0 };
		snprintf(buf, BUFSIZ, " Can't Find %s, %s", FuncNameCreateDesk, dlerror());
		LOG_ERROR << __FUNCTION__ << buf;
		exit(0);
	}
    //创建指定数量桌子
    for(uint32_t i = 0; i < roomInfo_->deskCount; ++i) {
        //创建桌子
         std::shared_ptr<IDesk> pDesk(new CDesk());
		//创建子游戏桌子代理
		std::shared_ptr<IDeskDelegate> deskDelegate = createDeskDelegate();
        if (!pDesk || !deskDelegate) {
            LOG_ERROR << __FUNCTION__ << " create deskID = " << i << " Failed!";
            break;
        }
		DeskState deskState = { 0 };
		deskState.deskID = i;
		deskState.bisLock = false;
		deskState.bisLookOn = false;
		pDesk->Init(logicThreads->getNextLoop(), deskDelegate, deskState, gameInfo, gameRoomInfo, threadTimer, logicThread);
		std::dynamic_pointer_cast<CDesk>(pDesk)->ReadStorageScore();
		deskDelegate->SetDesk(pDesk);
		desks_.push_back(pDesk);
		freeDesks_.push_back(pDesk);
    }
}

/// <summary>
/// 获取指定桌子
/// </summary>
/// <param name="deskID">桌子ID</param>
/// <returns></returns>
std::shared_ptr<IDesk> CDeskMgr::GetDesk(uint32_t deskID) {
	{
		READ_LOCK(mutex_);
		if (deskID != INVALID_DESK && deskID < desks_.size()) {
			return desks_[deskID];
		}
	}
	return std::shared_ptr<IDesk>();
}

/// <summary>
/// 新开辟n张桌子
/// </summary>
/// <param name="count"></param>
/// <returns></returns>
int CDeskMgr::MakeSuitDesk(int count) {
	int i = 0;
	{
		WRITE_LOCK(mutex_);
		for (; i < count; ++i) {
			if (!freeDesks_.empty()) {
				std::shared_ptr<IDesk> pDesk = freeDesks_.front();
				freeDesks_.pop_front();
				usedDesks_.push_back(pDesk);
			}
			else {
				break;
			}
		}
	}
	return i;
}

/// <summary>
/// 查找合适的桌子，没有则开辟一张出来
/// </summary>
/// <param name="pUser"></param>
/// <param name="ignoreDeskID"></param>
/// <returns></returns>
std::shared_ptr<IDesk> CDeskMgr::FindSuitDesk(std::shared_ptr<IPlayer>& pUser, uint32_t ignoreDeskID)
{
	std::list<std::shared_ptr<IDesk>> usedDesks;
	{
		READ_LOCK(mutex_);
		std::copy(usedDesks_.begin(), usedDesks_.end(), std::back_inserter(usedDesks));
	}
	//查找能进的桌子
	for (auto it : usedDesks) {
		std::shared_ptr<IDesk> pDesk = it;
		if (INVALID_DESK != ignoreDeskID && ignoreDeskID == pDesk->GetDeskID()) {
			continue;
		}
		if (pDesk->CanUserEnter(pUser)) {
			return pDesk;
		}
	}
	//找不到从空桌子里面取
	{
		WRITE_LOCK(mutex_);
		if (!freeDesks_.empty()) {
			std::shared_ptr<IDesk> pDesk = freeDesks_.front();
			freeDesks_.pop_front();
			usedDesks_.push_back(pDesk);
			return pDesk;
		}
	}
	return std::shared_ptr<CDesk>();
}

/// <summary>
/// 获取可用的桌子
/// </summary>
/// <param name="loop"></param>
/// <param name="usedDesks"></param>
void CDeskMgr::GetUsedDesks(
	muduo::net::EventLoop* loop,
	std::list<std::shared_ptr<IDesk>>& usedDesks) {
	usedDesks.clear();
	{
		WRITE_LOCK(mutex_);
		for (auto it : usedDesks_) {
			if (it->GetLoop() == loop) {
				usedDesks.push_back(it);
			}
		}
	}
}

/// <summary>
/// 回收桌子
/// </summary>
/// <param name="deskID">桌子ID</param>
void CDeskMgr::FreeDesk(uint32_t deskID) {
	WRITE_LOCK(mutex_);
	for (auto it = usedDesks_.begin(); it != usedDesks_.end(); ++it) {
		if ((*it)->GetDeskID() == deskID) {
			std::shared_ptr<IDesk> pDesk = *it;
			usedDesks_.erase(it);
			freeDesks_.push_back(pDesk);
			break;
		}
	}
}

/// <summary>
/// 踢出桌子全部玩家
/// </summary>
void CDeskMgr::KickAllPlayers() {
    roomInfo_->serverStatus = SERVER_STOPPED;
	std::list<std::shared_ptr<IDesk>> usedDesks;
	{
		READ_LOCK(mutex_);
		std::copy(usedDesks_.begin(), usedDesks_.end(), std::back_inserter(usedDesks));
	}
	for (auto it = usedDesks.begin(); it != usedDesks.end(); ++it) {
		std::shared_ptr<CDesk> pDesk = std::dynamic_pointer_cast<CDesk>(*it);
		if (pDesk) {
			RoomInfo* roomInfo = pDesk->GetGameRoomInfo();
			roomInfo->serverStatus = SERVER_STOPPED;
			pDesk->SetGameStatus(SERVER_STOPPED);
			//pDesk->DismissGame();
		}
	}
}