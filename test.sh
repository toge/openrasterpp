#! /bin/sh
set -e

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
BUILD_DIR="${ROOT_DIR}/build"
PREFIX_DIR="${BUILD_DIR}/install-prefix"
SMOKE_BUILD_DIR="${BUILD_DIR}/install-smoke"
DEPS_PREFIX="${ROOT_DIR}/vcpkg_installed/x64-linux"

if [ ! -d "${DEPS_PREFIX}" ]; then
    echo "Dependency prefix not found: ${DEPS_PREFIX}" >&2
    exit 1
fi

ctest --test-dir "${BUILD_DIR}" -V

rm -rf "${PREFIX_DIR}" "${SMOKE_BUILD_DIR}"
cmake --install "${BUILD_DIR}" --prefix "${PREFIX_DIR}"
cmake -S "${ROOT_DIR}/test/install_smoke" -B "${SMOKE_BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${PREFIX_DIR};${DEPS_PREFIX}"
cmake --build "${SMOKE_BUILD_DIR}" --parallel 4
