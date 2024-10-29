#!/bin/bash

SDK_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
echo "SDK_DIR: ${SDK_DIR}"

MILKV_DUO_SDK=${SDK_DIR}/duo-sdk
TOOLCHAIN_DIR=${MILKV_DUO_SDK}/riscv64-linux-musl-x86_64

export TOOLCHAIN_PREFIX=${TOOLCHAIN_DIR}/bin/riscv64-unknown-linux-musl-
export SYSROOT=${MILKV_DUO_SDK}/rootfs
export CC=${TOOLCHAIN_PREFIX}gcc

export CFLAGS="-mcpu=c906fdv -march=rv64imafdcv0p7xthead -mcmodel=medany -mabi=lp64d"
# -Os
export LDFLAGS="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"

echo "SDK environment is ready"