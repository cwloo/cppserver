#ifndef PLAYER_INCLUDE_H
#define PLAYER_INCLUDE_H

#include <stdint.h>
#include <string.h>
#include <vector>
#include <atomic>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/ThreadPool.h>
#include <muduo/net/TcpServer.h>
#include <muduo/base/ThreadLocalSingleton.h>

#include <boost/shared_ptr.hpp>

#include "GameDefine.h"

class IPlayer;
class IRobotDelegate;

/// <summary>
/// 玩家类
/// </summary>
class CPlayer : public IPlayer {
public:
    CPlayer();
    virtual ~CPlayer() = default;
    virtual void Reset();
	virtual bool IsRobot() { return false; }
	virtual bool IsValid() { return deskID_ != INVALID_DESK && chairID_ != INVALID_CHAIR && GetUserID() > 0; }
	virtual std::shared_ptr<IRobotDelegate> GetDelegate();
    virtual bool SendUserMessage(uint8_t mainId, uint8_t subId, const uint8_t* data, uint32_t len);
    virtual bool SendData(uint8_t subId, const uint8_t* data, uint32_t len);

    virtual int64_t  GetUserID() { return baseInfo_.userID; }
    virtual const std::string GetNickName() { return baseInfo_.account; }
    virtual uint8_t  GetHeaderId() { return baseInfo_.headId; }
    virtual const std::string GetAccount() { return baseInfo_.account; }
    void SetRank(uint32_t rank) { rank_ = rank; }
    uint32_t GetRank() { return rank_; }
    virtual uint32_t GetDeskID() { return (deskID_); }
    void SetDeskID(uint32_t deskID) { deskID_ = deskID; }
    virtual uint32_t GetChairID() { return chairID_; }
    void SetChairID(uint32_t chairID) { chairID_ = chairID; }
    virtual int64_t GetScore() { return baseInfo_.userScore; }
	virtual void SetScore(int64_t userScore) { baseInfo_.userScore = userScore; }
	virtual const uint32_t GetIp() { return baseInfo_.ip; }
    virtual const std::string GetLocation() { return baseInfo_.location; }
    virtual int GetStatus() { return status_; }
    virtual void SetStatus(uint8_t userSatus) { status_ = userSatus; }

	virtual bool IsGetout() { return sGetout == status_; }
	virtual bool IsSit() { return sSit == status_; }
	virtual bool IsReady() { return sReady == status_; }
	virtual bool IsPlaying() { return sPlaying == status_; }
	virtual bool IsBreakline() { return sOffline == status_; }
	virtual bool IsLookon() { return sLookon == status_; }
	virtual bool IsGetoutAtplaying() { return sGetoutAtplaying == status_; }
	virtual void SetUserReady() { SetStatus(sReady); }
	virtual void SetUserSit() { SetStatus(sSit); }
	virtual void SetOffline() { SetStatus(sOffline); }
	virtual void SetTrustee(bool bTrustship) { trustee_ = bTrustship; }
	virtual bool GetTrustee() { return trustee_; }
	int  GetTakeMaxScore() { return baseInfo_.takeMaxScore; }
	int  GetTakeMinScore() { return baseInfo_.takeMinScore; }
	
	virtual UserBaseInfo& GetUserBaseInfo() { return baseInfo_; }
	virtual void SetUserBaseInfo(const UserBaseInfo& info) { baseInfo_ = info; }
	
	BlacklistInfo& GetBlacklistInfo() { return blacklistInfo_; }
	void SetBlacklistInfo(const BlacklistInfo& info) { blacklistInfo_ = info; }
public:
	std::vector<std::string> m_sMthBlockList;
	bool inQuarantine = false;
protected:
	uint32_t deskID_;            //桌子ID
	uint32_t chairID_;           //座位ID
	uint8_t status_;			 //玩家状态
	bool trustee_;               //托管状态
	uint32_t rank_;              //用户排行
	UserBaseInfo baseInfo_;      //基础数据
	BlacklistInfo blacklistInfo_;//黑名单
};

#endif
