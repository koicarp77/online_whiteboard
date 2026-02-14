#include <iostream>
#include <string>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <mutex>
// 声明缺失的函数和全局变量
bool init_redis(const std::string& host, int port, const std::string& pwd);
void subscribe_redis(const std::string& channel);
void handle_client(int fd);
void log(const std::string& level, const std::string& msg);
extern void* redis_ctx;
extern std::mutex log_mtx;
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>      
#include <ctime>    
#include <hiredis/hiredis.h>
int main() {
    //初始化Redis 
    std::string redis_host = "redis";
    int redis_port = 6379;  //默认服务端口
    std::string redis_password = "123456";
    std::string redis_channel = "realtime_engine";

    if (!init_redis(redis_host, redis_port, redis_password)) {
        log("ERROR", "Redis初始化失败，程序退出");
        return 1;
    }

    //启动Redis监听线程
    std::thread redis_sub_thread(subscribe_redis, redis_channel);
    redis_sub_thread.detach();

    //启动TCP/HTTP服务器
    const int port = 8080;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log("ERROR", "创建Socket失败: " + std::string(strerror(errno)));
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log("ERROR", "设置Socket选项失败: " + std::string(strerror(errno)));
        close(server_fd);
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        log("ERROR", "绑定端口失败: " + std::string(strerror(errno)));
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 16) < 0) {
        log("ERROR", "监听端口失败: " + std::string(strerror(errno)));
        close(server_fd);
        return 1;
    }
    log("INFO", "C++实时引擎服务器启动，监听端口: http://0.0.0.0:" + std::to_string(port));

    //循环接收客户端连接
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            log("ERROR", "接受客户端连接失败: " + std::string(strerror(errno)));
            continue;
        }

        handle_client(client_fd);
    }

    // 收尾
    if (redis_ctx) {
        redisFree((redisContext*)redis_ctx);
    }
    close(server_fd);
    return 0;
}