#include "WebSocketServer.h"
#include <iostream>
#include <ctime>
#include <thread>
#include <sstream>
#include "jwt-cpp/jwt.h"

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
    if (pub_ctx_) {
        redisFree(pub_ctx_);
    }
    if (sub_ctx_) {
        redisFree(sub_ctx_);
    }
    std::cout << "服务器资源已释放" << std::endl;
}

// 连接Redis并验证密码，失败返回nullptr
static redisContext* connectRedis(const ServerConfig& config) {
    redisContext* ctx = redisConnect(config.redis_host.c_str(), config.redis_port);
    if (ctx == nullptr || ctx->err) {
        std::cerr << "Redis连接失败：" << (ctx ? ctx->errstr : "内存分配失败") << std::endl;
        if (ctx) redisFree(ctx);
        return nullptr;
    }
    // 验证密码
    redisReply* auth_reply = (redisReply*)redisCommand(ctx, "AUTH %s", config.redis_password.c_str());
    if (auth_reply == nullptr || auth_reply->type == REDIS_REPLY_ERROR) {
        std::cerr << "Redis密码验证失败：" << (auth_reply ? auth_reply->str : "未知错误") << std::endl;
        if (auth_reply) freeReplyObject(auth_reply);
        redisFree(ctx);
        return nullptr;
    }
    freeReplyObject(auth_reply);
    return ctx;
}

// 初始化Redis：发布和订阅各用一条独立连接。
// hiredis的连接不是线程安全的，且订阅模式会占用连接，
// 若共用一条连接，订阅线程的redisGetReply会吃掉PUBLISH的回复，还会持锁阻塞发布
bool WebSocketServer::initRedis() {
    pub_ctx_ = connectRedis(config_);
    sub_ctx_ = connectRedis(config_);
    if (!pub_ctx_ || !sub_ctx_) {
        if (pub_ctx_) redisFree(pub_ctx_);
        if (sub_ctx_) redisFree(sub_ctx_);
        pub_ctx_ = sub_ctx_ = nullptr;
        return false;
    }
    return true;
}

// JWT验证函数（jwt-cpp 0.7.0 API：先decode再verify）
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

// 从握手请求中提取token：优先Authorization头，其次URL查询参数。
// 浏览器WebSocket握手无法自定义Header，前端通常用 ?access_token=xxx 传token
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
        [this](boost::system::error_code ec, tcp::socket socket) {  //客户端连接并创建socket
            if (!ec) {
                std::thread(&WebSocketServer::handleSession, this, std::move(socket)).detach();
            } else {
                std::cerr << "接受连接失败：" << ec.message() << std::endl;
            }
            doAccept();
        }
    );
}

// 处理客户端会话
void WebSocketServer::handleSession(tcp::socket socket) {
    uint64_t session_id = 0;
    try {
        // 先读HTTP升级请求（此时socket还未被WebSocket流接管）
        beast::flat_buffer buffer;
        beast::http::request<beast::http::string_body> http_req;
        beast::http::read(socket, buffer, http_req);

        std::string token = extractTokenFromRequest(http_req);
        // 验证Token
        std::string user_id;
        bool expired = false;
        if (token.empty() || !validateJwtToken(token, user_id, expired)) {
            beast::http::response<beast::http::string_body> res;
            res.result(beast::http::status::unauthorized);
            res.set(beast::http::field::content_type, "application/json");
            if (expired) {
                res.body() = R"({"code":40101,"msg":"access token expired"})";
            } else {
                res.body() = R"({"code":40100,"msg":"invalid access token"})";
            }
            res.prepare_payload();
            beast::http::write(socket, res);
            return;
        }

        // 握手成功后创建会话并注册到会话管理器，纳入广播范围
        auto session = std::make_shared<Session>(sessions_.nextSessionId(), user_id, std::move(socket));
        session_id = session->id();
        auto& ws = session->ws();
        ws.accept(http_req);
        sessions_.add(session);
        std::cout << "WebSocket握手成功（用户：" << user_id << "，会话：" << session_id << "）" << std::endl;

        beast::flat_buffer read_buffer;
        while (is_running_) {
            ws.read(read_buffer);
            std::string message = beast::buffers_to_string(read_buffer.data());
            read_buffer.consume(read_buffer.size());
            publishToRedis(session_id, message);
        }

    } catch (const std::exception& e) {
        std::cerr << "会话异常（会话：" << session_id << "）：" << e.what() << std::endl;
    }
    // 连接断开/异常退出后，从会话管理器移除
    if (session_id != 0) {
        sessions_.remove(session_id);
        std::cout << "会话已断开并移除（会话：" << session_id << "）" << std::endl;
    }
}

// 将会话消息发布到Redis，信封格式："<会话ID>|<消息正文>"。
// 订阅线程收到后据此排除发送者，避免把A画的笔画回发给自己
void WebSocketServer::publishToRedis(uint64_t session_id, const std::string& payload) {
    std::string enveloped = std::to_string(session_id) + "|" + payload;
    std::lock_guard<std::mutex> lock(pub_mutex_);
    // 用 %b 按二进制发送，正文可能是protobuf二进制
    redisReply* reply = (redisReply*)redisCommand(
        pub_ctx_, "PUBLISH %s %b",
        config_.redis_channel.c_str(), enveloped.data(), enveloped.size()
    );
    if (reply) freeReplyObject(reply);
}

// 解析信封并广播。
// 本服务发布的消息带 "<会话ID>|" 前缀，用于排除发送者；
// 解析不出信封的消息（如Go服务发布的房间事件）广播给所有会话
void WebSocketServer::handleBroadcastMessage(const std::string& raw) {
    uint64_t except_id = 0;
    std::string payload = raw;

    auto pos = raw.find('|');
    bool has_envelope = pos != std::string::npos && pos > 0;
    for (size_t i = 0; has_envelope && i < pos; ++i) {
        if (raw[i] < '0' || raw[i] > '9') {
            has_envelope = false;
        }
    }
    if (has_envelope) {
        except_id = std::stoull(raw.substr(0, pos));
        payload = raw.substr(pos + 1);
    }

    sessions_.broadcast(payload, except_id);
    std::cout << "收到Redis消息，广播给除会话" << except_id << "外的所有会话" << std::endl;
}

// 订阅Redis频道：收到消息后广播给所有客户端
void WebSocketServer::subscribeRedis() {
    if (!sub_ctx_) return;
    redisReply* reply = (redisReply*)redisCommand(sub_ctx_, "SUBSCRIBE %s", config_.redis_channel.c_str());
    if (!reply) {
        std::cerr << "Redis订阅失败" << std::endl;
        return;
    }
    freeReplyObject(reply);

    while (is_running_) {
        if (redisGetReply(sub_ctx_, (void**)&reply) != REDIS_OK) break;
        if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3 &&
            reply->element[2]->type == REDIS_REPLY_STRING) {
            std::string raw(reply->element[2]->str, reply->element[2]->len);
            handleBroadcastMessage(raw);
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