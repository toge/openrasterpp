# CMake install/export design for openrasterpp

## Problem

`openrasterpp` builds as a CMake library target, but it does not currently provide
installation rules. A downstream project cannot install the library with
`cmake --install` or consume it with `find_package(openrasterpp CONFIG REQUIRED)`.

## Goal

Add standard CMake install/export support so that:

1. `cmake --install <build-dir> --prefix <prefix>` installs the library and its public header.
2. A downstream CMake project can use `find_package(openrasterpp CONFIG REQUIRED)`.
3. The installed package exposes a namespaced imported target:
   `openrasterpp::openrasterpp`.

## Non-goals

1. Add `CPack` packaging or OS-specific installer generation.
2. Restructure the source tree beyond what is required for installation metadata.
3. Change library behavior, ABI policy, or dependency selection.

## Current state

1. The root `CMakeLists.txt` defines a single library target `openrasterpp`.
2. The project uses C++26 and links `minizip-ng` and `lodepng`.
3. There is no project version, no `GNUInstallDirs`, no `install()` rules, and no package config files.

## Proposed design

### 1. Project metadata

Add a project version in the root `project(...)` declaration so a package version
file can be generated. The authoritative version for this change will be
`1.0.0`, matching `vcpkg.json`. The Conan recipe can remain unchanged for this task.

### 2. Standard install layout

Use `GNUInstallDirs` and install to the standard locations:

| Artifact | Destination |
| --- | --- |
| library archive/shared object/import library | `${CMAKE_INSTALL_LIBDIR}` |
| runtime binary (if built as shared on Windows) | `${CMAKE_INSTALL_BINDIR}` |
| public header `openraster.hpp` | `${CMAKE_INSTALL_INCLUDEDIR}` |
| CMake package files | `${CMAKE_INSTALL_LIBDIR}/cmake/openrasterpp` |

### 3. Target export

Install the `openrasterpp` target with an export set and export it as:

`openrasterpp::openrasterpp`

This gives downstream users a stable, namespaced target name without changing the
in-tree target name.

### 4. Package config files

Generate and install:

1. `openrasterppConfig.cmake`
2. `openrasterppConfigVersion.cmake`
3. `openrasterppTargets.cmake`

`CMakePackageConfigHelpers` will be used to generate the config and version files.
The config template will include the exported targets and `find_dependency(...)`
entries for the package dependencies required by the installed target.
The version file will use `ExactVersion` compatibility so this change does not
introduce a new ABI-compatibility promise.

### 5. Consumer experience

After installation, downstream CMake code should look like this:

```cmake
find_package(openrasterpp CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE openrasterpp::openrasterpp)
```

### 6. Verification

Validation should confirm:

1. The project still configures and builds normally.
2. `cmake --install` places the header, library, and CMake package files under the chosen prefix.
3. A small consumer smoke test can configure, link, and build against
   `find_package(openrasterpp CONFIG REQUIRED)`.

## Implementation notes

1. Keep the existing build behavior intact.
2. Do not add `CPack`.
3. Keep `INSTALL_INTERFACE` include directories aligned with `${CMAKE_INSTALL_INCLUDEDIR}`.
4. Keep changes focused to the root CMake configuration plus one package config template under `cmake/`.
