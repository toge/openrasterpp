cmake_minimum_required(VERSION 3.25)
project(openrasterpp_install_smoke_libpng LANGUAGES CXX)

find_package(openrasterpp CONFIG REQUIRED COMPONENTS png-libpng)

add_executable(install_smoke_libpng main.cpp)
target_link_libraries(install_smoke_libpng PRIVATE openrasterpp::openrasterpp-png-libpng)
target_compile_features(install_smoke_libpng PRIVATE cxx_std_26)
