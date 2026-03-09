import docker
from utils.database import get_server_data
from utils.csv import append_to_csv
from pathlib import Path
from core.harima import HARIMA
from core.docker import DockerManager
import logging
import time
import os
import math
import logging

os.environ["CUDA_VISIBLE_DEVICES"] = "-1"

root_dir = Path(__file__).resolve().parent.parent
logger = logging.getLogger()
logger.setLevel(logging.INFO)

formatter = logging.Formatter(
    "%(asctime)s [%(levelname)s] %(name)s: %(message)s"
)

console = logging.StreamHandler()
console.setFormatter(formatter)

file = logging.FileHandler("app.log")
file.setFormatter(formatter)

logger.addHandler(console)
logger.addHandler(file)

HORIZON = 10
WINDOW = 30
STEP = 3

harima = HARIMA(
                window=WINDOW,
                horizon=HORIZON,
                model_dir=f"{root_dir}/model")

def main():
    harima = HARIMA(
                window=WINDOW,
                horizon=HORIZON,
                model_dir=f"{root_dir}/model"
            )
    harima.load()

    docker = DockerManager()
    docker.register_image("test-web")
    while True:
        data = get_server_data(minutes=15)
        forecast = harima.predict(data)
        max_container = harima.max_per_step(forecast['required_container_per_step'], STEP)
        max_container = math.ceil(max_container)
        append_to_csv(max_container)
        docker.scale(max_container)
        time.sleep(WINDOW * STEP)


main()
# harima = HARIMA(
#     window=WINDOW,
#     horizon=HORIZON,
#     dataframe=data,
#     model_dir=f"{root_dir}/model"
# )

# harima.load()

# forecast = harima.predict()
# print(forecast)

# docker = DockerAPI()
# docker.register_image("test-web")

# docker.scale(5)
