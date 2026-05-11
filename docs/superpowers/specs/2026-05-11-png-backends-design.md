# PNG backend expansion design

## Problem

`openrasterpp` currently ships only the `lodepng` PNG backend. We need equivalent backend-specific adaptors for `libspng`, `libpng`, and `stb` while preserving the existing split between backend-independent core logic and backend-specific PNG codec logic.

## Goals

- Add backend-specific public APIs for `libspng`, `libpng`, and `stb` that match the current `lodepng` surface:
  - `read()`
  - `write()`
  - `encode_png()`
  - `render_preview_and_thumbnail()`
- Keep `openrasterpp-core` free of direct PNG implementation dependencies.
- Package each backend as its own installable/exported CMake component.
- Keep third-party headers out of the public adaptor headers.
- Extend tests, install-smoke coverage, and documentation for all backends.

## Non-goals

- Introducing runtime backend selection.
- Refactoring the generic ORA/ZIP/XML pipeline.
- Changing the semantics of the existing `lodepng` backend.

## Recommended approach

Add three new backends that mirror the current `lodepng` structure instead of introducing a new codec abstraction layer. This keeps the change aligned with the existing architecture, limits risk, and preserves the per-backend packaging model already documented by the project.

## Architecture

### Public surface

Add three new public façade headers:

- `openraster_libspng.hpp`
- `openraster_libpng.hpp`
- `openraster_stb.hpp`

Each header will include only `openraster.hpp` and expose a namespace-scoped façade:

- `ora::libspng::*`
- `ora::libpng::*`
- `ora::stb::*`

Each façade will provide:

- `read(std::string_view)`
- `write(std::string_view, OraDocument const&)`
- `encode_png(ImageBuffer const&)`
- `render_preview_and_thumbnail(OraDocument&)`

### Backend implementation units

Add three backend translation units:

- `src/backends/libspng_backend.cpp`
- `src/backends/libpng_backend.cpp`
- `src/backends/stb_backend.cpp`

Each translation unit will define an internal provider class that:

- wraps `detail::ArchiveXmlProvider` for archive and XML operations
- implements `encode_png()` and `decode_png()` using a backend-specific PNG library
- instantiates the existing generic `ora::read()`, `ora::write()`, and `ora::util::*` templates
- exposes the backend façade functions in the matching namespace

This preserves the current model where ORA container handling is shared and only the PNG codec is swapped per backend.

## PNG codec choices

### libspng backend

Use `libspng` as a full read/write backend:

- decode with `spng_ctx_new()`, `spng_set_png_buffer()`, `spng_decoded_image_size()`, and `spng_decode_image(..., SPNG_FMT_RGBA8, ...)`
- encode with an encoder context, `SPNG_ENCODE_TO_BUFFER`, `spng_set_ihdr()`, `spng_encode_image()`, and `spng_get_png_buffer()`

The implementation will copy the encoded buffer into `std::vector<uint8_t>` before freeing the library-owned memory.

### libpng backend

Use libpng's simplified in-memory API:

- decode with `png_image_begin_read_from_memory()` and `png_image_finish_read()`
- encode with `png_image_write_to_memory()`

The backend will normalize to `PNG_FORMAT_RGBA` so the provider contract remains identical to the other backends. `png_image_free()` will be called on all exit paths after `png_image.version = PNG_IMAGE_VERSION`.

### stb backend

Use `stb_image` for decode and `stb_image_write` for encode so the backend matches the lodepng façade surface:

- decode with `stbi_load_from_memory(..., STBI_rgb_alpha)`
- encode with `stbi_write_png_to_func()` into a `std::vector<uint8_t>`

`STB_IMAGE_IMPLEMENTATION` and `STB_IMAGE_WRITE_IMPLEMENTATION` will be defined only inside `src/backends/stb_backend.cpp` so the public headers remain clean and the implementation stays in exactly one translation unit.

## Error handling

Reuse the existing error model:

- `Error::Code::PngDecodeFailed`
- `Error::Code::PngEncodeFailed`

Backend implementations will translate library-specific failures into these codes with backend-qualified messages, for example:

- `libspng decode failed: ...`
- `libpng encode failed: ...`
- `stb decode failed: ...`

No new error codes are required because the failure mode remains "PNG codec operation failed".

## Build and packaging changes

### Dependencies

Extend `vcpkg.json` with:

- `libspng`
- `libpng`
- `stb`

Dependency resolution will be explicit per backend instead of copying the `lodepng` pattern blindly:

- `libspng`: `find_package(SPNG CONFIG REQUIRED)` and link whichever exported target exists, using the vcpkg-supported pattern `$<IF:$<TARGET_EXISTS:spng::spng>,spng::spng,spng::spng_static>`
- `libpng`: `find_package(PNG REQUIRED)` and link `PNG::PNG`
- `stb`: treat as a build-time header-only dependency for `openrasterpp-png-stb`; resolve it with `find_package(Stb REQUIRED)` and consume `${Stb_INCLUDE_DIR}` via `target_include_directories()` instead of assuming an imported target, and do not model it as a consumer-side linked dependency

### CMake targets

Add three new libraries:

- `openrasterpp-png-libspng`
- `openrasterpp-png-libpng`
- `openrasterpp-png-stb`

Each target will:

- publish the project include directory like the existing backend
- link `openrasterpp-core`
- link the backend-specific dependency resolved through vcpkg/CMake
- be installed/exported separately

### Package components

Add three new package components:

- `png-libspng`
- `png-libpng`
- `png-stb`

`cmake/openrasterppConfig.cmake.in` will expand its supported component list and conditionally include new backend-specific config fragments:

- `cmake/openrasterppConfig-png-libspng.cmake.in`
- `cmake/openrasterppConfig-png-libpng.cmake.in`
- `cmake/openrasterppConfig-png-stb.cmake.in`

The backend config fragments will also be backend-specific in their dependency handling:

- `png-libspng`: `find_dependency(SPNG CONFIG REQUIRED)` and include exported targets that already encode the `spng::spng` versus `spng::spng_static` selection
- `png-libpng`: `find_dependency(PNG REQUIRED)`
- `png-stb`: no `find_dependency()` for stb because the backend is already compiled and the public headers do not expose stb types or headers

Each backend config fragment must also set the component FOUND variables expected by `check_required_components(openrasterpp)`, following the current `png-lodepng` pattern:

- `openrasterpp_png-libspng_FOUND` and `openrasterpp_png_libspng_FOUND`
- `openrasterpp_png-libpng_FOUND` and `openrasterpp_png_libpng_FOUND`
- `openrasterpp_png-stb_FOUND` and `openrasterpp_png_stb_FOUND`

`cmake/openrasterppConfig.cmake.in` will also need its supported-component list and conditional includes expanded for these three new components.

This keeps package resolution consistent with the current `core` and `png-lodepng` component model while accounting for the fact that the new dependencies do not share one uniform CMake integration story.

## Data flow

The runtime flow does not change:

1. A façade function creates a backend-specific provider.
2. The generic `ora::read()` / `ora::write()` / `ora::util::*` pipeline performs ORA archive and XML work.
3. The provider handles only PNG encode/decode operations.

This is the same division of responsibility already used by `lodepng`, extended to three additional codecs.

## Testing

Add backend-specific tests mirroring `test/test_lodepng_backend.cpp`:

- `test/test_libspng_backend.cpp`
- `test/test_libpng_backend.cpp`
- `test/test_stb_backend.cpp`

Each test file will cover:

- `encode_png()` returns a valid PNG header
- `render_preview_and_thumbnail()` populates preview assets
- `write()` then `read()` round-trips an ORA document
- invalid layer PNG bytes are rejected during both `read()` and `render_preview_and_thumbnail()` with `Error::Code::PngDecodeFailed`

Extend install-smoke coverage with separate consumer projects for each new backend so exported package/component usage is validated after `cmake --install`.

Extend the existing `install_smoke_core` check so the `core` component is also verified not to leak `libspng`, `libpng`, or `stb`. For `libspng` and `libpng`, the check should cover exported link dependencies/imported targets. For `stb`, the check should also cover interface include directories and package-resolution side effects, so a mistaken `core`-only `find_package(openrasterpp COMPONENTS core)` does not end up resolving stb-related variables or paths.

Add a dedicated `png-stb` install-smoke consumer that verifies consumer-side stb resolution is unnecessary: after `find_package(openrasterpp CONFIG REQUIRED COMPONENTS png-stb)`, the smoke project should be able to link and use `openrasterpp::openrasterpp-png-stb` without calling `find_package(Stb)`, and it should assert that `Stb_FOUND` / `Stb_INCLUDE_DIR` were not introduced as a side effect of resolving the `png-stb` component.

## Documentation

Update `README.md` to include:

- the expanded target/component list
- backend-specific usage examples for the new façades
- updated dependency/backend documentation
- backend extension guidance that reflects multiple shipped backends

## Trade-off summary

This approach accepts some duplication across backend translation units in exchange for:

- minimal risk to existing generic logic
- clear isolation between backends
- easy-to-understand packaging
- straightforward future addition of more PNG codecs

The main alternative was introducing a new shared PNG codec abstraction layer first, but that would increase refactoring scope without providing enough value for this task.
