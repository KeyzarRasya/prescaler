package main

import (
	"encoding/json"
	"event/model"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
)

type Timeseries struct{}

func QueryRequestUrl() string {

	requestUrl := fmt.Sprintf(
		"http://%s/api/v3/query_sql",
		os.Getenv("HOST"),
	)
	params := url.Values{}
	params.Add("db", os.Getenv("TSDB_DATABASE"))
	params.Add("q", "SELECT * FROM SERVER")

	return fmt.Sprintf("%s?%s", requestUrl, params.Encode())
}

func (ts *Timeseries) FetchData() (*[]model.ServerStat, error) {
	client := &http.Client{}

	requestUrl := QueryRequestUrl()

	req, err := http.NewRequest(
		"GET",
		requestUrl,
		nil,
	)
	if err != nil {
		return nil, err
	}

	req.Header.Add("Authorization", fmt.Sprintf("Token %s", os.Getenv("TSDB_TOKEN")))
	req.Header.Add("Accept", "*/*")
	req.Close = true

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var stats []model.ServerStat
	if err := json.Unmarshal(body, &stats); err != nil {
		return nil, err
	}

	return &stats, nil
}
