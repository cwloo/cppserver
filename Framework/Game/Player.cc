
#include <muduo/base/Logging.h>

#include "IDesk.h"
#include "IDeskDelegate.h"
#include "IPlayer.h"
#include "IRobotDelegate.h"
//#include "IReplayRecord.h"

#include "Player.h"

CPlayer::CPlayer() {
    deskID_    = INVALID_DESK;
    chairID_   = INVALID_CHAIR;
    status_   = sFree;
    trustee_  = false;

    m_sMthBlockList.resize(20);
    rank_ = 0;
}

void CPlayer::Reset() {
	deskID_ = INVALID_DESK;
	chairID_ = INVALID_CHAIR;
	status_ = sFree;
	trustee_ = false;
	baseInfo_ = UserBaseInfo();
	blacklistInfo_.status = 0;
	rank_ = 0;
}

std::shared_ptr<IRobotDelegate> CPlayer::GetDelegate() {
	return std::shared_ptr<IRobotDelegate>();
}

bool CPlayer::SendUserMessage(uint8_t mainId, uint8_t subId, const uint8_t* data, uint32_t len) {
    LOG_ERROR << __FUNCTION__ << " only used for robot";
    return true;
}

bool CPlayer::SendData(uint8_t subId, const uint8_t* data, uint32_t datasize) {
    LOG_ERROR << __FUNCTION__ << " only used for robot";
    return true;
}