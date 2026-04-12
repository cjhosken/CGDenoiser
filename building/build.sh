#!/bin/bash
export SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
cd "$SCRIPT_DIR"

mkdir -p $SCRIPT_DIR/../lib/bin

cmake -S . -B ../build

cmake --build ../build --config Release -j 16