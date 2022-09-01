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
/// ��������
/// </summary>
void CRobot::Reset() {
	CPlayer::Reset();
}

/// <summary>
/// �Ƿ������
/// </summary>
/// <returns></returns>
bool CRobot::IsRobot() {
	return true;
}

/// <summary>
/// ����Ϸ�����˴���
/// </summary>
/// <returns></returns>
std::shared_ptr<IRobotDelegate> CRobot::GetDelegate() {
	return robotDelegate_;
}

/// <summary>
/// ����Ϸ�����˴���
/// </summary>
/// <param name="robotDelegate"></param>
void CRobot::SetDelegate(std::shared_ptr<IRobotDelegate> robotDelegate) {
	robotDelegate_ = robotDelegate;
}

/// <summary>
/// ����Ϸ�����˴��������Ϣ
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
/// ������Ϣ������Ϸ���Ӵ���
/// </summary>
/// <param name="subId">������ID</param>
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
