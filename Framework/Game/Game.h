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
	/// ??????????????????????????????????????????
	/// </summary>
	/// <param name="gameid"></param>
	/// <param name="roomid"></param>
	/// <returns></returns>
	bool init_server(uint32_t gameid, uint32_t roomid);
public:
	/// <summary>
	/// PING??????
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_keep_alive_ping(
		const muduo::net::TcpConnectionPtr& conn, BufferPtr buf);
	/// <summary>
	/// ????????????
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_enter_room(
		const muduo::net::TcpConnectionPtr& conn, BufferPtr buf);
	/// <summary>
	/// ????????????
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_user_ready(
		const muduo::net::TcpConnectionPtr& conn, BufferPtr buf);
	/// <summary>
	/// ????????????
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_user_left(
		const muduo::net::TcpConnectionPtr& conn, BufferPtr buf);
	/// <summary>
	/// ????????????
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_user_offline(
		const muduo::net::TcpConnectionPtr& conn, BufferPtr buf);
	/// <summary>
	/// ????????????
	/// </summary>
	/// <param name="conn"></param>
	/// <param name="msg"></param>
	/// <param name="msgLen"></param>
	/// <param name="header_"></param>
	/// <param name="pre_header_"></param>
	void cmd_game_message(
		const muduo::net::TcpConnectionPtr& conn, BufferPtr buf);
	/// <summary>
	/// ????????????
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
	//db??????game_kind??????
	bool db_load_room_kind_info(uint32_t gameid, uint32_t roomid);
	//redis??????token?????????????????????
	bool redis_get_token_info(
		std::string const& token,
		int64_t& userid, std::string& account, uint32_t& agentid);
	
	//db????????????????????????
	bool db_update_online_status(int64_t userid, int32_t status);
	//db??????????????????????????????
	void db_refresh_game_room_info();
	void db_update_game_room_info();
	//redis??????????????????????????????
	void redis_refresh_room_player_nums();
	void redis_update_room_player_nums();
	//redis??????????????????????????????
	void on_refresh_game_config(std::string msg);
public:
    //zookeeper
    bool initZookeeper(std::string const& ipaddr);
	void zookeeperConnectedHandler();
	//?????????zookeeper????????????
	void onTimerCheckSelf();
	//zk??????/????????????/??????
	std::shared_ptr<ZookeeperClient> zkclient_;
	std::string nodePath_, nodeValue_, invalidNodePath_;

    //RedisCluster
    bool initRedisCluster(std::string const& ipaddr, std::string const& passwd);
    bool initRedisCluster();
	//RedisLock
	bool initRedisLock();
	//redis??????/??????
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
	//??????worker??????
	//??????TCP?????????????????????
	void start(int numThreads, int numWorkerThreads, int maxSize);

    //?????????[S]??? <- ?????????[C]???
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
	//????????????
	void sendMessage(
		const muduo::net::TcpConnectionPtr& conn,
		uint8_t const* data, size_t len, packet::header_t const* header,
		packet::internal_prev_header_t const* pre_header);
private:
	//????????????????????????
    ::HallServer::GetServerPlayerNumResponse room_playernums_;
	mutable boost::shared_mutex room_playernums_mutex_;
public:
	//????????????map[roomid] = iplist
	std::map<int, std::vector<std::string>> room_servers_;
	mutable boost::shared_mutex room_servers_mutex_;

	//????????????map[roomid] = iplist
	//std::map<int, std::vector<std::string>> room_servers_;
	//mutable boost::shared_mutex room_servers_mutex_;
	
	GameInfo gameInfo_;
	RoomInfo roomInfo_;
	//????????????
	bool isdebug_;

	//??????????????????????????????
	/*static*/ CmdCallbacks handlers_;

    //IP????????????????????????
	CIpFinder ipFinder_;
    
	//????????????ipport
	std::string strIpAddr_;

	//?????????????????????
	int kMaxConnections_;

    //?????????????????????TCP??????
	muduo::net::TcpServer server_;

	//session hash??????????????????worker??????
	std::hash<std::string> hash_session_;

	//??????TCP?????????
	muduo::AtomicInt32 numConnected_;

	//??????I/O????????????worker?????????
	int numThreads_, numWorkerThreads_;

	//????????????????????????????????????????????????
	muduo::AtomicInt64 numTotalReq_, numTotalBadReq_;

	//map[userid] = weakConn
	INT::Entities entities_;

	//map[ip] = weakConn
	INT::Entities clients_;
	mutable boost::shared_mutex clients_mutex_;

	//worker????????????????????????????????????
	std::vector<std::shared_ptr<muduo::ThreadPool>> threadPool_;

	//???????????????
	std::shared_ptr<muduo::net::EventLoopThread> threadTimer_;

	//??????worker?????????
	std::shared_ptr<muduo::net::EventLoopThreadPool> deskThreadPool_;
};

#endif // GameSrv_H
