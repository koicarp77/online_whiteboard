#include "SessionManager.h"
#include <iostream>

// 发送消息给该会话（失败仅记录日志，不抛出，避免影响广播流程）
void Session::send(const std::string& text) {
    try {
        std::lock_guard<std::mutex> lock(write_mutex_);
        ws_.write(asio::buffer(text));
    } catch (const std::exception& e) {
        std::cerr << "发送消息失败（会话" << id_ << "）：" << e.what() << std::endl;
    }
}

void SessionManager::add(std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[session->id()] = std::move(session);
}

void SessionManager::remove(uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(id);
}

size_t SessionManager::size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

// 广播：先加锁拷贝快照再发送，避免发送阻塞期间持有管理器锁
void SessionManager::broadcast(const std::string& text, uint64_t except_id) {
    std::vector<std::shared_ptr<Session>> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot.reserve(sessions_.size());
        for (const auto& [id, session] : sessions_) {
            if (id != except_id) {
                snapshot.push_back(session);
            }
        }
    }
    for (const auto& session : snapshot) {
        session->send(text);
    }
}
