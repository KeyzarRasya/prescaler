import docker
from utils.database import get_server_data, get_cpu_data
from utils.csv import append_to_csv
from pathlib import Path
from core.harima import HARIMA
from core.docker import DockerManager
import logging
import time
import os
import math
from datetime import datetime

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

RESEARCH_MODE = 1


def main():
    harima = HARIMA(
                window=WINDOW,
                horizon=HORIZON,
                model_dir=f"{root_dir}/model"
            )
    harima.load()

    docker = DockerManager()
    docker.register_image("test-web")

    research = None
    if RESEARCH_MODE:
        from utils.research_logger import ResearchLogger
        research = ResearchLogger(docker)
        research.start()
        logger.info("RESEARCH_MODE is ON — data collection enabled")

    prev_forecast = None
    prev_forecast_time = None

    try:
        while True:
            data = get_server_data(minutes=15)
            forecast = harima.predict(data)
            max_container = harima.max_per_step(forecast['required_container_per_step'], STEP)
            max_container = math.ceil(max_container)
            append_to_csv(max_container)

            if RESEARCH_MODE and research:
                if prev_forecast is not None and prev_forecast_time is not None:
                    try:
                        actual_series = forecast.get('resampled_series')
                        research.log_actual_vs_forecast(
                            prev_forecast_time,
                            prev_forecast,
                            actual_series
                        )
                    except Exception as e:
                        logger.error("Error logging actual vs forecast: %s", e)

                count_before = docker.get_container_count()
                scaling_start = time.time()

            docker.scale(max_container)

            if RESEARCH_MODE and research:
                scaling_end = time.time()
                count_after = docker.get_container_count()

                try:
                    research.log_scaling_time(
                        scaling_start, scaling_end,
                        count_before, count_after
                    )
                except Exception as e:
                    logger.error("Error logging scaling time: %s", e)

                try:
                    research.log_forecast(forecast['forecast_rps'])
                except Exception as e:
                    logger.error("Error logging forecast: %s", e)

                try:
                    cpu_data = get_cpu_data(minutes=5)
                    research.log_cpu_container(cpu_data, count_after)
                except Exception as e:
                    logger.error("Error logging CPU+container: %s", e)

                prev_forecast = forecast['forecast_rps']
                prev_forecast_time = datetime.now().isoformat()

            time.sleep(WINDOW * STEP)

    except KeyboardInterrupt:
        logger.info("Shutting down...")
        if research:
            research.stop()


main()
