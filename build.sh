#!/usr/bin/env bash
set -euo pipefail

CXX="${CXX:-g++}"

if ! command -v "$CXX" >/dev/null 2>&1; then
    echo "Error: compiler '$CXX' not found." >&2
    exit 1
fi

rm -f mash
"$CXX" -std=c++17 -O2 -Wall -Wextra -Wshadow main.cpp command.cpp -o mash
