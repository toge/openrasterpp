cmake_minimum_required(VERSION 3.25)
project(openrasterpp_install_smoke_stb LANGUAGES CXX)

find_package(openrasterpp CONFIG REQUIRED COMPONENTS png-stb)

if(DEFINED Stb_FOUND)
  message(FATAL_ERROR "Stb_FOUND should not be introduced by openrasterpp png-stb component")
endif()

if(DEFINED Stb_INCLUDE_DIR)
  message(FATAL_ERROR "Stb_INCLUDE_DIR should not be introduced by openrasterpp png-stb component")
endif()

if(NOT "${openrasterpp_png-stb_FOUND}" STREQUAL "TRUE")
  message(FATAL_ERROR "openrasterpp_png-stb_FOUND should be TRUE")
endif()

if(NOT openrasterpp_png_stb_FOUND)
  message(FATAL_ERROR "openrasterpp_png_stb_FOUND should be TRUE")
endif()

add_executable(install_smoke_stb main.cpp)
target_link_libraries(install_smoke_stb PRIVATE openrasterpp::openrasterpp-png-stb)
target_compile_features(install_smoke_stb PRIVATE cxx_std_26)
