#pragma once
#include <string>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <hiredis/hiredis.h>
#include "SessionManager.h"
#include "proto_gen/white_board.pb.h"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

// 服务器配置结构体，存放整给服务器的配置信息
struct ServerConfig {
    int port = 8080;
    std::string redis_host = "redis";
    int redis_port = 6379;
    std::string redis_password = "123456";
    std::string jwt_secret = "online-whiteboard-secret";
    std::string redis_channel = "realtime_engine";
};

class WebSocketServer {
public:
    explicit WebSocketServer(const ServerConfig& config);
    ~WebSocketServer();

    // 启动服务器
    void run();

private:
    // 初始化Redis
    bool initRedis();
    // 异步接受客户端连接
    void doAccept();
    // 处理单个客户端会话
    void handleSession(tcp::socket socket);
    // 验证JWT Token
    bool validateJwtToken(const std::string& token, std::string& out_user_id);
    // 订阅Redis频道
    void subscribeRedis();
    // 将会话消息发布到Redis（带会话ID信封，供广播时排除发送者）
    void publishToRedis(uint64_t session_id, const std::string& payload);
    // 处理订阅线程收到的消息：解析信封并广播给客户端
    void handleBroadcastMessage(const std::string& raw);

    // 成员变量
    ServerConfig config_;
    asio::io_context io_context_;       // Boost.Asio核心上下文
    tcp::acceptor acceptor_;            // 异步监听器
    redisContext* pub_ctx_ = nullptr;   // 发布专用连接（多会话线程共享，pub_mutex_保护）
    redisContext* sub_ctx_ = nullptr;   // 订阅专用连接（仅订阅线程使用）
    std::mutex pub_mutex_;              // pub_ctx_ 线程安全锁
    SessionManager sessions_;           // 活跃会话管理
    bool is_running_ = false;
};