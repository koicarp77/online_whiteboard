package database
import(
	"context"
    "fmt"
    "log"
    "time"

    "github.com/go-redis/redis/v8"
)

var RDB *redis.Client


type RedisConfig struct {
	Host     string	//Redis 服务器主机地址
	Port     string //端口
	Password string
	DB       int	//数据库编号
}

func InitRedis(cfg RedisConfig) *redis.Client {//创建并配置 Redis 客户端，测试连接，然后将全局变量 RDB 赋值，最后返回客户端指针
	addr := fmt.Sprintf("%s:%s", cfg.Host, cfg.Port)
	rdb := redis.NewClient(&redis.Options{
		Addr:     addr,
		Password: cfg.Password,
		DB:       cfg.DB,
		PoolSize: 10,//连接池大小
	})
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	if err := rdb.Ping(ctx).Err(); err != nil {
		log.Fatalf("Redis连接失败: %v", err)
	}
	log.Println("Redis connected")
	RDB = rdb
	return rdb
}