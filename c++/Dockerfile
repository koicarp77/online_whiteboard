# 使用国内源支持更好的Alpine基础镜像
FROM alpine:3.18

# 安装GCC 12、g++、pthread及你需要的网络库
RUN sed -i 's/dl-cdn.alpinelinux.org/mirrors.ustc.edu.cn/g' /etc/apk/repositories && \
    apk add --no-cache \
    gcc \
    g++ \
    libc-dev \
    curl-dev \
    boost-dev \
    python3

# 设置工作目录
WORKDIR /app

# 复制源码
COPY . .

# 编译代码（根据你的需求调整链接库参数）
RUN g++ -o server main.cpp -std=c++17 -lpthread -lcurl -lboost_system

# 暴露端口
EXPOSE 8080

# 启动服务
CMD ["./server"]