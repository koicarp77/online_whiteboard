#include "WebSocketServer.h"
#include <iostream>
#include <cstdlib>

ServerConfig loadConfigFromEnv() {
    ServerConfig config;
    const char* port_env = std::getenv("PORT");
    if (port_env) config.port = std::atoi(port_env);
    const char* redis_host = std::getenv("REDIS_HOST");
    if (redis_host) config.redis_host = redis_host;
    const char* redis_port = std::getenv("REDIS_PORT");
    if (redis_port) config.redis_port = std::atoi(redis_port);
    const char* redis_pwd = std::getenv("REDIS_PASSWORD");
    if (redis_pwd) config.redis_password = redis_pwd;
    const char* jwt_secret = std::getenv("JWT_SECRET");
    if (jwt_secret) config.jwt_secret = jwt_secret;
    return config;
}

int main() {
    try {
        ServerConfig config = loadConfigFromEnv();
        WebSocketServer server(config);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "服务器异常：" << e.what() << std::endl;
        return 1;
    }
    return 0;
}