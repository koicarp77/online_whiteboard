package api

import (
	"net/http"

	"online-whiteboard-go-server/internal/handler"
)

func RegisterV1Routes(mux *http.ServeMux, roomHandler *handler.RoomHandler) {
	// 认证相关接口。
	mux.HandleFunc("/auth/login", roomHandler.Login)
	mux.HandleFunc("/auth/refresh", roomHandler.RefreshToken)
	mux.HandleFunc("/auth/logout", roomHandler.Logout)

	// 房间相关接口。
	mux.HandleFunc("/room/create", roomHandler.AuthMiddleware(roomHandler.CreateRoom))
	mux.HandleFunc("/room/list", roomHandler.AuthMiddleware(roomHandler.ListRooms))
	mux.HandleFunc("/room/join", roomHandler.AuthMiddleware(roomHandler.JoinRoom))
}
