package repository

import (
	"context"
	"online-whiteboard-go-server/internal/models"
)

type RoomRepository interface {
	Create(ctx context.Context, room *models.Room) error//插入一条新房间记录，接收指针以便 GORM 填充默认值（如 ID、创建时间）
	FindByID(ctx context.Context, id string) (*models.Room, error)//根据 ID 查询单个房间，排除已删除状态
	ListActive(ctx context.Context, limit,offset int) ([]models.Room, error)//分页查询状态为 active 的房间，按创建时间倒序
	UpdateStatus(ctx context.Context, id string, status models.RoomStatus) error//更新房间状态，支持 active、inactive、deleted
	Delete(ctx context.Context, id string) error//软删除房间，将状态设置为 deleted
}

type CacheRepository interface {
	SetRoomInfo(ctx context.Context, roomID string, room *models.Room) error//设置房间信息缓存
	GetRoomInfo(ctx context.Context, roomID string) (*models.Room, error)//获取房间信息
	AddToActiveSet(ctx context.Context, roomID string) error//将房间 ID 添加到活跃房间集合
	PublishRoomCreated(ctx context.Context, payload string) error//发布房间创建事件到频道
}
