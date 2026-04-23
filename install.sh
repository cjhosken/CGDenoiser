#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
cd "$SCRIPT_DIR"

# Defaults
ENABLE_OPTIX=ON
ENABLE_OIDN=ON

OIDN_CPU=ON
OIDN_CUDA=OFF
OIDN_SYCL=OFF
OIDN_METAL=OFF
OIDN_HIP=OFF

NUKE_DIR=""
BUILD_DIR="./build"
INSTALL_DIR="./install"

# Parse args
for arg in "$@"; do
  case $arg in
    --optix) ENABLE_OPTIX=ON ;;
    --no-optix) ENABLE_OPTIX=OFF ;;

    --oidn) ENABLE_OIDN=ON ;;
    --no-oidn) ENABLE_OIDN=OFF ;;

    --cpu) OIDN_CPU=ON ;;
    --no-cpu) OIDN_CPU=OFF ;;

    --cuda) OIDN_CUDA=ON ;;
    --no-cuda) OIDN_CUDA=OFF ;;

    --sycl) OIDN_SYCL=ON ;;
    --no-sycl) OIDN_SYCL=OFF ;;

    --metal) OIDN_METAL=ON ;;
    --no-metal) OIDN_METAL=OFF ;;

    --hip) OIDN_HIP=ON ;;
    --no-hip) OIDN_HIP=OFF ;;

    --nuke-dir=*)
      NUKE_DIR="${arg#*=}"
      ;;

    --build-dir=*)
      BUILD_DIR="${arg#*=}"
      ;;

    --install-dir=*)
      INSTALL_DIR="${arg#*=}"
      ;;

    *)
      echo "Unknown option: $arg"
      exit 1
      ;;
  esac
done

# Validate required input
if [[ -z "$NUKE_DIR" ]]; then
  echo "Error: --nuke-dir= is required"
  exit 1
fi

# Prepare dirs
mkdir -p "$BUILD_DIR"
mkdir -p "$INSTALL_DIR"

# Detect parallelism
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
export CMAKE_BUILD_PARALLEL_LEVEL="$JOBS"

# Configure
cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
  -DNuke_ROOT="$NUKE_DIR" \
  -DENABLE_OPTIX="$ENABLE_OPTIX" \
  -DENABLE_OIDN="$ENABLE_OIDN" \
  -DCPU="$OIDN_CPU" \
  -DCUDA="$OIDN_CUDA" \
  -DSYCL="$OIDN_SYCL" \
  -DMETAL="$OIDN_METAL" \
  -DHIP="$OIDN_HIP" \
  -Wno-dev

# Build + install
cmake --build "$BUILD_DIR" --target install