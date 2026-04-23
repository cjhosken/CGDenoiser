#!/bin/bash
export SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
cd "$SCRIPT_DIR"

BUILD_DIR="$SCRIPT_DIR/../build"
INSTALL_DIR="$SCRIPT_DIR/../lib"

ENABLE_OPTIX=OFF
ENABLE_OIDN=ON
ENABLE_CPU=ON
ENABLE_CUDA=OFF
ENABLE_SYCL=OFF
ENABLE_HIP=OFF
ENABLE_METAL=OFF

OIDN_VERSION="2.4.1"
OPTIX_VERSION="9.1.0"
TBB_VERSION="2021.13.0"
ISPC_VERSION="1.30.0"

for arg in "$@"; do
  case $arg in
    --optix) ENABLE_OPTIX=ON ;;
    --no-optix) ENABLE_OPTIX=OFF ;;

    --oidn) ENABLE_OIDN=ON ;;
    --no-oidn) ENABLE_OIDN=OFF ;;

    --cpu) ENABLE_CPU=ON ;;
    --no-cpu) ENABLE_CPU=OFF ;;

    --cuda) ENABLE_CUDA=ON ;;
    --no-cuda) ENABLE_CUDA=OFF ;;

    --sycl) ENABLE_SYCL=ON ;;
    --no-sycl) ENABLE_SYCL=OFF ;;

    --hip) ENABLE_HIP=ON ;;
    --no-hip) ENABLE_HIP=OFF ;;

    --metal) ENABLE_METAL=ON ;;
    --no-metal) ENABLE_METAL=OFF ;;

    --oidn-version=*) OIDN_VERSION="${arg#*=}" ;;
    --optix-version=*) OPTIX_VERSION="${arg#*=}" ;;
    --tbb-version=*) TBB_VERSION="${arg#*=}" ;;
    --ispc-version=*) ISPC_VERSION="${arg#*=}" ;;

    --build-dir=*) BUILD_DIR="${arg#*=}" ;;
    --install-dir=*) INSTALL_DIR="${arg#*=}" ;;

    *)
      echo "Unknown option: $arg"
      exit 1
      ;;
  esac
done

mkdir -p "$BUILD_DIR"
mkdir -p "$INSTALL_DIR/bin"

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DENABLE_OPTIX=$ENABLE_OPTIX  \
    -DENABLE_OIDN=$ENABLE_OIDN \
    -DENABLE_CPU=$ENABLE_CPU \
    -DENABLE_CUDA=$ENABLE_CUDA \
    -DENABLE_SYCL=$ENABLE_SYCL \
    -DENABLE_HIP=$ENABLE_HIP \
    -DENABLE_METAL=$ENABLE_METAL \
    -DOIDN_VERSION="$OIDN_VERSION" \
    -DOPTIX_VERSION="$OPTIX_VERSION" \
    -DTBB_VERSION="$TBB_VERSION" \
    -Wno-dev

cmake --build "$BUILD_DIR" --config Release -j $(nproc)