#ifndef HALL_SERVER_H
#define HALL_SERVER_H

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

//@@ HallSrv
class HallSrv : public boost::noncopyable {
public:
	//@@ CmdCallback
    typedef std::function<
        void(const muduo::net::TcpConnectionPtr&,
            uint8_t const*, size_t,
			packet::header_t const*,
			packet::internal_prev_header_t const*)> CmdCallback;
	//@@ CmdCallbacks
	typedef std::map<uint32_t, CmdCallback> CmdCallbacks;

public:
    //HallSrv ctor
    HallSrv(muduo::net::EventLoop* loop,
        const muduo::net::InetAddress& listenAddr);
    //Gateway dctor
    ~HallSrv();
	void quit();
	void initHandlers();

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
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	/// <summary>
	/// 玩家登录
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_login_servers(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	/// <summary>
	/// 玩家离线
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_on_user_offline(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	/// <summary>
	/// 获取game_kind信息(所有游戏房间信息)
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_get_game_info(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	/// <summary>
	/// 获取玩家游戏情况(断线重连) 
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_get_playing_game_info(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	/// <summary>
	/// 查询指定类型游戏节点 
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_get_game_server_message(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	//获取房间人数
	void cmd_get_room_player_nums(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	//修改玩家头像
	void cmd_set_headid(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	//修改玩家昵称
	void cmd_set_nickname(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	//查询玩家积分
	void cmd_get_userscore(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	//玩家游戏记录
	void cmd_get_play_record(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	//游戏记录详情
	void cmd_get_play_record_detail(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	//大厅维护接口
	void cmd_repair_hallserver(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	//获取任务列表
	void cmd_get_task_list(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	//获取任务奖励
	void cmd_get_task_award(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* msg, size_t msgLen,
		packet::header_t const* header_,
		packet::internal_prev_header_t const* pre_header_);
	/// <summary>
	/// 随机一个指定类型游戏节点
	/// </summary>
	/// <param name="roomid"></param>
	/// <param name="ipport"></param>
	void random_game_server_ipport(uint32_t roomid, std::string& ipport);
public:
	//redis查询token，判断是否过期
	bool redis_get_token_info(
		std::string const& token,
		int64_t& userid, std::string& account, uint32_t& agentid);
	//db更新用户登陆信息(登陆IP，时间)
	bool db_update_login_info(
		int64_t userid,
		std::string const& loginIp,
		std::chrono::system_clock::time_point& lastLoginTime,
		std::chrono::system_clock::time_point& now);
	//db更新用户在线状态
	bool db_update_online_status(int64_t userid, int32_t status);
	//db添加用户登陆日志
	bool db_add_login_logger(
		int64_t userid,
		std::string const& loginIp,
		std::string const& location,
		std::chrono::system_clock::time_point& now,
		uint32_t status, uint32_t agentid);
	//db添加用户登出日志
	bool db_add_logout_logger(
		int64_t userid,
		std::chrono::system_clock::time_point& loginTime,
		std::chrono::system_clock::time_point& now);
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
	void onGatewayWatcherHandler(
		int type, int state,
		const std::shared_ptr<ZookeeperClient>& zkClientPtr,
		const std::string& path, void* context);
	void onGameWatcherHandler(int type, int state,
		const shared_ptr<ZookeeperClient>& zkClientPtr,
		const std::string& path, void* context);
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
    ::HallServer::GetGameMessageResponse gameinfo_;
	mutable boost::shared_mutex gameinfo_mutex_;
    ::HallServer::GetServerPlayerNumResponse room_playernums_;
	mutable boost::shared_mutex room_playernums_mutex_;
public:
	//房间节点map[roomid] = iplist
	std::map<int, std::vector<std::string>> room_servers_;
	mutable boost::shared_mutex room_servers_mutex_;

	//房间节点map[roomid] = iplist
	//std::map<int, std::vector<std::string>> room_servers_;
	//mutable boost::shared_mutex room_servers_mutex_;
	
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

	//worker线程池，内部任务消息队列
	std::vector<std::shared_ptr<muduo::ThreadPool>> threadPool_;

	//公共定时器
	std::shared_ptr<muduo::net::EventLoopThread> threadTimer_;
};

#endif // HallSrv_H
