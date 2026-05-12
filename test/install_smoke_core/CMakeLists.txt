cmake_minimum_required(VERSION 3.25)
project(openrasterpp_install_smoke_core LANGUAGES CXX)

find_package(openrasterpp CONFIG REQUIRED COMPONENTS core)

add_executable(install_smoke_core main.cpp)
target_link_libraries(install_smoke_core PRIVATE openrasterpp::openrasterpp-core)
target_compile_features(install_smoke_core PRIVATE cxx_std_26)

get_target_property(core_links openrasterpp::openrasterpp-core INTERFACE_LINK_LIBRARIES)
get_target_property(core_includes openrasterpp::openrasterpp-core INTERFACE_INCLUDE_DIRECTORIES)

# --- lodepng anti-leak ---
if(core_links MATCHES "lodepng")
  message(FATAL_ERROR "openrasterpp-core must not leak lodepng")
endif()

if(TARGET lodepng)
  message(FATAL_ERROR "core component must not resolve lodepng")
endif()

# --- libspng anti-leak ---
if(core_links MATCHES "spng")
  message(FATAL_ERROR "openrasterpp-core must not leak libspng")
endif()

if(TARGET spng::spng OR TARGET spng::spng_static)
  message(FATAL_ERROR "core component must not resolve spng/libspng")
endif()

# --- libpng anti-leak ---
set(core_libpng_leak_token "")
foreach(core_link IN LISTS core_links)
  string(TOLOWER "${core_link}" core_link_lower)
  if(core_link_lower MATCHES "(^|[^a-z0-9_])(png::png|png|libpng)([^a-z0-9_]|$)")
    set(core_libpng_leak_token "${core_link}")
    break()
  endif()
endforeach()

if(NOT core_libpng_leak_token STREQUAL "")
  message(FATAL_ERROR "openrasterpp-core must not leak libpng token: ${core_libpng_leak_token}")
endif()

if(TARGET PNG::PNG OR TARGET png OR TARGET libpng OR TARGET libpng::libpng)
  message(FATAL_ERROR "core component must not resolve libpng (PNG::PNG/png/libpng)")
endif()

# --- stb anti-leak via interface include paths ---
if(core_includes MATCHES "[Ss]tb")
  message(FATAL_ERROR "openrasterpp-core must not leak stb include paths")
endif()

# --- stb anti-leak via package-resolution side effects ---
if(DEFINED Stb_FOUND)
  message(FATAL_ERROR "core component must not introduce Stb_FOUND")
endif()

if(DEFINED Stb_INCLUDE_DIR)
  message(FATAL_ERROR "core component must not introduce Stb_INCLUDE_DIR")
endif()
