cmake_minimum_required(VERSION 3.25)
project(openrasterpp_install_smoke LANGUAGES CXX)

find_package(openrasterpp CONFIG REQUIRED)

add_executable(install_smoke main.cpp)
target_link_libraries(install_smoke PRIVATE openrasterpp::openrasterpp)
target_compile_features(install_smoke PRIVATE cxx_std_26)
