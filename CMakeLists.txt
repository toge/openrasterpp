cmake_minimum_required(VERSION 3.25)
project(openrasterpp VERSION 0.1.0 LANGUAGES CXX)

# WASI では lodepng C API ラッパー（C ソース）をビルドするため C を有効化。
# 既存環境では CXX のみで変化なし。
if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
  enable_language(C)
endif()

option(BUILD_TEST "Enable tests" ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# C++ Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
# WASI (wasip1) ではスレッド・例外・一部依存が使えないため、検出して既定値を切り替える。
# 既存の Linux/macOS/Windows 環境の動作は変えない。
if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
  set(OPENRASTERPP_WASI ON)
else()
  set(OPENRASTERPP_WASI OFF)
endif()

# WASI では libpng/libspng/fpng を既定 OFF（libpng は zlib+setjmp 系、fpng は mutex で問題が出る）。
# lodepng/stb はヘッダ主体で WASI 親和性が高い。必要なら -D で明示 ON 可能。
option(OPENRASTERPP_ENABLE_LODEPNG "Enable lodepng backend" ON)
option(OPENRASTERPP_ENABLE_LIBSPNG "Enable libspng backend" ON)
option(OPENRASTERPP_ENABLE_LIBPNG "Enable libpng backend" ON)
option(OPENRASTERPP_ENABLE_STB "Enable stb_image backend" ON)
option(OPENRASTERPP_ENABLE_FPNG "Enable fpng backend" ON)
if(OPENRASTERPP_WASI)
  set(OPENRASTERPP_ENABLE_LIBSPNG OFF CACHE BOOL "Enable libspng backend" FORCE)
  set(OPENRASTERPP_ENABLE_LIBPNG OFF CACHE BOOL "Enable libpng backend" FORCE)
  set(OPENRASTERPP_ENABLE_FPNG OFF CACHE BOOL "Enable fpng backend" FORCE)
endif()

# WASI では Threads 検出を行わない（pthread 前提の -pthread リンクを避ける）。
if(OPENRASTERPP_WASI)
  set(CMAKE_THREAD_LIBS_INIT "" CACHE STRING "" FORCE)
  set(THREADS_FOUND TRUE CACHE BOOL "" FORCE)
  set(Threads_FOUND TRUE CACHE BOOL "" FORCE)
  if(NOT TARGET Threads::Threads)
    add_library(Threads::Threads INTERFACE IMPORTED)
  endif()
else()
# Workaround: FindThreads compile-test may fail on older toolchains (e.g. clang 19 / Fedora 41).
# Probe QUIET; on failure, force-cache the result so find_dependency() from downstream configs skips re-detection.
find_package(Threads QUIET)
if(NOT Threads_FOUND)
  set(CMAKE_THREAD_LIBS_INIT "-pthread" CACHE STRING "" FORCE)
  set(THREADS_FOUND TRUE CACHE BOOL "" FORCE)
  set(Threads_FOUND TRUE CACHE BOOL "" FORCE)
  add_library(Threads::Threads INTERFACE IMPORTED)
  set_target_properties(Threads::Threads PROPERTIES
    INTERFACE_LINK_LIBRARIES "-pthread"
  )
endif()
endif()

# WASI 用の ZIP 実装選択: 内蔵実装（zlib 直利用）のため minizip 不要。
# 既存環境では従来通り minizip-ng を使う。
if(NOT OPENRASTERPP_WASI)
  find_package(minizip-ng CONFIG REQUIRED)
endif()

find_package(FastFloat CONFIG REQUIRED)

if(OPENRASTERPP_ENABLE_LODEPNG)
    find_package(lodepng CONFIG REQUIRED)
endif()

if(OPENRASTERPP_ENABLE_LIBSPNG)
    find_package(SPNG CONFIG REQUIRED)
endif()

if(OPENRASTERPP_ENABLE_LIBPNG)
    find_package(PNG REQUIRED)
endif()

if(OPENRASTERPP_ENABLE_STB)
    find_package(Stb REQUIRED)
endif()

if(OPENRASTERPP_ENABLE_FPNG)
    find_package(unofficial-fpng CONFIG REQUIRED)
endif()

# Libraries
add_library(openrasterpp-core
    openraster_core.cpp
    openraster.hpp
    src/core/archive_xml_provider.hpp
)
if(OPENRASTERPP_WASI)
    # WASI 内蔵 ZIP 実装。zlib 直利用のため ZLIB を探す。
    # target 名は環境差あり（ZLIB::ZLIB / ZLIB::ZLIBSTATIC）。
    target_sources(openrasterpp-core PRIVATE src/core/zip_wasi.cpp)
    find_package(ZLIB REQUIRED)
    if(TARGET ZLIB::ZLIB)
        target_link_libraries(openrasterpp-core PUBLIC ZLIB::ZLIB)
    else()
        target_link_libraries(openrasterpp-core PUBLIC ZLIB::ZLIBSTATIC)
    endif()
endif()
target_compile_features(openrasterpp-core PUBLIC cxx_std_23)

if(OPENRASTERPP_ENABLE_LODEPNG)
    add_library(openrasterpp-png-lodepng
        src/backends/lodepng_backend.cpp
        openraster_lodepng.hpp
    )
    target_compile_features(openrasterpp-png-lodepng PUBLIC cxx_std_23)
    if(OPENRASTERPP_WASI)
        # C リンケージのラッパーを同梱（lodepng-c 実体への橋渡し）。
        target_sources(openrasterpp-png-lodepng PRIVATE src/backends/lodepng_c_wrap.c)
    endif()
endif()

if(OPENRASTERPP_ENABLE_LIBSPNG)
    add_library(openrasterpp-png-libspng
        src/backends/libspng_backend.cpp
        openraster_libspng.hpp
    )
    target_compile_features(openrasterpp-png-libspng PUBLIC cxx_std_23)
endif()

if(OPENRASTERPP_ENABLE_LIBPNG)
    add_library(openrasterpp-png-libpng
        src/backends/libpng_backend.cpp
        openraster_libpng.hpp
    )
    target_compile_features(openrasterpp-png-libpng PUBLIC cxx_std_23)
endif()

if(OPENRASTERPP_ENABLE_STB)
    add_library(openrasterpp-png-stb
        src/backends/stb_backend.cpp
        openraster_stb.hpp
    )
    target_compile_features(openrasterpp-png-stb PUBLIC cxx_std_23)
endif()

if(OPENRASTERPP_ENABLE_FPNG)
    add_library(openrasterpp-png-fpng
        src/backends/fpng_backend.cpp
        openraster_fpng.hpp
    )
    target_compile_features(openrasterpp-png-fpng PUBLIC cxx_std_23)
endif()

set(openrasterpp_install_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/openrasterpp")

target_include_directories(openrasterpp-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(OPENRASTERPP_ENABLE_LODEPNG)
    target_include_directories(openrasterpp-png-lodepng PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
endif()

if(OPENRASTERPP_ENABLE_LIBSPNG)
    target_include_directories(openrasterpp-png-libspng PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
endif()

if(OPENRASTERPP_ENABLE_LIBPNG)
    target_include_directories(openrasterpp-png-libpng PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
endif()

if(OPENRASTERPP_ENABLE_STB)
    target_include_directories(openrasterpp-png-stb PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_include_directories(openrasterpp-png-stb PRIVATE ${Stb_INCLUDE_DIR})
endif()

if(OPENRASTERPP_ENABLE_FPNG)
    target_include_directories(openrasterpp-png-fpng PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
endif()

if(NOT OPENRASTERPP_WASI)
  # 既存環境は従来通り minizip-ng。WASI は内蔵 ZIP 実装のため不要。
  target_link_libraries(openrasterpp-core PUBLIC
      MINIZIP::minizip-ng
  )
endif()

# 数値パースに fast_float を使う（ヘッダオンリー、例外なし）。
# 実体は openraster_core.cpp 側に閉じるため PRIVATE でよい。
target_link_libraries(openrasterpp-core PRIVATE
    FastFloat::fast_float
)

if(OPENRASTERPP_ENABLE_LODEPNG)
  if(OPENRASTERPP_WASI)
    # WASI では例外なしで使える C API 版を使う（C++ 版は __cxa_throw を参照する）。
    find_package(lodepng-c CONFIG REQUIRED)
    target_link_libraries(openrasterpp-png-lodepng PUBLIC
        openrasterpp-core
        lodepng-c
    )
  else()
    target_link_libraries(openrasterpp-png-lodepng PUBLIC
        openrasterpp-core
        lodepng
    )
  endif()
endif()

if(OPENRASTERPP_ENABLE_LIBSPNG)
    target_link_libraries(openrasterpp-png-libspng PUBLIC
        openrasterpp-core
        $<IF:$<TARGET_EXISTS:spng::spng>,spng::spng,spng::spng_static>
    )
endif()


if(OPENRASTERPP_ENABLE_LIBPNG)
    target_link_libraries(openrasterpp-png-libpng PUBLIC
        openrasterpp-core
        PNG::PNG
    )
endif()

if(OPENRASTERPP_ENABLE_STB)
    target_link_libraries(openrasterpp-png-stb PUBLIC
        openrasterpp-core
    )
endif()

if(OPENRASTERPP_ENABLE_FPNG)
    target_link_libraries(openrasterpp-png-fpng PUBLIC
        openrasterpp-core
        unofficial::fpng::fpng
    )
endif()

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

if(OPENRASTERPP_ENABLE_LODEPNG)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/openrasterppConfig-png-lodepng.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-lodepng.cmake"
        @ONLY
    )
endif()

if(OPENRASTERPP_ENABLE_LIBSPNG)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/openrasterppConfig-png-libspng.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-libspng.cmake"
        @ONLY
    )
endif()

if(OPENRASTERPP_ENABLE_LIBPNG)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/openrasterppConfig-png-libpng.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-libpng.cmake"
        @ONLY
    )
endif()

if(OPENRASTERPP_ENABLE_STB)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/openrasterppConfig-png-stb.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-stb.cmake"
        @ONLY
    )
endif()

if(OPENRASTERPP_ENABLE_FPNG)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/openrasterppConfig-png-fpng.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-fpng.cmake"
        @ONLY
    )
endif()

install(
    TARGETS openrasterpp-core
    EXPORT openrasterppCoreTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    FILES openraster.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(OPENRASTERPP_ENABLE_LODEPNG)
    install(
        TARGETS openrasterpp-png-lodepng
        EXPORT openrasterppPngLodepngTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
        FILES openraster_lodepng.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(OPENRASTERPP_ENABLE_LIBSPNG)
    install(
        TARGETS openrasterpp-png-libspng
        EXPORT openrasterppPngLibspngTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
        FILES openraster_libspng.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(OPENRASTERPP_ENABLE_LIBPNG)
    install(
        TARGETS openrasterpp-png-libpng
        EXPORT openrasterppPngLibpngTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
        FILES openraster_libpng.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(OPENRASTERPP_ENABLE_STB)
    install(
        TARGETS openrasterpp-png-stb
        EXPORT openrasterppPngStbTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
        FILES openraster_stb.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(OPENRASTERPP_ENABLE_FPNG)
    install(
        TARGETS openrasterpp-png-fpng
        EXPORT openrasterppPngFpngTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
        FILES openraster_fpng.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

install(
    EXPORT openrasterppCoreTargets
    FILE openrasterppCoreTargets.cmake
    NAMESPACE openrasterpp::
    DESTINATION "${openrasterpp_install_cmakedir}"
)

if(OPENRASTERPP_ENABLE_LODEPNG)
    install(
        EXPORT openrasterppPngLodepngTargets
        FILE openrasterppPngLodepngTargets.cmake
        NAMESPACE openrasterpp::
        DESTINATION "${openrasterpp_install_cmakedir}"
    )
endif()

if(OPENRASTERPP_ENABLE_LIBSPNG)
    install(
        EXPORT openrasterppPngLibspngTargets
        FILE openrasterppPngLibspngTargets.cmake
        NAMESPACE openrasterpp::
        DESTINATION "${openrasterpp_install_cmakedir}"
    )
endif()

if(OPENRASTERPP_ENABLE_LIBPNG)
    install(
        EXPORT openrasterppPngLibpngTargets
        FILE openrasterppPngLibpngTargets.cmake
        NAMESPACE openrasterpp::
        DESTINATION "${openrasterpp_install_cmakedir}"
    )
endif()

if(OPENRASTERPP_ENABLE_STB)
    install(
        EXPORT openrasterppPngStbTargets
        FILE openrasterppPngStbTargets.cmake
        NAMESPACE openrasterpp::
        DESTINATION "${openrasterpp_install_cmakedir}"
    )
endif()

if(OPENRASTERPP_ENABLE_FPNG)
    install(
        EXPORT openrasterppPngFpngTargets
        FILE openrasterppPngFpngTargets.cmake
        NAMESPACE openrasterpp::
        DESTINATION "${openrasterpp_install_cmakedir}"
    )
endif()

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfigVersion.cmake"
    DESTINATION "${openrasterpp_install_cmakedir}"
)

if(OPENRASTERPP_ENABLE_LODEPNG)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-lodepng.cmake"
        DESTINATION "${openrasterpp_install_cmakedir}"
    )
endif()

if(OPENRASTERPP_ENABLE_LIBSPNG)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-libspng.cmake"
        DESTINATION "${openrasterpp_install_cmakedir}"
    )
endif()

if(OPENRASTERPP_ENABLE_LIBPNG)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-libpng.cmake"
        DESTINATION "${openrasterpp_install_cmakedir}"
    )
endif()

if(OPENRASTERPP_ENABLE_STB)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-stb.cmake"
        DESTINATION "${openrasterpp_install_cmakedir}"
    )
endif()

if(OPENRASTERPP_ENABLE_FPNG)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-fpng.cmake"
        DESTINATION "${openrasterpp_install_cmakedir}"
    )
endif()

# minizip-ng compatibility headers might be needed if they are not in the default path
# In some cases we might need to add the include directory explicitly
# target_include_directories(openrasterpp PRIVATE ${MINIZIP_INCLUDE_DIRS})

# Warnings and optimizations
if(MSVC)
    target_compile_options(openrasterpp-core PRIVATE /W4 /Ox)
    if(OPENRASTERPP_ENABLE_LODEPNG)
        target_compile_options(openrasterpp-png-lodepng PRIVATE /W4 /Ox)
    endif()
    if(OPENRASTERPP_ENABLE_LIBSPNG)
        target_compile_options(openrasterpp-png-libspng PRIVATE /W4 /Ox)
    endif()
    if(OPENRASTERPP_ENABLE_LIBPNG)
        target_compile_options(openrasterpp-png-libpng PRIVATE /W4 /Ox)
    endif()
    if(OPENRASTERPP_ENABLE_STB)
        target_compile_options(openrasterpp-png-stb PRIVATE /W4 /Ox)
    endif()
    if(OPENRASTERPP_ENABLE_FPNG)
        target_compile_options(openrasterpp-png-fpng PRIVATE /W4 /Ox)
    endif()
else()
    # WASI では -march=native が未対応のため外す。既存環境は従来通り。
    if(OPENRASTERPP_WASI)
      set(OPENRASTERPP_OPT_FLAGS -Wall -Wextra -pedantic -O3)
    else()
      set(OPENRASTERPP_OPT_FLAGS -Wall -Wextra -pedantic -O3 -march=native)
    endif()
    if(OPENRASTERPP_WASI)
      # WASI/wasmedge では C++ 例外が使えないため、ライブラリ全体を例外なしで統一する。
      # STL 内部の throw 参照も消える。既存環境には影響しない。
      set(OPENRASTERPP_WASI_FLAGS -fno-exceptions)
    else()
      set(OPENRASTERPP_WASI_FLAGS)
    endif()
    target_compile_options(openrasterpp-core PRIVATE ${OPENRASTERPP_OPT_FLAGS} ${OPENRASTERPP_WASI_FLAGS})
    if(OPENRASTERPP_ENABLE_LODEPNG)
        target_compile_options(openrasterpp-png-lodepng PRIVATE ${OPENRASTERPP_OPT_FLAGS} ${OPENRASTERPP_WASI_FLAGS})
    endif()
    if(OPENRASTERPP_ENABLE_LIBSPNG)
        target_compile_options(openrasterpp-png-libspng PRIVATE ${OPENRASTERPP_OPT_FLAGS} ${OPENRASTERPP_WASI_FLAGS})
    endif()
    if(OPENRASTERPP_ENABLE_LIBPNG)
        target_compile_options(openrasterpp-png-libpng PRIVATE ${OPENRASTERPP_OPT_FLAGS} ${OPENRASTERPP_WASI_FLAGS})
    endif()
    if(OPENRASTERPP_ENABLE_STB)
        target_compile_options(openrasterpp-png-stb PRIVATE ${OPENRASTERPP_OPT_FLAGS} ${OPENRASTERPP_WASI_FLAGS})
    endif()
    if(OPENRASTERPP_ENABLE_FPNG)
        target_compile_options(openrasterpp-png-fpng PRIVATE ${OPENRASTERPP_OPT_FLAGS} ${OPENRASTERPP_WASI_FLAGS})
    endif()
endif()


# Tests
if(BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif()
