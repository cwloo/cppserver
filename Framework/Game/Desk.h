#ifndef DESK_INCLUDE_H
#define DESK_INCLUDE_H

#include <atomic>
#include "muduo/net/TcpConnection.h"
#include <muduo/net/EventLoop.h>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time.hpp>

#include "public/Global.h"
#include "public/Packet.h"
#include "public/StdRandom.h"
#include "GameDefine.h"

using namespace boost::posix_time;
using namespace boost::gregorian;

class IDesk;
class IDeskDelegate;
class IPlayer;
class IReplayRecord;
struct UserBaseInfo;

/// <summary>
/// 桌子类
/// </summary>
class CDesk : public IDesk, public IReplayRecord {
public:
    CDesk();
    virtual ~CDesk();
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
        ILogicThread* logicThread);
	/// <summary>
    /// 桌子牌局编号
    /// </summary>
    /// <returns></returns>
    virtual std::string NewRoundID();
	/// <summary>
	/// 机器人入桌
	/// </summary>
	/// <returns></returns>
	int checkRobotEnter();
protected:
    bool sendData(std::shared_ptr<IPlayer>& pPlayer, uint8_t mainId, uint8_t subId, std::vector<uint8_t> &data, packet::header_t const*commandHeader, int enctype);

public:
    virtual bool SendData(uint32_t chairID, uint8_t subId, const uint8_t* data, int size, bool bRecord = true);
    virtual bool SendData(std::shared_ptr<IPlayer>& pUser, uint8_t subId, const uint8_t* data, int size, bool bRecord = true);
    virtual bool SendGameMessage(uint32_t chairID, const std::string& szMessage, uint8_t msgType, int64_t score = 0);
public:
    //桌子线程
    virtual muduo::net::EventLoop* GetLoop() { return loop_; }
    //桌子信息
    virtual void GetDeskInfo(DeskState& TableInfo);
    //桌子ID
    virtual uint32_t GetDeskID();
    //桌子定时器
    virtual std::shared_ptr<muduo::net::EventLoopThread> GetLoopThread();
    //解散桌子
    virtual bool DismissGame();
     //结束游戏
    virtual bool ConcludeGame(uint8_t nGameStatus);
    //计算抽水
    virtual int64_t CalculateRevenue(int64_t nScore);
    //椅子ID获取玩家
    virtual std::shared_ptr<IPlayer> GetPlayer(uint32_t chairID);
    //用户ID获取玩家
    virtual std::shared_ptr<IPlayer> GetPlayerBy(int64_t userID);
    //椅子ID是否有人
    virtual bool IsExist(uint32_t chairID);
	/// <summary>
    /// 是否机器人
    /// </summary>
    /// <param name="chairID">座位号</param>
    /// <returns></returns>
    virtual bool IsRobot(uint32_t chairID);
    //设置桌子状态
    virtual void SetGameStatus(uint8_t cbStatus = GAME_STATUS_FREE);
    //返回桌子状态
    virtual uint8_t GetGameStatus();
    //玩家托管状态
    virtual void SetUserTrustee(uint32_t chairID, bool bTrustee);
    virtual bool GetUserTrustee(uint32_t chairID);
    //玩家准备状态
    //virtual void SetUserReady(uint32_t chairID);
    //用户离开
    virtual bool OnUserLeave(std::shared_ptr<IPlayer>& pUser, bool bSendStateMyself=true, bool bForceLeave=false);
    //用户离线
    virtual bool OnUserOffline(std::shared_ptr<IPlayer>& pUser,bool bLeaveGs=false);
    //能否进入
    virtual bool CanUserEnter(std::shared_ptr<IPlayer>& pUser);
    //能否离开
    virtual bool CanUserLeave(int64_t userID);
    //桌子人数
    virtual uint32_t GetPlayerCount(bool bIncludeRobot=true);
    virtual uint32_t GetRobotCount();
    virtual void GetPlayerCount(uint32_t &nRealCount, uint32_t &nRobotCount);
	//人数限制
	virtual uint32_t GetMaxPlayerCount();
    //房间信息
    virtual RoomInfo* GetGameRoomInfo();
    //清除玩家
    virtual void ClearPlayer(uint32_t chairID = INVALID_CHAIR, bool bSendState = true, bool bSendToSelf = true, uint8_t cbSendErrorCode = 0);
	/// <summary>
    /// 子游戏桌子代理接收消息
    /// </summary>
    /// <param name="chairID">消息来自椅子ID</param>
    /// <param name="subId">子命令ID</param>
    /// <param name="data"></param>
    /// <param name="size"></param>
    /// <returns></returns>
    virtual bool OnGameEvent(uint32_t chairID, uint8_t subId, const uint8_t *data, int size);
    //开始游戏
    //virtual void OnGameStart();
     //游戏是否开始
    virtual bool IsGameStarted() { return status_ >= GAME_STATUS_START && status_ < GAME_STATUS_END; }
    //开始条件检查
    //virtual bool CheckGameStart();
    //用户进入
    virtual bool OnUserEnterAction(
		const muduo::net::WeakTcpConnectionPtr& weakConn,
		BufferPtr buf,
        std::shared_ptr<IPlayer>& pUser, bool bDistribute = false);
    //准备离开
    virtual bool OnUserStandup(std::shared_ptr<IPlayer>& pPlayer, bool bSendState = true, bool bSendToSelf = false);
    //玩家完成入座
    void SendUserSitdownFinish(std::shared_ptr<IPlayer>& pPlayer, packet::header_t const* commandHeader, bool bDistribute=false);
    //广播玩家积分
    void BroadcastUserScore(std::shared_ptr<IPlayer>& pUser);
    //广播玩家给桌子其余玩家
    virtual void BroadcastUserToOthers(std::shared_ptr<IPlayer>& pUser);
	//发送其他玩家给指定玩家
	virtual void SendOtherToUser(std::shared_ptr<IPlayer>& pUser, UserInfo& userInfo);
    //发送其余玩家给指定玩家
    virtual void SendOthersToUser(std::shared_ptr<IPlayer>& pUser);
    //桌子内广播用户状态
    virtual void BroadcastUserStatus(std::shared_ptr<IPlayer>& pUser, bool bSendTySelf = true);
    //写入玩家积分
    virtual bool WriteUserScore(UserScoreInfo* pScoreInfo, uint32_t nCount, std::string &strRound);
    //写入玩家积分
    virtual bool WriteSpecialUserScore(SpecialScoreInfo* pScoreInfo, uint32_t nCount, std::string &strRound);
    //更新库存变化
    virtual int  UpdateStorageScore(int64_t changeStockScore);
    //获取当前库存
    virtual bool GetStorageScore(StorageInfo& storageInfo);
	//保存游戏记录
    virtual bool SaveReplay(GameReplay& replay);

public:
	//写入黑名单日志
	bool WriteBlacklistLog(std::shared_ptr<IPlayer>& pPlayer, int status);
    bool ReadStorageScore();
    bool WriteGameChangeStorage(int64_t changeStockScore);
    void DeleteUserToProxy(std::shared_ptr<IPlayer>& pPlayer, int32_t nKickType = KICK_GS|KICK_CLOSEONLY);

    bool DelUserOnlineInfo(int64_t userID,bool bonlyExpired=false);
    bool SetUserOnlineInfo(int64_t userID);
    //玩家积分入库
    bool UpdateUserScoreToDB(int64_t userID, UserScoreInfo* pScoreInfo);
    bool UpdateUserScoreToDB(int64_t userID, SpecialScoreInfo* pScoreInfo);

    bool AddUserGameInfoToDB(UserBaseInfo &userBaseInfo, UserScoreInfo *scoreInfo, std::string &strRoundId, int32_t userCount, bool bAndroidUser = false);
    bool AddUserGameInfoToDB(SpecialScoreInfo *scoreInfo, std::string &strRoundId, int32_t userCount, bool bAndroidUser = false);

    bool AddScoreChangeRecordToDB(UserBaseInfo &userBaseInfo, int64_t sourceScore, int64_t addScore, int64_t targetScore);
    bool AddScoreChangeRecordToDB(SpecialScoreInfo *scoreInfo);

    bool AddUserGameLogToDB(UserBaseInfo &userBaseInfo, UserScoreInfo *scoreInfo, std::string &strRoundId);
    bool AddUserGameLogToDB(SpecialScoreInfo *scoreInfo, std::string &strRoundId);

public:
    void dumpUserList();
   

	

private:
    bool SaveReplayDetailJson(GameReplay& replay);
    bool SaveReplayDetailBlob(GameReplay& replay);
public:
    uint8_t status_;                                            //桌子游戏状态
	GameInfo* gameInfo_;                                        //所属游戏类型
	RoomInfo* roomInfo_;                                        //所属房间信息
	ILogicThread* logicThread_;                                 //桌子逻辑线程
	DeskState deskState_;                                       //桌子独有状态
	std::vector<std::shared_ptr<IPlayer>> players_;             //桌子玩家players[chairid]
    std::shared_ptr<IDeskDelegate> deskDelegate_;               //桌子代理
    std::shared_ptr<muduo::net::EventLoopThread> threadTimer_;  //桌子定时器
    muduo::net::EventLoop* loop_;                               //桌子逻辑线程
    mutable boost::shared_mutex mutex_;

    short allocChairID() {
        short chairid = INVALID_DESK;
        if (!freevec_.empty()) {
            chairid = freevec_.front();
            freevec_.pop_front();
            assert(!players_[chairid]);
        }
        return chairid;
    }
    void freeChairID(short chairid) {
        assert(chairid != INVALID_CHAIR);
        assert(
            chairid >= 0 &&
            chairid < players_.size());
        freevec_.push_back(chairid);
    }
public:
    int realc_, robotc_;
    std::list<short> freevec_;
    STD::Weight weight_;
private:
    static std::atomic_llong curStorage_;   //系统当前库存
    static double lowStorage_;              //系统最小库存，系统输分不得低于库存下限，否则赢分
    static double highStorage_;             //系统最大库存，系统赢分不得大于库存上限，否则输分
    static int sysKillAllRatio_;            //系统通杀率
    static int sysReduceRatio_;             //系统库存衰减
    static int sysChangeCardRatio_;         //系统换牌率
};

#endif