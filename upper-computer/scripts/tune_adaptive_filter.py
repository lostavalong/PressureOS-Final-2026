#!/usr/bin/env python3
"""Replay real PressureOS serial traces and tune the adaptive periodic filter.

The script is deliberately independent of the UI.  It parses the strace files,
applies the deployed calibration equation, replays a small linear Kalman model
and writes machine-readable metrics for regression testing.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
from dataclasses import dataclass
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
FRAME = re.compile(
    rb"@PS1,M,(\d+),(\d+),(\d+),(\d+),([0-9A-Fa-f]{8})\*([0-9A-Fa-f]{4})"
)
CAL_OFFSET = -102.3969598250119
CAL_LINEAR = 0.0002085601271357902
CAL_QUADRATIC = 2.895347824842157e-13
FREQUENCIES_HZ = tuple(np.arange(0.10, 0.241, 0.005))


def calibrated_pressure(raw_code: np.ndarray) -> np.ndarray:
    value = raw_code.astype(float)
    return (CAL_QUADRATIC * value + CAL_LINEAR) * value + CAL_OFFSET


def load_trace(path: Path) -> tuple[np.ndarray, np.ndarray]:
    matches = FRAME.findall(path.read_bytes())
    rows: dict[int, tuple[int, int]] = {}
    for sequence, timestamp, code, _temperature, _flags, _crc in matches:
        rows[int(sequence)] = (int(timestamp), int(code))
    ordered = [rows[key] for key in sorted(rows)]
    timestamp = np.asarray([row[0] for row in ordered], dtype=np.int64)
    code = np.asarray([row[1] for row in ordered], dtype=np.int64)
    # Device millisecond timestamps are monotonic inside each acquisition.
    keep = np.concatenate(([True], np.diff(timestamp) > 0))
    timestamp = timestamp[keep]
    pressure = calibrated_pressure(code[keep])
    return (timestamp - timestamp[0]) / 1000.0, pressure


def median_tail(values: list[float], count: int = 5) -> float:
    return float(np.median(values[-count:]))


@dataclass
class FilterConfig:
    dynamic_rate_kpa_s: float = 1.20
    dynamic_level_gap_kpa: float = 0.60
    recovery_seconds: float = 6.50
    precision_blend_seconds: float = 1.50
    fast_tau_seconds: float = 0.10
    model_window_seconds: float = 18.0
    model_update_seconds: float = 0.40
    minimum_periodic_amplitude_kpa: float = 0.045
    maximum_periodic_amplitude_kpa: float = 0.45
    output_guard_kpa: float = 0.55


class AdaptivePeriodicFilter:
    """Sliding harmonic regression with a robust dynamic bypass.

    Every precision estimate is an intercept fitted from the current plateau.
    There is deliberately no velocity state and therefore no free prediction.
    A confirmed pressure change clears the plateau window immediately.
    """

    def __init__(self, config: FilterConfig):
        self.config = config
        self.recent: list[float] = []
        self.fast_history: list[tuple[float, float]] = []
        self.plateau: list[tuple[float, float]] = []
        self.last_time: float | None = None
        self.last_fast = 0.0
        self.fast_output = 0.0
        self.quiet_since: float | None = None
        self.precision_since: float | None = None
        self.last_model_time: float | None = None
        self.baseline = 0.0
        self.frequency_hz = 0.0
        self.periodic_amplitude = 0.0
        self.model_ready = False
        self.mode = "dynamic"
        self.trigger = "startup"

    def _clear_plateau(self) -> None:
        self.plateau.clear()
        self.quiet_since = None
        self.precision_since = None
        self.last_model_time = None
        self.model_ready = False
        self.frequency_hz = 0.0
        self.periodic_amplitude = 0.0

    def _fit_model(self, timestamp: float) -> None:
        if len(self.plateau) < 30:
            return
        time = np.asarray([sample[0] for sample in self.plateau])
        value = np.asarray([sample[1] for sample in self.plateau])
        relative = time - time[-1]
        constant_residual = value - np.mean(value)
        constant_error = float(constant_residual @ constant_residual)
        best: tuple[float, float, float, float] | None = None
        for frequency in FREQUENCIES_HZ:
            angle = 2.0 * math.pi * frequency * relative
            design = np.column_stack((np.ones(value.size), np.sin(angle), np.cos(angle)))
            gram = design.T @ design
            # Mild ridge regularisation prevents a partial cycle from moving
            # the fitted centre far away from the actual waveform.
            gram[1, 1] += 0.08
            gram[2, 2] += 0.08
            if np.linalg.cond(gram) > 400.0:
                continue
            coefficients = np.linalg.solve(gram, design.T @ value)
            residual = value - design @ coefficients
            error = float(residual @ residual)
            amplitude = float(math.hypot(coefficients[1], coefficients[2]))
            if amplitude > self.config.maximum_periodic_amplitude_kpa:
                continue
            candidate = (error, float(coefficients[0]), float(frequency), amplitude)
            if best is None or candidate[0] < best[0]:
                best = candidate

        if best is None:
            self.baseline = float(np.mean(value))
            self.frequency_hz = 0.0
            self.periodic_amplitude = 0.0
        else:
            error, baseline, frequency, amplitude = best
            improvement = 1.0 - error / max(constant_error, 1e-12)
            if (
                amplitude >= self.config.minimum_periodic_amplitude_kpa
                and improvement >= 0.35
            ):
                self.baseline = baseline
                self.frequency_hz = frequency
                self.periodic_amplitude = amplitude
            else:
                self.baseline = float(np.mean(value))
                self.frequency_hz = 0.0
                self.periodic_amplitude = 0.0
        self.baseline = min(
            max(self.baseline, float(np.median(value)) - 0.30),
            float(np.median(value)) + 0.30,
        )
        self.model_ready = True
        self.last_model_time = timestamp

    def process(self, timestamp: float, measurement: float) -> tuple[float, str]:
        if self.last_time is None or timestamp <= self.last_time:
            self.recent = [measurement]
            self.fast_history = [(timestamp, measurement)]
            self.last_time = timestamp
            self.last_fast = measurement
            self.fast_output = measurement
            self.quiet_since = timestamp
            self.plateau = [(timestamp, measurement)]
            self.precision_since = None
            self.mode = "dynamic"
            self.baseline = measurement
            return measurement, self.mode

        dt = min(max(timestamp - self.last_time, 0.001), 1.0)
        self.last_time = timestamp
        self.recent.append(measurement)
        if len(self.recent) > 32:
            del self.recent[0]
        fast = median_tail(self.recent)
        self.fast_history.append((timestamp, fast))
        while len(self.fast_history) >= 2 and timestamp - self.fast_history[0][0] > 0.70:
            del self.fast_history[0]
        alpha = 1.0 - math.exp(-dt / self.config.fast_tau_seconds)
        self.fast_output += alpha * (fast - self.fast_output)
        rate_span = timestamp - self.fast_history[0][0]
        rate = (
            abs(fast - self.fast_history[0][1]) / rate_span
            if rate_span >= 0.35
            else 0.0
        )
        self.last_fast = fast

        level_gap = fast - self.baseline
        is_dynamic = (
            rate > self.config.dynamic_rate_kpa_s
            or (self.model_ready and abs(level_gap) > self.config.dynamic_level_gap_kpa)
        )

        if is_dynamic:
            self.trigger = (
                "rate" if rate > self.config.dynamic_rate_kpa_s
                else "level_gap"
            )
            self.mode = "dynamic"
            self._clear_plateau()
            self.baseline = self.fast_output
            return self.fast_output, self.mode

        self.trigger = ""
        if self.quiet_since is None:
            self.quiet_since = timestamp
        self.plateau.append((timestamp, fast))
        cutoff = timestamp - self.config.model_window_seconds
        while len(self.plateau) >= 2 and self.plateau[1][0] < cutoff:
            del self.plateau[0]
        quiet_seconds = timestamp - self.quiet_since
        if quiet_seconds < self.config.recovery_seconds:
            self.mode = "recovery"
            return self.fast_output, self.mode

        if (
            self.last_model_time is None
            or timestamp - self.last_model_time >= self.config.model_update_seconds
        ):
            self._fit_model(timestamp)
        if self.precision_since is None:
            self.precision_since = timestamp
        self.mode = "precision"
        precision = self.baseline if self.model_ready else self.fast_output
        blend = min(
            1.0,
            (timestamp - self.precision_since) / self.config.precision_blend_seconds,
        )
        output = self.fast_output * (1.0 - blend) + precision * blend
        # A final invariant prevents any model fault from placing the indication
        # outside the current measured waveform by a material amount.
        output = min(
            max(output, fast - self.config.output_guard_kpa),
            fast + self.config.output_guard_kpa,
        )
        return output, self.mode


def replay(times: np.ndarray, values: np.ndarray, config: FilterConfig):
    signal_filter = AdaptivePeriodicFilter(config)
    output = np.zeros_like(values)
    modes: list[str] = []
    triggers: list[str] = []
    for index, (timestamp, value) in enumerate(zip(times, values, strict=True)):
        output[index], mode = signal_filter.process(float(timestamp), float(value))
        modes.append(mode)
        triggers.append(signal_filter.trigger)
    return output, modes, triggers


def plateau_metrics(
    times: np.ndarray,
    raw: np.ndarray,
    filtered: np.ndarray,
    target: float,
    start: float,
    end: float,
) -> dict[str, float]:
    selected = (times >= start) & (times <= end)
    raw_selected = raw[selected]
    filtered_selected = filtered[selected]
    if filtered_selected.size < 3:
        return {}
    slope = np.polyfit(times[selected] - times[selected][0], filtered_selected, 1)[0]
    q01, q99 = np.quantile(filtered_selected, (0.01, 0.99))
    return {
        "target_kpa": target,
        "samples": int(filtered_selected.size),
        "raw_mean_kpa": float(np.mean(raw_selected)),
        "filtered_mean_kpa": float(np.mean(filtered_selected)),
        "target_bias_kpa": float(np.mean(filtered_selected) - target),
        "raw_center_bias_kpa": float(np.mean(filtered_selected) - np.mean(raw_selected)),
        "std_kpa": float(np.std(filtered_selected)),
        "p2p_98_kpa": float(q99 - q01),
        "slope_kpa_per_s": float(slope),
    }


def infer_plateaus(times: np.ndarray, values: np.ndarray) -> list[tuple[float, float, float]]:
    """Return long intervals nearest the known 20/100 kPa dynamic-test levels."""
    labels = np.where(np.abs(values - 20.0) <= np.abs(values - 100.0), 20.0, 100.0)
    # Only accept samples close to a requested plateau; transition samples split runs.
    labels[np.minimum(np.abs(values - 20.0), np.abs(values - 100.0)) > 2.0] = np.nan
    runs: list[tuple[float, float, float]] = []
    start = 0
    for index in range(1, len(labels) + 1):
        if index < len(labels) and labels[index] == labels[start]:
            continue
        if not np.isnan(labels[start]):
            duration = times[index - 1] - times[start]
            if duration >= 8.0:
                runs.append((float(labels[start]), float(times[start]), float(times[index - 1])))
        start = index
        if start >= len(labels):
            break
    return runs


def evaluate_trace(path: Path, config: FilterConfig, csv_path: Path | None = None):
    times, raw = load_trace(path)
    filtered, modes, triggers = replay(times, raw, config)
    plateaus = []
    for target, start, end in infer_plateaus(times, raw):
        # Exclude the reacquisition interval and evaluate the settled portion.
        eval_start = min(end, start + 6.0)
        plateaus.append(plateau_metrics(times, raw, filtered, target, eval_start, end))
    if csv_path is not None:
        csv_path.parent.mkdir(parents=True, exist_ok=True)
        with csv_path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow(("time_s", "raw_kpa", "filtered_kpa", "mode", "trigger"))
            writer.writerows(zip(times, raw, filtered, modes, triggers, strict=True))
    return {
        "file": str(path),
        "samples": int(raw.size),
        "duration_s": float(times[-1] - times[0]),
        "sample_rate_hz": float((raw.size - 1) / (times[-1] - times[0])),
        "plateaus": plateaus,
        "mode_fraction": {
            name: modes.count(name) / len(modes)
            for name in ("dynamic", "recovery", "precision")
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "traces",
        nargs="*",
        type=Path,
        default=[
            ROOT / "artifacts" / "pressureos_dynamic_20260826.trace",
            ROOT / "artifacts" / "pressureos_dynamic_down_20260826.trace",
        ],
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "artifacts" / "adaptive_filter_replay_20260826.json",
    )
    parser.add_argument("--csv", action="store_true")
    args = parser.parse_args()
    config = FilterConfig()
    reports = []
    for trace in args.traces:
        csv_path = args.output.with_name(f"{trace.stem}_adaptive.csv") if args.csv else None
        reports.append(evaluate_trace(trace, config, csv_path))
    report = {"config": config.__dict__, "frequencies_hz": FREQUENCIES_HZ, "traces": reports}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
