#ifndef IPLAYER_INCLUDE_H
#define IPLAYER_INCLUDE_H

#include <stdint.h>
#include <string>

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Callbacks.h>

//#include "public/Global.h"
#include "public/Packet.h"
#include "GameDefine.h"

class IRobotDelegate;

class ILogicThread {
public:
    virtual void KickUserIdToProxyOffLine(int64_t userID, int32_t nKickType) = 0;
    virtual void SavePlayGameTime(int64_t userID) = 0;
    virtual boost::tuples::tuple<muduo::net::WeakTcpConnectionPtr, std::shared_ptr<packet::internal_prev_header_t>>
        GetProxyConnectionWeakPtrFromUserId(int64_t userID) = 0;
    virtual bool IsServerStoped() = 0;
    virtual void clearUserIdProxyConnectionInfo(int64_t userID) = 0;
};

/// <summary>
/// 玩家接口
/// </summary>
class IPlayer {
public:
    IPlayer() = default;
	virtual ~IPlayer() = default;
	virtual void Reset() = 0;
	virtual bool IsRobot() = 0;
	virtual bool IsValid() = 0;
	virtual UserBaseInfo& GetUserBaseInfo() = 0;
	virtual void SetUserBaseInfo(const UserBaseInfo& info) = 0;
	virtual std::shared_ptr<IRobotDelegate> GetDelegate() = 0;

	virtual bool SendUserMessage(uint8_t mainId, uint8_t subId, const uint8_t* data, uint32_t len) = 0;
	virtual bool SendData(uint8_t subId, const uint8_t* data, uint32_t len) = 0;

	virtual int64_t GetUserID() = 0;
	virtual const std::string GetAccount() = 0;
	virtual const std::string GetNickName() = 0;
	virtual uint8_t GetHeaderId() = 0;
	virtual uint32_t GetDeskID() = 0;
	virtual void SetDeskID(uint32_t deskID) = 0;
	virtual uint32_t GetChairID() = 0;
	virtual void SetChairID(uint32_t chairID) = 0;
	virtual int64_t GetScore() = 0;
	virtual void SetScore(int64_t userScore) = 0;
	virtual const std::string GetLocation() = 0;
	virtual int GetStatus() = 0;
	virtual void SetStatus(uint8_t) = 0;
	virtual bool IsGetout() = 0;
	virtual bool IsSit() = 0;
	virtual bool IsReady() = 0;
	virtual bool IsPlaying() = 0;
	virtual bool IsBreakline() = 0;
	virtual bool IsLookon() = 0;
	virtual bool IsGetoutAtplaying() = 0;
	virtual void SetUserReady() = 0;
	virtual void SetUserSit() = 0;
	virtual void SetOffline() = 0;
	virtual void SetTrustee(bool bTrustship) = 0;
	virtual bool GetTrustee() = 0;
};

#endif