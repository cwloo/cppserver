/************************************************************************/
/*    @author create by andy_ro@qq.com                                  */
/*    @Date		   03.18.2020                                           */
/************************************************************************/
#include "Clients.h"
#include  <assert.h>
#include <muduo/base/Logging.h>

//@@ TcpClient
TcpClient::TcpClient(
	muduo::net::EventLoop* loop,
	const muduo::net::InetAddress& serverAddr,
	const std::string& name,
	Connector* owner)
	: client_(loop, serverAddr, name)
    , owner_(owner) {
	client_.setConnectionCallback(
		std::bind(&TcpClient::onConnection, this, std::placeholders::_1));
	client_.setMessageCallback(
		std::bind(&TcpClient::onMessage, this,
			std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

const std::string& TcpClient::name() const {
	return client_.name();
}

muduo::net::TcpConnectionPtr TcpClient::connection() const {
	return client_.connection();
}

muduo::net::EventLoop* TcpClient::getLoop() const {
	return client_.getLoop();
}

void TcpClient::connect() {
	client_.connect();
}

void TcpClient::reconnect() {
#if 0
	client_.connect();
#else
	client_.reconnect();
#endif
}

void TcpClient::disconnect() {
	client_.disconnect();
}

void TcpClient::stop() {
	client_.stop();
}

bool TcpClient::retry() const {
	return client_.retry();
}

void TcpClient::enableRetry() {
	client_.enableRetry();
}

void TcpClient::onConnection(const muduo::net::TcpConnectionPtr& conn) {
	
	conn->getLoop()->assertInLoopThread();
	
	if (conn->connected()) {
		owner_->onConnected(conn, shared_from_this());
	}
	else {
		owner_->onClosed(conn, client_.name());
	}
}

void TcpClient::onMessage(
	const muduo::net::TcpConnectionPtr& conn,
	muduo::net::Buffer* buf, muduo::Timestamp receiveTime) {
	owner_->onMessage(conn, buf, receiveTime);
}

//@@ Connector
Connector::Connector(
	muduo::net::EventLoop* loop)
	: loop_(CHECK_NOTNULL(loop)) {
}

Connector::~Connector() {
	closeAll();
}

//add
void Connector::add(
	std::string const& name,
	const muduo::net::InetAddress& serverAddr) {
	QueueInLoop(loop_,
		std::bind(&Connector::addInLoop, this, name, serverAddr));
}

//remove
void Connector::remove(std::string const& name, bool lazy) {
	QueueInLoop(loop_,
		std::bind(&Connector::removeInLoop, this, name, lazy));
}

//check
void Connector::check(std::string const& name, bool exist) {
	QueueInLoop(loop_,
		std::bind(&Connector::checkInLoop, this, name, exist));
}

//exists
bool Connector::exists(std::string const& name) /*const*/ {
	bool bok = false;
	bool exist = false;
	QueueInLoop(loop_,
		std::bind(&Connector::existInLoop, this, name, std::ref(exist), std::ref(bok)));
	//spin lock until asynchronous return
	while (!bok);
	return exist;
}

//count
size_t const Connector::count() /*const*/ {
	bool bok = false;
	size_t size = 0;
	QueueInLoop(loop_,
		std::bind(&Connector::countInLoop, this, std::ref(size), std::ref(bok)));
	//spin lock until asynchronous return
	while (!bok);
	return size;
}

//get
void Connector::get(std::string const& name, ClientConn& client) {
	bool bok = false;
	QueueInLoop(loop_,
		std::bind(&Connector::getInLoop, this, name, std::ref(client), std::ref(bok)));
	//spin lock until asynchronous return
	while (!bok);
}

//getAll
void Connector::getAll(ClientConnList& clients) {
	assert(clients.size() == 0);
	bool bok = false;
	QueueInLoop(loop_,
		std::bind(&Connector::getAllInLoop, this, std::ref(clients), std::ref(bok)));
	//spin lock until asynchronous return
	while (!bok);
}

void Connector::getInLoop(std::string const& name, ClientConn& client, bool& bok) {

	loop_->assertInLoopThread();

	TcpClientMap::const_iterator it = clients_.find(name);
	if (it != clients_.end()) {
		if (it->second->connection() &&
			it->second->connection()->connected()) {
			client.first = it->first;
			client.second = it->second->connection();
		}
		else {
		}
	}
	bok = true;
}

void Connector::getAllInLoop(ClientConnList& clients, bool& bok) {

	loop_->assertInLoopThread();
	
	for (TcpClientMap::const_iterator it = clients_.begin();
		it != clients_.end(); ++it) {
		if (it->second->connection() &&
			it->second->connection()->connected()) {
			clients.emplace_back(ClientConn(it->first, it->second->connection()));
		}
		else {
		}
	}
	bok = true;
}

void Connector::addInLoop(
	std::string const& name,
	const muduo::net::InetAddress& serverAddr) {
	
	loop_->assertInLoopThread();
	
	TcpClientMap::iterator it = clients_.find(name);
	if (it == clients_.end()) {
		//name?????????
		TcpClientPtr client(new TcpClient(loop_, serverAddr, name, this));
		LOG_ERROR << __FUNCTION__ << " ???????????? name = " << client->name();
		//192.168.2.93:20000
		clients_[client->name()] = client;
		client->enableRetry();
		client->connect();
	}
	else {
		//name?????????
		TcpClientPtr& client = it->second;
		if (client) {
			if (!client->connection() ||
				!client->connection()->connected()) {
				//?????????????????????
				if (!client->retry()) {
					LOG_ERROR << __FUNCTION__ << " ???????????? name = " << client->name();
					client->reconnect();
				}
			}
			else {
				assert(
					client->connection() &&
					client->connection()->connected());
			}
		}
		else {
			it->second.reset(new TcpClient(loop_, serverAddr, name, this));
			LOG_ERROR << __FUNCTION__ << " ???????????? name = " << name;
			it->second->enableRetry();
			it->second->connect();
		}
	}
}

void Connector::countInLoop(size_t& size, bool& bok) {

	loop_->assertInLoopThread();

	size = clients_.size();
	bok = true;
}

void Connector::checkInLoop(std::string const& name, bool exist) {

	loop_->assertInLoopThread();

	TcpClientMap::const_iterator it = clients_.find(name);
	if (it == clients_.end()) {
		//name?????????
		if (exist) {
			assert(false);
		}
	}
	else {
		//name?????????
		TcpClientPtr const& client = it->second;;
		if (exist) {
			assert(client);
			assert(
				client->connection() &&
				client->connection()->connected());
		}
		else {
			//????????????
			if (client) {
				assert(
					!client->connection() ||
					!client->connection()->connected());
			}
		}
	}
}

void Connector::existInLoop(std::string const& name, bool& exist, bool& bok) {

	loop_->assertInLoopThread();

	TcpClientMap::const_iterator it = clients_.find(name);
	exist = (it != clients_.end());
	bok = true;
}

void Connector::closeAll() {
	QueueInLoop(loop_,
		std::bind(&Connector::closeAllInLoop, this));
}

void Connector::onConnected(const muduo::net::TcpConnectionPtr& conn, const TcpClientPtr& client) {
	
	conn->getLoop()->assertInLoopThread();
	
	int32_t num = numConnected_.incrementAndGet();
	
	QueueInLoop(loop_,
		std::bind(&Connector::newConnection, this, conn, client));
}

void Connector::newConnection(const muduo::net::TcpConnectionPtr& conn, const TcpClientPtr& client) {
	
	loop_->assertInLoopThread();
	{
#if 0
		clients_[client->name()] = client;
		conn->setTcpNoDelay(true);
#else
		TcpClientMap::iterator it = clients_.find(client->name());
		assert(it != clients_.end());
#endif
	}
	
	QueueInLoop(conn->getLoop(), std::bind(&Connector::connectionCallback, this, conn));
}

void Connector::onClosed(const muduo::net::TcpConnectionPtr& conn, const std::string& name) {
	
	conn->getLoop()->assertInLoopThread();
	
	int32_t num = numConnected_.decrementAndGet();
	//if (num == 0) {
	//	QueueInLoop(conn->getLoop(),
	//		std::bind(&muduo::net::EventLoop::quit, conn->getLoop()));
	//}
	
	QueueInLoop(loop_,
		std::bind(&Connector::removeConnection, this, conn, name));
}

void Connector::removeConnection(const muduo::net::TcpConnectionPtr& conn, const std::string& name) {
	
	loop_->assertInLoopThread();
	{
#if 1
		if (1 == removes_.erase(name)) {
			//TcpClientMap::const_iterator it = clients_.find(name);
			//assert(it != clients_.end());
			//it->second->stop();
			//it->second.reset();
			//clients_.erase(it);
			QueueInLoop(loop_,
				std::bind(&Connector::removeInLoop, this, name, true));
		}
		else {
			//TcpClientMap::iterator it = clients_.find(name);
			//assert(it != clients_.end());
			//it->second->stop();
		}
#else
		size_t n = clients_.erase(name);
		(void)n;
		assert(n == 1);
#endif
	}
	QueueInLoop(conn->getLoop(), std::bind(&Connector::connectionCallback, this, conn));
}

void Connector::connectionCallback(const muduo::net::TcpConnectionPtr& conn) {

	conn->getLoop()->assertInLoopThread();

	if (connectionCallback_) {
		connectionCallback_(conn);
	}
}

void Connector::removeInLoop(std::string const& name, bool lazy) {
	
	loop_->assertInLoopThread();
	
	TcpClientMap::const_iterator it = clients_.find(name);
	if (it != clients_.end()) {
		//??????????????????????????????
		if (!it->second->connection() ||
			!it->second->connection()->connected()) {
			LOG_ERROR << __FUNCTION__ << " ???????????? name = " << it->first;
			it->second->stop();
			clients_.erase(it);
		}
		else if (lazy) {
			//??????????????????????????????????????????
			removes_[name] = true;
		}
	}
}

void Connector::cleanupInLoop() {

	loop_->assertInLoopThread();

	for (TcpClientMap::const_iterator it = clients_.begin();
		it != clients_.end(); ++it) {
		//????????????????????????
		if (!it->second->connection() ||
			!it->second->connection()->connected()) {
			it->second->stop();
			clients_.erase(it);
		}
		else {
			removes_[it->first] = true;
		}
	}
}

void Connector::closeAllInLoop() {

	loop_->assertInLoopThread();
	
	RunInLoop(loop_,
		std::bind(&Connector::cleanupInLoop, this));

	for (TcpClientMap::const_iterator it = clients_.begin();
		it != clients_.end(); ++it) {
		it->second->disconnect();
	}
}

void Connector::onMessage(
	const muduo::net::TcpConnectionPtr& conn,
	muduo::net::Buffer* buf, muduo::Timestamp receiveTime) {
	if (messageCallback_) {
		messageCallback_(conn, buf, receiveTime);
	}
}

//void Connector::quit() {
//	QueueInLoop(loop_,
//		std::bind(&muduo::net::EventLoop::quit, loop_));
//}