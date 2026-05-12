cmake_minimum_required(VERSION 3.25)
project(openrasterpp VERSION 0.1.0 LANGUAGES CXX)

option(ENABLE_TESTS "Enable tests" ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# C++ Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
find_package(minizip-ng CONFIG REQUIRED)
find_package(lodepng CONFIG REQUIRED)
find_package(SPNG CONFIG REQUIRED)

# Libraries
add_library(openrasterpp-core
    openraster_core.cpp
    openraster.hpp
    src/core/archive_xml_provider.hpp
)
target_compile_features(openrasterpp-core PUBLIC cxx_std_23)

add_library(openrasterpp-png-lodepng
    src/backends/lodepng_backend.cpp
    openraster_lodepng.hpp
)
target_compile_features(openrasterpp-png-lodepng PUBLIC cxx_std_23)

add_library(openrasterpp-png-libspng
    src/backends/libspng_backend.cpp
    openraster_libspng.hpp
)
target_compile_features(openrasterpp-png-libspng PUBLIC cxx_std_23)

set(openrasterpp_install_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/openrasterpp")

target_include_directories(openrasterpp-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_include_directories(openrasterpp-png-lodepng PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_include_directories(openrasterpp-png-libspng PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(openrasterpp-core PUBLIC
    MINIZIP::minizip-ng
)

target_link_libraries(openrasterpp-png-lodepng PUBLIC
    openrasterpp-core
    lodepng
)

target_link_libraries(openrasterpp-png-libspng PUBLIC
    openrasterpp-core
    spng::spng
)

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

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/openrasterppConfig-png-lodepng.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-lodepng.cmake"
    @ONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/openrasterppConfig-png-libspng.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-libspng.cmake"
    @ONLY
)

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

install(
    EXPORT openrasterppCoreTargets
    FILE openrasterppCoreTargets.cmake
    NAMESPACE openrasterpp::
    DESTINATION "${openrasterpp_install_cmakedir}"
)

install(
    EXPORT openrasterppPngLodepngTargets
    FILE openrasterppPngLodepngTargets.cmake
    NAMESPACE openrasterpp::
    DESTINATION "${openrasterpp_install_cmakedir}"
)

install(
    EXPORT openrasterppPngLibspngTargets
    FILE openrasterppPngLibspngTargets.cmake
    NAMESPACE openrasterpp::
    DESTINATION "${openrasterpp_install_cmakedir}"
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfigVersion.cmake"
    DESTINATION "${openrasterpp_install_cmakedir}"
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-lodepng.cmake"
    DESTINATION "${openrasterpp_install_cmakedir}"
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig-png-libspng.cmake"
    DESTINATION "${openrasterpp_install_cmakedir}"
)

# minizip-ng compatibility headers might be needed if they are not in the default path
# In some cases we might need to add the include directory explicitly
# target_include_directories(openrasterpp PRIVATE ${MINIZIP_INCLUDE_DIRS})

# Warnings and optimizations
if(MSVC)
    target_compile_options(openrasterpp-core PRIVATE /W4 /Ox)
    target_compile_options(openrasterpp-png-lodepng PRIVATE /W4 /Ox)
    target_compile_options(openrasterpp-png-libspng PRIVATE /W4 /Ox)
else()
    target_compile_options(openrasterpp-core PRIVATE -Wall -Wextra -pedantic -O3 -march=native)
    target_compile_options(openrasterpp-png-lodepng PRIVATE -Wall -Wextra -pedantic -O3 -march=native)
    target_compile_options(openrasterpp-png-libspng PRIVATE -Wall -Wextra -pedantic -O3 -march=native)
endif()

# Tests
if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
