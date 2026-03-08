#pragma once
#include <string>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <hiredis/hiredis.h>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

// 服务器配置
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

    // 成员变量
    ServerConfig config_;
    asio::io_context io_context_;       // Boost.Asio核心上下文
    tcp::acceptor acceptor_;            // 异步监听器
    redisContext* redis_ctx_ = nullptr; // Redis上下文
    std::mutex redis_mutex_;            // Redis线程安全锁
    bool is_running_ = false;
};