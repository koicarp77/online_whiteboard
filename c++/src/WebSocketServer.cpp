#include "WebSocketServer.h"
#include <iostream>
#include <ctime>
#include <thread>
#include <sstream>
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
bool WebSocketServer::validateJwtToken(const std::string& token, std::string& out_user_id, bool& out_expired) {
    out_expired = false;
    try {
        auto decoded = jwt::decode(token);

        // 验证算法
        if (decoded.get_header_claim("alg").as_string() != "HS256") {
            std::cerr << "JWT算法错误" << std::endl;
            return false;
        }

        // 验证签名
        jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{config_.jwt_secret})
            .verify(decoded);

        // 提取user_id和exp
        out_user_id = decoded.get_payload_claim("user_id").as_string();
        std::int64_t now = std::time(nullptr);
        std::int64_t exp = decoded.get_payload_claim("exp").as_integer();
        if (now > exp) {
            std::cerr << "JWT已过期" << std::endl;
            out_expired = true;
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "JWT验证失败：" << e.what() << std::endl;
        return false;
    }
}

std::string WebSocketServer::extractTokenFromRequest(const beast::http::request<beast::http::string_body>& req) {
    auto it = req.find(beast::http::field::authorization);
    if (it != req.end()) {
        std::string auth(it->value().data(), it->value().size());
        if (auth.rfind("Bearer ", 0) == 0 && auth.size() > 7) {
            return auth.substr(7);
        }
    }

    std::string target(req.target().data(), req.target().size());
    auto query_pos = target.find('?');
    if (query_pos == std::string::npos || query_pos + 1 >= target.size()) {
        return "";
    }

    std::string query = target.substr(query_pos + 1);
    std::stringstream ss(query);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        auto eq = pair.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = pair.substr(0, eq);
        std::string value = pair.substr(eq + 1);
        if (key == "access_token" || key == "token") {
            return value;
        }
    }
    return "";
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
        beast::flat_buffer buffer;
        beast::http::request<beast::http::string_body> http_req;
        beast::http::read(ws.next_layer(), buffer, http_req);

        std::string token = extractTokenFromRequest(http_req);
        // 验证Token
        std::string user_id;
        bool expired = false;
        if (token.empty() || !validateJwtToken(token, user_id, expired)) {
            // Token无效，返回401
            beast::http::response<beast::http::string_body> res;
            res.result(beast::http::status::unauthorized);
            res.set(beast::http::field::content_type, "application/json");
            if (expired) {
                res.body() = R"({"code":40101,"msg":"access token expired"})";
            } else {
                res.body() = R"({"code":40100,"msg":"invalid access token"})";
            }
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
    redisContext* sub_ctx = redisConnect(config_.redis_host.c_str(), config_.redis_port);
    if (sub_ctx == nullptr || sub_ctx->err) {
        std::cerr << "Redis订阅连接失败" << std::endl;
        if (sub_ctx) redisFree(sub_ctx);
        return;
    }

    redisReply* auth_reply = (redisReply*)redisCommand(sub_ctx, "AUTH %s", config_.redis_password.c_str());
    if (!auth_reply || auth_reply->type == REDIS_REPLY_ERROR) {
        std::cerr << "Redis订阅鉴权失败" << std::endl;
        if (auth_reply) freeReplyObject(auth_reply);
        redisFree(sub_ctx);
        return;
    }
    freeReplyObject(auth_reply);

    redisReply* reply = (redisReply*)redisCommand(sub_ctx, "SUBSCRIBE %s", config_.redis_channel.c_str());
    if (!reply) {
        std::cerr << "Redis订阅失败" << std::endl;
        redisFree(sub_ctx);
        return;
    }
    freeReplyObject(reply);

    while (is_running_) {
        if (redisGetReply(sub_ctx, (void**)&reply) != REDIS_OK) break;
        if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
            std::cout << "Redis消息：" << reply->element[2]->str << std::endl;
        }
        freeReplyObject(reply);
    }

    redisFree(sub_ctx);
}

// 启动服务器
void WebSocketServer::run() {
    std::cout << "Boost.Asio WebSocket服务器启动，端口：" << config_.port << std::endl;
    doAccept();
    io_context_.run(); 
}