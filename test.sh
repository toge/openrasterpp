#! /bin/sh
set -e

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
BUILD_DIR="${ROOT_DIR}/build"
PREFIX_DIR="${BUILD_DIR}/install-prefix"
SMOKE_CORE_BUILD_DIR="${BUILD_DIR}/install-smoke-core"
SMOKE_BACKEND_BUILD_DIR="${BUILD_DIR}/install-smoke"
SMOKE_LIBSPNG_BUILD_DIR="${BUILD_DIR}/install-smoke-libspng"
SMOKE_LIBPNG_BUILD_DIR="${BUILD_DIR}/install-smoke-libpng"
SMOKE_STB_BUILD_DIR="${BUILD_DIR}/install-smoke-stb"
DEPS_PREFIX="${ROOT_DIR}/vcpkg_installed/x64-linux"

if [ ! -d "${DEPS_PREFIX}" ]; then
    echo "Dependency prefix not found: ${DEPS_PREFIX}" >&2
    exit 1
fi

ctest --test-dir "${BUILD_DIR}" -V

rm -rf "${PREFIX_DIR}" \
    "${SMOKE_CORE_BUILD_DIR}" \
    "${SMOKE_BACKEND_BUILD_DIR}" \
    "${SMOKE_LIBSPNG_BUILD_DIR}" \
    "${SMOKE_LIBPNG_BUILD_DIR}" \
    "${SMOKE_STB_BUILD_DIR}"
cmake --install "${BUILD_DIR}" --prefix "${PREFIX_DIR}"

cmake -S "${ROOT_DIR}/test/install_smoke_core" -B "${SMOKE_CORE_BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${PREFIX_DIR};${DEPS_PREFIX}"
cmake -S "${ROOT_DIR}/test/install_smoke" -B "${SMOKE_BACKEND_BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${PREFIX_DIR};${DEPS_PREFIX}"
cmake -S "${ROOT_DIR}/test/install_smoke_libspng" -B "${SMOKE_LIBSPNG_BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${PREFIX_DIR};${DEPS_PREFIX}"
cmake -S "${ROOT_DIR}/test/install_smoke_libpng" -B "${SMOKE_LIBPNG_BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${PREFIX_DIR};${DEPS_PREFIX}"
cmake -S "${ROOT_DIR}/test/install_smoke_stb" -B "${SMOKE_STB_BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${PREFIX_DIR};${DEPS_PREFIX}"

cmake --build "${SMOKE_CORE_BUILD_DIR}" --parallel 4
cmake --build "${SMOKE_BACKEND_BUILD_DIR}" --parallel 4
cmake --build "${SMOKE_LIBSPNG_BUILD_DIR}" --parallel 4
cmake --build "${SMOKE_LIBPNG_BUILD_DIR}" --parallel 4
cmake --build "${SMOKE_STB_BUILD_DIR}" --parallel 4
