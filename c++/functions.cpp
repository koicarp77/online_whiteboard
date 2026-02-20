#include "functions.h"
#include <iostream>
#include <mutex>
#include <ctime>
#include <chrono>
#include <cstring>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>

// 全局变量定义（与头文件声明对应）
redisContext* redis_ctx = nullptr;
std::mutex log_mtx;

//日志工具函数实现
void log(const std::string& level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(log_mtx);
    std::time_t now = std::time(nullptr);
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    std::cout << "[" << time_buf << "] [" << level << "] " << msg << std::endl;
}

//Redis操作函数实现 
bool init_redis(const std::string& host, int port, const std::string& password) {
    redis_ctx = redisConnect(host.c_str(), port);
    if (redis_ctx == nullptr || redis_ctx->err) {
        if (redis_ctx) {
            log("ERROR", "Redis连接失败: " + std::string(redis_ctx->errstr));
            redisFree(redis_ctx);
            redis_ctx = nullptr;
        } else {
            log("ERROR", "Redis连接失败: 无法创建上下文");
        }
        return false;
    }
    log("INFO", "Redis连接成功: " + host + ":" + std::to_string(port));

    if (!password.empty()) {
        redisReply* reply = (redisReply*)redisCommand(redis_ctx, "AUTH %s", password.c_str());
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            log("ERROR", "Redis密码认证失败: " + (reply ? std::string(reply->str) : "空回复"));
            freeReplyObject(reply);
            redisFree(redis_ctx);
            redis_ctx = nullptr;
            return false;
        }
        freeReplyObject(reply);
        log("INFO", "Redis密码认证成功");
    }

    return true;
}

bool publish_to_redis(const std::string& channel, const std::string& msg) {
    if (redis_ctx == nullptr) {
        log("ERROR", "Redis未连接，无法投放消息");
        return false;
    }

    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "PUBLISH %s %s", channel.c_str(), msg.c_str());
    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
        log("ERROR", "投放消息到Redis失败: " + (reply ? std::string(reply->str) : "空回复"));
        freeReplyObject(reply);
        return false;
    }

    log("INFO", "成功投放消息到Redis频道[" + channel + "]，内容: " + msg + "，接收客户端数: " + std::to_string(reply->integer));
    freeReplyObject(reply);
    return true;
}

void subscribe_redis(const std::string& channel) {
    if (redis_ctx == nullptr) {
        log("ERROR", "Redis未连接，无法监听消息");
        return;
    }

    redisReply* reply = (redisReply*)redisCommand(redis_ctx, "SUBSCRIBE %s", channel.c_str());
    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
        log("ERROR", "订阅Redis频道失败: " + (reply ? std::string(reply->str) : "空回复"));
        freeReplyObject(reply);
        return;
    }
    freeReplyObject(reply);
    log("INFO", "开始监听Redis频道: " + channel);

    while (true) {
        if (redisGetReply(redis_ctx, (void**)&reply) != REDIS_OK) {
            log("ERROR", "读取Redis消息失败，停止监听");
            break;
        }

        if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
            std::string msg_type = reply->element[0]->str;
            std::string msg_channel = reply->element[1]->str;
            std::string msg_content = reply->element[2]->str;

            if (msg_type == "message") {
                log("INFO", "收到Redis频道[" + msg_channel + "]消息: " + msg_content);
            }
        }

        freeReplyObject(reply);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// 客户端处理函数实现 
void handle_client(int client_fd) {
    char buf[1024] = {0};
    ssize_t recv_len = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (recv_len <= 0) {
        close(client_fd);
        return;
    }
    std::string client_msg(buf);
    log("INFO", "收到客户端请求: " + client_msg.substr(0, 50) + "...");

    publish_to_redis("realtime_engine", client_msg.substr(0, 200));

    const std::string body = "已接收请求，并投放至Redis！\n你发送的内容（前50字符）：" + client_msg.substr(0, 50);
    const std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n" + body;

    send(client_fd, response.c_str(), response.size(), 0);
    close(client_fd);
}