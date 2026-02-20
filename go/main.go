package main

import (
	"log"
	"net/http"
	"os"
	"strconv"

	"online-whiteboard-go-server/api"
	"online-whiteboard-go-server/internal/database"
	"online-whiteboard-go-server/internal/handler"
	"online-whiteboard-go-server/internal/models"
	"online-whiteboard-go-server/internal/repository"
	"online-whiteboard-go-server/internal/service"
)

func main() {
	// 初始化数据库与缓存连接，再进行路由组装。
	mysqlCfg := database.MySQLConfig{
		Host:     getEnv("MYSQL_HOST", "mysql"),
		Port:     getEnv("MYSQL_PORT", "3306"),
		User:     getEnv("MYSQL_USER", "root"),
		Password: getEnv("MYSQL_PASSWORD", "123456"),
		Database: getEnv("MYSQL_DATABASE", "proj_db"),
	}
	if err := database.InitMySQL(mysqlCfg); err != nil {
		log.Fatalf("MySQL init failed: %v", err)
	}
	if err := database.AutoMigrate(&models.Room{}); err != nil {
		log.Fatalf("AutoMigrate failed: %v", err)
	}

	redisCfg := database.RedisConfig{
		Host:     getEnv("REDIS_HOST", "redis"),
		Port:     getEnv("REDIS_PORT", "6379"),
		Password: getEnv("REDIS_PASSWORD", "123456"),
		DB:       getEnvInt("REDIS_DB", 0),
	}
	rdb := database.InitRedis(redisCfg)

	// 组装仓储、服务与处理器。
	roomRepo := repository.NewRoomRepository(database.DB)
	cacheRepo := repository.NewCacheRepository(rdb)
	roomService := service.NewRoomService(roomRepo, cacheRepo)
	roomHandler := handler.NewRoomHandler(roomService)

	// 注册 HTTP 路由并启动服务。
	mux := http.NewServeMux()
	api.RegisterV1Routes(mux, roomHandler)

	if err := http.ListenAndServe(":9090", mux); err != nil {
		log.Fatalf("server start failed: %v", err)
	}
}

func getEnv(key, def string) string {
	v := os.Getenv(key)
	if v == "" {
		return def
	}
	return v
}

func getEnvInt(key string, def int) int {
	v := os.Getenv(key)
	if v == "" {
		return def
	}
	i, err := strconv.Atoi(v)
	if err != nil {
		return def
	}
	return i
}