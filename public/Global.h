#ifndef GLOBAL_INCLUDE_H
#define GLOBAL_INCLUDE_H

#include <stdint.h>
#include <iostream>
#include <string>
#include <deque>
#include <map>

#include <boost/thread.hpp>
#include <muduo/base/Mutex.h>
#include <muduo/base/ThreadLocalSingleton.h>

//#define likely(x)                   __builtin_expect(!!(x), 1)
//#define unlikely(x)                 __builtin_expect(!!(x), 0)

#define READ_LOCK(mutex)            boost::shared_lock<boost::shared_mutex> guard(mutex)
#define WRITE_LOCK(mutex)           boost::lock_guard<boost::shared_mutex> guard(mutex)

#define CALLBACK_0(__selector__,__target__, ...) std::bind(&__selector__,__target__, ##__VA_ARGS__)
#define CALLBACK_1(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, ##__VA_ARGS__)
#define CALLBACK_2(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, std::placeholders::_2, ##__VA_ARGS__)
#define CALLBACK_3(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, ##__VA_ARGS__)
#define CALLBACK_4(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, ##__VA_ARGS__)
#define CALLBACK_5(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, ##__VA_ARGS__)

#define REDISCLIENT muduo::ThreadLocalSingleton<RedisClient>::instance()
#define MONGODBCLIENT MongoDBClient::ThreadLocalSingleton::instance()
#define REDISLOCK RedisLock::ThreadLocalSingleton::instance()

#endif