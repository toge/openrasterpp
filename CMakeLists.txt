cmake_minimum_required(VERSION 3.25)
project(openrasterpp VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# C++ Standard
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
find_package(minizip-ng CONFIG REQUIRED)
find_package(lodepng CONFIG REQUIRED)

# Library
add_library(openrasterpp openraster.cpp openraster.hpp)
target_compile_features(openrasterpp PUBLIC cxx_std_26)

set(openrasterpp_install_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/openrasterpp")

target_include_directories(openrasterpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# minizip-ng and lodepng linkage
target_link_libraries(openrasterpp PUBLIC
    MINIZIP::minizip-ng
    lodepng
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

install(
    TARGETS openrasterpp
    EXPORT openrasterppTargets
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
    EXPORT openrasterppTargets
    FILE openrasterppTargets.cmake
    NAMESPACE openrasterpp::
    DESTINATION "${openrasterpp_install_cmakedir}"
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/openrasterppConfigVersion.cmake"
    DESTINATION "${openrasterpp_install_cmakedir}"
)

# minizip-ng compatibility headers might be needed if they are not in the default path
# In some cases we might need to add the include directory explicitly
# target_include_directories(openrasterpp PRIVATE ${MINIZIP_INCLUDE_DIRS})

# Warnings and optimizations
if(MSVC)
    target_compile_options(openrasterpp PRIVATE /W4 /Ox)
else()
    target_compile_options(openrasterpp PRIVATE -Wall -Wextra -pedantic -O3 -march=native)
endif()

# Tests
enable_testing()
add_subdirectory(test)

# Link tests to the library
# (This is handled in test/CMakeLists.txt but we need to ensure the library is available)
