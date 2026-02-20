//Redis 缓存仓储
package repository

import (
	"context"
	"encoding/json"
	"time"

	"online-whiteboard-go-server/internal/models"
	"github.com/go-redis/redis/v8"
)

type cacheRepository struct {
	rdb *redis.Client
}

func NewCacheRepository(rdb *redis.Client) CacheRepository {
	return &cacheRepository{rdb: rdb}
}

func (r *cacheRepository) SetRoomInfo(ctx context.Context, roomID string, room *models.Room) error {//接收上下文、房间 ID、房间对象指针
	data, err := json.Marshal(room)
	if err != nil {
		return err
	}
	//执行 Redis SET 命令，键名为 "room:" + roomID，值为序列化后的 data，过期时间为 2 小时
	return r.rdb.Set(ctx, "room:"+roomID, data, time.Hour*2).Err()
}

func (r *cacheRepository) GetRoomInfo(ctx context.Context, roomID string) (*models.Room, error) {
	data, err := r.rdb.Get(ctx, "room:"+roomID).Bytes()
	if err != nil {
		return nil, err
	}
	var room models.Room
	if err := json.Unmarshal(data, &room); err != nil {//将 JSON 数据解析到 room 变量中
		return nil, err
	}
	return &room, nil
}

func (r *cacheRepository) AddToActiveSet(ctx context.Context, roomID string) error {//将房间 ID 添加到 Redis Set 结构，键名为 "rooms:active"
	return r.rdb.SAdd(ctx, "rooms:active", roomID).Err()
}

func (r *cacheRepository) PublishRoomCreated(ctx context.Context, payload string) error {
	return r.rdb.Publish(ctx, "realtime_engine", payload).Err()
}
