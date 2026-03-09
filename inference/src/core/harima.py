import numpy as np
import pandas as pd
import joblib
from tensorflow import keras
from statsmodels.tsa.arima.model import ARIMA
import logging

logger = logging.getLogger(__name__)

class HARIMA:

    def __init__(self, window, horizon, model_dir, capacity_per_container=60):

        self.window = window
        self.horizon = horizon
        self.model_dir = model_dir
        self.capacity = capacity_per_container


    # -------------------------------------------------
    # LOAD MODEL
    # -------------------------------------------------

    def load(self):

        self.arima_model = joblib.load(f"{self.model_dir}/arima.pkl")
        self.scaler = joblib.load(f"{self.model_dir}/scaler.pkl")
        self.lstm_model = keras.models.load_model(
            f"{self.model_dir}/lstm_model.keras",
            compile=False
        )


    # -------------------------------------------------
    # PREPROCESSING (SAMA PERSIS SEPERTI TRAINING)
    # -------------------------------------------------

    def aggregate_port(self, df):

        df_original = df.groupby('_time').agg({
            'rps': 'sum',
            'cpu': 'mean'
        })

        df_original.index = pd.to_datetime(df_original.index)

        return df_original


    def resample(self, data, freq="30s"):

        df_resampled = data.resample(freq).mean()

        return df_resampled


    def preprocess(self, df):

        df_agg = self.aggregate_port(df)

        df_resampled = self.resample(df_agg)

        df_resampled = df_resampled.sort_index()

        df_resampled.index = df_resampled.index.tz_localize(None)

        series = df_resampled["rps"].asfreq("30s")

        if len(series) < self.window:
            logger.error("Data series (%s) is not enough for window (%s)", len(series), self.window)
            raise ValueError("Not enough data for window")

        return series


    # -------------------------------------------------
    # ARIMA STEP
    # -------------------------------------------------

    def run_arima(self, series):

        model = ARIMA(series, order=(2,0,5))

        model_fit = model.fit()

        fitted = model_fit.fittedvalues

        residual = series - fitted

        return model_fit, residual


    # -------------------------------------------------
    # RESIDUAL FORECAST WITH LSTM
    # -------------------------------------------------

    def forecast_residual_lstm(self, residual):

        residual_scaled = self.scaler.transform(
            residual.values.reshape(-1, 1)
        )

        last_window = residual_scaled[-self.window:]

        current_window = last_window.copy()

        residual_forecast_scaled = []

        for _ in range(self.horizon):

            X = current_window.reshape(1, self.window, 1)

            pred_scaled = self.lstm_model.predict(X, verbose=0)

            pred_value = pred_scaled[0, 0]

            residual_forecast_scaled.append(pred_value)

            current_window = np.append(
                current_window[1:],
                pred_value
            )

        residual_forecast_scaled = np.array(
            residual_forecast_scaled
        ).reshape(-1, 1)

        residual_forecast = self.scaler.inverse_transform(
            residual_forecast_scaled
        ).flatten()

        return residual_forecast


    # -------------------------------------------------
    # FINAL HYBRID FORECAST
    # -------------------------------------------------

    def run_hybrid(self, df):

        series = self.preprocess(df)

        arima_applied, residual = self.run_arima(series)

        arima_forecast = arima_applied.forecast(steps=self.horizon)

        residual_forecast = self.forecast_residual_lstm(residual)

        hybrid_forecast = arima_forecast.values + residual_forecast

        return hybrid_forecast


    # -------------------------------------------------
    # CONTAINER CALCULATION
    # -------------------------------------------------

    def compute_required_container(self, forecast):

        required_per_step = np.ceil(
            forecast / self.capacity
        )

        final_required = int(
            np.max(required_per_step)
        )

        return required_per_step, final_required


    # -------------------------------------------------
    # PUBLIC METHOD
    # -------------------------------------------------

    def predict(self, df):

        hybrid_forecast = self.run_hybrid(df)

        required_per_step, final_required = self.compute_required_container(
            hybrid_forecast
        )

        return {
            "forecast_rps": hybrid_forecast,
            "required_container_per_step": required_per_step,
            "max_required_container": final_required
        }
    
    def max_per_step(self, required, step):
        value = 0

        for i in range(step):
            if required[i] > value:
                value = required[i]
            
        return value