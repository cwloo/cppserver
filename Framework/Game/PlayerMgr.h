#ifndef PLAYERMGR_INCLUDE_H
#define PLAYERMGR_INCLUDE_H

#include <map>
#include <list>
#include <stdint.h>

#include <boost/serialization/singleton.hpp>
#include <boost/pool/object_pool.hpp>

#include "GameDefine.h"

class IPlayer;

using boost::serialization::singleton;

/// <summary>
/// 玩家管理类
/// </summary>
class CPlayerMgr : public singleton<CPlayerMgr> {
public:
    CPlayerMgr();
    virtual ~CPlayerMgr();
public:
    void Init(RoomInfo* roomInfo);
    /// <summary>
    /// 创建玩家对象
    /// </summary>
    /// <param name="userID">用户ID</param>
    /// <returns>新创建玩家对象</returns>
    std::shared_ptr<IPlayer> New(int64_t userID);
    /// <summary>
    /// 查找玩家对象
    /// </summary>
    /// <param name="userID">用户ID</param>
    /// <returns>玩家对象</returns>
    std::shared_ptr<IPlayer> Get(int64_t userID);
    /// <summary>
    /// 回收玩家对象
    /// </summary>
    /// <param name="userID">用户ID</param>
    /// <returns></returns>
    bool Delete(int64_t userID);
protected:
    RoomInfo* roomInfo_;
	std::map<int64_t, std::shared_ptr<IPlayer>> players_;
    std::list<std::shared_ptr<IPlayer>> freeList_;
    mutable boost::shared_mutex mutex_;
};

#endif