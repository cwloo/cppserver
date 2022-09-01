#ifndef REPLAYRECORD_INCLUDE_H
#define REPLAYRECORD_INCLUDE_H

#include "GameDefine.h"

/// <summary>
/// 游戏记录接口
/// </summary>
class IReplayRecord {
public:
    virtual bool SaveReplay(GameReplay& replay) = 0;
};

#endif
