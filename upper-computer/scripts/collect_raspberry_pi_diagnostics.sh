#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REVIEW_ROOT="${PRESSUREOS_REVIEW_ROOT:-$HOME/pressureos-review}"
OUT_DIR="$REVIEW_ROOT/diagnostics"
OUT_FILE="$OUT_DIR/PressureOS_diagnostics_$(date +%Y%m%d_%H%M%S).txt"
mkdir -p "$OUT_DIR"

run_if_available() {
  local command_name="$1"
  shift
  if command -v "$command_name" >/dev/null 2>&1; then
    "$command_name" "$@"
  else
    echo "$command_name: not installed"
  fi
}

{
  echo "PressureOS Raspberry Pi diagnostics"
  echo "Generated: $(date --iso-8601=seconds 2>/dev/null || date)"
  echo "Project: $ROOT"
  echo

  echo "===== OS / ARCH ====="
  uname -a
  [[ -r /etc/os-release ]] && cat /etc/os-release
  echo

  echo "===== SESSION ====="
  echo "DISPLAY=${DISPLAY:-<empty>}"
  echo "WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-<empty>}"
  echo "XDG_SESSION_TYPE=${XDG_SESSION_TYPE:-<empty>}"
  echo "XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-<empty>}"
  echo

  echo "===== TOOLCHAIN ====="
  run_if_available cmake --version
  run_if_available qmake6 -query QT_VERSION
  run_if_available ninja --version
  echo

  echo "===== MEMORY / STORAGE ====="
  run_if_available free -h
  run_if_available df -h "$HOME"
  echo

  echo "===== DISPLAY ====="
  run_if_available wlr-randr
  echo

  echo "===== USB ====="
  run_if_available lsusb
  echo

  echo "===== BUILD ====="
  if [[ -x "$ROOT/build-rpi/pressureos" ]]; then
    ls -lh "$ROOT/build-rpi/pressureos"
    run_if_available ldd "$ROOT/build-rpi/pressureos"
  else
    echo "build-rpi/pressureos: missing"
  fi
  echo

  echo "===== CTEST ====="
  if [[ -d "$ROOT/build-rpi" ]] && command -v ctest >/dev/null 2>&1; then
    ctest --test-dir "$ROOT/build-rpi" --output-on-failure
  else
    echo "CTest skipped: build directory or ctest is missing"
  fi
  echo

  echo "===== RECENT PRESSUREOS LOGS ====="
  if [[ -d "$REVIEW_ROOT/logs" ]]; then
    find "$REVIEW_ROOT/logs" -maxdepth 1 -type f -printf '%TY-%Tm-%Td %TH:%TM %p\n' 2>/dev/null | sort
  else
    echo "No review log directory"
  fi
  echo

  echo "===== USER SERVICE LOG ====="
  if command -v journalctl >/dev/null 2>&1; then
    journalctl --user -u pressureos.service -n 80 --no-pager 2>&1 || true
  fi
} >"$OUT_FILE" 2>&1

echo "诊断信息已生成：$OUT_FILE"
echo "请把这个 txt 文件和问题照片/视频一起发给软件负责人。"
