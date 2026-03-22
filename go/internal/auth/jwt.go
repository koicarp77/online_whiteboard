package auth

import (
	"time"
	"errors"
	"github.com/golang-jwt/jwt/v5"
    "os"
    "log"
)

const AccessTokenTTL = 15 * time.Minute

//JWT 密钥，实际项目中应从配置文件或环境变量加载
var jwtSecret []byte
func init() {
    secret := os.Getenv("JWT_SECRET")//JWT_SECRET定义在 docker-compose.yml 中，值为 "online-whiteboard-secret"（该值后续可讨论更改）
    if secret == "" {
        log.Fatal("JWT_SECRET environment variable not set")
    }
    jwtSecret = []byte(secret)
}

//自定义 Claims 结构，包含用户 ID 和标准注册声明（如过期时间、发行时间等）
type Claims struct {
	UserID string `json:"user_id"`
	jwt.RegisteredClaims
}

//生成 JWT 访问令牌，包含用户 ID 和标准注册声明（如过期时间、发行时间等）
func GenerateAccessToken(userID string) (string, error) {
    claims := Claims{
        UserID: userID,
        RegisteredClaims: jwt.RegisteredClaims{
            ExpiresAt: jwt.NewNumericDate(time.Now().Add(AccessTokenTTL)),//令牌过期时间，，目前设置为 15 分钟
            IssuedAt:  jwt.NewNumericDate(time.Now()),//令牌发行时间，设置为当前时间
            NotBefore: jwt.NewNumericDate(time.Now()),//令牌生效时间，设置为当前时间
            Issuer:    "go-server",//签发者标识，目前固定为 "go-server"
            Subject:   userID,//主题标识，暂时设置为用户 ID(后续也可根据实际使用来调整)
        },
    }
    token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)//使用 HS256 签名算法创建 JWT 令牌对象，传入自定义 Claims 结构
    return token.SignedString(jwtSecret)//使用密钥 jwtSecret 对令牌进行签名，并返回完整的 JWT 字符串。若签名失败，则返回错误
}

//解析并验证给定的 JWT 字符串，若有效则返回其中携带的用户 ID，否则返回错误
func ParseAccessToken(tokenString string) (string, error) {
    userID, _, err := ParseAccessTokenWithStatus(tokenString)
    return userID, err
}

func ParseAccessTokenWithStatus(tokenString string) (string, bool, error) {
    token, err := jwt.ParseWithClaims(tokenString, &Claims{}, func(token *jwt.Token) (interface{}, error) {//传入令牌字符串、自定义 Claims 结构（空实例，解析后会自动填充）和一个密钥查找函数（即回调）
        if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {//验证令牌的签名算法是否为 HMAC（HS256），如果不是则返回错误
            return nil, errors.New("unexpected signing method")
        }
        return jwtSecret, nil
    })
    if err != nil {
        if errors.Is(err, jwt.ErrTokenExpired) {
            return "", true, err
        }
        return "", false, err
    }
    if claims, ok := token.Claims.(*Claims); ok && token.Valid {
        return claims.UserID, false, nil
    }
    return "", false, errors.New("invalid token")//令牌无效
}