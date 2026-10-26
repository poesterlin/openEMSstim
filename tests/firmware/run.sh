#!/bin/sh
# Builds and runs the host-side firmware tests (needs a C++11 compiler).
set -e
cd "$(dirname "$0")"
FW=../../arduino-openEMSstim
OUT="${TMPDIR:-/tmp}/openEMSstim-firmware-tests"
${CXX:-c++} -std=c++11 -Wall -Wextra -Wno-unused-parameter -I mock -I "$FW" \
	test_firmware.cpp "$FW/EMSSystem.cpp" "$FW/EMSChannel.cpp" "$FW/AD5252.cpp" -o "$OUT"
"$OUT"
