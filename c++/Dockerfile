# 使用国内源支持更好的Alpine基础镜像
FROM alpine:3.18

# 【增量修改】1. 补充Boost.Asio线程依赖 + Redis客户端依赖（不删除原有依赖）
# 备注：新增项仅为适配Boost.Asio+Redis，不影响原有server编译运行
RUN sed -i 's/dl-cdn.alpinelinux.org/mirrors.ustc.edu.cn/g' /etc/apk/repositories && \
    apk add --no-cache \
    gcc \
    g++ \
    libc-dev \
    curl-dev \
    boost-dev \         
     # 原有：基础Boost库
    boost-thread \       
    # 新增：Boost.Asio线程模块 
    libhiredis-dev \     
    # 新增：Redis C客户端依赖 
    python3 \
    && rm -rf /var/cache/apk/*  
    # 新增：清理缓存，不影响功能

# 设置工作目录
WORKDIR /app

# 复制源码
COPY . .

# 【核心保留】1. 编译合作者的server程序（完全不变，保证零修改可用）
RUN g++ -o server main.cpp -std=c++17 -lpthread -lcurl -lboost_system

# 【增量新增】2. 编译Boost.Asio+Redis的realtime_engine程序（新增，不影响server）
# 备注：仅为自身功能新增编译指令，合作者无需关注
RUN g++ -o realtime_engine /app/main.cpp -std=c++17 \
    -lpthread -lcurl -lboost_system -lboost_thread -lhiredis

# 暴露端口（保留原有8080，新增3000适配WebSocket）
EXPOSE 8080
EXPOSE 3000  
# 【增量新增】适配Boost.Asio WebSocket端口 | 合作者无需注释，不影响原有使用

# 【核心保留】2. 默认启动server（关键：合作者零修改即可按旧方式使用）
CMD ["./server"]