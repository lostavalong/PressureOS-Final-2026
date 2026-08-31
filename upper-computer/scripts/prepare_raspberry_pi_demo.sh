#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REVIEW_ROOT="${PRESSUREOS_REVIEW_ROOT:-$HOME/pressureos-review}"
LOG_DIR="$REVIEW_ROOT/logs"
LOG_FILE="$LOG_DIR/install_$(date +%Y%m%d_%H%M%S).log"

mkdir -p "$LOG_DIR"
exec > >(tee -a "$LOG_FILE") 2>&1

on_error() {
  local code=$?
  echo
  echo "[失败] 安装或编译在第 ${BASH_LINENO[0]} 行停止，退出码：$code"
  echo "请运行：$ROOT/scripts/collect_raspberry_pi_diagnostics.sh"
  echo "并把诊断文件和日志发回软件负责人：$LOG_FILE"
  exit "$code"
}
trap on_error ERR

echo "============================================================"
echo " PressureOS 树莓派交互 Demo：环境准备、编译与测试"
echo "============================================================"
echo "工程目录：$ROOT"
echo "安装日志：$LOG_FILE"
echo "系统架构：$(uname -m)"

if [[ ! -r /etc/os-release ]]; then
  echo "[失败] 无法读取 /etc/os-release。请使用 Raspberry Pi OS 64-bit Bookworm Desktop。"
  exit 2
fi

# shellcheck disable=SC1091
source /etc/os-release
echo "操作系统：${PRETTY_NAME:-unknown}"

case "$(uname -m)" in
  aarch64|arm64)
    ;;
  armv7l)
    echo "[警告] 当前是 32 位 ARM 系统。Demo 可能编译，但建议改用 64 位 Raspberry Pi OS。"
    ;;
  *)
    echo "[警告] 当前不是常见树莓派 ARM 架构；请确认是否在目标设备上运行。"
    ;;
esac

if ! command -v apt-get >/dev/null 2>&1; then
  echo "[失败] 未找到 apt-get。本脚本面向 Raspberry Pi OS / Debian Bookworm。"
  exit 2
fi

if [[ "${SKIP_APT:-0}" != "1" ]]; then
  packages=(
    build-essential cmake ninja-build pkg-config git sqlite3
    qt6-base-dev qt6-declarative-dev qt6-tools-dev qt6-tools-dev-tools
    qml6-module-qtquick qml6-module-qtquick-window
    qml6-module-qtquick-controls qml6-module-qtquick-layouts
    qml6-module-qtquick-dialogs qml6-module-qtqml-workerscript
    libqt6sql6-sqlite fonts-noto-cjk
    libinput-tools evtest
  )

  echo
  echo "[1/4] 更新软件索引并安装 Qt 6、编译工具和运行依赖。"
  echo "这一步只下载系统软件包，与 PressureOS 的真实 Wi-Fi 功能无关。"
  sudo apt-get update
  sudo apt-get install -y "${packages[@]}"

  if apt-cache show qt6-wayland >/dev/null 2>&1; then
    sudo apt-get install -y qt6-wayland
  fi
else
  echo
  echo "[1/4] 已设置 SKIP_APT=1，跳过系统软件包安装。"
fi

echo
echo "[2/4] 检查关键工具。"
cmake --version | head -n 1
qmake6 -query QT_VERSION
ninja --version

echo
echo "[3/4] 以 Release 模式编译 PressureOS。"
chmod +x "$ROOT/scripts/build_raspberry_pi.sh"
"$ROOT/scripts/build_raspberry_pi.sh"

echo
echo "[4/4] 运行后端回归测试。"
ctest --test-dir "$ROOT/build-rpi" --output-on-failure

mkdir -p "$REVIEW_ROOT/data" "$REVIEW_ROOT/export"

echo
echo "============================================================"
echo "[成功] PressureOS 已完成编译和测试。"
echo "窗口模式：./scripts/run_raspberry_pi_demo.sh windowed"
echo "全屏模式：./scripts/run_raspberry_pi_demo.sh fullscreen"
echo "体验数据：$REVIEW_ROOT/data"
echo "导出文件：$REVIEW_ROOT/export"
echo "安装日志：$LOG_FILE"
echo "============================================================"

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
  echo "[提示] 当前终端没有图形会话变量。编译已完成，请在树莓派本机桌面终端启动界面。"
fi
