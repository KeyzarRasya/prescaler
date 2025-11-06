package model

type EventType string

const (
	DataReached EventType = "data.reached"
	DataAdded   EventType = "data.added"
)

type Event struct {
	Type EventType   `json:"event_type"`
	Data interface{} `json:"data"`
}
