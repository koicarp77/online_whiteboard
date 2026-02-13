package database

import (
	"fmt"
	"log"
	"time"

	"gorm.io/driver/mysql"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

var DB *gorm.DB

type MySQLConfig struct {//封装连接 MySQL 所需的配置项
	Host     string
	Port     string
	User     string
	Password string
	Database string
}

func InitMySQL(config MySQLConfig) error {//建立数据库连接，配置连接池，并将全局变量 DB 赋值
	dsn := fmt.Sprintf("%s:%s@tcp(%s:%s)/%s?charset=utf8mb4&parseTime=True&loc=Local",
		config.User, config.Password, config.Host, config.Port, config.Database)

	var err error
	DB, err = gorm.Open(mysql.Open(dsn), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Info),
	})
	if err != nil {
		return fmt.Errorf("failed to connect to MySQL: %v", err)
	}

	// 配置连接池
	sqlDB, err := DB.DB()
	if err != nil {
		return err
	}

	sqlDB.SetMaxIdleConns(10)//设置连接池中的最大空闲连接数
	sqlDB.SetMaxOpenConns(100)//设置连接池中的最大连接数（包括正在使用的和空闲的）
	sqlDB.SetConnMaxLifetime(time.Hour*5)//设置连接的最大生命周期，超过这个时间的连接将被关闭并从连接池中移除

	log.Println("MySQL connected successfully")
	return nil
}

func AutoMigrate(models ...interface{}) error {
	//调用 GORM 的 AutoMigrate 方法，根据传入的结构体自动创建或更新数据库表，
	//通常在程序启动时，连接数据库后调用此函数，确保数据表与模型定义同步
	return DB.AutoMigrate(models...)
}