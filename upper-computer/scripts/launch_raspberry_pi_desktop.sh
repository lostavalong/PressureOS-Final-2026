#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXE="$ROOT/build-rpi/pressureos"
REVIEW_ROOT="${PRESSUREOS_REVIEW_ROOT:-$HOME/pressureos-review}"

# A second tap on the desktop icon must not open another full-screen instance.
if pgrep -x pressureos >/dev/null 2>&1; then
  exit 0
fi

if [[ ! -x "$EXE" ]]; then
  command -v zenity >/dev/null 2>&1 \
    && zenity --error --title="PressureOS" --text="PressureOS 尚未完成编译，请联系软件负责人。"
  exit 3
fi

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
if [[ -z "${WAYLAND_DISPLAY:-}" && -S "$XDG_RUNTIME_DIR/wayland-0" ]]; then
  export WAYLAND_DISPLAY=wayland-0
fi
export QT_QUICK_CONTROLS_STYLE="${QT_QUICK_CONTROLS_STYLE:-Basic}"
export PRESSUREOS_DATA_ROOT="${PRESSUREOS_DATA_ROOT:-$REVIEW_ROOT/data}"
export PRESSUREOS_EXPORT_ROOT="${PRESSUREOS_EXPORT_ROOT:-$REVIEW_ROOT/export}"
export PRESSUREOS_DEVICE_SOURCE="${PRESSUREOS_DEVICE_SOURCE:-auto}"

mkdir -p "$PRESSUREOS_DATA_ROOT" "$PRESSUREOS_EXPORT_ROOT" "$REVIEW_ROOT/logs"
LOG_FILE="$REVIEW_ROOT/logs/desktop_$(date +%Y%m%d_%H%M%S).log"

cd "$ROOT"
exec "$EXE" --fullscreen --page home --device-source "$PRESSUREOS_DEVICE_SOURCE" >>"$LOG_FILE" 2>&1
