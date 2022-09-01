#ifndef GAME_SERVER_H
#define GAME_SERVER_H

#include <muduo/base/Exception.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
//#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpServer.h>
#include <muduo/base/ThreadPool.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>  // memset
#include <string>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>
#include <map>
#include <list>
#include <vector>
#include <memory>
#include <iomanip>
//#include <sstream>
//#include <fstream>
#include <functional>
//#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "proto/Game.Common.pb.h"
#include "proto/HallServer.Message.pb.h"
#include "proto/GameServer.Message.pb.h"

#include "public/Global.h"
#include "public/IPFinder.h"
#include "public/Packet.h"
#include "public/StdRandom.h"
#include "public/zookeeperclient/zookeeperclient.h"
#include "public/zookeeperclient/zookeeperlocker.h"
#include "public/RedisLock/redlock.h"
#include "public/RedisClient/RedisClient.h"
#include "public/MongoDB/MongoDBClient.h"
#include "public/TraceMsg/TraceMsg.h"

#include "Entities.h"
#include "GameDefine.h"

#if BOOST_VERSION < 104700
namespace boost
{
template <typename T>
inline size_t hash_value(const boost::shared_ptr<T> &x)
{
    return boost::hash_value(x.get());
}
} // namespace boost
#endif

//@@
typedef std::shared_ptr<muduo::net::Buffer> BufferPtr;

//@@ GameSrv
class GameSrv : public boost::noncopyable {
public:
	//@@ CmdCallback
    //typedef std::function<
    //    void(const muduo::net::TcpConnectionPtr&,
    //        uint8_t const*, size_t,
	//		packet::header_t const*,
	//		packet::internal_prev_header_t const*)> CmdCallback;
	typedef std::function<
		void(const muduo::net::TcpConnectionPtr&,
			BufferPtr buf)> CmdCallback;
	//@@ CmdCallbacks
	typedef std::map<uint32_t, CmdCallback> CmdCallbacks;

public:
    //GameSrv ctor
    GameSrv(muduo::net::EventLoop* loop,
        const muduo::net::InetAddress& listenAddr);
    //Gateway dctor
    ~GameSrv();
	void quit();
	void initHandlers();
	/// <summary>
	/// 游戏初始化，创建桌子，机器人
	/// </summary>
	/// <param name="gameid"></param>
	/// <param name="roomid"></param>
	/// <returns></returns>
	bool init_server(uint32_t gameid, uint32_t roomid);
public:
	/// <summary>
	/// PING心跳
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_keep_alive_ping(
		const muduo::net::TcpConnectionPtr& conn, BufferPtr buf);
	/// <summary>
	/// 进入桌子
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_enter_room(
		const muduo::net::TcpConnectionPtr& conn, BufferPtr buf);
	/// <summary>
	/// 玩家准备
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_user_ready(
		const muduo::net::TcpConnectionPtr& conn, BufferPtr buf);
	/// <summary>
	/// 离开桌子
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_user_left(
		const muduo::net::TcpConnectionPtr& conn, BufferPtr buf);
	/// <summary>
	/// 玩家离线
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_user_offline(
		const muduo::net::TcpConnectionPtr& conn, BufferPtr buf);
	/// <summary>
	/// 逻辑消息
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_game_message(
		const muduo::net::TcpConnectionPtr& conn, BufferPtr buf);
	/// <summary>
	/// 节点维护
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_notifyRepairServerResp(
		const muduo::net::TcpConnectionPtr& conn, BufferPtr buf);

	bool GetUserBaseInfo(int64_t userid,
		UserBaseInfo& baseInfo);
	bool SendGameErrorCode(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t mainid, uint8_t subid,
		uint32_t errcode, std::string errmsg,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
public:
	//db加载game_kind信息
	bool db_load_room_kind_info(uint32_t gameid, uint32_t roomid);
	//redis查询token，判断是否过期
	bool redis_get_token_info(
		std::string const& token,
		int64_t& userid, std::string& account, uint32_t& agentid);
	
	//db更新用户在线状态
	bool db_update_online_status(int64_t userid, int32_t status);
	//db刷新所有游戏房间信息
	void db_refresh_game_room_info();
	void db_update_game_room_info();
	//redis刷新所有房间游戏人数
	void redis_refresh_room_player_nums();
	void redis_update_room_player_nums();
	//redis通知刷新游戏房间配置
	void on_refresh_game_config(std::string msg);
public:
    //zookeeper
    bool initZookeeper(std::string const& ipaddr);
	void zookeeperConnectedHandler();
	//自注册zookeeper节点检查
	void onTimerCheckSelf();
	//zk节点/服务注册/发现
	std::shared_ptr<ZookeeperClient> zkclient_;
	std::string nodePath_, nodeValue_, invalidNodePath_;

    //RedisCluster
    bool initRedisCluster(std::string const& ipaddr, std::string const& passwd);
    bool initRedisCluster();
	//RedisLock
	bool initRedisLock();
	//redis订阅/发布
	std::shared_ptr<RedisClient> redisClient_;
	std::string redisIpaddr_;
	std::string redisPasswd_;
	//redisLock
	std::vector<std::string> redlockVec_;

	//MongoDB
	bool initMongoDB(std::string const& url);
	bool initMongoDB();
	std::string mongoDBUrl_;
public:
	//MongoDB/RedisCluster/RedisLock
	void threadInit();
	//启动worker线程
	//启动TCP监听网关客户端
	void start(int numThreads, int numWorkerThreads, int maxSize);

    //大厅服[S]端 <- 网关服[C]端
private:
	void onConnection(const muduo::net::TcpConnectionPtr& conn);

	void onMessage(
		const muduo::net::TcpConnectionPtr& conn,
		muduo::net::Buffer* buf,
		muduo::Timestamp receiveTime);

	void asyncClientHandler(
		muduo::net::WeakTcpConnectionPtr const& weakConn,
		BufferPtr& buf,
		muduo::Timestamp receiveTime);
	//发送消息
	void sendMessage(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* data, size_t len, packet::header_t const* header,
		packet::internal_prev_header_t const* pre_header);
private:
	//所有游戏房间信息
    ::HallServer::GetServerPlayerNumResponse room_playernums_;
	mutable boost::shared_mutex room_playernums_mutex_;
public:
	//房间节点map[roomid] = iplist
	std::map<int, std::vector<std::string>> room_servers_;
	mutable boost::shared_mutex room_servers_mutex_;

	//房间节点map[roomid] = iplist
	//std::map<int, std::vector<std::string>> room_servers_;
	//mutable boost::shared_mutex room_servers_mutex_;
	
	GameInfo gameInfo_;
	RoomInfo roomInfo_;
	//是否调试
	bool isdebug_;

	//命令消息回调处理函数
	/*static*/ CmdCallbacks handlers_;

    //IP地址定位国家地域
	CIpFinder ipFinder_;
    
	//绑定网卡ipport
	std::string strIpAddr_;

	//最大连接数限制
	int kMaxConnections_;

    //监听网关客户端TCP请求
	muduo::net::TcpServer server_;

	//session hash运算得到用户worker线程
	std::hash<std::string> hash_session_;

	//当前TCP连接数
	muduo::AtomicInt32 numConnected_;

	//网络I/O线程数，worker线程数
	int numThreads_, numWorkerThreads_;

	//累计接收请求数，累计未处理请求数
	muduo::AtomicInt64 numTotalReq_, numTotalBadReq_;

	//map[userid] = weakConn
	INT::Entities entities_;

	//map[ip] = weakConn
	INT::Entities clients_;
	mutable boost::shared_mutex clients_mutex_;

	//worker线程池，内部任务消息队列
	std::vector<std::shared_ptr<muduo::ThreadPool>> threadPool_;

	//公共定时器
	std::shared_ptr<muduo::net::EventLoopThread> threadTimer_;

	//桌子worker线程池
	std::shared_ptr<muduo::net::EventLoopThreadPool> deskThreadPool_;
};

#endif // GameSrv_H
