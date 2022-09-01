#ifndef ROBOTMGR_INCLUDE_H
#define ROBOTMGR_INCLUDE_H

#include <list>
#include <set>
#include <map>
#include <deque>

#include <boost/serialization/singleton.hpp>

#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <muduo/net/TcpClient.h>

#include "GameDefine.h"
#include "public/StdRandom.h"

class IRobotDelegate;
class IPlayer;

using boost::serialization::singleton;

/// <summary>
/// 机器人管理类
/// </summary>
class CRobotMgr : public singleton<CRobotMgr> {
public:
    CRobotMgr();
    virtual ~CRobotMgr();
public:
    /// <summary>
    /// 机器人初始化
    /// </summary>
    /// <param name="gameInfo">游戏类型</param>
    /// <param name="roomInfo">房间信息</param>
    /// <param name="logicThread">逻辑线程</param>
    void Init(GameInfo* gameInfo, RoomInfo * roomInfo, ILogicThread* logicThread);
	/// <summary>
	/// 取出一个机器人
	/// </summary>
	/// <returns></returns>
	std::shared_ptr<IPlayer> Pick();
    /// <summary>
    /// 回收机器人对象
    /// </summary>
    /// <param name="userID">用户ID</param>
    /// <returns></returns>
    bool Delete(int64_t userID);
	/// <summary>
    /// 机器人库存
    /// </summary>
    /// <returns></returns>
    bool Empty();
	/// <summary>
	/// 机器人定时入桌
	/// </summary>
	void OnTimerRobotEnter(muduo::net::EventLoop* loop);
public:
    GameInfo* gameInfo_; //游戏类型
    RoomInfo* roomInfo_;//房间信息
    RobotStrategyParam robotStrategy_;//机器人策略
    ILogicThread* logicThread_;//逻辑线程
    std::list<std::shared_ptr<IPlayer>> usedRobots_;
    std::list<std::shared_ptr<IPlayer>> freeRobots_;
    mutable boost::shared_mutex mutex_;
    STD::Weight weight_;
};

typedef std::shared_ptr<IRobotDelegate>(*RobotDelegateCreator)();   //生成机器人
typedef void* (*RobotDelegateDeleter)(IRobotDelegate* robotDelegate);//删除机器人

#endif