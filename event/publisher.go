package main

import (
	"context"
	"encoding/json"
	"event/model"
	"fmt"
	"os"
	"time"

	"github.com/redis/go-redis/v9"
)

type Publisher struct {
	Timeseries Timeseries
	Data       int
	Redis      *redis.Client
}

func (pb *Publisher) CheckDataLength() bool {
	datas, err := pb.Timeseries.FetchData()

	if err != nil {
		fmt.Println(err)
		return false
	}

	fmt.Println(len(*datas))
	pb.Data = pb.Data + 1
	if pb.Data <= 1000 {
		return true
	} else {
		return false
	}
}

func (pb *Publisher) Publish(
	eventType model.EventType,
	data interface{},
) error {
	var event model.Event = model.Event{
		Type: eventType,
		Data: data,
	}

	payload, err := json.Marshal(event)
	if err != nil {
		return err
	}

	err = pb.Redis.Publish(
		context.Background(),
		os.Getenv("CHANNEL_REDIS"),
		payload,
	).Err()

	return err
}

func (pb *Publisher) CheckAndPublish() error {
	for pb.CheckDataLength() {
		fmt.Printf("DATA NOT ENOUGH ")
		if pb.Data >= 1000 {
			stats, err := pb.Timeseries.FetchData()
			if err != nil {
				return err
			}
			return pb.Publish(model.DataReached, stats)
		}
		fmt.Println(pb.Data)
		time.Sleep(time.Second)
	}
	return nil
}
