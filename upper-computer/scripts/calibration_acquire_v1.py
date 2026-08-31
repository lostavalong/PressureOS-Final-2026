#!/usr/bin/env python3
"""Traceable PressureOS V1 calibration acquisition for Linux/Raspberry Pi.

The script deliberately stores the V1 raw ADC code as the authoritative input.
The currently deployed pressure equation is calculated only as a diagnostic
column; a new calibration must never be fitted from an already converted value.
"""

from __future__ import annotations

import argparse
import csv
import fcntl
import json
import math
import os
import select
import statistics
import termios
import time
from datetime import datetime, timezone
from pathlib import Path


DEFAULT_PORT = (
    "/dev/serial/by-id/"
    "usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_"
    "a83bd7f37698ef1197dfca63a8793231-if00-port0"
)

POINT_COLUMNS = [
    "point_id", "captured_at", "standard_kpa", "direction", "cycle",
    "plateau_repeat", "sample_seconds", "frame_count", "rate_hz",
    "first_sequence", "last_sequence", "dropped_frames", "crc_errors",
    "invalid_v1_lines", "non_v1_lines", "firmware_version", "device_id",
    "pressure_raw_mean", "pressure_raw_std", "pressure_raw_min",
    "pressure_raw_max", "pressure_raw_p2p", "nominal_kpa_mean",
    "nominal_kpa_std", "nominal_kpa_p2p", "nominal_kpa_slope_per_s",
    "repeat_window_means_kpa", "repeat_window_range_kpa",
    "stability_mode", "periodic_period_s", "captured_periods",
    "max_window_range_limit_kpa",
    "temperature_c_mean", "temperature_c_min", "temperature_c_max",
    "pressure_fault_frames", "temperature_fault_frames",
    "system_fault_frames", "accepted", "rejection_reasons", "note",
    "raw_file",
]


def crc16_ccitt_false(payload: bytes) -> int:
    crc = 0xFFFF
    for byte in payload:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def encode_command(command_id: int, command: str) -> bytes:
    payload = f"PS1,C,{command_id},{command.upper()}".encode("ascii")
    return b"@" + payload + f"*{crc16_ccitt_false(payload):04X}\r\n".encode("ascii")


def parse_frame(line: bytes) -> tuple[str, list[str]]:
    if not line.startswith(b"@PS1,") or b"*" not in line:
        raise ValueError("not-v1")
    body, crc_text = line[1:].rsplit(b"*", 1)
    if len(crc_text) != 4:
        raise ValueError("crc")
    try:
        expected = int(crc_text, 16)
    except ValueError as exc:
        raise ValueError("crc") from exc
    if expected != crc16_ccitt_false(body):
        raise ValueError("crc")
    fields = body.decode("ascii").split(",")
    if len(fields) < 2 or fields[0] != "PS1":
        raise ValueError("invalid")
    return fields[1], fields


def configure_serial(fd: int) -> None:
    attributes = termios.tcgetattr(fd)
    attributes[0] = 0
    attributes[1] = 0
    attributes[2] = termios.CLOCAL | termios.CREAD | termios.CS8
    attributes[3] = 0
    attributes[4] = termios.B115200
    attributes[5] = termios.B115200
    attributes[6][termios.VMIN] = 0
    attributes[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attributes)
    termios.tcflush(fd, termios.TCIOFLUSH)


def pressure_nominal_kpa(raw_code: int) -> float:
    """Current deployed equation, retained only for capture diagnostics."""
    raw = float(raw_code)
    return 3.3631e-13 * raw * raw + 2.0807e-4 * raw - 102.2342


def temperature_celsius(raw_code: int) -> float:
    voltage = ((raw_code / 8388608.0) - 1.0) / 4.0 * 0.66
    resistance = voltage / 200.0 * 1_000_000.0
    return (resistance - 100.0) / 0.385


def iso_now() -> str:
    return datetime.now().astimezone().isoformat(timespec="milliseconds")


def safe_token(value: str) -> str:
    rendered = []
    for char in value:
        rendered.append(char if char.isalnum() or char in "-_." else "_")
    return "".join(rendered).strip("_") or "point"


def mean(values: list[float]) -> float:
    return statistics.fmean(values) if values else float("nan")


def sample_std(values: list[float]) -> float:
    return statistics.stdev(values) if len(values) >= 2 else 0.0


def linear_slope(xs: list[float], ys: list[float]) -> float:
    if len(xs) < 2 or len(xs) != len(ys):
        return 0.0
    x_mean = mean(xs)
    y_mean = mean(ys)
    denominator = sum((x - x_mean) ** 2 for x in xs)
    if denominator <= 0.0:
        return 0.0
    return sum((x - x_mean) * (y - y_mean) for x, y in zip(xs, ys)) / denominator


def split_window_means(values: list[float], count: int = 3) -> list[float]:
    if not values:
        return []
    count = max(1, min(count, len(values)))
    result = []
    for index in range(count):
        start = index * len(values) // count
        end = (index + 1) * len(values) // count
        result.append(mean(values[start:end]))
    return result


def write_json_atomic(path: Path, data: object) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def load_session(session_dir: Path) -> dict:
    session_path = session_dir / "session.json"
    if not session_path.is_file():
        raise RuntimeError(f"Calibration session is not initialized: {session_path}")
    return json.loads(session_path.read_text(encoding="utf-8"))


def command_init(args: argparse.Namespace) -> int:
    session_dir = Path(args.session).expanduser().resolve()
    if session_dir.exists() and any(session_dir.iterdir()) and not args.force:
        raise RuntimeError("Session directory is not empty; use a new directory or --force")
    session_dir.mkdir(parents=True, exist_ok=True)
    (session_dir / "raw").mkdir(exist_ok=True)

    points = [float(item.strip()) for item in args.points.split(",") if item.strip()]
    if not points:
        raise RuntimeError("At least one calibration point is required")
    if min(points) < args.range_min or max(points) > args.range_max:
        raise RuntimeError("Calibration plan contains a point outside the declared range")
    points = sorted(set(points))

    session = {
        "schema": "PressureOS.CalibrationSession/1.0",
        "session_id": args.session_id or session_dir.name,
        "created_at": iso_now(),
        "device_id": args.device_id,
        "sensor_id": args.sensor_id,
        "pressure_type": args.pressure_type,
        "range_kpa": [args.range_min, args.range_max],
        "target_grade_percent_fs": args.target_grade,
        "target_allowed_error_kpa": (args.range_max - args.range_min) * args.target_grade / 100.0,
        "standard": {
            "name": args.standard_name,
            "model": args.standard_model,
            "serial": args.standard_serial,
            "accuracy": args.standard_accuracy,
            "certificate": args.certificate,
            "valid_until": args.standard_valid_until,
        },
        "operator": args.operator,
        "reviewer": args.reviewer,
        "environment_policy": "ambient temperature recorded; no temperature compensation",
        "firmware_required": args.firmware_required,
        "protocol_required": "PressureOS V1 ASCII + CRC16/CCITT-FALSE",
        "sampling_rate_min_hz": args.min_rate,
        "point_stability": {
            "sample_seconds": args.sample_seconds,
            "mode": "periodic-average" if args.periodic_period_s > 0.0 else "instantaneous",
            "maximum_p2p_kpa": args.max_p2p_kpa,
            "maximum_abs_slope_kpa_per_s": args.max_slope_kpa_s,
            "periodic_period_s": args.periodic_period_s,
            "minimum_periods": args.minimum_periods,
            "maximum_repeat_window_range_kpa": args.max_window_range_kpa,
        },
        "cycles": args.cycles,
        "points_kpa": points,
        "notes": args.notes,
    }
    write_json_atomic(session_dir / "session.json", session)

    order = 0
    with (session_dir / "plan.csv").open("w", newline="", encoding="utf-8-sig") as output:
        writer = csv.DictWriter(output, fieldnames=["order", "cycle", "direction", "standard_kpa", "state"])
        writer.writeheader()
        for cycle in range(1, args.cycles + 1):
            for direction, sequence in (("up", points), ("down", list(reversed(points)))):
                for standard in sequence:
                    order += 1
                    writer.writerow({
                        "order": order,
                        "cycle": cycle,
                        "direction": direction,
                        "standard_kpa": f"{standard:.6f}",
                        "state": "pending",
                    })

    print(json.dumps({
        "session": str(session_dir),
        "plan_points": order,
        "allowed_error_kpa": session["target_allowed_error_kpa"],
        "initialized": True,
    }, ensure_ascii=False, indent=2))
    return 0


def acquire_frames(port: str, settle_seconds: float, sample_seconds: float) -> dict:
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    configure_serial(fd)
    buffer = bytearray()
    started = time.monotonic()
    sample_started = started + settle_seconds
    deadline = sample_started + sample_seconds
    records = []
    info_frames = []
    crc_errors = 0
    invalid_v1_lines = 0
    non_v1_lines = 0
    info_sent = False
    try:
        while time.monotonic() < deadline:
            now = time.monotonic()
            if not info_sent and now - started >= 0.25:
                os.write(fd, encode_command(int(time.time()) & 0x7FFFFFFF, "GET_INFO"))
                info_sent = True
            readable, _, _ = select.select([fd], [], [], 0.2)
            if not readable:
                continue
            chunk = os.read(fd, 4096)
            if not chunk:
                continue
            buffer.extend(chunk)
            while b"\n" in buffer:
                raw_line, _, remainder = buffer.partition(b"\n")
                buffer = bytearray(remainder)
                line = raw_line.strip()
                if not line:
                    continue
                observed_monotonic = time.monotonic()
                try:
                    frame_type, fields = parse_frame(line)
                except ValueError as error:
                    if str(error) == "crc":
                        crc_errors += 1
                    elif line.startswith(b"@PS1,"):
                        invalid_v1_lines += 1
                    else:
                        non_v1_lines += 1
                    continue
                if frame_type == "I":
                    info_frames.append(fields)
                    continue
                if frame_type != "M" or len(fields) != 7:
                    continue
                record = {
                    "host_time": datetime.now(timezone.utc).astimezone().isoformat(timespec="milliseconds"),
                    "host_monotonic": observed_monotonic,
                    "phase": "sample" if observed_monotonic >= sample_started else "settle",
                    "sequence": int(fields[2]),
                    "uptime_ms": int(fields[3]),
                    "pressure_raw": int(fields[4]),
                    "temperature_raw": int(fields[5]),
                    "status": int(fields[6], 16),
                }
                record["nominal_kpa"] = pressure_nominal_kpa(record["pressure_raw"])
                record["temperature_c"] = (
                    temperature_celsius(record["temperature_raw"])
                    if record["temperature_raw"] > 0 else None
                )
                records.append(record)
    finally:
        fcntl.flock(fd, fcntl.LOCK_UN)
        os.close(fd)
    return {
        "records": records,
        "info_frames": info_frames,
        "crc_errors": crc_errors,
        "invalid_v1_lines": invalid_v1_lines,
        "non_v1_lines": non_v1_lines,
    }


def summarize_capture(acquisition: dict, args: argparse.Namespace, session: dict, raw_name: str) -> dict:
    sample = [item for item in acquisition["records"] if item["phase"] == "sample"]
    if not sample:
        raise RuntimeError("No valid V1 measurement frame was captured")

    dropped = 0
    for previous, current in zip(sample, sample[1:]):
        expected = (previous["sequence"] + 1) & 0xFFFFFFFF
        if current["sequence"] != expected:
            gap = (current["sequence"] - expected) & 0xFFFFFFFF
            if gap < 0x80000000:
                dropped += gap

    rate_hz = 0.0
    if len(sample) >= 2:
        elapsed_ms = sample[-1]["uptime_ms"] - sample[0]["uptime_ms"]
        if elapsed_ms > 0:
            rate_hz = (len(sample) - 1) * 1000.0 / elapsed_ms

    raw_values = [float(item["pressure_raw"]) for item in sample]
    pressure_values = [float(item["nominal_kpa"]) for item in sample]
    temperatures = [float(item["temperature_c"]) for item in sample if item["temperature_c"] is not None]
    elapsed_s = [(item["uptime_ms"] - sample[0]["uptime_ms"]) / 1000.0 for item in sample]
    window_means = split_window_means(pressure_values, 3)

    pressure_fault_mask = 0x00000004 | 0x00000010 | 0x00000020
    temperature_fault_mask = 0x00000008 | 0x00000040
    system_fault_mask = 0x00000001 | 0x00000002 | 0x00000080 | 0x00000400
    pressure_fault_frames = sum(bool(item["status"] & pressure_fault_mask) for item in sample)
    temperature_fault_frames = sum(bool(item["status"] & temperature_fault_mask) for item in sample)
    system_fault_frames = sum(bool(item["status"] & system_fault_mask) for item in sample)

    firmware_version = ""
    device_id = ""
    if acquisition["info_frames"]:
        latest = acquisition["info_frames"][-1]
        if len(latest) >= 6:
            firmware_version = latest[4]
            device_id = latest[5]

    p2p_kpa = max(pressure_values) - min(pressure_values)
    slope_kpa_s = linear_slope(elapsed_s, pressure_values)
    window_range_kpa = max(window_means) - min(window_means) if window_means else 0.0
    periodic_mode = args.periodic_period_s > 0.0
    captured_periods = args.seconds / args.periodic_period_s if periodic_mode else 0.0
    reasons = []
    if firmware_version != session.get("firmware_required", "1.0.5"):
        reasons.append(f"firmware={firmware_version or 'unknown'}")
    if rate_hz < float(session.get("sampling_rate_min_hz", args.min_rate)):
        reasons.append(f"rate={rate_hz:.3f}Hz")
    if dropped:
        reasons.append(f"dropped={dropped}")
    if acquisition["crc_errors"]:
        reasons.append(f"crc={acquisition['crc_errors']}")
    if acquisition["invalid_v1_lines"] or acquisition["non_v1_lines"]:
        reasons.append("invalid-or-non-v1-lines")
    if pressure_fault_frames or temperature_fault_frames or system_fault_frames:
        reasons.append("device-status-fault")
    if not temperatures:
        reasons.append("temperature-missing")
    if periodic_mode:
        if captured_periods < args.minimum_periods:
            reasons.append(f"periods={captured_periods:.3f}")
        if window_range_kpa > args.max_window_range_kpa:
            reasons.append(f"window-range={window_range_kpa:.6f}kPa")
    elif p2p_kpa > args.max_p2p_kpa:
        reasons.append(f"p2p={p2p_kpa:.6f}kPa")
    if abs(slope_kpa_s) > args.max_slope_kpa_s:
        reasons.append(f"slope={slope_kpa_s:.6f}kPa/s")

    point_id = Path(raw_name).stem
    return {
        "point_id": point_id,
        "captured_at": sample[0]["host_time"],
        "standard_kpa": args.standard_kpa,
        "direction": args.direction,
        "cycle": args.cycle,
        "plateau_repeat": args.plateau_repeat,
        "sample_seconds": args.seconds,
        "frame_count": len(sample),
        "rate_hz": round(rate_hz, 6),
        "first_sequence": sample[0]["sequence"],
        "last_sequence": sample[-1]["sequence"],
        "dropped_frames": dropped,
        "crc_errors": acquisition["crc_errors"],
        "invalid_v1_lines": acquisition["invalid_v1_lines"],
        "non_v1_lines": acquisition["non_v1_lines"],
        "firmware_version": firmware_version,
        "device_id": device_id,
        "pressure_raw_mean": mean(raw_values),
        "pressure_raw_std": sample_std(raw_values),
        "pressure_raw_min": min(raw_values),
        "pressure_raw_max": max(raw_values),
        "pressure_raw_p2p": max(raw_values) - min(raw_values),
        "nominal_kpa_mean": mean(pressure_values),
        "nominal_kpa_std": sample_std(pressure_values),
        "nominal_kpa_p2p": p2p_kpa,
        "nominal_kpa_slope_per_s": slope_kpa_s,
        "repeat_window_means_kpa": window_means,
        "repeat_window_range_kpa": window_range_kpa,
        "stability_mode": "periodic-average" if periodic_mode else "instantaneous",
        "periodic_period_s": args.periodic_period_s,
        "captured_periods": captured_periods,
        "max_window_range_limit_kpa": args.max_window_range_kpa,
        "temperature_c_mean": mean(temperatures) if temperatures else None,
        "temperature_c_min": min(temperatures) if temperatures else None,
        "temperature_c_max": max(temperatures) if temperatures else None,
        "pressure_fault_frames": pressure_fault_frames,
        "temperature_fault_frames": temperature_fault_frames,
        "system_fault_frames": system_fault_frames,
        "accepted": not reasons,
        "rejection_reasons": reasons,
        "note": args.note,
        "raw_file": str(Path("raw") / raw_name),
    }


def write_raw_capture(path: Path, records: list[dict]) -> None:
    columns = [
        "host_time", "phase", "sequence", "uptime_ms", "pressure_raw",
        "temperature_raw", "status_hex", "nominal_kpa", "temperature_c",
    ]
    with path.open("w", newline="", encoding="utf-8-sig") as output:
        writer = csv.DictWriter(output, fieldnames=columns)
        writer.writeheader()
        for item in records:
            writer.writerow({
                "host_time": item["host_time"],
                "phase": item["phase"],
                "sequence": item["sequence"],
                "uptime_ms": item["uptime_ms"],
                "pressure_raw": item["pressure_raw"],
                "temperature_raw": item["temperature_raw"],
                "status_hex": f"0x{item['status']:08X}",
                "nominal_kpa": f"{item['nominal_kpa']:.9f}",
                "temperature_c": "" if item["temperature_c"] is None else f"{item['temperature_c']:.6f}",
            })


def append_summary_csv(path: Path, summary: dict) -> None:
    exists = path.is_file() and path.stat().st_size > 0
    row = dict(summary)
    row["repeat_window_means_kpa"] = json.dumps(row["repeat_window_means_kpa"], ensure_ascii=False)
    row["rejection_reasons"] = "; ".join(row["rejection_reasons"])
    with path.open("a", newline="", encoding="utf-8-sig") as output:
        writer = csv.DictWriter(output, fieldnames=POINT_COLUMNS)
        if not exists:
            writer.writeheader()
        writer.writerow({key: row.get(key, "") for key in POINT_COLUMNS})


def command_capture(args: argparse.Namespace) -> int:
    session_dir = Path(args.session).expanduser().resolve()
    session = load_session(session_dir)
    range_min, range_max = session["range_kpa"]
    if not range_min <= args.standard_kpa <= range_max:
        raise RuntimeError("Standard pressure is outside the declared session range")

    stamp = datetime.now().strftime("%Y%m%dT%H%M%S")
    pressure_token = f"{args.standard_kpa:+010.3f}".replace("+", "p").replace("-", "m")
    raw_name = safe_token(
        f"C{args.cycle}_{args.direction}_{pressure_token}_R{args.plateau_repeat}_{stamp}"
    ) + ".csv"
    acquisition = acquire_frames(args.port, args.settle_seconds, args.seconds)
    summary = summarize_capture(acquisition, args, session, raw_name)
    write_raw_capture(session_dir / "raw" / raw_name, acquisition["records"])
    append_summary_csv(session_dir / "points.csv", summary)
    with (session_dir / "points.jsonl").open("a", encoding="utf-8") as output:
        output.write(json.dumps(summary, ensure_ascii=False) + "\n")
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0 if summary["accepted"] else 2


def command_status(args: argparse.Namespace) -> int:
    session_dir = Path(args.session).expanduser().resolve()
    session = load_session(session_dir)
    accepted = 0
    rejected = 0
    point_count = 0
    path = session_dir / "points.jsonl"
    latest = None
    if path.is_file():
        for line in path.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            item = json.loads(line)
            latest = item
            point_count += 1
            if item.get("accepted"):
                accepted += 1
            else:
                rejected += 1
    print(json.dumps({
        "session_id": session.get("session_id"),
        "session": str(session_dir),
        "captured_points": point_count,
        "accepted_points": accepted,
        "rejected_points": rejected,
        "latest_point": latest,
    }, ensure_ascii=False, indent=2))
    return 0


def command_self_test(_: argparse.Namespace) -> int:
    fixed = b"PS1,M,1523,483920,490980,9514440,00000000"
    assert crc16_ccitt_false(fixed) == 0x3EC2
    frame_type, fields = parse_frame(b"@" + fixed + b"*3EC2")
    assert frame_type == "M" and fields[2] == "1523"
    values = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0]
    assert split_window_means(values, 3) == [1.5, 3.5, 5.5]
    assert math.isclose(linear_slope([0.0, 1.0, 2.0], [1.0, 3.0, 5.0]), 2.0)
    print(json.dumps({"passed": True, "crc_fixed_vector": "3EC2"}, indent=2))
    return 0


def add_common_stability_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--min-rate", type=float, default=10.0)
    parser.add_argument("--max-p2p-kpa", type=float, default=0.10)
    parser.add_argument("--max-slope-kpa-s", type=float, default=0.02)
    parser.add_argument("--periodic-period-s", type=float, default=0.0,
                        help="Known unavoidable periodic interference; 0 disables periodic averaging mode")
    parser.add_argument("--minimum-periods", type=float, default=5.0)
    parser.add_argument("--max-window-range-kpa", type=float, default=0.05,
                        help="Maximum range among the three consecutive sub-window means")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="PressureOS V1 formal calibration acquisition")
    subparsers = parser.add_subparsers(dest="command", required=True)

    init = subparsers.add_parser("init", help="Initialize an immutable calibration session directory")
    init.add_argument("--session", required=True)
    init.add_argument("--session-id")
    init.add_argument("--device-id", required=True)
    init.add_argument("--sensor-id", required=True)
    init.add_argument("--pressure-type", choices=["gauge", "absolute", "differential"], required=True)
    init.add_argument("--range-min", type=float, default=-100.0)
    init.add_argument("--range-max", type=float, default=600.0)
    init.add_argument("--target-grade", type=float, default=0.05)
    init.add_argument("--standard-name", required=True)
    init.add_argument("--standard-model", required=True)
    init.add_argument("--standard-serial", required=True)
    init.add_argument("--standard-accuracy", required=True)
    init.add_argument("--certificate", default="")
    init.add_argument("--standard-valid-until", default="")
    init.add_argument("--operator", required=True)
    init.add_argument("--reviewer", default="")
    init.add_argument("--points", default="-100,-80,-50,0,50,100,200,300,400,500,600")
    init.add_argument("--cycles", type=int, default=2)
    init.add_argument("--sample-seconds", type=float, default=15.0)
    init.add_argument("--firmware-required", default="1.0.5")
    init.add_argument("--notes", default="")
    init.add_argument("--force", action="store_true")
    add_common_stability_arguments(init)
    init.set_defaults(func=command_init)

    capture = subparsers.add_parser("capture", help="Capture one stable pressure plateau")
    capture.add_argument("--session", required=True)
    capture.add_argument("--port", default=DEFAULT_PORT)
    capture.add_argument("--standard-kpa", type=float, required=True)
    capture.add_argument("--direction", choices=["up", "down", "zero", "check"], required=True)
    capture.add_argument("--cycle", type=int, default=1)
    capture.add_argument("--plateau-repeat", type=int, default=1)
    capture.add_argument("--settle-seconds", type=float, default=3.0)
    capture.add_argument("--seconds", type=float, default=15.0)
    capture.add_argument("--note", default="")
    add_common_stability_arguments(capture)
    capture.set_defaults(func=command_capture)

    status = subparsers.add_parser("status", help="Show session progress")
    status.add_argument("--session", required=True)
    status.set_defaults(func=command_status)

    self_test = subparsers.add_parser("self-test", help="Run parser and statistic unit checks")
    self_test.set_defaults(func=command_self_test)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        return args.func(args)
    except BlockingIOError:
        print(json.dumps({
            "passed": False,
            "error": "serial-port-busy",
            "message": "Stop PressureOS or any other serial reader before formal capture.",
        }, ensure_ascii=False, indent=2))
        return 3
    except Exception as error:
        print(json.dumps({"passed": False, "error": str(error)}, ensure_ascii=False, indent=2))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
