cmake_minimum_required(VERSION 3.25)
project(openrasterpp_install_smoke_core LANGUAGES CXX)

find_package(openrasterpp CONFIG REQUIRED COMPONENTS core)

add_executable(install_smoke_core main.cpp)
target_link_libraries(install_smoke_core PRIVATE openrasterpp::openrasterpp-core)
target_compile_features(install_smoke_core PRIVATE cxx_std_26)

get_target_property(core_links openrasterpp::openrasterpp-core INTERFACE_LINK_LIBRARIES)
if(core_links MATCHES "lodepng")
  message(FATAL_ERROR "openrasterpp-core must not leak lodepng")
endif()

if(TARGET lodepng)
  message(FATAL_ERROR "core component must not resolve lodepng")
endif()
