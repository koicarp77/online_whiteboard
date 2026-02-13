//MySQL 房间仓储
package repository

import (
	"context"
	"online-whiteboard-go-server/internal/models"
	"gorm.io/gorm"
)

type roomRepository struct {//定义私有结构体：首字母小写，包外不可见
	db *gorm.DB
}

func NewRoomRepository(db *gorm.DB) RoomRepository {
	return &roomRepository{db: db}//构造函数，接收数据库连接并返回接口实现
}

func (r *roomRepository) Create(ctx context.Context, room *models.Room) error {
	return r.db.WithContext(ctx).Create(room).Error
}

func (r *roomRepository) FindByID(ctx context.Context, id string) (*models.Room, error) {
	var room models.Room //用于存储查询结果
	err := r.db.WithContext(ctx).
		Where("id = ? AND status != ?", id, models.RoomStatusDeleted).
		First(&room).Error
	return &room, err
}

func (r *roomRepository) ListActive(ctx context.Context, limit, offset int) ([]models.Room, error) {
	var rooms []models.Room//用于存储多条记录
	err := r.db.WithContext(ctx).
	Where("status = ?", models.RoomStatusActive). //只查询激活状态
	Order("created_at DESC"). //按创建时间倒序排列（最新的在前）
	Limit(limit). //限制返回记录数
	Offset(offset). //偏移量，用于分页
	Find(&rooms).Error //执行查询，将结果填充到 rooms 切片
	return rooms, err
}

func (r *roomRepository) UpdateStatus(ctx context.Context, id string, status models.RoomStatus) error {
	return r.db.WithContext(ctx).
		Model(&models.Room{}).
		Where("id = ?", id).
		Update("status", status).Error
}

func (r *roomRepository) Delete(ctx context.Context, id string) error {
	return r.db.WithContext(ctx).
		Model(&models.Room{}).
		Where("id = ?", id).
		Update("status", models.RoomStatusDeleted).Error
}