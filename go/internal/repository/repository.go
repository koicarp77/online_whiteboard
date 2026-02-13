package repository

import (
	"online-whiteboard-go-server/internal/models"

	"gorm.io/gorm"
)

type RoomRepository interface {
	Create(room *models.Room) error//插入一条新房间记录，接收指针以便 GORM 填充默认值（如 ID、创建时间）
	FindByID(id string) (*models.Room, error)//根据 ID 查询单个房间，排除已删除状态
	ListActive(limit,offset int) ([]models.Room, error)//分页查询状态为 active 的房间，按创建时间倒序
	UpdateStatus(id string, status models.RoomStatus) error//更新房间状态，支持 active、inactive、deleted
	Delete(id string) error//软删除房间，将状态设置为 deleted
}

type roomRepository struct {//定义私有结构体：首字母小写，包外不可见
	db *gorm.DB
}

func NewRoomRepository(db *gorm.DB) RoomRepository{
	return &roomRepository{db: db}//构造函数，接收数据库连接并返回接口实现
}

func (r *roomRepository) Create(room *models.Room) error {
	return r.db.Create(room).Error
}

func (r *roomRepository) FindByID(id string) (*models.Room, error) {
	var room models.Room//用于存储查询结果
	err := r.db.Where("id = ? AND status != ?", id, models.RoomStatusDeleted).First(&room).Error
	return &room, err
}

func (r *roomRepository) ListActive(limit, offset int) ([]models.Room, error) {
	var rooms []models.Room//用于存储多条记录
	err := r.db.Where("status = ?", models.RoomStatusActive). //只查询激活状态
	Order("created_at DESC"). //按创建时间倒序排列（最新的在前）
	Limit(limit). //限制返回记录数
	Offset(offset). //偏移量，用于分页
	Find(&rooms).Error //执行查询，将结果填充到 rooms 切片
	return rooms, err
}

func (r *roomRepository) UpdateStatus(id string, status models.RoomStatus) error {
	return r.db.Model(&models.Room{}).Where("id = ?", id).Update("status", status).Error
}

func (r *roomRepository) Delete(id string) error {
	return r.db.Model(&models.Room{}).Where("id = ?", id).Update("status", models.RoomStatusDeleted).Error
}