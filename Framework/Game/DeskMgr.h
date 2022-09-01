#ifndef DESKMGR_INCLUDE_H
#define DESKMGR_INCLUDE_H

#include <boost/serialization/singleton.hpp>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/EventLoopThreadPool.h>
#include <vector>
#include <map>
#include <list>
#include <stdint.h>

#include "GameDefine.h"

using boost::serialization::singleton;

class IPlayer;

/// <summary>
/// 桌子管理类
/// </summary>
class CDeskMgr : public singleton<CDeskMgr> {
public:
    CDeskMgr();
    virtual ~CDeskMgr();
    void Clear();
    /// <summary>
    /// 初始化
    /// </summary>
    /// <param name="gameInfo">游戏类型</param>
    /// <param name="roomInfo">房间信息</param>
    /// <param name="threadTimer">桌子定时器</param>
    /// <param name="logicThread">桌子逻辑线程</param>
    void Init(
        GameInfo* gameInfo,
        RoomInfo* roomInfo,
        std::shared_ptr<muduo::net::EventLoopThread>& threadTimer,
        ILogicThread* logicThread,
        std::shared_ptr<muduo::net::EventLoopThreadPool>& logicThreads);
    /// <summary>
    /// 获取指定桌子
    /// </summary>
    /// <param name="deskID">桌子ID</param>
    /// <returns></returns>
    std::shared_ptr<IDesk> GetDesk(uint32_t deskID);
    /// <summary>
    /// 新开辟n张桌子
    /// </summary>
    /// <param name="count"></param>
    /// <returns></returns>
    int MakeSuitDesk(int count);
	/// <summary>
	/// 查找合适的桌子，没有则开辟一张出来
	/// </summary>
	/// <param name="pUser"></param>
	/// <param name="ignoreDeskID"></param>
	/// <returns></returns>
	std::shared_ptr<IDesk> FindSuitDesk(std::shared_ptr<IPlayer>& pUser, uint32_t ignoreDeskID = INVALID_DESK);
	/// <summary>
	/// 获取可用的桌子
	/// </summary>
	/// <param name="loop"></param>
	/// <param name="usedDesks"></param>
	void GetUsedDesks(
		muduo::net::EventLoop* loop,
		std::list<std::shared_ptr<IDesk>>& usedDesks);
    /// <summary>
    /// 回收桌子
    /// </summary>
    /// <param name="deskID">桌子ID</param>
    void FreeDesk(uint32_t deskID);
	/// <summary>
	/// 踢出桌子全部玩家
	/// </summary>
	void KickAllPlayers();
public:
	GameInfo* gameInfo_;
	RoomInfo* roomInfo_;
    //desks_[deskid] = desk
    std::vector<std::shared_ptr<IDesk>> desks_;
    std::list<std::shared_ptr<IDesk>> usedDesks_;
    std::list<std::shared_ptr<IDesk>> freeDesks_;
    mutable boost::shared_mutex mutex_;
};

#endif