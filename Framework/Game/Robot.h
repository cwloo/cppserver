#ifndef ROBOT_INCLUDE_H
#define ROBOT_INCLUDE_H

#include <stdint.h>

class IRobotDelegate;
class CPlayer;

/// <summary>
/// 机器人类
/// </summary>
class CRobot : public CPlayer {
public:
    CRobot();
    virtual ~CRobot();
    /// <summary>
    /// 数据重置
    /// </summary>
    virtual void Reset();
public:
    /// <summary>
    /// 是否机器人
    /// </summary>
    /// <returns></returns>
    virtual bool IsRobot();
    /// <summary>
    /// 子游戏机器人代理
    /// </summary>
    /// <returns></returns>
    virtual std::shared_ptr<IRobotDelegate> GetDelegate();
    /// <summary>
    /// 子游戏机器人代理
    /// </summary>
    /// <param name="robotDelegate"></param>
    void SetDelegate(std::shared_ptr<IRobotDelegate> robotDelegate);
	/// <summary>
	/// 子游戏机器人代理接收消息
	/// </summary>
	/// <param name="mainId"></param>
	/// <param name="subId"></param>
	/// <param name="data"></param>
	/// <param name="size"></param>
	/// <returns></returns>
    virtual bool SendUserMessage(uint8_t mainId, uint8_t subId, const uint8_t* data, uint32_t size);
	/// <summary>
	/// 发送消息到子游戏桌子代理
	/// </summary>
	/// <param name="subId">子命令ID</param>
	/// <param name="data"></param>
	/// <param name="len"></param>
	/// <returns></returns>
    virtual bool SendData(uint8_t subId, const uint8_t *data, uint32_t len);
protected:
    std::shared_ptr<IRobotDelegate> robotDelegate_;
};

#endif