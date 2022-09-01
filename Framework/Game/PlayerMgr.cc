#include <muduo/base/Logging.h>

#include "IDesk.h"
#include "IDeskDelegate.h"
#include "IPlayer.h"
#include "IRobotDelegate.h"
//#include "IReplayRecord.h"

#include "Player.h"
#include "PlayerMgr.h"

#include "public/Global.h"

CPlayerMgr::CPlayerMgr()
	: roomInfo_(NULL) {
}

CPlayerMgr::~CPlayerMgr() {
    freeList_.clear();
    players_.clear();
}

void CPlayerMgr::Init(RoomInfo* roomInfo) {
	assert(roomInfo);
	roomInfo_ = roomInfo;
}

/// <summary>
/// 创建玩家对象
/// </summary>
/// <param name="userID">用户ID</param>
/// <returns>新创建玩家对象</returns>
std::shared_ptr<IPlayer> CPlayerMgr::New(int64_t userID) {
	{
		READ_LOCK(mutex_);
		assert(players_->find(userID) == players_.end());
	}
	std::shared_ptr<IPlayer> player;
	if (!freeList_.empty()) {
		{
			WRITE_LOCK(mutex_);
			player = freeList_.back();
			freeList_.pop_back();
		}
		player->Reset();
	}
	else {
		try {
			player = std::shared_ptr<IPlayer>(new CPlayer());
		}
		catch (...) {
			assert(false);
		}
	}
    {
        WRITE_LOCK(mutex_);
        players_[userID]  = player;
    }
    return player;
}

/// <summary>
/// 查找玩家对象
/// </summary>
/// <param name="userID">用户ID</param>
/// <returns>玩家对象</returns>
std::shared_ptr<IPlayer> CPlayerMgr::Get(int64_t userID) {
	{
		READ_LOCK(mutex_);
        std::map<int64_t, std::shared_ptr<IPlayer>>::iterator it = players_.find(userID);
		if (it != players_.end()) {
			return players_[userID];
		}
	}
	return std::shared_ptr<IPlayer>();
}

/// <summary>
/// 回收玩家对象
/// </summary>
/// <param name="userID">用户ID</param>
/// <returns></returns>
bool CPlayerMgr::Delete(int64_t userID) {
	WRITE_LOCK(mutex_);
	std::map<int64_t, std::shared_ptr<IPlayer>>::iterator it = players_.find(userID);
	if (it != players_.end()) {
		std::shared_ptr<IPlayer>& player = it->second;
		players_.erase(it);
		player->Reset();
		freeList_.push_back(player);
		return true;
	}
	return false;
}