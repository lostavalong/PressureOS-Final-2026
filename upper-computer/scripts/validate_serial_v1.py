#!/usr/bin/env python3
"""Dependency-free PressureOS V1 serial acceptance test for Linux/Raspberry Pi."""

from __future__ import annotations

import argparse
import fcntl
import json
import os
import select
import termios
import time


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
    termios.tcflush(fd, termios.TCIFLUSH)


def parse_frame(line: bytes) -> tuple[str, list[str]]:
    if not line.startswith(b"@PS1,") or b"*" not in line:
        raise ValueError("not-v1")
    body, crc_text = line[1:].rsplit(b"*", 1)
    if len(crc_text) != 4 or int(crc_text, 16) != crc16_ccitt_false(body):
        raise ValueError("crc")
    fields = body.decode("ascii").split(",")
    return fields[1], fields


def temperature_celsius(raw_code: int) -> float:
    voltage = ((raw_code / 8388608.0) - 1.0) / 4.0 * 0.66
    resistance = voltage / 200.0 * 1_000_000.0
    return (resistance - 100.0) / 0.385


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--seconds", type=float, default=12.0)
    parser.add_argument("--min-rate", type=float, default=10.0)
    parser.add_argument("--expected-firmware", default="1.0.5")
    parser.add_argument("--output")
    args = parser.parse_args()

    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    configure_serial(fd)

    measurements: list[dict[str, int]] = []
    info_frames: list[list[str]] = []
    invalid_lines = 0
    crc_errors = 0
    other_lines = 0
    buffer = bytearray()
    started = time.monotonic()
    info_sent = False

    try:
        while time.monotonic() - started < args.seconds:
            if not info_sent and time.monotonic() - started >= 0.5:
                os.write(fd, encode_command(240824, "GET_INFO"))
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
                try:
                    frame_type, fields = parse_frame(line)
                except ValueError as error:
                    if str(error) == "crc":
                        crc_errors += 1
                    elif line.startswith(b"@PS1,"):
                        invalid_lines += 1
                    else:
                        other_lines += 1
                    continue
                if frame_type == "M" and len(fields) == 7:
                    measurements.append({
                        "sequence": int(fields[2]),
                        "uptime_ms": int(fields[3]),
                        "pressure_raw": int(fields[4]),
                        "temperature_raw": int(fields[5]),
                        "status": int(fields[6], 16),
                    })
                elif frame_type == "I":
                    info_frames.append(fields)
    finally:
        fcntl.flock(fd, fcntl.LOCK_UN)
        os.close(fd)

    dropped = 0
    for previous, current in zip(measurements, measurements[1:]):
        expected = (previous["sequence"] + 1) & 0xFFFFFFFF
        if current["sequence"] != expected:
            gap = (current["sequence"] - expected) & 0xFFFFFFFF
            if gap < 0x80000000:
                dropped += gap

    rate_hz = 0.0
    if len(measurements) >= 2:
        elapsed_ms = measurements[-1]["uptime_ms"] - measurements[0]["uptime_ms"]
        if elapsed_ms > 0:
            rate_hz = (len(measurements) - 1) * 1000.0 / elapsed_ms

    pressure_fault_mask = 0x00000004 | 0x00000010 | 0x00000020
    temperature_fault_mask = 0x00000008 | 0x00000040
    system_fault_mask = 0x00000001 | 0x00000002 | 0x00000080 | 0x00000400
    pressure_fault_frames = sum(bool(frame["status"] & pressure_fault_mask) for frame in measurements)
    temperature_fault_frames = sum(bool(frame["status"] & temperature_fault_mask) for frame in measurements)
    system_fault_frames = sum(bool(frame["status"] & system_fault_mask) for frame in measurements)
    temperature_codes = [frame["temperature_raw"] for frame in measurements if frame["temperature_raw"] > 0]

    firmware_version = ""
    device_id = ""
    if info_frames:
        firmware_version = info_frames[-1][4]
        device_id = info_frames[-1][5]

    result = {
        "port": args.port,
        "duration_s": args.seconds,
        "measurement_frames": len(measurements),
        "rate_hz": round(rate_hz, 3),
        "minimum_required_hz": args.min_rate,
        "dropped_frames": dropped,
        "crc_errors": crc_errors,
        "invalid_v1_lines": invalid_lines,
        "non_v1_lines": other_lines,
        "pressure_fault_frames": pressure_fault_frames,
        "temperature_fault_frames": temperature_fault_frames,
        "system_fault_frames": system_fault_frames,
        "firmware_version": firmware_version,
        "expected_firmware_version": args.expected_firmware,
        "device_id": device_id,
        "temperature_samples_present": bool(temperature_codes),
        "latest_temperature_c": round(temperature_celsius(temperature_codes[-1]), 3)
        if temperature_codes else None,
        "first_sequence": measurements[0]["sequence"] if measurements else None,
        "last_sequence": measurements[-1]["sequence"] if measurements else None,
        "first_pressure_raw": measurements[0]["pressure_raw"] if measurements else None,
        "last_pressure_raw": measurements[-1]["pressure_raw"] if measurements else None,
    }
    passed = (
        len(measurements) >= 3
        and rate_hz > args.min_rate
        and dropped == 0
        and crc_errors == 0
        and pressure_fault_frames == 0
        and temperature_fault_frames == 0
        and system_fault_frames == 0
        and bool(temperature_codes)
        and firmware_version == args.expected_firmware
    )
    result["passed"] = passed
    rendered = json.dumps(result, ensure_ascii=False, indent=2)
    print(rendered)
    if args.output:
        with open(args.output, "w", encoding="utf-8") as output_file:
            output_file.write(rendered + "\n")
    return 0 if passed else 2


if __name__ == "__main__":
    raise SystemExit(main())
