package service

import (
	"online-whiteboard-go-server/internal/models"
	"online-whiteboard-go-server/internal/repository"
	"time"
	"github.com/google/uuid"
)

type RoomService interface {
	CreateRoom(req models.CreateRoomRequest) (*models.RoomResponse, error)//创建新房间，接收请求参数，返回房间响应
	ListActiveRooms(limit, offset int) ([]models.RoomResponse, error)//分页查询活跃房间列表
	GetRoomByID(id string) (*models.RoomResponse, error)//根据 ID 查询单个房间信息
}

type roomService struct {
	roomRepo repository.RoomRepository//房间仓储接口（用于MySQL 操作）
}

func NewRoomService(roomRepo repository.RoomRepository) RoomService {
	return &roomService{roomRepo: roomRepo}
}

func (s *roomService) CreateRoom(req models.CreateRoomRequest) (*models.RoomResponse, error) {
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
	if err := s.roomRepo.Create(room); err != nil {
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

func (s *roomService) ListActiveRooms(limit, offset int) ([]models.RoomResponse, error) {
	rooms, err := s.roomRepo.ListActive(limit, offset)
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

func (s *roomService) GetRoomByID(id string) (*models.RoomResponse, error) {
	room, err := s.roomRepo.FindByID(id)
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