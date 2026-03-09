import pandas as pd
import csv
from io import StringIO
from datetime import datetime
import os
import logging

logger = logging.getLogger(__name__)

CSV_FILE = "max_container_log.csv"

def response_to_csv(text):
    return pd.read_csv(StringIO(text))

def append_to_csv(value):
    file_exists = os.path.isfile(CSV_FILE)

    with open(CSV_FILE, "a", newline="") as f:
        writer = csv.writer(f)

        # tulis header jika file baru
        if not file_exists:
            writer.writerow(["timestamp", "max_container"])

        writer.writerow([datetime.now().isoformat(), value])
        logger.debug("Successfully write into CSV: %s", value)