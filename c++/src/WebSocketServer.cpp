#include "WebSocketServer.h"
#include <iostream>
#include <ctime>
#include <thread>
#include "jwt-cpp/include/jwt-cpp/jwt.h"

// 构造函数，初始化Asio监听器
WebSocketServer::WebSocketServer(const ServerConfig& config)
    : config_(config),
      acceptor_(io_context_, tcp::endpoint(tcp::v4(), config.port)) {
    // 初始化Redis
    if (!initRedis()) {
        throw std::runtime_error("Redis初始化失败");
    }
    is_running_ = true;
    std::thread(&WebSocketServer::subscribeRedis, this).detach();
}

// 析构函数
WebSocketServer::~WebSocketServer() {
    is_running_ = false;
    io_context_.stop();
    if (redis_ctx_) {
        redisFree(redis_ctx_);
    }
    std::cout << "服务器资源已释放" << std::endl;
}

// 初始化Redis
bool WebSocketServer::initRedis() {
    std::lock_guard<std::mutex> lock(redis_mutex_);
    redis_ctx_ = redisConnect(config_.redis_host.c_str(), config_.redis_port);
    if (redis_ctx_ == nullptr || redis_ctx_->err) {
        std::cerr << "Redis连接失败：" << (redis_ctx_ ? redis_ctx_->errstr : "内存分配失败") << std::endl;
        if (redis_ctx_) redisFree(redis_ctx_);
        return false;
    }
    // 验证密码
    redisReply* auth_reply = (redisReply*)redisCommand(redis_ctx_, "AUTH %s", config_.redis_password.c_str());
    if (auth_reply == nullptr || auth_reply->type == REDIS_REPLY_ERROR) {
        std::cerr << "Redis密码验证失败：" << (auth_reply ? auth_reply->str : "未知错误") << std::endl;
        if (auth_reply) freeReplyObject(auth_reply);
        redisFree(redis_ctx_);
        return false;
    }
    freeReplyObject(auth_reply);
    return true;
}

// JWT验证函数
bool WebSocketServer::validateJwtToken(const std::string& token, std::string& out_user_id) {
    try {
        auto decoded = jwt::decode(token);
        auto payload = decoded.get_payload_claims();

        // 验证算法
        auto headers = decoded.get_header_claims();
        if (headers["alg"].as_string() != "HS256") {
            std::cerr << "JWT算法错误" << std::endl;
            return false;
        }

        // 验证签名
        jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{config_.jwt_secret})
            .verify(decoded);

        // 提取user_id和exp
        if (!payload.count("user_id") || !payload.count("exp")) {
            std::cerr << "JWT缺少必要字段" << std::endl;
            return false;
        }
        out_user_id = payload["user_id"].as_string();
        std::int64_t now = std::time(nullptr);
        std::int64_t exp = payload["exp"].as_int64();
        if (now > exp) {
            std::cerr << "JWT已过期" << std::endl;
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "JWT验证失败：" << e.what() << std::endl;
        return false;
    }
}

// 异步接受连接
void WebSocketServer::doAccept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::thread(&WebSocketServer::handleSession, this, std::move(socket)).detach();
            } else {
                std::cerr << "接受连接失败：" << ec.message() << std::endl;
            }
            doAccept();
        }
    );
}

// 处理客户端会话（Boost.Beast封装WebSocket）
void WebSocketServer::handleSession(tcp::socket socket) {
    try {
        websocket::stream<tcp::socket> ws(std::move(socket));

        auto& req = ws.next_layer().socket();
        beast::flat_buffer buffer;
        beast::http::request<beast::http::string_body> http_req;
        beast::http::read(ws.next_layer(), buffer, http_req);
        std::string token;
        auto it = http_req.find(beast::http::field::authorization);
        if (it != http_req.end()) {
            std::string auth = it->value().to_string();
            if (auth.substr(0, 7) == "Bearer ") {
                token = auth.substr(7);
            }
        }
        // 验证Token
        std::string user_id;
        if (token.empty() || !validateJwtToken(token, user_id)) {
            // Token无效，返回401
            beast::http::response<beast::http::string_body> res;
            res.result(beast::http::status::unauthorized);
            res.set(beast::http::field::content_type, "application/json");
            res.body() = R"({"code":1001,"msg":"Token过期/无效"})";
            res.prepare_payload();
            beast::http::write(ws.next_layer(), res);
            return;
        }
        ws.accept(http_req);
        std::cout << "WebSocket握手成功（用户：" << user_id << "）" << std::endl;

        beast::flat_buffer read_buffer;
        while (is_running_) {
            ws.read(read_buffer);
            std::string message = beast::buffers_to_string(read_buffer.data());
            read_buffer.consume(read_buffer.size());

            std::lock_guard<std::mutex> lock(redis_mutex_);
            redisReply* publish = (redisReply*)redisCommand(
                redis_ctx_, "PUBLISH %s %s", 
                config_.redis_channel.c_str(), message.c_str()
            );
            if (publish) freeReplyObject(publish);
        }

    } catch (const std::exception& e) {
        std::cerr << "会话异常：" << e.what() << std::endl;
    }
}

// 订阅Redis频道
void WebSocketServer::subscribeRedis() {
    if (!redis_ctx_) return;
    std::lock_guard<std::mutex> lock(redis_mutex_);
    redisReply* reply = (redisReply*)redisCommand(redis_ctx_, "SUBSCRIBE %s", config_.redis_channel.c_str());
    if (!reply) {
        std::cerr << "Redis订阅失败" << std::endl;
        return;
    }
    freeReplyObject(reply);

    while (is_running_) {
        if (redisGetReply(redis_ctx_, (void**)&reply) != REDIS_OK) break;
        if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
            std::cout << "Redis消息：" << reply->element[2]->str << std::endl;
        }
        freeReplyObject(reply);
    }
}

// 启动服务器
void WebSocketServer::run() {
    std::cout << "Boost.Asio WebSocket服务器启动，端口：" << config_.port << std::endl;
    doAccept();
    io_context_.run(); 
}