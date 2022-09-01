#include <muduo/base/Logging.h>

#include <sys/time.h>
#include <stdarg.h>

#include "IDesk.h"
#include "IDeskDelegate.h"
#include "IPlayer.h"
#include "IRobotDelegate.h"
#include "IReplayRecord.h"

#include "Player.h"
#include "Robot.h"

#include "Desk.h"
#include "DeskMgr.h"

#include "../../proto/Game.Common.pb.h"

CRobot::CRobot() {
}

CRobot::~CRobot() {
}

/// <summary>
/// 数据重置
/// </summary>
void CRobot::Reset() {
	CPlayer::Reset();
}

/// <summary>
/// 是否机器人
/// </summary>
/// <returns></returns>
bool CRobot::IsRobot() {
	return true;
}

/// <summary>
/// 子游戏机器人代理
/// </summary>
/// <returns></returns>
std::shared_ptr<IRobotDelegate> CRobot::GetDelegate() {
	return robotDelegate_;
}

/// <summary>
/// 子游戏机器人代理
/// </summary>
/// <param name="robotDelegate"></param>
void CRobot::SetDelegate(std::shared_ptr<IRobotDelegate> robotDelegate) {
	robotDelegate_ = robotDelegate;
}

/// <summary>
/// 子游戏机器人代理接收消息
/// </summary>
/// <param name="mainId"></param>
/// <param name="subId"></param>
/// <param name="data"></param>
/// <param name="size"></param>
/// <returns></returns>
bool CRobot::SendUserMessage(uint8_t mainId, uint8_t subId, const uint8_t* data, uint32_t size) {
	if ((mainId == 0) || mainId == Game::Common::MAIN_MESSAGE_CLIENT_TO_GAME_LOGIC) {
		if (robotDelegate_) {
			robotDelegate_->OnGameMessage(subId, data, size);
		}
	}
	return true;
}

/// <summary>
/// 发送消息到子游戏桌子代理
/// </summary>
/// <param name="subId">子命令ID</param>
/// <param name="data"></param>
/// <param name="len"></param>
/// <returns></returns>
bool CRobot::SendData(uint8_t subId, const uint8_t* data, uint32_t len) {
	bool bok = false;
	if (INVALID_DESK != deskID_) {
		std::shared_ptr<IDesk> pDesk = CDeskMgr::get_mutable_instance().GetDesk(GetDeskID());
		if (pDesk) {
			bok = pDesk->OnGameEvent(GetChairID(), subId, data, len);
		}
	}
	else {
	}
	return bok;
}
