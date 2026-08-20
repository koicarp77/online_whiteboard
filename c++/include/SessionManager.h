#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

// 单个客户端会话：封装 WebSocket 流与写锁
class Session {
public:
    Session(uint64_t id, std::string user_id, tcp::socket socket)
        : id_(id), user_id_(std::move(user_id)), ws_(std::move(socket)) {}

    uint64_t id() const { return id_; }
    const std::string& user_id() const { return user_id_; }
    websocket::stream<tcp::socket>& ws() { return ws_; }

    // 发送文本/二进制消息。Beast 的 ws 不支持并发写，
    // 广播线程和会话线程可能同时写，加锁串行化
    void send(const std::string& text);

private:
    uint64_t id_;
    std::string user_id_;
    websocket::stream<tcp::socket> ws_;
    std::mutex write_mutex_;
};

// 管理所有活跃会话：注册、注销、广播
class SessionManager {
public:
    SessionManager() = default;

    // 分配会话ID（从1开始，0保留给"广播所有人"的哨兵值）
    uint64_t nextSessionId() { return ++next_id_; }

    void add(std::shared_ptr<Session> session);
    void remove(uint64_t id);
    size_t size();

    // 广播消息给所有会话，except_id 用于排除发送者（0表示不排除）
    void broadcast(const std::string& text, uint64_t except_id = 0);

private:
    std::mutex mutex_;
    std::unordered_map<uint64_t, std::shared_ptr<Session>> sessions_;
    std::atomic<uint64_t> next_id_{0};
};
