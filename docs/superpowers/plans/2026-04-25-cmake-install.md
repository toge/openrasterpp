# CMake Install/Export Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `openrasterpp` installable with `cmake --install` and consumable via `find_package(openrasterpp CONFIG REQUIRED)`.

**Architecture:** Add standard `GNUInstallDirs`-based install rules to the root CMake project, generate package config/export files with `CMakePackageConfigHelpers`, and verify the installed package with a downstream smoke-test consumer. Keep the source tree and library behavior unchanged; this is a packaging/installation change only.

**Tech Stack:** CMake 3.25, `GNUInstallDirs`, `CMakePackageConfigHelpers`, CTest/Catch2, POSIX shell scripts

---

## File map

- Modify: `CMakeLists.txt`
  - Add project version, install destinations, target export, package config generation, and install rules.
- Create: `cmake/openrasterppConfig.cmake.in`
  - Installed package config template that loads dependencies and exported targets.
- Create: `test/install_smoke/CMakeLists.txt`
  - Downstream consumer fixture used to prove the installed package can be found and linked.
- Create: `test/install_smoke/main.cpp`
  - Minimal consumer source that includes `openraster.hpp` and links against `openrasterpp::openrasterpp`.
- Modify: `test.sh`
  - Extend the existing test workflow to run the install smoke test after `ctest`.

### Task 1: Create the failing install smoke test

**Files:**
- Create: `test/install_smoke/CMakeLists.txt`
- Create: `test/install_smoke/main.cpp`
- Test: `test/install_smoke/CMakeLists.txt`

- [ ] **Step 1: Write the failing test fixture**

```cmake
cmake_minimum_required(VERSION 3.25)
project(openrasterpp_install_smoke LANGUAGES CXX)

find_package(openrasterpp CONFIG REQUIRED)

add_executable(install_smoke main.cpp)
target_link_libraries(install_smoke PRIVATE openrasterpp::openrasterpp)
target_compile_features(install_smoke PRIVATE cxx_std_26)
```

```cpp
#include <openraster.hpp>

auto main() -> int {
  auto const mode = ora::to_string(ora::BlendMode::SrcOver);
  return mode == "svg:src-over" ? 0 : 1;
}
```

- [ ] **Step 2: Run the smoke test to verify it fails**

Run:

```bash
ROOT_DIR="$PWD" && \
./build.sh && \
rm -rf "${ROOT_DIR}/build/install-prefix" "${ROOT_DIR}/build/install-smoke" && \
cmake --install "${ROOT_DIR}/build" --prefix "${ROOT_DIR}/build/install-prefix" && \
cmake -S "${ROOT_DIR}/test/install_smoke" -B "${ROOT_DIR}/build/install-smoke" \
  -DCMAKE_PREFIX_PATH="${ROOT_DIR}/build/install-prefix" \
  -DCMAKE_TOOLCHAIN_FILE="${ROOT_DIR}/build/conan_toolchain.cmake"
```

Expected: FAIL during the consumer configure step because `openrasterppConfig.cmake` / `openrasterppTargets.cmake` do not exist yet.

- [ ] **Step 3: Commit the failing test fixture**

```bash
git add test/install_smoke/CMakeLists.txt test/install_smoke/main.cpp
git commit -m "test: add install smoke fixture"
```

### Task 2: Implement install/export support

**Files:**
- Modify: `CMakeLists.txt`
- Create: `cmake/openrasterppConfig.cmake.in`
- Test: `test/install_smoke/CMakeLists.txt`

- [ ] **Step 1: Add the failing installation plumbing target to the plan**

Update the root project metadata and install/export wiring:

```cmake
project(openrasterpp VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(openrasterpp_install_cmakedir
    "${CMAKE_INSTALL_LIBDIR}/cmake/openrasterpp")
```

- [ ] **Step 2: Install the library and public header**

Add install rules that preserve the current build target while exporting it:

```cmake
install(TARGETS openrasterpp
  EXPORT openrasterppTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES openraster.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
```

Also keep the target include directories aligned with the installed layout:

```cmake
target_include_directories(openrasterpp PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
```

- [ ] **Step 3: Generate package config, version, and exported targets**

Create `cmake/openrasterppConfig.cmake.in`:

```cmake
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)
find_dependency(minizip-ng CONFIG REQUIRED)
find_dependency(lodepng CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/openrasterppTargets.cmake")
```

Generate and install the package metadata:

```cmake
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY ExactVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/openrasterppConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig.cmake"
  INSTALL_DESTINATION "${openrasterpp_install_cmakedir}"
)

install(EXPORT openrasterppTargets
  FILE openrasterppTargets.cmake
  NAMESPACE openrasterpp::
  DESTINATION "${openrasterpp_install_cmakedir}"
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfigVersion.cmake"
  DESTINATION "${openrasterpp_install_cmakedir}"
)
```

- [ ] **Step 4: Run the smoke test to verify it now passes**

Run:

```bash
ROOT_DIR="$PWD" && \
./build.sh && \
rm -rf "${ROOT_DIR}/build/install-prefix" "${ROOT_DIR}/build/install-smoke" && \
cmake --install "${ROOT_DIR}/build" --prefix "${ROOT_DIR}/build/install-prefix" && \
cmake -S "${ROOT_DIR}/test/install_smoke" -B "${ROOT_DIR}/build/install-smoke" \
  -DCMAKE_PREFIX_PATH="${ROOT_DIR}/build/install-prefix" \
  -DCMAKE_TOOLCHAIN_FILE="${ROOT_DIR}/build/conan_toolchain.cmake" && \
cmake --build "${ROOT_DIR}/build/install-smoke" --parallel 4
```

Expected: PASS. The consumer project configures successfully, finds `openrasterpp`, and builds `install_smoke`.

- [ ] **Step 5: Commit the install/export implementation**

```bash
git add CMakeLists.txt cmake/openrasterppConfig.cmake.in
git commit -m "feat: add cmake install export support"
```

### Task 3: Fold the smoke test into the existing test workflow

**Files:**
- Modify: `test.sh`
- Test: `test.sh`, `test/install_smoke/CMakeLists.txt`

- [ ] **Step 1: Extend `test.sh` to run the install smoke test after `ctest`**

Replace the current `cd build` flow with repo-root anchored commands and fail-fast shell behavior:

```sh
set -e

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
BUILD_DIR="${ROOT_DIR}/build"
PREFIX_DIR="${BUILD_DIR}/install-prefix"
SMOKE_BUILD_DIR="${BUILD_DIR}/install-smoke"

ctest --test-dir "${BUILD_DIR}" -V

rm -rf "${PREFIX_DIR}" "${SMOKE_BUILD_DIR}"
cmake --install "${BUILD_DIR}" --prefix "${PREFIX_DIR}"
cmake -S "${ROOT_DIR}/test/install_smoke" -B "${SMOKE_BUILD_DIR}" \
  -DCMAKE_PREFIX_PATH="${PREFIX_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${BUILD_DIR}/conan_toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "${SMOKE_BUILD_DIR}" --parallel 4
```

- [ ] **Step 2: Run the full local verification flow**

Run:

```bash
./build.sh && ./test.sh
```

Expected: PASS. Existing CTest cases still pass, installation completes, and the smoke-test consumer builds against the installed package.

- [ ] **Step 3: Spot-check installed outputs**

Run:

```bash
find "$PWD/build/install-prefix" -maxdepth 4 | sort
```

Expected to include:

```text
build/install-prefix/include/openraster.hpp
build/install-prefix/lib/cmake/openrasterpp/openrasterppConfig.cmake
build/install-prefix/lib/cmake/openrasterpp/openrasterppConfigVersion.cmake
build/install-prefix/lib/cmake/openrasterpp/openrasterppTargets.cmake
```

- [ ] **Step 4: Commit the test workflow update**

```bash
git add test.sh
git commit -m "test: verify installed cmake package"
```
