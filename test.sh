#! /bin/sh
set -e

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
BUILD_DIR="${ROOT_DIR}/build"

ctest --test-dir "${BUILD_DIR}" -R 'install_smoke|install_package' -V
