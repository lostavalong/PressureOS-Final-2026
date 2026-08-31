#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-windowed}"
PAGE="${2:-home}"
MODE="${MODE#--}"
EXE="$ROOT/build-rpi/pressureos"
REVIEW_ROOT="${PRESSUREOS_REVIEW_ROOT:-$HOME/pressureos-review}"

case "$MODE" in
  windowed|fullscreen)
    ;;
  *)
    echo "用法：$0 [windowed|fullscreen] [home|measure|tasks|runner|templates|data|device]"
    exit 2
    ;;
esac

case "$PAGE" in
  home|measure|tasks|runner|templates|data|device)
    ;;
  *)
    echo "[失败] 未知页面：$PAGE"
    echo "可用页面：home measure tasks runner templates data device"
    exit 2
    ;;
esac

if [[ ! -x "$EXE" ]]; then
  echo "[失败] 尚未找到已编译程序：$EXE"
  echo "请先运行：./scripts/prepare_raspberry_pi_demo.sh"
  exit 3
fi

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
  echo "[失败] 当前终端没有连接到图形桌面。"
  echo "请在树莓派本机桌面打开终端后运行本脚本；SSH 只用于上传和编译。"
  exit 4
fi

export PRESSUREOS_DATA_ROOT="${PRESSUREOS_DATA_ROOT:-$REVIEW_ROOT/data}"
export PRESSUREOS_EXPORT_ROOT="${PRESSUREOS_EXPORT_ROOT:-$REVIEW_ROOT/export}"
export PRESSUREOS_DEVICE_SOURCE="${PRESSUREOS_DEVICE_SOURCE:-auto}"
export QT_QUICK_CONTROLS_STYLE="${QT_QUICK_CONTROLS_STYLE:-Basic}"
mkdir -p "$PRESSUREOS_DATA_ROOT" "$PRESSUREOS_EXPORT_ROOT" "$REVIEW_ROOT/logs"

LOG_FILE="$REVIEW_ROOT/logs/run_$(date +%Y%m%d_%H%M%S).log"

echo "启动 PressureOS：$MODE / $PAGE"
echo "体验数据：$PRESSUREOS_DATA_ROOT"
echo "导出文件：$PRESSUREOS_EXPORT_ROOT"
echo "运行日志：$LOG_FILE"
echo "测量数据源：$PRESSUREOS_DEVICE_SOURCE（auto 会优先连接 CP2102N 下位机）"

exec "$EXE" "--$MODE" --page "$PAGE" --device-source "$PRESSUREOS_DEVICE_SOURCE" 2> >(tee -a "$LOG_FILE" >&2)
