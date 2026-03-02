package handler

import (
	"encoding/json"
	"net/http"
	"strconv"

	"online-whiteboard-go-server/internal/models"
	"online-whiteboard-go-server/internal/service"
)

type RoomHandler struct {
	roomService service.RoomService
}

type APIResponse struct {
	Code    int         `json:"code"`
	Message string      `json:"message"`
	Data    interface{} `json:"data,omitempty"`
}

func NewRoomHandler(roomService service.RoomService) *RoomHandler {
	return &RoomHandler{roomService: roomService}
}

func (h *RoomHandler) CreateRoom(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeJSON(w, http.StatusMethodNotAllowed, APIResponse{Code: 405, Message: "method not allowed"})
		return
	}

	// 绑定并校验创建房间的 JSON 参数。
	var req models.CreateRoomRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, APIResponse{Code: 400, Message: err.Error()})
		return
	}

	room, err := h.roomService.CreateRoom(r.Context(), req)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, APIResponse{Code: 500, Message: err.Error()})
		return
	}

	writeJSON(w, http.StatusOK, APIResponse{Code: 0, Message: "ok", Data: room})
}

func (h *RoomHandler) ListRooms(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeJSON(w, http.StatusMethodNotAllowed, APIResponse{Code: 405, Message: "method not allowed"})
		return
	}

	// 解析分页参数并设置默认值。
	limit := parseIntOrDefault(r.URL.Query().Get("limit"), 20)
	offset := parseIntOrDefault(r.URL.Query().Get("offset"), 0)
	if limit <= 0 {
		limit = 20
	}
	if offset < 0 {
		offset = 0
	}

	rooms, err := h.roomService.ListActiveRooms(r.Context(), limit, offset)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, APIResponse{Code: 500, Message: err.Error()})
		return
	}

	writeJSON(w, http.StatusOK, APIResponse{Code: 0, Message: "ok", Data: rooms})
}

func parseIntOrDefault(raw string, def int) int {
	if raw == "" {
		return def
	}
	v, err := strconv.Atoi(raw)
	if err != nil {
		return def
	}
	return v
}

func writeJSON(w http.ResponseWriter, status int, resp APIResponse) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(resp)
}
