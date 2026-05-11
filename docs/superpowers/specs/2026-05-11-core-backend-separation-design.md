# openrasterpp core/backend separation design

## Problem

The current `openrasterpp` package mixes three concerns in one target and one translation unit:

1. ORA domain model and high-level read/write flow
2. ZIP/package and `stack.xml` handling
3. PNG codec implementation through `lodepng`

That layout causes `lodepng` to leak through package linkage. The installed package config always resolves `lodepng`, and the exported `openrasterpp` target links it publicly. In static-link builds, that makes symbol ownership ambiguous when a consumer already links another library that embeds the same PNG implementation.

## Goals

- Split the package into a PNG-agnostic core target and one target per PNG backend.
- Keep runtime polymorphism out of the design. No `virtual`, vtables, or runtime backend registry.
- Use concept/policy boundaries so backend choice happens at build/link time.
- Keep PNG implementation headers out of the core public surface.
- Keep normal consumer usage simple through backend-specific facade APIs.
- Leave a clear path for future backends such as `spng` or custom codecs.

## Non-goals

- Supporting backend selection at runtime
- Preserving backend-agnostic convenience wrappers such as `ora::read(...)`
- Refactoring unrelated ORA model or blend logic

## Current leak points

### Public API surface

- `openraster.hpp` does not include `lodepng.h`, which is good.
- However, the default wrapper APIs (`ora::read`, `ora::write`, `ora::util::encode_png`, `ora::util::render_preview_and_thumbnail`) imply a built-in PNG backend and therefore a single monolithic library design.

### Target dependency surface

- `openrasterpp` currently links `MINIZIP::minizip-ng` and `lodepng` as `PUBLIC`.
- `cmake/openrasterppConfig.cmake.in` unconditionally calls `find_dependency(lodepng CONFIG REQUIRED)`.
- As a result, consumers that only want ORA model/core logic still inherit the PNG dependency.

### Implementation/layout surface

- `openraster.cpp` includes both minizip headers and `lodepng.h`.
- The default provider mixes ZIP, XML, and PNG codec responsibilities in one class.
- Core logic cannot be built or installed independently of `lodepng`.

## Chosen design

### 1. Target structure

- `openrasterpp-core`
  - Domain model, errors, blend helpers
  - ZIP/package handling
  - `stack.xml` serialization/deserialization
  - High-level template algorithms parameterized by policy/concept
- `openrasterpp-png-lodepng`
  - `lodepng` encode/decode implementation
  - Thin non-template facade APIs for normal consumers
  - Explicit instantiation of selected template entry points when useful

Future backends follow the same shape:

- `openrasterpp-png-spng`
- `openrasterpp-png-custom`

### 2. Policy boundary

Core defines the contracts, not the concrete PNG backend:

- `PngCodec` concept for `encode_png` / `decode_png`
- archive/XML support remains implemented by core
- the existing provider-style generic API remains available in core for advanced users and tests
- backend targets compose a core archive/XML provider with a codec policy that satisfies the same core concept surface

The codec is selected by the linked backend target, not by runtime switching.

### 3. Public API shape

Normal consumers use backend-specific facade headers:

```cpp
#include <openraster_lodepng.hpp>

auto doc = ora::lodepng::read("example.ora");
```

Facade functions stay non-template:

- `ora::lodepng::read`
- `ora::lodepng::write`
- `ora::lodepng::encode_png`
- `ora::lodepng::render_preview_and_thumbnail`

Advanced consumers may still use concept/policy-based APIs from core, but those are no longer the default path shown in docs.

### 3a. Existing provider-based API treatment

Keep these generic APIs in `openrasterpp-core`:

- `ora::read(provider, ...)`
- `ora::write(provider, ...)`
- `ora::util::encode_png(provider, ...)`
- `ora::util::render_preview_and_thumbnail(provider, ...)`

These signatures stay source-compatible. Backend facades use an internal adapter/composed policy that satisfies the same concept surface.

Remove only the backend-implicit wrappers that instantiate a built-in default provider:

- `ora::read(...)`
- `ora::write(...)`
- `ora::util::encode_png(...)`
- `ora::util::render_preview_and_thumbnail(...)`

This preserves the current extensibility model while removing the monolithic built-in backend assumption.

### 4. Compatibility stance

`ora::read(...)` and similar backend-implicit wrappers are removed rather than preserved. The migration path is deliberate and explicit:

- include backend header
- link backend target
- rename calls to backend namespace

This is a small source change, but it removes the dependency ambiguity that caused the redesign.

## File/layout direction

Representative end state:

```text
openraster.hpp                  # core public API and policy-based templates
openraster_core.cpp             # core non-template implementation
openraster_lodepng.hpp          # lodepng facade declarations
src/backends/lodepng.cpp        # lodepng codec + facade definitions
test/test_core.cpp              # provider/policy/core tests
test/test_lodepng_backend.cpp   # backend facade tests
```

Exact filenames may vary slightly to match repository style, but the dependency direction is fixed:

`core -> minizip-ng`

`lodepng backend -> core + lodepng`

Consumers choose one backend target explicitly.

## CMake/package design

### Targets

- Export `openrasterpp::openrasterpp-core`
- Export `openrasterpp::openrasterpp-png-lodepng`

### Components

Use package components so backend targets are loaded only when requested:

- `core`
- `png-lodepng`

Planned consumer shape:

```cmake
find_package(openrasterpp CONFIG REQUIRED COMPONENTS png-lodepng)
target_link_libraries(app PRIVATE openrasterpp::openrasterpp-png-lodepng)
```

The `png-lodepng` component loads the backend target file and resolves `lodepng`.
Core-only consumers can request `core` only and avoid any `lodepng` lookup.
If no component is specified, the package loads `core` only.

### Link interfaces

- `openrasterpp-core`
  - public: include directories, C++ standard requirements
  - private/public only for dependencies actually required by its headers
  - must not expose `lodepng`
- `openrasterpp-png-lodepng`
  - links `openrasterpp-core`
  - links `lodepng`
  - only this target exposes or resolves the lodepng dependency

### Package config

`openrasterppConfig.cmake` must stop unconditionally resolving backend-only dependencies. It should load core targets and dependencies by default, then load backend-specific target fragments only for requested components.

## Test plan

1. Core/provider tests continue to verify concept-based generic logic without any concrete PNG dependency.
2. Backend tests verify `openrasterpp-png-lodepng` facade read/write/render behavior.
3. Install smoke test links only `openrasterpp::openrasterpp-png-lodepng` for real end-to-end use.
4. Add a CMake-level assertion that `openrasterpp::openrasterpp-core` does not carry `lodepng` in its link interface.
5. Add a core-only package smoke path that requests `COMPONENTS core` and verifies configuration succeeds without `lodepng`.

## Migration guide

Before:

```cpp
#include <openraster.hpp>

auto doc = ora::read("example.ora");
```

After:

```cpp
#include <openraster_lodepng.hpp>

auto doc = ora::lodepng::read("example.ora");
```

Before:

```cmake
find_package(openrasterpp CONFIG REQUIRED)
target_link_libraries(app PRIVATE openrasterpp::openrasterpp)
```

After:

```cmake
find_package(openrasterpp CONFIG REQUIRED COMPONENTS png-lodepng)
target_link_libraries(app PRIVATE openrasterpp::openrasterpp-png-lodepng)
```

## Implementation notes for planning

- Preserve existing provider-friendly generic tests where possible.
- Avoid forcing backend implementation types into public headers.
- Prefer simple composition over CRTP or inheritance-heavy patterns.
- Keep the change surgical: separate targets and APIs without rewriting unrelated blend/XML logic.
- README updates cover: new target selection, new backend include/header, advanced provider-based API position, backend extension recipe.
