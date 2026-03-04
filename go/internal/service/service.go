package service

import (
	"encoding/json"
	"errors"
	"context"//用于传递超时、取消信号，贯穿整个请求生命周期
	"online-whiteboard-go-server/internal/models"
	"online-whiteboard-go-server/internal/repository"
	"time"
	"github.com/google/uuid"
)

type RoomService interface {
	CreateRoom(ctx context.Context, req models.CreateRoomRequest) (*models.RoomResponse, error)//创建新房间，接收请求参数，返回房间响应
	ListActiveRooms(ctx context.Context, limit, offset int) ([]models.RoomResponse, error)//分页查询活跃房间列表
	GetRoomByID(ctx context.Context, id string) (*models.RoomResponse, error)//根据 ID 查询单个房间信息
}

type roomService struct {
	roomRepo repository.RoomRepository//房间仓储接口（用于MySQL 操作）
	cacheRepo repository.CacheRepository//缓存仓储接口（用于 Redis 操作）

}

func NewRoomService(roomRepo repository.RoomRepository, cacheRepo repository.CacheRepository) RoomService {
	return &roomService{roomRepo: roomRepo, cacheRepo: cacheRepo}
}

func (s *roomService) CreateRoom(ctx context.Context, req models.CreateRoomRequest) (*models.RoomResponse, error) {
	// 业务校验
	if req.MaxUsers <= 0 {
		req.MaxUsers = 100
	}
	if req.MaxUsers > 100 {
		return nil, errors.New("max_users 不能超过 100")
	}

	room := &models.Room{
		ID:          uuid.New().String(),
		Name:        req.Name,
		Description: req.Description,
		OwnerID:     req.OwnerID,
		MaxUsers:    req.MaxUsers,
		Status:      models.RoomStatusActive,
		CreatedAt:   time.Now(),
		UpdatedAt:   time.Now(),
	}
	//保存到 MySQL
	if err := s.roomRepo.Create(ctx,room); err != nil {
		return nil, err
	}
	//写入 Redis 
	_ = s.cacheRepo.SetRoomInfo(ctx, room.ID, room)
	_ = s.cacheRepo.AddToActiveSet(ctx, room.ID)

	eventPayload, _ := json.Marshal(map[string]interface{}{
		"event": "room_created",
		"room": map[string]interface{}{
			"id":          room.ID,
			"name":        room.Name,
			"description": room.Description,
			"owner_id":    room.OwnerID,
			"max_users":   room.MaxUsers,
			"status":      room.Status,
			"created_at":  room.CreatedAt,
			"updated_at":  room.UpdatedAt,
		},
	})
	if len(eventPayload) > 0 {
		_ = s.cacheRepo.PublishRoomCreated(ctx, string(eventPayload))
	}

	return &models.RoomResponse{
		ID:          room.ID,
		Name:        room.Name,
		Description: room.Description,
		OwnerID:     room.OwnerID,
		MaxUsers:    room.MaxUsers,
		Status:      room.Status,
		CreatedAt:   room.CreatedAt,
		UpdatedAt:   room.UpdatedAt,
	}, nil
}

func (s *roomService) ListActiveRooms(ctx context.Context, limit, offset int) ([]models.RoomResponse, error) {
	rooms, err := s.roomRepo.ListActive(ctx, limit, offset)
	if err != nil {
		return nil, err
	}
	responses := make([]models.RoomResponse, len(rooms))
	for i, room := range rooms {
		responses[i] = models.RoomResponse{
			ID:          room.ID,
			Name:        room.Name,
			Description: room.Description,
			OwnerID:     room.OwnerID,
			MaxUsers:    room.MaxUsers,
			Status:      room.Status,
			CreatedAt:   room.CreatedAt,
			UpdatedAt:   room.UpdatedAt,
		}
	}
	return responses, nil
}

func (s *roomService) GetRoomByID(ctx context.Context, id string) (*models.RoomResponse, error) {
	room, err := s.roomRepo.FindByID(ctx, id)
	if err != nil {
		return nil, err
	}
	return &models.RoomResponse{
		ID:          room.ID,
		Name:        room.Name,
		Description: room.Description,
		OwnerID:     room.OwnerID,
		MaxUsers:    room.MaxUsers,
		Status:      room.Status,
		CreatedAt:   room.CreatedAt,
		UpdatedAt:   room.UpdatedAt,
	}, nil
}