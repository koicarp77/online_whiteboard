package handler

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"strings"
	"strconv"
	"time"

	"online-whiteboard-go-server/internal/auth"
	"online-whiteboard-go-server/internal/models"
	"online-whiteboard-go-server/internal/service"

	"github.com/go-redis/redis/v8"
	"github.com/google/uuid"
)

const (
	CodeTokenInvalid = 40100
	CodeTokenExpired = 40101
)

type contextKey string

const userIDContextKey contextKey = "user_id"

type RoomHandler struct {
	roomService service.RoomService
	rdb         *redis.Client
	cppHost     string
	cppPort     int
}

type APIResponse struct {
	Code    int         `json:"code"`
	Message string      `json:"message"`
	Data    interface{} `json:"data,omitempty"`
}

func NewRoomHandler(roomService service.RoomService) *RoomHandler {
	return &RoomHandler{roomService: roomService}
}

func NewRoomHandlerWithAuth(roomService service.RoomService, rdb *redis.Client, cppHost string, cppPort int) *RoomHandler {
	return &RoomHandler{roomService: roomService, rdb: rdb, cppHost: cppHost, cppPort: cppPort}
}

func (h *RoomHandler) AuthMiddleware(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		authHeader := r.Header.Get("Authorization")
		if authHeader == "" {
			writeJSON(w, http.StatusUnauthorized, APIResponse{Code: CodeTokenInvalid, Message: "missing access token"})
			return
		}
		parts := strings.SplitN(authHeader, " ", 2)
		if len(parts) != 2 || !strings.EqualFold(parts[0], "Bearer") {
			writeJSON(w, http.StatusUnauthorized, APIResponse{Code: CodeTokenInvalid, Message: "invalid authorization header"})
			return
		}

		userID, expired, err := auth.ParseAccessTokenWithStatus(parts[1])
		if err != nil {
			if expired {
				writeJSON(w, http.StatusUnauthorized, APIResponse{Code: CodeTokenExpired, Message: "access token expired"})
				return
			}
			writeJSON(w, http.StatusUnauthorized, APIResponse{Code: CodeTokenInvalid, Message: "invalid access token"})
			return
		}

		ctx := context.WithValue(r.Context(), userIDContextKey, userID)
		next(w, r.WithContext(ctx))
	}
}

func (h *RoomHandler) Login(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeJSON(w, http.StatusMethodNotAllowed, APIResponse{Code: 405, Message: "method not allowed"})
		return
	}
	if h.rdb == nil {
		writeJSON(w, http.StatusInternalServerError, APIResponse{Code: 500, Message: "redis not initialized"})
		return
	}

	var req models.LoginRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, APIResponse{Code: 400, Message: err.Error()})
		return
	}
	if strings.TrimSpace(req.Username) == "" || strings.TrimSpace(req.Password) == "" {
		writeJSON(w, http.StatusBadRequest, APIResponse{Code: 400, Message: "username and password are required"})
		return
	}

	userID := req.Username
	accessToken, err := auth.GenerateAccessToken(userID)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, APIResponse{Code: 500, Message: "failed to generate access token"})
		return
	}

	refreshToken := uuid.NewString()
	if err := h.rdb.Set(r.Context(), refreshTokenRedisKey(refreshToken), userID, 7*24*time.Hour).Err(); err != nil {
		writeJSON(w, http.StatusInternalServerError, APIResponse{Code: 500, Message: "failed to persist refresh token"})
		return
	}

	resp := models.LoginResponse{
		AccessToken:  accessToken,
		RefreshToken: refreshToken,
		ExpiresIn:    int64(auth.AccessTokenTTL / time.Second),
	}
	writeJSON(w, http.StatusOK, APIResponse{Code: 0, Message: "ok", Data: resp})
}

func (h *RoomHandler) RefreshToken(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeJSON(w, http.StatusMethodNotAllowed, APIResponse{Code: 405, Message: "method not allowed"})
		return
	}
	if h.rdb == nil {
		writeJSON(w, http.StatusInternalServerError, APIResponse{Code: 500, Message: "redis not initialized"})
		return
	}

	var req models.RefreshTokenRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, APIResponse{Code: 400, Message: err.Error()})
		return
	}
	if strings.TrimSpace(req.RefreshToken) == "" {
		writeJSON(w, http.StatusBadRequest, APIResponse{Code: 400, Message: "refresh_token is required"})
		return
	}

	userID, err := h.rdb.Get(r.Context(), refreshTokenRedisKey(req.RefreshToken)).Result()
	if errors.Is(err, redis.Nil) {
		writeJSON(w, http.StatusUnauthorized, APIResponse{Code: 40110, Message: "refresh token invalid or expired"})
		return
	}
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, APIResponse{Code: 500, Message: "failed to query refresh token"})
		return
	}

	accessToken, err := auth.GenerateAccessToken(userID)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, APIResponse{Code: 500, Message: "failed to generate access token"})
		return
	}

	resp := models.RefreshTokenResponse{
		AccessToken: accessToken,
		ExpiresIn:   int64(auth.AccessTokenTTL / time.Second),
	}
	writeJSON(w, http.StatusOK, APIResponse{Code: 0, Message: "ok", Data: resp})
}

func (h *RoomHandler) Logout(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeJSON(w, http.StatusMethodNotAllowed, APIResponse{Code: 405, Message: "method not allowed"})
		return
	}
	if h.rdb == nil {
		writeJSON(w, http.StatusInternalServerError, APIResponse{Code: 500, Message: "redis not initialized"})
		return
	}

	var req models.LogoutRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, APIResponse{Code: 400, Message: err.Error()})
		return
	}
	if strings.TrimSpace(req.RefreshToken) == "" {
		writeJSON(w, http.StatusBadRequest, APIResponse{Code: 400, Message: "refresh_token is required"})
		return
	}

	if err := h.rdb.Del(r.Context(), refreshTokenRedisKey(req.RefreshToken)).Err(); err != nil {
		writeJSON(w, http.StatusInternalServerError, APIResponse{Code: 500, Message: "failed to revoke refresh token"})
		return
	}

	writeJSON(w, http.StatusOK, APIResponse{Code: 0, Message: "ok"})
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
	if userID := userIDFromContext(r.Context()); userID != "" {
		req.OwnerID = userID
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

func (h *RoomHandler) JoinRoom(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeJSON(w, http.StatusMethodNotAllowed, APIResponse{Code: 405, Message: "method not allowed"})
		return
	}

	var req models.JoinRoomRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, APIResponse{Code: 400, Message: err.Error()})
		return
	}
	if strings.TrimSpace(req.RoomID) == "" {
		writeJSON(w, http.StatusBadRequest, APIResponse{Code: 400, Message: "room_id is required"})
		return
	}

	room, err := h.roomService.GetRoomByID(r.Context(), req.RoomID)
	if err != nil {
		writeJSON(w, http.StatusNotFound, APIResponse{Code: 404, Message: "room not found"})
		return
	}

	resp := models.JoinRoomResponse{
		Room:    room,
		WSHost:  h.cppHost,
		WSPort:  h.cppPort,
		WSProto: "ws",
	}
	writeJSON(w, http.StatusOK, APIResponse{Code: 0, Message: "ok", Data: resp})
}

func refreshTokenRedisKey(refreshToken string) string {
	return "refresh_token:" + refreshToken
}

func userIDFromContext(ctx context.Context) string {
	if v := ctx.Value(userIDContextKey); v != nil {
		if userID, ok := v.(string); ok {
			return userID
		}
	}
	return ""
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
