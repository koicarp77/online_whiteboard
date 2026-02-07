# 编译阶段：基于golang官方镜像，包含编译工具
FROM golang:1.22-alpine AS builder
# 开启mod
ENV GO111MODULE=on
# 配置国内镜像（加速go mod download）
RUN go env -w GOPROXY=https://goproxy.cn,direct
WORKDIR /app
# 先复制go.mod/go.sum，利用镜像缓存（避免源码修改重复下载依赖）
COPY go.mod go.sum ./
RUN go mod download
# 复制所有Go源码
COPY . .
# 编译：CGO_ENABLED=0 静态编译，不依赖系统库；GOOS=linux 适配linux容器
RUN CGO_ENABLED=0 GOOS=linux go build -a -installsuffix cgo -o server main.go

# 开发阶段：支持热重载
FROM golang:1.22-alpine AS dev
ENV GO111MODULE=on
ENV GOPROXY=https://goproxy.cn,direct GOSUMDB=off
RUN go env -w GOPROXY=https://goproxy.cn,direct
RUN apk add --no-cache git
WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN go install github.com/air-verse/air@v1.52.3
EXPOSE 9090
CMD ["air", "-c", ".air.toml"]

# 运行阶段：用alpine轻量镜像，仅保留可执行文件
FROM alpine:latest
# 安装必要依赖（如时区、证书，根据你的代码加）
RUN apk add --no-cache tzdata ca-certificates
# 同步时区（可选，根据需要）
ENV TZ=Asia/Shanghai
WORKDIR /root/
# 从编译阶段复制可执行文件
COPY --from=builder /app/server .
# 暴露端口（和Go代码、docker-compose里的端口一致）
EXPOSE 9090
# 启动Go服务
CMD ["./server"]