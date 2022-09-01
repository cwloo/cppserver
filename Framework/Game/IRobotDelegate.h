#ifndef IROBOTDELEGATE_INCLUDE_H
#define IROBOTDELEGATE_INCLUDE_H

#include <stdint.h>
#include <string>

//#include "public/Global.h"
#include "GameDefine.h"

class IDesk;
class IPlayer;

/// <summary>
/// 机器人代理接口
/// </summary>
class IRobotDelegate
{
public:
	IRobotDelegate() {}
	virtual ~IRobotDelegate() {}

public:
	//设置机器人
	virtual bool SetPlayer(std::shared_ptr<IPlayer>& pUser) = 0;
    //设置桌子
	virtual void SetDesk(std::shared_ptr<IDesk>& pDesk) = 0;
	//机器人复位
    virtual bool Reposition() = 0;
	//初始化
    virtual bool Initialization(std::shared_ptr<IDesk>& pDesk, std::shared_ptr<IPlayer>& pPlayer) = 0;
    //游戏消息
    virtual bool OnGameMessage(uint8_t subid, const uint8_t* data, uint32_t dataLen) = 0;
    //机器人策略
    virtual void SetRobotStrategy(RobotStrategyParam* strategy) = 0;
    virtual RobotStrategyParam* GetRobotStrategy() = 0;
};

typedef std::shared_ptr<IRobotDelegate> (*RobotDelegateCreator)();   //生成机器人
typedef void* (*RobotDelegateDeleter)(IRobotDelegate* robotDelegate);//删除机器人

//机器人实例创建/销毁函数名
#define FuncNameCreateRobot		("CreateRobotDelegate")
#define FuncNameDeleteRobot		("DeleteRobotDelegate")

#endif