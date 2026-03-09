import requests
from pathlib import Path
from dotenv import load_dotenv
import os
import pandas as pd
from .csv import response_to_csv

root_dir = Path(__file__).resolve().parent.parent.parent
dotenv_path = root_dir / ".env"
load_dotenv(dotenv_path)

def get_server_data(minutes):
    token = os.getenv("INFLUXDB_TOKEN")
    url = f"{os.getenv("INFLUXDB_HOST")}/api/v2/query?org={os.getenv("INFLUXDB_ORG")}"
    headers = {
        "Authorization": f"Token {token}",
        "Content-Type": "application/vnd.flux",
        "Accept": "application/csv"
    }
    data = f'''
    from(bucket: "prescal")
        |> range(start: -{minutes}m)
        |> filter(fn: (r) => r._measurement == "server")
        |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
        |> keep(columns: ["_time", "port", "cpu", "rps"])
        |> sort(columns: ["_time"])
    '''

    response = requests.post(url, headers=headers, data=data)
    
    return response_to_csv(response.text)


def get_cpu_data(minutes=5):
    """Fetch CPU usage data over the specified time range from InfluxDB."""
    token = os.getenv("INFLUXDB_TOKEN")
    url = f'{os.getenv("INFLUXDB_HOST")}/api/v2/query?org={os.getenv("INFLUXDB_ORG")}'
    headers = {
        "Authorization": f"Token {token}",
        "Content-Type": "application/vnd.flux",
        "Accept": "application/csv"
    }
    data = f'''
    from(bucket: "prescal")
        |> range(start: -{minutes}m)
        |> filter(fn: (r) => r._measurement == "server")
        |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
        |> keep(columns: ["_time", "port", "cpu"])
        |> sort(columns: ["_time"])
    '''
    response = requests.post(url, headers=headers, data=data)
    return response_to_csv(response.text)