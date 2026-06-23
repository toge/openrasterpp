cmake_minimum_required(VERSION 3.25)
project(openrasterpp_install_smoke_libspng LANGUAGES CXX)

find_package(openrasterpp CONFIG REQUIRED COMPONENTS png-libspng)

add_executable(install_smoke_libspng main.cpp)
target_link_libraries(install_smoke_libspng PRIVATE openrasterpp::openrasterpp-png-libspng)
target_compile_features(install_smoke_libspng PRIVATE cxx_std_23)
