package api

import (
	"net/http"

	"online-whiteboard-go-server/internal/handler"
)

func RegisterV1Routes(mux *http.ServeMux, roomHandler *handler.RoomHandler) {
	// 房间相关接口。
	mux.HandleFunc("/room/create", roomHandler.CreateRoom)
	mux.HandleFunc("/room/list", roomHandler.ListRooms)
}
