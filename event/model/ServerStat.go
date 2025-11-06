package model

type ServerStat struct {
	Port string  `json:"port"`
	RPS  float64 `json:"rps"`
	CPU  float64 `json:"cpu"`
	Time string  `json:"time"`
}
