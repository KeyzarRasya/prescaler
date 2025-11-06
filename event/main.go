package main

import (
	"context"
	"fmt"

	"github.com/joho/godotenv"
	"github.com/redis/go-redis/v9"
)

// client := redis.NewClient(&redis.Options{
// 	Addr:     "localhost:6379",
// 	Password: "",
// 	Protocol: 2,
// 	DB:       0,
// })

// ctx := context.Background()
// client.Publish(ctx, "prescal", "HI THERE")

func main() {

	godotenv.Load()
	client := redis.NewClient(&redis.Options{
		Addr:     "localhost:6379",
		Password: "",
		Protocol: 2,
		DB:       0,
	})

	var ts Timeseries = Timeseries{}

	var pub Publisher = Publisher{
		Timeseries: ts,
		Data:       995,
		Redis:      client,
	}

	sub := client.Subscribe(context.Background(), "prescal")

	go func() {
		for msg := range sub.Channel() {
			fmt.Println(msg.Payload)
		}
	}()

	pub.CheckAndPublish()
}
