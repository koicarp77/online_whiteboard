package models
import (
	"time"
)
type RoomStatus string//定义房间状态类型

const (//定义房间具体状态常量
	RoomStatusActive RoomStatus = "active"
	RoomStatusInactive RoomStatus = "inactive"
	RoomStatusDeleted RoomStatus = "deleted"
)

type Room struct {//房间数据模型，对应数据库表，带有 GORM 和 JSON 标签
	ID          string     `gorm:"primaryKey;type:varchar(36)" json:"id"`
	Name        string     `gorm:"type:varchar(255)" json:"name"`
	Description string     `gorm:"type:text" json:"description"`
	OwnerID     string     `gorm:"type:varchar(36);index" json:"owner_id"`
	MaxUsers    int        `gorm:"default:100" json:"max_users"`//默认房间最大人数为 100
	Status      RoomStatus `gorm:"type:varchar(20);default:'active'" json:"status"`
	CreatedAt   time.Time  `json:"created_at"`
	UpdatedAt   time.Time  `json:"updated_at"`
	DeletedAt   *time.Time `gorm:"index" json:"deleted_at,omitempty"`
}

type CreateRoomRequest struct {//接收前端或者C++语言服务端创建房间请求的参数及校验规则
	Name		string `json:"name" binding:"required"`
	Description string `json:"description" binding:"max=1000"`
	OwnerID     string `json:"owner_id" binding:"required"`
	MaxUsers    int    `json:"max_users" binding:"gte=1,lte=1000"`
}

type RoomResponse struct {//返回给前端或者C++语言服务端的房间信息结构体
	ID          string     `json:"id"`
	Name		string     `json:"name"`
	Description string     `json:"description"`
	OwnerID     string     `json:"owner_id"`
	MaxUsers    int        `json:"max_users"`
	Status      RoomStatus `json:"status"`
	CreatedAt   time.Time  `json:"created_at"`
	UpdatedAt   time.Time  `json:"updated_at"`
}

