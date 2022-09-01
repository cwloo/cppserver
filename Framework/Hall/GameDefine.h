#ifndef GAME_DEFINE_H
#define GAME_DEFINE_H

#include <stdint.h>
#include <string>
#include <vector>
#include <chrono>


#define LANDY_DEBUF          (0)


#define HEADER_SIGN          (0x5F5F)
#define PROTO_BUF_SIGN       (0xF5F5F5F5)


// declare the invalidate now.
#define INVALID_TABLE           (0xFFFF)
#define INVALID_CHAIR           (0xFFFF)
#define INVALID_CARD            (0xFFFF)

#define MAX_USER_ONLINE_INFO_IDLE_TIME   (60*3)


#define TIME_RELOADPARAM     (60*5)


#define COIN_RATE           100 //金币比率

#define TASK_ROOM           99999
#define TASK_SCORE_CHANGE_TYPE  4

#define REDIS_LOGIN_3S_CHECK   "login_3s_check_"
#define REDIS_CUR_STOCKS        "stocks"    //当前库存Key




enum GameType
{
    GameType_BaiRen = 0,
    GameType_Confrontation = 1,
};

enum MongoDBOptType
{
    INSERT_ONE,
    INSERT_MANY,
    UPDATE_ONE,
    UPDATE_MANY,
    DELETE_ONE,
    DEELTE_MANY
};


enum GameStatus
{
    GAME_STATUS_INIT    = 0,
    GAME_STATUS_FREE	= 1,			// 游戏Free
    GAME_STATUS_START	= 100,			// 游戏进行
    GAME_STATUS_END		= 200,			// 游戏End
};

enum eServerState
{
    SERVER_STOPPED = 0,
    SERVER_RUNNING = 1,
    SERVER_REPAIRING  = 2,
};


//#pragma pack(1)

struct tagGameInfo
{
    uint32_t    gameId;       // game id.
    std::string      gameName;
    uint32_t    sortId;       // game sort id.
    uint8_t     gameType;     // 0-bairen   1-duizhan
    std::string      gameServiceName;
    uint8_t     revenueRatio;  // revenue
    bool        matchforbids[10];// forbid match types
};

struct tagGameRoomInfo
{
    uint32_t    gameId;                // game id.
    uint32_t    roomId;                // room kind id.
    std::string      roomName;              // room kind name.

    uint16_t    tableCount;            // table count.

    int64_t     floorScore;            // cell score.
    int64_t     ceilScore;             // cell score.
    int64_t     enterMinScore;         // enter min score.
    int64_t     enterMaxScore;         // enter max score.

    uint32_t    minPlayerNum;          // start min player.
    uint32_t    maxPlayerNum;          // start max player.

    uint32_t    androidCount;          // start android count
    uint32_t    androidMaxUserCount;   // real user

    int64_t     broadcastScore;        // broadcast score.
    int64_t     maxJettonScore;        // max Jetton Score

    int64_t     totalStock;
    int64_t     totalStockLowerLimit;
    int64_t     totalStockHighLimit;

    uint32_t    systemKillAllRatio;
    uint32_t    systemReduceRatio;
    uint32_t    changeCardRatio;

    uint8_t     serverStatus;          // server status.
    uint8_t     bEnableAndroid;       // is enable android.

    std::vector<int64_t> jettons;

    uint32_t    updatePlayerNumTimes;
    std::vector<float> enterAndroidPercentage;  //Control the number of android entering according to the time
    uint32_t    realChangeAndroid; //join 'realChangeAndroid' real user change out one android user (n:1)

    int64_t     totalJackPot[5];          //预存N个奖池信息

//    uint8_t     bisKeepAndroidin;       // is keep android in room.
//    uint8_t     bisLeaveAnyTime;        // is user can leave game any time.
//    uint8_t     bisAndroidWaitList;     // DDZ: android have to do wait list.
//    uint8_t     bisDynamicJoin;         // is game can dynamic join.
//    uint8_t     bisAutoReady;           // is game auto ready.
//    uint8_t     bisEnterIsReady;		// is enter is ready.
//    uint8_t     bisQipai;               // need to wait player ready.
};

struct TableState
{
    uint32_t    nTableID;
    uint8_t		bisLock;
    uint8_t		bisLookOn;
};

struct tagScoreInfo
{
    tagScoreInfo()
    {
//        userId = -1;
        chairId = INVALID_CHAIR;
        isBanker = 0;
//        beforeScore = -1;
        addScore = 0;
        betScore = 0;
        revenue = 0;
        rWinScore = 0;
        cellScore.clear();
        cardValue = "";

//        bWriteScore = true;
//        bWriteRecord = true;
    }

    void clear()
    {
//        userId = -1;
        chairId = INVALID_CHAIR;
        isBanker = 0;
//        beforeScore = -1;
        addScore = 0;
        betScore = 0;
        revenue = 0;
        rWinScore = 0;
        cellScore.clear();
        cardValue = "";
    }

//    int64_t    userId;
    uint32_t   chairId;               // 椅子号
    uint32_t   isBanker;
//    int64_t    beforeScore;
    int64_t    addScore;              // 当局输赢分数
    int64_t    betScore;              // 总压注
    int64_t    revenue;               // 当局税收
    int64_t    rWinScore;   //有效投注额：税前输赢
    std::vector<int64_t> cellScore;        // 每一方压注

    std::chrono::system_clock::time_point startTime;  //当局开始时间
    std::string     cardValue;             // 当局开牌

//    bool       bWriteScore;           // 写分
//    bool       bWriteRecord;          // 写记录
};

struct tagSpecialScoreInfo
{
    tagSpecialScoreInfo()
    {
        userId = -1;
        account = "";
        chairId = INVALID_CHAIR;
        isBanker = 0;
        agentId = 0;
        lineCode = "";
        beforeScore = -1;
        addScore = 0;
        betScore = 0;
        revenue = 0;
        rWinScore = 0;
        cellScore.clear();
        cardValue = "";

        bWriteScore = true;
        bWriteRecord = true;
    }

    void clear()
    {
        userId = -1;
        account = "";
        chairId = INVALID_CHAIR;
        isBanker = 0;
        agentId = 0;
        lineCode="";
        beforeScore = -1;
        addScore = 0;
        betScore = 0;
        revenue = 0;
        rWinScore = 0;
        cellScore.clear();
        cardValue = "";
    }

    int64_t    userId;
    std::string     account;
    uint32_t   chairId;               // 椅子号
    uint32_t   isBanker;
    uint32_t   agentId;
    std::string     lineCode;
    int64_t    beforeScore;
    int64_t    addScore;              // 当局输赢分数
    int64_t    betScore;              // 总压注
    int64_t    revenue;               // 当局税收
    int64_t    rWinScore;   //有效投注额：税前输赢
    std::vector<int64_t> cellScore;        // 每一方压注

    std::chrono::system_clock::time_point startTime;  //当局开始时间
    std::string     cardValue;             // 当局开牌

    bool       bIsAndroid;
    bool       bWriteScore;           // 写分
    bool       bWriteRecord;          // 写记录
};


struct RobotInfo
{
    int64_t    userId;
    std::string     account;
    std::string     nickName;
    uint8_t    headId;

    int64_t    score;

    std::string     enterTime;
    std::string     leaveTime;
    int64_t    takeMinScore;
    int64_t    takeMaxScore;

    std::string    location;
};

struct AndroidStrategyArea
{
    int32_t weight;
    int32_t lowTimes;
    int32_t highTimes;
};
//机器人策略
struct tagAndroidStrategyParam
{
    int32_t gameId;
    int32_t roomId;
    int64_t exitLowScore;
    int64_t exitHighScore;
    int64_t minScore;
    int64_t maxScore;
    std::vector<AndroidStrategyArea> areas;
};

//记录水果机免费游戏剩余次数 add by caiqing
struct tagSgjFreeGameRecord
{
    int64_t     userId;             //UserID
    int64_t     betInfo;			//下注数量信息
	int32_t		freeLeft;			//剩余免费次数
    int32_t		marryLeft;			//剩余玛丽数
	int32_t		allMarry;			//总玛丽数
};

struct tagUserInfo
{
    int64_t    userId;
    std::string     account;
    std::string     nickName;
    uint8_t    headId;
    uint32_t   tableId;
    uint32_t   chairId;
    int64_t    score;

    uint8_t    status;
    std::string     location;
};
//对局单步操作
struct tagReplayStep
{
    int32_t time;
    std::string bet;
    int32_t round;
    int32_t ty;//操作类型
    int32_t chairId;//操作位置
    int32_t pos;//被操作位置，如比牌的被比方
    bool flag;//is valid

};
//对局结果
struct tagReplayResult
{
    int32_t chairId;
    int32_t pos;
    int64_t bet;
    int64_t win;
    std::string cardtype;
    bool isbanker;
    bool flag;// is valid
};
//对局玩家信息
struct  tagReplayPlayer
{
     int32_t userid;
     std::string accout;
     int64_t score;
     int32_t chairid;
     bool flag;// is valid
};
//对局记录
struct tagGameReplay
{
    //uint16_t          gameid;//游戏类型
    std::string              gameinfoid;//对局id，对应gameids
    std::string              roomname;//全名，如：炸金花高级场
    int32_t             cellscore;//底注
	bool                saveAsStream;//对局详情格式 true-binary false-jsondata
    std::string              detailsData;//对局详情 binary/jsondata
    std::vector<tagReplayPlayer>    players;//玩家
    std::vector<tagReplayStep> steps;//游戏过程
    std::vector<tagReplayResult> results;//游戏结果
    void clear()
    {
       for(std::vector<tagReplayStep>::iterator it = steps.begin();it != steps.end(); it++)
       {
           it->flag=false;
       }
       for(std::vector<tagReplayPlayer>::iterator it = players.begin();it != players.end(); it++)
       {
           it->flag=false;
       }
       for(std::vector<tagReplayResult>::iterator it = results.begin();it != results.end(); it++)
       {
           it->flag=false;
       }
    }

    // 添加结果
    void addResult(int32_t chairId,int32_t pos,int64_t bet,int64_t win, std::string cardtype,bool isBanker)
    {
        std::vector<tagReplayResult>::iterator it = results.begin();
        for(;it != results.end(); it++)
        {
            if(it->flag == false)
            {
                break;
            }
        }
        if(it == results.end())
        {
            tagReplayResult result;
            result.chairId = chairId;
            result.pos = pos;
            result.bet = bet;
            result.win = win;
            result.cardtype = cardtype;
            result.isbanker = isBanker;
            result.flag = true;
            results.push_back(result);
        }else{
            it->chairId = chairId;
            it->pos = pos;
            it->bet = bet;
            it->win = win;
            it->isbanker = isBanker;
            it->cardtype = cardtype;
            it->flag = true;
        }
    }
    // 添加玩家
    void addPlayer(int32_t userid, std::string account,int64_t score,int32_t chairid)
    {
        std::vector<tagReplayPlayer>::iterator it = players.begin();
        for(;it != players.end(); it++)
        {
            if(it->flag == false)
            {
                break;
            }
        }
        if(it == players.end())
        {
            tagReplayPlayer player;
            player.accout = account;
            player.userid = userid;
            player.score = score;
            player.chairid = chairid;
            player.flag = true;
            players.push_back(player);
        }else{
            it->accout = account;
            it->userid = userid;
            it->score = score;
            it->chairid = chairid;
            it->flag = true;
        }
    }
    // 添加步骤
    void addStep(int32_t time, std::string bet, int32_t round, int32_t ty, int32_t chairId, int32_t pos)
    {
        std::vector<tagReplayStep>::iterator it = steps.begin();
        for(;it != steps.end(); it++)
        {
            if(it->flag == false)
            {
                break;
            }
        }
        if(it == steps.end())
        {
            tagReplayStep step;
            step.time = time;
            step.bet = bet;
            step.round = round;
            step.ty = ty;
            step.chairId = chairId;
            step.pos = pos;
            step.flag = true;
            steps.push_back(step);
        }else{
            it->time = time;
            it->bet = bet;
            it->round = round;
            it->ty = ty;
            it->chairId = chairId;
            it->pos = pos;
            it->flag = true;
        }
    }
};

//黑名单信息
struct tagBlacklistInfo
{
    int64_t total;//total control value
    int64_t current;//current control value
    int64_t weight;// control weight
    short status;//control status:0 disabled; 1 undercontrol;other, it is other
    std::map<std::string, int16_t> listRoom;
    tagBlacklistInfo() {}
};

enum eEnterRoomErrCode
{
    ERROR_ENTERROOM_NOSESSION = 1,      // 对不起,连接会话丢失,请稍后重试.
    ERROR_ENTERROOM_GAMENOTEXIST,       // 对不起,当前游戏服务不存在,请稍后重试.
    ERROR_ENTERROOM_TABLE_FULL,         // 对不起,当前房间已满,请稍后重试.
    ERROR_ENTERROOM_SEAT_FULL,          // 对不起,当前桌台已满,请稍后重试.
    ERROR_ENTERROOM_USERNOTEXIST,       // 对不起,查询玩家信息失败,请稍后重试.
    ERROR_ENTERROOM_SCORENOENOUGH,      // 对不起,您的金币不足,请充值后重试.
    ERROR_ENTERROOM_ALREAY_START,       // 对不起,当前游戏已经开始,请耐心等待下一局.
    ERROR_ENTERROOM_SCORELIMIT,         // 对不起,您的金币过多,无法进入当前房间.
    ERROR_ENTERROOM_USERINGAME,         // 对不起,您当前正在别的游戏中,无法进入当前房间.
    ERROR_ENTERROOserver_STOP,         // 对不起,当前游戏服务器正在维护,请稍后重试.
    ERROR_ENTERROOM_LONGTIME_NOOP,      // 对不起,您长时间没有操作,已被请出当前房间.
    ERROR_ENTERROOM_SWITCHTABLEFAIL,    // 对不起,当前游戏已经开始,请在游戏结束后换桌.
    ERROR_ENTERROOM_GAME_IS_END,        // 对不起,断线重连，游戏已结束
    ERROR_ENTERROOM_PASSWORD_ERROR,
    ERROR_ENTERROOM_STOP_CUR_USER,
    ERROR_ENTERROOM_USER_ORDER_SCORE,    //您正在下分，请稍后进入房间
    ERROR_ENTERROOM_MATCH_WAIT_JION,     //加入过于频繁，加入等待
    ERROR_ENTERROOM_MATCH_WAIT_OPEN,     //比赛等待开放
};

enum eUserStatus
{
    sGetout     = 0,        // player get out.
    sFree,                  // player is free.
    sSit,                   // player is sitdown.
    sReady,                 // player is ready.
    sPlaying,               // player is playing.
    sOffline,               // player is offline.
    sLookon,                // player is lookon.
    sGetoutAtplaying        // player is get out at playing.
};
//战绩详情的操作类型
enum eReplayOperType
{                       //  炸金花  | 抢庄牛牛,三公  | 21点
    opStart         = 1,// 锅底     |               |
    opBet           = 2,//          |   抢庄        | 开始下注
    opFollow        = 3,//  跟注    |               | 发牌
    opAddBet        = 4,// 加注     |   下注        |
    opCmprOrLook    = 5,// 比牌     |               | 庄家看牌（保险）
    opLkOrCall      = 6,// 看牌     |               | 闲家要牌
    opQuitOrBCall   = 7,// 弃牌     |               | 庄家要牌
    opShowCard      = 8,//          |               | 摊牌
    opUnkonw0       = 9,//
    opUnkonw1       = 10,//
    opCmprFail      = 11,// 比牌失败 |               | 押注？
    opBuyInsure     = 12,//         |               | 买保险
    opCall          = 13,//         |               | 要牌
    opLeave         = 14,// 离开    |               | 分牌
    opDouble        = 15,//         |               | 加倍
    opStop          = 16,//         |               | 停牌
    opBanker        = 17,//         |               | 定庄
    opPass          = 18,//  过牌    |            |
};
//#pragma pack()

enum eMatchForbidBits
{
    MTH_FORB_SAME_AGENT     = 0,//forbid match same agent
    MTH_FORB_DIFF_AGENT    = 1,//forbid match different agent
    MTH_FORB_SAME_IP       = 2,//forbid match same ip
    MTH_FORB_DIFF_IP       = 3,//forbid match diffrent ip
    MTH_SINGLE_PLAYER      = 4,//forbid match diffrent ip
    MTH_PLAYER_CNT         = 5,//count player in list
    MTH_QUANRANTE_AREA     = 6,//quanrante area
    MTH_BLACKLIST           = 7,
    MTH_MAX                 = 8,
};

//add by caiqing
//redis public msg
enum class eRedisPublicMsg
{
    bc_luckyGame     = 0,       //broadcast lucky Game 
    bc_marquee       = 1,       //Marquee
    bc_lkJackpot     = 2,       //Jackpot
    bc_updateTask    = 3,       //updateTask
    bc_update_proxy_info    = 4,   //update proxy info
    bc_update_white_list    = 5,   //update white list info
    bc_update_game_ver      = 6,     //update game version
    bc_apiserver_repair     = 7,
    bc_update_user_white_list,     //update user white list 
    bc_update_temp_player_info,     //update temp player info
    bc_uphold_game_server = 20, //维护游戏服
    bc_uphold_login_server=21,  //维护登录服
    bc_uphold_order_server=22,  //维护上下分服
    bc_load_login_server_cfg=23,//加载登录服配置

    bc_public_notice=50,  //广播公告消息
    bc_kick_out_notice,  //踢人消息
    
    bc_others        = 100,     //broadcast others 
};

// key ID 
enum class eRedisKey
{
    //set 集合
    set_lkGameUserId            = 5,//正在玩幸运转盘的玩家ID
    //list 列表
    lst_lkGameMsg               = 100,//幸运转盘的获奖信息
    lst_sgj_JackpotMsg          = 102,//水果机奖池获奖信息
    // 
    has_lkJackpot               = 105,//奖池信息
    has_incr_userid             = 110,//用于玩家自增的ID
    // string 字符类型
    str_lockId_lkMsg            = 200,//redis,lk锁
    str_lockId_jp_1             = 201,//redis,jp锁
    str_lockId_jp_2,                  //redis,jp2锁
    str_lockId_jp_3,                  //redis,jp2锁
    str_lockId_jp_4,                  //redis,jp2锁
    str_lockId_jp_5,                  //redis,jp2锁
    str_demo_ip_limit,                // 试玩环境限制IP访问频率
    str_others                  = 301,//其它字符类型
};
// 彩金池类型（共5个池）
enum class eJackPotPoolId
{
    jp_sgj           = 0,       //0号彩金
};
// 操作彩金池方式
enum class eOpJackPotType
{
    op_inc           = 0,       //累加彩金
    op_set           = 1,       //设置彩金
};
// 公共函数接口ID类型
enum class eCommFuncId
{
    fn_sgj_jackpot_rec           = 0,       //水果机奖记录
    fn_id_1,                                //
    fn_id_2,                                //
    fn_id_3,                                //
    fn_id_4,                                //
    fn_id_5,                                //
    fn_id_6,                                //
    fn_id_7,                                //
    fn_id_8,                                //
    fn_id_9,                                //
    fn_id_10,                                //
};
// 服务器维护状态(0不维护/1维护/2不开下局维护)
enum eUpholdType
{
    op_start          = 0,       //不维护
    op_stop           = 1,       //维护
    op_stop_next      = 2,       //不开下局维护
};

// 游戏ID
enum class eGameKindId
{
    honghei      = 210,      //红黑大战
    zjh          = 220,      //炸金花
    jszjh        = 230,      //极速炸金花
    qzzjh        = 240,      //抢庄炸金花
    qzxszzjh     = 300,      //选三张（炸金花）
    gswz         = 400,      //港式五张
    jcfish       = 500,      //捕鱼
    hjk          = 600,      //21点
    ddz          = 610,      //斗地主
    dzpk         = 620,      //德州扑克
    s3s          = 630,      //十三水 
    xlch         = 650,      //血流成河
    ebg          = 720,      //二八杠
    pj           = 730,      //牌九
    erqs         = 740,      //二人麻将
    qzxszsg      = 820,      //选三张（三公）
    qznn         = 830,      //抢庄牛牛
    sg           = 860,      //三公
    tbnn         = 870,      //通比牛牛
    bbqznn       = 880,      //抢庄牛牛（百变场）
    kpqznn       = 890,      //看牌抢庄牛牛
    lh           = 900,      //龙虎
    bjl          = 910,      //百家乐
    slwh         = 920,      //森林舞会
    brnn         = 930,      //百人牛牛
    sgj          = 1810,     //水果机
    jsys         = 1940,     //金鲨银鲨
    bcbm         = 1960,     //奔驰宝马
};




#define MAX_VERIFY_CODE_LOGIN_IDLE_TIME  (58)
#define MAX_FUNC_TIME        (100)



static std::string REDIS_KEY = "AliceLandy1234567890";
static std::string KEY = "50222EF5352341CA9725940AC299C701";
static int KEY_SIZE = 2455;


/* encrypted private key.
static std::string PRI_KEY =
        "90F1E9388E3EB5C58406A94430298B8536CEEDA2BF6FDF42CBA172148E8E8C96D0CFBB708E90C6C42FE8A177C243BC92B30BB21C4E805F691FB622EADD8CE85"
        "4BCC449BF297F55F41CD923A5427A47FCB58DA4362060B581088BF0EFE498541A93760127FC25748CB0844F33BEC1CF972E6E375510D971C62DCBA5709F7379"
        "289016C2836677A135DBC1CD6CC7C04C0BBF4E48332C86B61DD3BBE13296D5C0D4543A89599E86695B7AB387C6FAEE7AF50C9CA8BA543DE175B7990AB7433F3"
        "BC44CFC90C4CA2BF61BE74039AEFF0E4F5147D8F527C4941F655E7D2B4851D463FAC1CB9500E555B3169FD2884400303CA91F09601541E69A15930C921708EC"
        "D1C13E74179F2E3AFA7E7668EA943D5EE37F01FA7851B537B9CB2E6070781716815BA4E2D31E1B29B6EBD9CAF0DE0A51AADC9988C65136A4B7CDE771F1EF465"
        "B49E0C4A32970F5850EF23B0A9C541E403AB98E63CBCEF80A050F1A045FFDBA0D0AA9411FF0D888F2B575514A379838067CC63A3C5225F9BA56A15B6372FBA5"
        "D0A35494C1E6C56DDCE41E5DF83AAA5A95B1CE442354C6640E8C6F6B452837A9D9513AB8557ED42559FA2E242240E698181BC7BCFD01F305FEDA1AE52654512"
        "95E5ECD0FA829CBE44C69ADC181444337C7C4EC6D2D6F54124600A37D25BE06E4C818222F929D4426A9259D70132D77CB8036E93161D91FD61810959B44E115"
        "E95A350928159F30023DB635A2014511CDA9ABA51AD39F387D608BE9C00F88FDC8DB44084B6F8F238CF058252C94E8140F6458CCD508B902978ADCE30A5066E"
        "19BE02A2C151F32D0BA154E3F5EC3220F8F8DBDC649B01E2D283B4B4FB3B338C0C49F231F3712672253CB52EDE6A632000AD2B1CF96ABE7400B6F9D896F7D20"
        "06E253DF82F7C5D7A49ABB77613452679EF8F261E28087D0359B249DBE597A54CEB1F4BA89CB30963C240C8337C7E2782E4A3A4DB0C707CAC49D0E923E0FFA0"
        "83E0AA6096FB0AB2973F6B2D35A87146A0D60DD649249A94B32A6C1ED341248DCB0CAAD96E6D66E327748C0178EA2472A9B0FDAE6E23E8088637F5B33C859AF"
        "05F24E5B6A29107860DA14A61DCD1DAAA1E0F17E71BED64E0D888534B24B655B8A2A910D7B5CD87AB3077E980937AE22923C827F25A1726A2C500CA42CC2C52"
        "B8C1F540046268585B5F98C782CB08FFD608F1BA3BE67D810C4CF310B654C0D1D9F63935DBDB1A65678577F485DCE55EB78D29ED567D05AD88F02E6E4E33F6D"
        "E57FD10D00ED2EDD790D513FA8FEA9E59A15357B4EE5BAC5D3CB2022B9DB7503174DACED9651EB919B47FDBBA57163CDD4CAE9319B2B64F567C3E45CD7C66AB"
        "52BD3A69B6DAD42B7A7715F500867468195ECA02FAE7D7ED038B191B0494F949224FB57AE6AE54B9E4F9F6D0E26E2DF06EC402C1161AA3B06283B42B798F26E"
        "BCCCFC51A505AEC75DC518D10F73A4C03E99932D2FA45B670FF776FF91B85D44D91933A4161EA1AB6F4CEA80E8B59CEF8F9C13E45D480FCCC74307B26BB32DE"
        "E1003CDFCF99103B4FA80A49B9CDBF9A086AC63F1D5609656EC8CB36BD442BD1389887935CE2A2CBB220D9C166F3E2ABD724A105899A0D00219AA832E8DBFC8"
        "06EB350F647D2EBD1BEBB2FCE9531DEE77CB2063C46168BA35B93772AD73EBB5714136EF461AFA087F7D4E618F4ADC8798783828AE0D4A3C326A6E549209BFB"
        "581538229C6B86701E3A56A8A93E83614EFC8495BBF6DCAEF7B68F2192C07DB6A7CB6A918607AF7B2524E05148FB0921B6701AEE348E05BCE5C1FA5CFDA5B82"
        "718BF15759AD0E395B1B0CA6FB36833BC9E77F3961DCE127CDB5DD96DE77C12205C1AF798BD0DDFEDC6B0D1989EE2223930C45848DF190129C22716A5809178"
        "AEFFFA2B6CF7ECFF00564AE19D8031CDBFFD0E2B8F37FB96F57E3CE5740CAE3E46CF597D497BF1596B1F13F81A6699C9CEE9A8FAC61383C127522D26112F900"
        "9FB890C92AA04DA337CB78E69A08D3BF02108CD07EED29339D9B68B3075B4A01AED40B992788204B18E10F76CFAE81A5360E29F92614F4AB8D8AD5BCC8EB04D"
        "BEEB4B014B3B6E98F971EF0A13971A3DE48A5C95E809A10A15E0E45AD272E01D8F18CCB10D53217BAB58D69F225054759760E7ED6266E30E7DB43D24EDE50D0"
        "05F0F684A32519C39EE4EC17C59A4A0B4E2AC587DEA2C3C489AC5DF307E478069114BF35FEC5C1F98EDED56B4BA4A45396780486AC07DDA08DD0015750F5EBA"
        "9655BA4E0D8CD6F4C670B5C7027BB2D3CC73106FE65FACC1897F6A7D87EC6FD1DCE70414B22058FE6788883D0BE01F0480CF719FB934D1B9CA543E2A980A15B"
        "91A140A8E1F97A74807CFCB6FB1D8FFCB4E492DACA26D2041509687C96FA136198D7F69DD300C2C98EA091EDB8F466AFB8EBBF94ED6DB50138B2F102F0E775B"
        "FE7660648B544E3192EF35D1B7367DA262D0DE22FF23287A11F177B68B2D08ADA541A7A5D0DEF66E650490289DF1CD30333D158714EC855E2B801ECF9735557"
        "DAD1A62963574181714A8CF7F8791BC2B38CAE1554E107F1E8B80400725B2BBB7EA88C8DEEE743EF8A0618EA6BC6880A66BD0918E82FB17F43DAE24ED48FC6F"
        "EB7B3DBFBF24A8485342B692B303D72174E971A995C9229F49ECCC79BEE0FAE822B654EDA038F25C381712518F365F8DD341AC891DC0848649DD982CFD6E0AC"
        "697800D374EBD6C5FF3CAFCD6CAD086B0BF1BEBB633CD25398B733C146A13BDCC23177185BEA16AB913F7481BC12B483833F8E72428DA480BBD66A73832BC8B"
        "499F92A877F590FF0261022F1A32491AE4A7D4F5308C49925228CC4CAC8D8365517A822E539B48666F3667723624499C0C804E0D5A27B1F23C128D8E5BBA56C"
        "5605784379528E89E3DD1E3F009070A4DEA56DAE3B0469FACEFA809184A23C0F3943CB99DF06D24A8162879E851E190509D7C73983AF280294742E452648103"
        "058C7492460C532BC144459A58FBD96C2862E81EFDAC467A279D0565F262D5A927DACAECCA6846C6FFEC3E8EF69F26FECFAE01B60B7F4E2BB1DB375901066E3"
        "6D1473316826F02657D7688836BB906CF3170D823F886B15E50F619C27C67ED33483A381C63AB4099AEDEFB585F33B1021BAEC8CBC7009998C45F1555ABD116"
        "FDDF49BD2D1BA52A9774DA99B01CC1E7B4E2770B0DE62D96E40AF7C8E16187526239A6C8DECB0ACD77A5D222B9E734103054319B64D7D89A43879649B6FC2C0"
        "D2859D606F7A912A7436AFF4B0CC3A55D9E8B3726627C90CDBC8AA2570FC8D489B33B81AC847CD53906AD808E7EB48A3F48065F2588916A0EDA0CE1BAA576E1"
        "BFF4D80A4B34081359C93C768C9623B54E121994BE1F24CD65531FACFB17ED7D59922E1196D714DE01E034973D8D1A5C667D018ADC6EE4ACF0538A265E52BB1"
        "CFD2C29F4FBA14BDC120B0C86AAE11AE35C9EF47CBF06EC3F7FB9BCEBE445E8C3F7C81AD286A2C3DD288B73AE40BA304797628000000000000";
*/


static std::string ORG_PRI_KEY =
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "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\n"
        "-----END RSA PRIVATE KEY-----\n";



// per frame timeout millisecond.
#define TIMEOUT_PERFRAME        (50)

// define the kind name now.
#define KINDNAME_LEN            (32)
#define GAMEDLL_LEN             (32)


// define score type value.
#define SCORE_TYPE_NULL         (0)
#define SCORE_TYPE_WIN          (1)
#define SCORE_TYPE_LOST         (2)

// define the special kick.
#define KICK_GS                 (0x01)
#define KICK_HALL               (0x02)
#define KICK_CLOSEONLY          (0x100)
#define KICK_LEAVEGS            (0x200)


// max header id is 16 value.
#define MAX_HEADID                  (16)
#define MAX_HEADBOXID               (32)

#define LEN_SERVER                  (32)

// max header id is 16 value.
#define MAX_HEADID                  (16)
#define MAX_HEADBOXID               (32)



// define the special redis cache account.
#define REDIS_EARNSCORE_PREFIX      "earnscore_"
#define REDIS_ACCOUNT_PREFIX        "account_"
#define REDIS_ONLINE_PREFIX         "online_"
#define REDIS_GSINFO_PREFIX         "GameServer:"
#define REDIS_SCORE_PREFIX          "scores:"
#define REDIS_WINSCORE              "winscore"
#define REDIS_ADDSCORE              "addscore"
#define REDIS_SUBSCORE              "subscore"
//公共消息( + 消息ID)
#define REDIS_PUBLIC_MSG            "rs_public_msg_"
// 公共Key_ 
#define REDIS_KEY_ID                "rs_keyId_"

// define the nick name length.
#define LEN_ACCOUNT                 (33)
#define LEN_NICKNAME                (33)
#define LEN_HEAD_URL                (256)
#define LEN_IP                      (16)
#define LEN_USER_LOCATE             (64)

#define LEN_UUID                    (33)
#define LEN_PASS                    (33)
#define LEN_DYNAMICPASS             (33)

#define LEN_MOBILE_NUM              (12)
#define LEN_MACHINETYPE             (64)
#define LEN_MACHINESERIAL           (64)
#define LEN_ALIPAY_ACCOUNT          (40)
#define LEN_REALNAME                (30)
#define LEN_BANKCARD                (21)
#define LEN_REALNAME                (30)



#pragma pack(1)


// server status value.
enum eServerStatus
{
    SERVER_STAT_DISABLE = 0,
    SERVER_STAT_ENABLE  = 1,
};

// server state value.


// user status value.


//game end tag.
enum eGameEndTag
{
    GER_NORMAL = 0,
    GER_USER_LEFT,
    GER_DISMISS,
    GER_FLOW_BUREAU,
};

// enter room status.



//// global User base info.
//struct Global_UserBaseInfo
//{
//    uint32_t   nUserId;                            // set the user id.
//    uint32_t   nPromoterId;                        // self promoter id.
//    uint32_t   nBindPromoterId;                    // binded promoter id.

//    uint32_t   nGem;                               // set the gem.
//    uint32_t   nPlatformId;                        // set the platform id.
//    uint32_t   nChannelId;                         // channel id.

//    uint8_t  nOSType;                             // ostype

//    uint8_t  nGender;                            // set the gender.
//    uint8_t  nHeadId;                            // set the header id.
//    uint8_t  nHeadboxId;                         // set the header box id.
//    uint8_t  nVipLevel;                          // set the vip level.
//    uint8_t  nTemp;                              // is temp account.
//    uint8_t  nIsManager;                         // is manager account.
//    uint8_t  nIsSuperAccount;                    // is super account

//    uint32_t   nTotalRecharge;                     // total recharge.
//    int64_t   nUserScore;                         // set the score info.
//    int64_t   nBankScore;                         // banker score value.
//    int64_t   nChargeAmount;                      // user charge amount.
//    int64_t nLoginTime;
//    int64_t nGameStartTime;

//    char    szHeadUrl[LEN_HEAD_URL];            // set the header url.
//    char    szAccount[LEN_ACCOUNT];             // add the account value.
//    char    szNickName[LEN_NICKNAME];           // set the nick name.
//    char    szIp[LEN_IP];                       // set the ip.
//    char    szLocation[LEN_USER_LOCATE];        // set the localtion.

//    char    szPassword[LEN_PASS];               // login password.
//    char    szDynamicPass[LEN_DYNAMICPASS];
//    char    szBankPassword[LEN_PASS];           // bank  password.

//    char    szMobileNum[LEN_MOBILE_NUM];        // mobile phone number.
//    char    szMachineType[LEN_MACHINETYPE];     // machine type.
//    char    szMachineSerial[LEN_MACHINESERIAL]; // machine serial.

//    // alipay,bank card id.
//    char    szAlipayAccount[LEN_ALIPAY_ACCOUNT];// alipay account.
//    char    szAlipayName[LEN_REALNAME];         // alipay real name.
//    char    szBankCardNum[LEN_BANKCARD];        // bank card number.
//    char    szBankCardName[LEN_REALNAME];       // bank card name.

//    uint8_t  cbUserStatus;                       // account status (1:enable, 2:disabled, 3:deleted.)
//};

//// the user score data info.
//struct Global_UserScoreInfo
//{
//    uint32_t   nUserId;                // set the user id
//    uint32_t   nWinCount;              // set the win count.
//    uint32_t   nLostCount;             // set the lost count.
//    uint32_t   nDrawCount;             // set the draw count.
//    uint32_t   nFleeCount;             // set the flee count.
//    uint32_t   nPlayTime;              // set the play time.
//};


////score kind value.
//enum eScoreKind
//{
//    ScoreKind_Win,		//win
//    ScoreKind_Lost,		//lost
//    ScoreKind_Draw,		//he
//    ScoreKind_Flee		//flee
//};



// score change type.
enum eScoreChangeType
{
    SCORE_CHANGE_UNKNOWN = 0,
    SCORE_CHANGE_EXCHANGE,
    SCORE_CHANGE_RECHARGE,
    SCORE_CHANGE_PRESENT,
    SCORE_CHANGE_RECHARGERET,
    SCORE_CHANGE_BENEFIT_REWARD,
    SCORE_CHANGE_REDPAPER,
    SCORE_CHANGE_BANKSAVE,
    SCORE_CHANGE_BANKTAKEN,
    SCORE_CHANGE_TASK_REWARD,
    SCORE_CHANGE_EXCHANGERET,
    SCORE_CHANGE_MAIL_REWARD,
    SCORE_CHANGE_PLAYGAME,
    SCORE_CHANGE_EXCHANGE_RET,
    SCORE_CHANGE_REPORT
};



// system message type.
enum eSysMsgType
{
    SMT_CHAT   = 1,						// chat message.
    SMT_EJECT  = 2,						// eject message.
    SMT_GLOBAL = 4,						// global message.
    SMT_PRMOPT = 8,						// prompt message.
    SMT_SCROLL = 16,					// scroll text.
    SMT_JACKPOT= 32,					// jackpot message.
};

// define score type value.
#define SCORE_TYPE_NULL (0)             // score type null.
#define SCORE_TYPE_FLEE (0)             // score type free.
#define SCORE_TYPE_WIN  (1)             // player win.
#define SCORE_TYPE_LOST (2)             // player lost.



// define the game status play status value.
#define GAME_STATUS_PLAY (GAME_STATUS_START)


struct tagStockInfo
{
    int64_t   nStorageControl;            // 库存控制值,控牌对应值
    int64_t   nStorageLowerLimit;         // 库存控制下限值
    int64_t   nAndroidStorage;            // 机器人库存控制值
    int64_t   nAndroidStorageLowerLimit;  // 机器人库存
    uint32_t  wSystemAllKillRatio;        // 系统通杀概率(百分比)
};


struct tagStorageInfo
{
    int64_t lEndStorage;					// 当前库存
    int64_t lLowlimit;					    // 最小库存
    int64_t lUplimit;						// 最大库存
    int64_t lSysAllKillRatio;				// 系统通杀率
    int64_t lReduceUnit;                    // 库存衰减
    int64_t lSysChangeCardRatio;			// 系统换牌率
};


#define REC_MAXPLAYER   (5)         // max player count is 5.
struct tagRecPlayer
{
    tagRecPlayer()
    {
        userid  = 0;
        account = 0;
        changed_score = 0;
    }

    int64_t changed_score;            // the score has been changed.
    int   account;                  // the special account id now.
    int   userid;                   // the userid of current chair.
};

// game record playback now.
struct tagGameRecPlayback
{
    tagGameRecPlayback()
    {
        // initialize value.
        rec_roundid   = 0;
        banker_userid = 0;
    }

    int   rec_roundid;                      // output current game round id(set 0 if input).
    int banker_userid;                      // current banker user item id content value.
    tagRecPlayer player[REC_MAXPLAYER];     // the special player array content item.
    std::string content;                         // the content of record protobuf serial data.
};




#pragma pack()



//#define REDIS_REGISTER_LOGIN_3S_CHECK   "register_login_3s_check_"


#endif
