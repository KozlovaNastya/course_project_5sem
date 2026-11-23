#!/bin/sh
# Для Linux/macOS (проверка ядра без Photoshop).
cd "$(dirname "$0")/.." && g++ -std=c++17 -O1 -Wall -Wextra tests/test_core.cpp src/GaussianBlurCore.cpp -o tests/test_core && tests/test_core
