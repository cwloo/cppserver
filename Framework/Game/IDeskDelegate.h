#ifndef IDESKDELEGATE_INCLUDE_H
#define IDESKDELEGATE_INCLUDE_H

#include <stdint.h>
#include <string>

//#include "public/Global.h"
#include "GameDefine.h"

class IDesk;
class IPlayer;

/// <summary>
/// 桌子代理接口
/// </summary>
class IDeskDelegate
{
public:
    IDeskDelegate() {}
    virtual ~IDeskDelegate() {}
	//桌子指针
	virtual bool SetDesk(std::shared_ptr<IDesk>& pDesk) = 0;
	//桌子复位
	virtual void Reposition() {}
public:
    //游戏开始
    virtual void OnGameStart() = 0;
    //游戏结束
    virtual bool OnGameConclude(uint32_t chairid, uint8_t nEndTag) = 0;
    //发送场景
    virtual bool OnGameScene(uint32_t chairid, bool bisLookon)    = 0;
	//游戏消息
	virtual bool OnGameMessage(uint32_t chairid, uint8_t subid, const uint8_t* data, uint32_t dataLen) = 0;
public:
    //用户进入
    virtual bool OnUserEnter(int64_t userid, bool isLookon) = 0;
    //用户准备
    virtual bool OnUserReady(int64_t userid, bool isLookon) = 0;
    //用户离开
    virtual bool OnUserLeave(int64_t userid, bool isLookon) = 0;
    //能否进入
    virtual bool CanUserEnter(std::shared_ptr<IPlayer>& pUser) = 0;
    //能否离开
    virtual bool CanUserLeave(int64_t userid) = 0;
};

typedef std::shared_ptr<IDeskDelegate>(*DeskDelegateCreator)(void);
typedef void* (*DeskDelegateDeleter)(IDeskDelegate* deskDelegate);

extern "C" std::shared_ptr<IDeskDelegate> GetDeskDelegate(void);

#define FuncNameCreateDesk      ("CreateDeskDelegate")
#define FuncNameDeleteDesk      ("DeleteDeskDelegate")


#endif