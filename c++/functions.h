
#pragma once  
#include <string>
#include <hiredis/hiredis.h>
#include <mutex>

// 全局变量声明（供外部文件使用）
extern redisContext* redis_ctx;  // Redis上下文
extern std::mutex log_mtx;       // 日志互斥锁


void log(const std::string& level, const std::string& msg);
/**
 *日志工具函数log说明
 * @brief 生成带时间戳的线程安全日志
 * @param level 日志级别（INFO/ERROR）
 * @param msg 日志内容
 */



bool init_redis(const std::string& host, int port, const std::string& password);
//init_Redis操作函数说明
/**
 * @brief 初始化Redis连接（适配Docker中的Redis服务）
 * @param host Redis主机（docker-compose中为redis）
 * @param port Redis端口（默认6379）
 * @param password Redis密码（默认123456）
 * @return 成功返回true，失败返回false
 */


bool publish_to_redis(const std::string& channel, const std::string& msg);
/**
 *publish_to_redis函数说明
 * @brief 向Redis指定频道投放消息
 * @param channel Redis频道名
 * @param msg 要投放的消息内容
 * @return 成功返回true，失败返回false
 */


void subscribe_redis(const std::string& channel);
/**
 *subscribe_redis函数说明
 * @brief 持续监听Redis指定频道的消息（独立线程运行）
 * @param channel 要监听的Redis频道名
 */


void handle_client(int client_fd);
/**
 *handle_client：客户端处理函数
 * @brief 处理客户端TCP连接，接收请求并投放至Redis
 * @param client_fd 客户端套接字描述符
 */