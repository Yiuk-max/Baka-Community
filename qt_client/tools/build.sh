#!/usr/bin/env bash
# Linux / macOS 构建脚本（Windows 用 tools/build.bat）
#   QT_PREFIX=~/Qt/6.11.1/gcc_64 tools/build.sh
# 依赖（Ubuntu）：
#   build-essential cmake ninja-build libprotobuf-dev protobuf-compiler libssl-dev
#   Qt 需要含 WebSockets 模块（aqt install-qt ... -m qtwebsockets）
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QT_PREFIX="${QT_PREFIX:-$HOME/Qt/6.11.1/gcc_64}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"

if [ ! -d "$QT_PREFIX" ]; then
    echo "找不到 Qt：$QT_PREFIX" >&2
    echo "用 QT_PREFIX=... 指定，或先安装：aqt install-qt linux desktop 6.11.1 linux_gcc_64 -m qtwebsockets -O ~/Qt" >&2
    exit 1
fi

if [ ! -f "$QT_PREFIX/lib/cmake/Qt6WebSockets/Qt6WebSocketsConfig.cmake" ]; then
    echo "警告：$QT_PREFIX 里没有 Qt WebSockets 模块，QWebSocket 会编译失败。" >&2
    echo "      补装：aqt install-qt linux desktop 6.11.1 linux_gcc_64 -m qtwebsockets -O ~/Qt" >&2
fi

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Release}" \
      -DCMAKE_PREFIX_PATH="$QT_PREFIX"

cmake --build "$BUILD_DIR" -j"${JOBS:-4}"

echo "OK -> $BUILD_DIR/baka_client"
echo "运行：$BUILD_DIR/baka_client"
