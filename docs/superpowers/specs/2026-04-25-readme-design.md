# Japanese README design for openrasterpp

## Problem

This repository does not have a Japanese `README.md` that explains what
`openrasterpp` does, how to build it, how to test it, and how to consume the
installed CMake package.

## Goal

Create a Japanese `README.md` for both users and developers that:

1. Explains the library purpose and core features.
2. Documents requirements and dependencies.
3. Shows how to build and test the project from source.
4. Shows how to install and consume the package with CMake.
5. Includes a short API usage example aligned with the current public header.

## Non-goals

1. Add separate developer-only documentation files.
2. Document CI, release automation, or packaging formats beyond current CMake install support.
3. Invent APIs or workflows not present in the repository.

## Current state

1. The project is a C++26 library target named `openrasterpp`.
2. It depends on `minizip-ng` and `lodepng`.
3. Tests use Catch2.
4. The project now supports CMake install/export and downstream usage with `find_package(openrasterpp CONFIG REQUIRED)`.

## Proposed README structure

1. Project overview
2. Main features
3. Requirements and dependencies
4. Build instructions
5. Test instructions
6. Install and downstream CMake usage
7. Short C++ usage example using `ora::ImageBuffer::make_blank`,
   `ora::OraDocument`, `ora::write`, and `ora::read`

## Content rules

1. Write all prose in Japanese.
2. Keep command examples practical and consistent with the current repository layout.
3. Use only verified build/test flows and existing public APIs.
4. Include a `find_package(openrasterpp CONFIG REQUIRED)` example with `target_link_libraries(... openrasterpp::openrasterpp)`.
5. Note that downstream CMake consumers must make both `openrasterpp` and its
   dependencies discoverable through `CMAKE_PREFIX_PATH` or equivalent package locations.

## Verification

Validation should confirm:

1. `README.md` exists at the repository root.
2. The documented build/test/package usage matches the current repository files.
3. The README examples do not contradict the current public header or CMake configuration.
