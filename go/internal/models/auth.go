package models

//前端发送登录请求时应当提供的JSON数据格式
type LoginRequest struct {
	Username string `json:"username" binding:"required"`
	Password string `json:"password" binding:"required"`
}

type RefreshTokenRequest struct {
	RefreshToken string `json:"refresh_token"`
}

type LogoutRequest struct {
	RefreshToken string `json:"refresh_token"`
}

//表示服务器在登录成功后返回给客户端的JSON数据格式
type LoginResponse struct {
	AccessToken  string `json:"access_token"`//访问令牌（JWT类型），用于后续API请求的身份认证
	RefreshToken string `json:"refresh_token"`//刷新令牌，用于在访问令牌过期后获取新的访问令牌
	ExpiresIn    int64  `json:"expires_in"` // Access Token过期秒数，访问令牌的有效时长，单位是秒
}

type RefreshTokenResponse struct {
	AccessToken string `json:"access_token"`
	ExpiresIn   int64  `json:"expires_in"`
}