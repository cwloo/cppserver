#include "Game.h"
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
//#include <boost/algorithm/algorithm.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/regex.hpp>

#include "public/Env.h"
#include "public/NetCardIP.h"

GameSrv* gServer = NULL;
muduo::net::EventLoop* gMainEventLoop = NULL;


//StopService 服务终止
static void StopService(int signo) {
	if (gServer) {
		gServer->quit();
	}
	if (gMainEventLoop) {
		gMainEventLoop->quit();
	}
}


int main(int argc, char* argv[]) {
	if (argc < 3) {
		LOG_INFO << "argc < 2, error param gameid & roomid";
		exit(1);
	}
	uint32_t gameid = strtol(argv[1], NULL, 10);
	uint32_t roomid = strtol(argv[2], NULL, 10);
	//检查配置文件
	if (!boost::filesystem::exists("./conf/game.conf")) {
		LOG_INFO << "./conf/game.conf not exists";
		return -1;
	}
    
	//读取配置文件
	boost::property_tree::ptree pt;
	boost::property_tree::read_ini("./conf/game.conf", pt);
	
	//日志目录/文件 logdir/logname
	std::string logdir = pt.get<std::string>("Game.logdir", "./log/Game/");
	std::string logname = pt.get<std::string>("Game.logname", "Game");
	int loglevel = pt.get<int>("Game.loglevel", 1);
	if (setEnv(logdir, logname, loglevel) < 0) {
		return -1;
	}
	LOG_INFO << __FUNCTION__ << " --- *** " << logdir + logname << " 日志级别 = " << loglevel;
	
	//获取指定网卡ipaddr
	std::string strIpAddr;
	std::string netcardName = pt.get<std::string>("Global.netcardName", "eth0");
	if (IpByNetCardName(netcardName, strIpAddr) < 0) {
		LOG_FATAL << __FUNCTION__ << " --- *** 获取网卡IP失败";
		return -1;
	}
    LOG_INFO << __FUNCTION__ << " --- *** " << "网卡名称 = " << netcardName << " 绑定IP = " << strIpAddr;
	
	//////////////////////////////////////////////////////////////////////////
	//zookeeper服务器集群IP
	std::string strZookeeperIps = "";
	{
		auto const& childs = pt.get_child("Zookeeper");
		for (auto const& child : childs) {
			if (child.first.substr(0, 7) == "Server.") {
				if (!strZookeeperIps.empty()) {
					strZookeeperIps += ",";
				}
				strZookeeperIps += child.second.get_value<std::string>();
			}
		}
		LOG_INFO << __FUNCTION__ << " --- *** " << "ZookeeperIP = " << strZookeeperIps;
	}
	//////////////////////////////////////////////////////////////////////////
	//RedisCluster服务器集群IP
	std::map<std::string, std::string> mapRedisIps;
	std::string redisPasswd = pt.get<std::string>("RedisCluster.Password", "");
	std::string strRedisIps = "";
	{
		auto const& childs = pt.get_child("RedisCluster");
		for (auto const& child : childs) {
			if (child.first.substr(0, 9) == "Sentinel.") {
				if (!strRedisIps.empty()) {
					strRedisIps += ",";
				}
				strRedisIps += child.second.get_value<std::string>();
			}
			else if (child.first.substr(0, 12) == "SentinelMap.") {
				std::string const& ipport = child.second.get_value<std::string>();
				std::vector<std::string> vec;
				boost::algorithm::split(vec, ipport, boost::is_any_of(","));
				assert(vec.size() == 2);
				mapRedisIps[vec[0]] = vec[1];
			}
		}
		LOG_INFO << __FUNCTION__ << " --- *** " << "RedisClusterIP = " << strRedisIps;
	}
	//////////////////////////////////////////////////////////////////////////
	//redisLock分布式锁
	std::string strRedisLockIps = "";
	{
		auto const& childs = pt.get_child("RedisLock");
		for (auto const& child : childs) {
			if (child.first.substr(0, 9) == "Sentinel.") {
				if (!strRedisLockIps.empty()) {
					strRedisLockIps += ",";
				}
				strRedisLockIps += child.second.get_value<std::string>();
			}
		}
		LOG_INFO << __FUNCTION__ << " --- *** " << "RedisLockIP = " << strRedisLockIps;
	}
	//////////////////////////////////////////////////////////////////////////
	 //MongoDB
	std::string strMongoDBUrl = pt.get<std::string>("MongoDB.Url");

	//TCP监听网关客户端，server_
	std::string tcpIp = pt.get<std::string>("Game.ip", "");
	int16_t tcpPort = pt.get<int>("Game.port", 8120);
	//网络I/O线程数
	int16_t numThreads = pt.get<int>("Game.numThreads", 10);
	//worker线程数
	int16_t numWorkerThreads = pt.get<int>("Game.numWorkerThreads", 10);
	//Worker线程单任务队列大小
	int kMaxQueueSize = pt.get<int>("Game.kMaxQueueSize", 1000);
	//是否调试
	bool isdebug = pt.get<int>("Game.debug", 1);
	if (!tcpIp.empty() && boost::regex_match(tcpIp,
		boost::regex(
			"^(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|[1-9])\\." \
			"(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)\\." \
			"(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)\\." \
			"(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)$"))) {
		strIpAddr = tcpIp;
	}
	//主线程EventLoop，I/O监听/连接读写 accept(read)/connect(write)
	muduo::net::EventLoop loop;
	muduo::net::InetAddress listenAddr(strIpAddr, tcpPort);
	//server
	GameSrv server(&loop, listenAddr);
	server.isdebug_ = isdebug;
	server.strIpAddr_ = strIpAddr;

	boost::algorithm::split(server.redlockVec_, strRedisLockIps, boost::is_any_of(","));
	if (
		server.initZookeeper(strZookeeperIps) &&
		server.initMongoDB(strMongoDBUrl) &&
		server.initRedisCluster(strRedisIps, redisPasswd)) {
		gServer = &server;
		if (server.init_server(gameid, roomid)) {
			//registerSignalHandler(SIGTERM, StopService);
			registerSignalHandler(SIGINT, StopService);
			//网络I/O线程池，I/O收发读写 recv(read)/send(write)，worker线程池，处理游戏业务逻辑
			server.start(numThreads, numWorkerThreads, kMaxQueueSize);
			loop.loop();
		}
	}
    return 0;
}
