#!/usr/bin/env python3
"""Reproduce the final PressureOS calibration/model and periodic-noise analysis."""

from __future__ import annotations

import csv
import json
import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from scipy.interpolate import PchipInterpolator
from scipy.optimize import linprog


ROOT = Path(__file__).resolve().parents[1]
DATA = (
    ROOT
    / "artifacts"
    / "formal_calibration_20260824"
    / "backup_20260824_160900_full"
)
PERIOD_S = 5.0284056


@dataclass
class Point:
    pressure: float
    raw: float
    direction: str
    point_id: str
    raw_file: Path


def load_points() -> list[Point]:
    points: list[Point] = []
    with (DATA / "points.csv").open(encoding="utf-8-sig", newline="") as handle:
        for row in csv.DictReader(handle):
            if row["accepted"].strip().lower() != "true":
                continue
            points.append(
                Point(
                    pressure=float(row["standard_kpa"]),
                    raw=float(row["pressure_raw_mean"]),
                    direction=row["direction"],
                    point_id=row["point_id"],
                    raw_file=DATA / row["raw_file"],
                )
            )
    return points


def normalized_design(raw: np.ndarray, degree: int, center: float, scale: float) -> np.ndarray:
    z = (raw - center) / scale
    return np.column_stack([z**power for power in range(degree + 1)])


def fit_ls(raw: np.ndarray, pressure: np.ndarray, degree: int = 2):
    center = float((raw.min() + raw.max()) / 2.0)
    scale = float((raw.max() - raw.min()) / 2.0)
    design = normalized_design(raw, degree, center, scale)
    coef, *_ = np.linalg.lstsq(design, pressure, rcond=None)
    return center, scale, coef


def fit_minimax(raw: np.ndarray, pressure: np.ndarray, degree: int = 2):
    center = float((raw.min() + raw.max()) / 2.0)
    scale = float((raw.max() - raw.min()) / 2.0)
    design = normalized_design(raw, degree, center, scale)
    count = len(raw)
    # Variables are polynomial coefficients followed by maximum absolute error t.
    objective = np.zeros(degree + 2)
    objective[-1] = 1.0
    upper = np.column_stack((design, -np.ones(count)))
    lower = np.column_stack((-design, -np.ones(count)))
    result = linprog(
        objective,
        A_ub=np.vstack((upper, lower)),
        b_ub=np.concatenate((pressure, -pressure)),
        bounds=[(None, None)] * (degree + 1) + [(0.0, None)],
        method="highs",
    )
    if not result.success:
        raise RuntimeError(result.message)
    return center, scale, result.x[:-1], result.x[-1]


def predict(model, raw: np.ndarray) -> np.ndarray:
    center, scale, coef = model[:3]
    return normalized_design(raw, len(coef) - 1, center, scale) @ coef


def raw_power_coefficients(model) -> np.ndarray:
    center, scale, coef = model[:3]
    poly = np.polynomial.Polynomial(coef)
    z = np.polynomial.Polynomial([-center / scale, 1.0 / scale])
    return np.asarray(poly(z).coef)


def metrics(actual: np.ndarray, estimated: np.ndarray) -> dict[str, float]:
    error = estimated - actual
    return {
        "rmse": float(np.sqrt(np.mean(error**2))),
        "mean": float(np.mean(error)),
        "max_abs": float(np.max(np.abs(error))),
    }


def load_raw(point: Point) -> tuple[np.ndarray, np.ndarray]:
    times: list[float] = []
    values: list[float] = []
    with point.raw_file.open(encoding="utf-8-sig", newline="") as handle:
        for row in csv.DictReader(handle):
            if row["phase"] != "sample":
                continue
            times.append(float(row["uptime_ms"]) / 1000.0)
            values.append(float(row["pressure_raw"]))
    time = np.asarray(times)
    return time - time[0], np.asarray(values)


def exact_window_mean(times: np.ndarray, values: np.ndarray, window_s: float) -> np.ndarray:
    """Causal exact-time-window mean, interpolating irregular samples."""
    output = np.empty_like(values, dtype=float)
    for index, now in enumerate(times):
        start = now - window_s
        if start <= times[0]:
            output[index] = np.trapezoid(values[: index + 1], times[: index + 1]) / max(
                now - times[0], 1e-12
            )
            continue
        first = int(np.searchsorted(times, start, side="right"))
        start_value = float(np.interp(start, times[first - 1 : first + 1], values[first - 1 : first + 1]))
        local_t = np.concatenate(([start], times[first : index + 1]))
        local_x = np.concatenate(([start_value], values[first : index + 1]))
        output[index] = np.trapezoid(local_x, local_t) / window_s
    return output


def half_period_comb(times: np.ndarray, values: np.ndarray, period_s: float) -> np.ndarray:
    delayed_time = times - period_s / 2.0
    delayed = np.interp(delayed_time, times, values, left=np.nan)
    output = (values + delayed) / 2.0
    output[np.isnan(output)] = values[np.isnan(output)]
    return output


def ema(values: np.ndarray, alpha: float) -> np.ndarray:
    output = np.empty_like(values, dtype=float)
    output[0] = values[0]
    for index in range(1, len(values)):
        output[index] = output[index - 1] + alpha * (values[index] - output[index - 1])
    return output


def settled_metrics(values: np.ndarray, times: np.ndarray, after_s: float) -> dict[str, float]:
    selected = values[times >= after_s]
    return {
        "std": float(np.std(selected)),
        "p2p": float(np.ptp(selected)),
        "mean": float(np.mean(selected)),
    }


def main() -> None:
    points = load_points()
    up = [point for point in points if point.direction == "up"]
    down = [point for point in points if point.direction == "down"]
    raw_up = np.asarray([point.raw for point in up])
    y_up = np.asarray([point.pressure for point in up])
    raw_down = np.asarray([point.raw for point in down])
    y_down = np.asarray([point.pressure for point in down])
    raw_all = np.asarray([point.raw for point in points])
    y_all = np.asarray([point.pressure for point in points])

    models = {
        "up_ls_q2": fit_ls(raw_up, y_up, 2),
        "all_ls_q2": fit_ls(raw_all, y_all, 2),
        "all_minimax_q2": fit_minimax(raw_all, y_all, 2),
        "up_ls_q3": fit_ls(raw_up, y_up, 3),
    }
    report: dict[str, object] = {"point_count": len(points), "models": {}}
    for name, model in models.items():
        report["models"][name] = {
            "raw_coefficients_low_to_high": raw_power_coefficients(model).tolist(),
            "up": metrics(y_up, predict(model, raw_up)),
            "down": metrics(y_down, predict(model, raw_down)),
            "all": metrics(y_all, predict(model, raw_all)),
        }
        if len(model) == 4:
            report["models"][name]["optimized_max_abs"] = float(model[3])

    # Model-selection test: each up-curve pressure level is excluded once.
    loocv_errors: dict[str, list[float]] = {"q2": [], "q3": [], "linear_lut": [], "pchip": []}
    order = np.argsort(raw_up)
    ordered_raw = raw_up[order]
    ordered_y = y_up[order]
    for held in range(len(ordered_raw)):
        keep = np.arange(len(ordered_raw)) != held
        x_train = ordered_raw[keep]
        y_train = ordered_y[keep]
        x_test = ordered_raw[held]
        y_test = ordered_y[held]
        loocv_errors["q2"].append(float(predict(fit_ls(x_train, y_train, 2), np.asarray([x_test]))[0] - y_test))
        loocv_errors["q3"].append(float(predict(fit_ls(x_train, y_train, 3), np.asarray([x_test]))[0] - y_test))
        if held not in (0, len(ordered_raw) - 1):
            loocv_errors["linear_lut"].append(float(np.interp(x_test, x_train, y_train) - y_test))
        loocv_errors["pchip"].append(float(PchipInterpolator(x_train, y_train, extrapolate=True)(x_test) - y_test))
    report["loocv_up"] = {
        name: metrics(np.zeros(len(errors)), np.asarray(errors))
        for name, errors in loocv_errors.items()
    }

    linear_down = np.interp(raw_down, ordered_raw, ordered_y)
    pchip_down = PchipInterpolator(ordered_raw, ordered_y, extrapolate=True)(raw_down)
    report["up_curve_interpolators_on_down"] = {
        "linear_lut": metrics(y_down, linear_down),
        "pchip": metrics(y_down, pchip_down),
    }

    # Select a representative zero record and quantify candidate suppression filters.
    zero = next(point for point in up if point.pressure == 0.0)
    times, raw_codes = load_raw(zero)
    final_model = models["up_ls_q2"]
    pressure = predict(final_model, raw_codes)
    candidates = {
        "raw": pressure,
        "ema_0.045": ema(pressure, 0.045),
        "half_period_comb": half_period_comb(times, pressure, PERIOD_S),
        "one_period_mean": exact_window_mean(times, pressure, PERIOD_S),
        "twenty_second_precision": exact_window_mean(times, pressure, 20.0),
    }
    report["zero_filter_after_15s"] = {
        name: settled_metrics(values, times, 15.0) for name, values in candidates.items()
    }

    # Frequency robustness of the two cancellation strategies on a pure 0.275 kPa sine.
    sample_rate = 12.727
    synthetic_t = np.arange(0.0, 90.0, 1.0 / sample_rate)
    robustness: dict[str, object] = {}
    for frequency in (0.190, 0.195, 1.0 / PERIOD_S, 0.202, 0.207):
        wave = 0.275 * np.sin(2.0 * math.pi * frequency * synthetic_t + 0.73)
        robustness[f"{frequency:.6f}"] = {
            "comb_std": settled_metrics(half_period_comb(synthetic_t, wave, PERIOD_S), synthetic_t, 15.0)["std"],
            "mean_std": settled_metrics(exact_window_mean(synthetic_t, wave, PERIOD_S), synthetic_t, 15.0)["std"],
        }
    report["frequency_robustness"] = robustness

    precision_point_results: list[dict[str, object]] = []
    for point in points:
        point_times, point_codes = load_raw(point)
        point_pressure = predict(final_model, point_codes)
        point_filtered = exact_window_mean(point_times, point_pressure, 20.0)
        selected = point_times >= 25.0
        errors = point_filtered[selected] - point.pressure
        precision_point_results.append(
            {
                "point_id": point.point_id,
                "standard_kpa": point.pressure,
                "direction": point.direction,
                "mean_error_kpa": float(np.mean(errors)),
                "maximum_absolute_error_kpa": float(np.max(np.abs(errors))),
                "filtered_p2p_kpa": float(np.ptp(point_filtered[selected])),
                "filtered_std_kpa": float(np.std(point_filtered[selected])),
            }
        )
    report["twenty_second_precision_all_points"] = {
        "maximum_absolute_error_kpa": max(
            result["maximum_absolute_error_kpa"] for result in precision_point_results
        ),
        "maximum_filtered_p2p_kpa": max(
            result["filtered_p2p_kpa"] for result in precision_point_results
        ),
        "worst_error_point": max(
            precision_point_results, key=lambda result: result["maximum_absolute_error_kpa"]
        ),
        "worst_p2p_point": max(
            precision_point_results, key=lambda result: result["filtered_p2p_kpa"]
        ),
    }

    output = ROOT / "artifacts" / "formal_calibration_20260824" / "final_analysis_20260826.json"
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    print(f"\nWrote {output}")


if __name__ == "__main__":
    main()
