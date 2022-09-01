#ifndef IDESK_INCLUDE_H
#define IDESK_INCLUDE_H

#include <stdint.h>
#include <string>

#include "muduo/base/Mutex.h"
#include <muduo/net/EventLoop.h>
#include "muduo/net/EventLoopThread.h"

//#include "public/Global.h"
#include "public/Packet.h"
#include "GameDefine.h"

class IPlayer;
class IDeskDelegate;
class ILogicThread;

/// <summary>
/// 桌子接口
/// </summary>
class IDesk : public std::enable_shared_from_this<IDesk>
{
public:
    IDesk()          = default;
    virtual ~IDesk() = default;

public:
	/// <summary>
	/// 桌子初始化
	/// </summary>
	/// <param name="deskDelegate">子游戏桌子代理</param>
	/// <param name="deskState">桌子特殊状态</param>
	/// <param name="gameInfo">游戏类型</param>
	/// <param name="roomInfo">游戏房间</param>
	/// <param name="threadTimer">桌子定时器</param>
	/// <param name="logicThread">桌子逻辑线程</param>
    virtual void Init(
        muduo::net::EventLoop* loop,
        std::shared_ptr<IDeskDelegate>& deskDelegate,
        DeskState& deskState,
        GameInfo* gameInfo,
        RoomInfo* roomInfo,
        std::shared_ptr<muduo::net::EventLoopThread>& threadTimer,
        ILogicThread* logicThread) = 0;
	/// <summary>
	/// 桌子牌局编号
	/// </summary>
	/// <returns></returns>
    virtual std::string NewRoundID() = 0;
	/// <summary>
	/// 机器人入桌
	/// </summary>
	/// <returns></returns>
    virtual int checkRobotEnter() = 0;
public:
    virtual bool SendData(uint32_t chairID, uint8_t subId, const uint8_t* data = 0, int len = 0, bool isRecord = true) = 0;
    virtual bool SendData(std::shared_ptr<IPlayer>& pPlayer, uint8_t subId, const uint8_t* data, int size, bool isRecord = true) = 0;
    virtual bool SendGameMessage(uint32_t chairID, const std::string& szMesage, uint8_t msgType, int64_t score = 0) = 0;
public:
	//桌子线程
    virtual muduo::net::EventLoop* GetLoop() = 0;
    //桌子信息
    virtual void GetDeskInfo(DeskState& TableInfo) = 0;
    //桌子ID
    virtual uint32_t GetDeskID() = 0;
    //桌子定时器
    virtual std::shared_ptr<muduo::net::EventLoopThread> GetLoopThread() = 0;
    //解散桌子
    virtual bool DismissGame() = 0;
    //结束游戏
    virtual bool ConcludeGame(uint8_t gameStatus) = 0;
    //计算抽水
    virtual int64_t CalculateRevenue(int64_t score) = 0;
    //椅子ID获取玩家
    virtual std::shared_ptr<IPlayer> GetPlayer(uint32_t chairID) = 0;
    //用户ID获取玩家
    virtual std::shared_ptr<IPlayer> GetPlayerBy(int64_t userID) = 0;
    //椅子ID是否有人
    virtual bool IsExist(uint32_t chairID) = 0;
	//椅子ID是否机器人
	virtual bool IsRobot(uint32_t chairID) = 0;
    //设置桌子状态
    virtual void SetGameStatus(uint8_t status = GAME_STATUS_FREE) = 0;
    //返回桌子状态
    virtual uint8_t GetGameStatus() = 0;
    //玩家托管状态
    virtual void SetUserTrustee(uint32_t chairID, bool bTrustee) = 0;
    virtual bool GetUserTrustee(uint32_t chairID) = 0;
    //玩家准备状态
    //virtual void SetUserReady(uint32_t chairID) = 0;
    //用户离开
    virtual bool OnUserLeave(std::shared_ptr<IPlayer>& pUser, bool bSendToSelf=true, bool bForceLeave=false) = 0;
    //用户离线
    virtual bool OnUserOffline(std::shared_ptr<IPlayer>& pUser, bool bLeavedGS=false) = 0;
    //能否进入
    virtual bool CanUserEnter(std::shared_ptr<IPlayer>& pUser) = 0;
    //能否离开
    virtual bool CanUserLeave(int64_t userID) = 0;
    //桌子人数
    virtual uint32_t GetPlayerCount(bool bIncludeRobot) = 0;
    virtual uint32_t GetRobotCount() = 0;
    virtual void GetPlayerCount(uint32_t &nRealCount, uint32_t &nRobotCount)=0;
    //人数限制
    virtual uint32_t GetMaxPlayerCount() = 0;
    //房间信息
    virtual RoomInfo* GetGameRoomInfo() = 0;
    //清除玩家
    virtual void ClearPlayer(uint32_t chairID = INVALID_CHAIR, bool bSendState = true, bool bSendToSelf = true, uint8_t sendErrorCode = 0) = 0;
    //游戏消息
    virtual bool OnGameEvent(uint32_t chairID, uint8_t subId, const uint8_t* pData, int dataSize) = 0;
    //开始游戏
    //virtual void OnGameStart() = 0;
    //游戏是否开始
    virtual bool IsGameStarted() =  0;
    //开始条件检查
    //virtual bool CheckGameStart() = 0;
    //用户进入
    virtual bool OnUserEnterAction(
		const muduo::net::WeakTcpConnectionPtr& weakConn,
		BufferPtr buf,
        std::shared_ptr<IPlayer>& pUser, bool bDistribute = false) = 0;
    //准备离开
    virtual bool OnUserStandup(std::shared_ptr<IPlayer>& pPlayer, bool bSendState = true, bool bSendToSelf = false) = 0;
    //广播玩家给桌子其余玩家
    virtual void BroadcastUserToOthers(std::shared_ptr<IPlayer>& pUser) = 0;
	//发送其他玩家给指定玩家
	virtual void SendOtherToUser(std::shared_ptr<IPlayer>& pUser, UserInfo& userInfo) = 0;
    //发送其余玩家给指定玩家
    virtual void SendOthersToUser(std::shared_ptr<IPlayer>& pUser) = 0;
    //桌子内广播用户状态
    virtual void BroadcastUserStatus(std::shared_ptr<IPlayer>& pUser, bool bSendToSelf = false) = 0;
public:
    //写入玩家积分
    virtual bool WriteUserScore(UserScoreInfo* pScoreInfo, uint32_t nCount, std::string &strRound) = 0;
    //写入玩家积分
    virtual bool WriteSpecialUserScore(SpecialScoreInfo* pScoreInfo, uint32_t nCount, std::string &strRound) = 0;
    //更新库存变化
    virtual int  UpdateStorageScore(int64_t changeStockScore) = 0;
    //获取当前库存
    virtual bool GetStorageScore(StorageInfo& storageInfo) = 0;
    //保存游戏记录
    virtual bool SaveReplay(GameReplay& replay) = 0;
};

#endif