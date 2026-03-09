import csv
import os
import threading
import time
import logging
from datetime import datetime

logger = logging.getLogger(__name__)

RESEARCH_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "research_data"
)


class ResearchLogger:
    """Handles all research data collection and CSV writing.

    Only instantiated and used when RESEARCH_MODE is enabled.
    """

    def __init__(self, docker_manager):
        self.docker = docker_manager
        self._stop_event = threading.Event()
        self._container_thread = None

        os.makedirs(RESEARCH_DIR, exist_ok=True)

        self.container_count_csv = os.path.join(RESEARCH_DIR, "research_container_count.csv")
        self.scaling_time_csv = os.path.join(RESEARCH_DIR, "research_scaling_time.csv")
        self.forecast_csv = os.path.join(RESEARCH_DIR, "research_forecast.csv")
        self.actual_vs_forecast_csv = os.path.join(RESEARCH_DIR, "research_actual_vs_forecast.csv")
        self.cpu_container_csv = os.path.join(RESEARCH_DIR, "research_cpu_container.csv")

        self._init_csv(self.container_count_csv, ["timestamp", "container_count"])
        self._init_csv(self.scaling_time_csv, [
            "timestamp", "scaling_start", "scaling_end",
            "duration_seconds", "from_count", "to_count"
        ])
        self._forecast_header_written = os.path.isfile(self.forecast_csv)
        self._init_csv(self.actual_vs_forecast_csv, [
            "forecast_timestamp", "actual_timestamp", "step_index",
            "forecasted_rps", "actual_rps"
        ])
        self._init_csv(self.cpu_container_csv, [
            "timestamp", "avg_cpu_usage", "container_count"
        ])

        logger.info("ResearchLogger initialized. Output dir: %s", RESEARCH_DIR)

    @staticmethod
    def _init_csv(filepath, headers):
        """Write CSV header if the file does not yet exist."""
        if not os.path.isfile(filepath):
            with open(filepath, "w", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(headers)

    @staticmethod
    def _append_row(filepath, row):
        """Append a single row to a CSV file."""
        with open(filepath, "a", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(row)

    def _container_count_worker(self):
        """Background thread that logs container count every 30 seconds."""
        logger.info("Container count worker started (interval: 30s)")
        while not self._stop_event.is_set():
            try:
                count = self.docker.get_container_count()
                ts = datetime.now().isoformat()
                self._append_row(self.container_count_csv, [ts, count])
                logger.debug("Logged container count: %d", count)
            except Exception as e:
                logger.error("Error logging container count: %s", e)
            self._stop_event.wait(30) 

    def start(self):
        """Start the background container count logging thread."""
        if self._container_thread is not None and self._container_thread.is_alive():
            logger.warning("Container count worker already running")
            return
        self._stop_event.clear()
        self._container_thread = threading.Thread(
            target=self._container_count_worker,
            daemon=True,
            name="research-container-count"
        )
        self._container_thread.start()
        logger.info("ResearchLogger started")

    def stop(self):
        """Stop the background thread gracefully."""
        self._stop_event.set()
        if self._container_thread is not None:
            self._container_thread.join(timeout=5)
        logger.info("ResearchLogger stopped")


    def log_scaling_time(self, scaling_start, scaling_end, from_count, to_count):
        """Log the time taken for a scaling operation."""
        duration = scaling_end - scaling_start
        ts = datetime.now().isoformat()
        self._append_row(self.scaling_time_csv, [
            ts,
            datetime.fromtimestamp(scaling_start).isoformat(),
            datetime.fromtimestamp(scaling_end).isoformat(),
            round(duration, 4),
            from_count,
            to_count
        ])
        logger.info(
            "Logged scaling time: %.4fs (%d -> %d containers)",
            duration, from_count, to_count
        )

    def log_forecast(self, forecast_rps):
        """Log forecasted RPS values for all steps."""
        ts = datetime.now().isoformat()
        n_steps = len(forecast_rps)

        if not self._forecast_header_written:
            headers = ["timestamp"] + [f"step_{i}" for i in range(n_steps)]
            with open(self.forecast_csv, "w", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(headers)
            self._forecast_header_written = True

        row = [ts] + [round(float(v), 4) for v in forecast_rps]
        self._append_row(self.forecast_csv, row)
        logger.info("Logged forecast RPS (%d steps)", n_steps)

    def log_actual_vs_forecast(self, forecast_timestamp, forecast_rps, actual_series):
        """Compare previously forecasted RPS with actual observed RPS.

        Args:
            forecast_timestamp: ISO timestamp when the forecast was made
            forecast_rps: array of forecasted RPS values (one per step)
            actual_series: pandas Series with actual RPS values (resampled 30s)
        """
        if actual_series is None or len(actual_series) == 0:
            logger.warning("No actual data available for comparison")
            return

        actual_ts = datetime.now().isoformat()
        n_compare = min(len(forecast_rps), len(actual_series))

        for i in range(n_compare):
            actual_val = float(actual_series.iloc[-(n_compare - i)])
            self._append_row(self.actual_vs_forecast_csv, [
                forecast_timestamp,
                actual_ts,
                i,
                round(float(forecast_rps[i]), 4),
                round(actual_val, 4)
            ])

        logger.info(
            "Logged actual vs forecast comparison (%d steps)", n_compare
        )


    def log_cpu_container(self, cpu_data_df, container_count):
        """Log aggregated CPU usage alongside container count.

        Args:
            cpu_data_df: DataFrame from InfluxDB with 'cpu' column
            container_count: current number of running containers
        """
        ts = datetime.now().isoformat()
        try:
            if cpu_data_df is not None and 'cpu' in cpu_data_df.columns and len(cpu_data_df) > 0:
                avg_cpu = round(float(cpu_data_df['cpu'].mean()), 4)
            else:
                avg_cpu = 0.0
                logger.warning("No CPU data available, logging 0.0")
        except Exception as e:
            avg_cpu = 0.0
            logger.error("Error computing avg CPU: %s", e)

        self._append_row(self.cpu_container_csv, [ts, avg_cpu, container_count])
        logger.info("Logged CPU (%.4f) + containers (%d)", avg_cpu, container_count)
