find_package(Catch2 REQUIRED CONFIG)

if(NOT TARGET openrasterpp-core)
  add_library(openrasterpp-core ALIAS openrasterpp)
endif()

if(NOT TARGET openrasterpp-png-lodepng)
  add_library(openrasterpp-png-lodepng ALIAS openrasterpp)
endif()

add_executable(core_test test_openraster.cpp)
target_link_libraries(core_test PRIVATE
  openrasterpp-core
  Catch2::Catch2WithMain
)
target_compile_features(core_test PRIVATE cxx_std_26)
add_test(NAME core_test COMMAND core_test)

add_executable(lodepng_backend_test test_lodepng_backend.cpp)
target_link_libraries(lodepng_backend_test PRIVATE
  openrasterpp-png-lodepng
  Catch2::Catch2WithMain
)
target_compile_features(lodepng_backend_test PRIVATE cxx_std_26)
add_test(NAME lodepng_backend_test COMMAND lodepng_backend_test)

set(install_smoke_prefix "${CMAKE_BINARY_DIR}/test/install-prefix")
set(install_smoke_core_build_dir "${CMAKE_BINARY_DIR}/test/install-smoke-core")
set(install_smoke_backend_build_dir "${CMAKE_BINARY_DIR}/test/install-smoke")
set(install_smoke_prefix_path "${install_smoke_prefix}")
set(install_smoke_generator "${CMAKE_GENERATOR}")
set(install_smoke_generator_instance "${CMAKE_GENERATOR_INSTANCE}")
set(install_smoke_generator_platform "${CMAKE_GENERATOR_PLATFORM}")
set(install_smoke_generator_toolset "${CMAKE_GENERATOR_TOOLSET}")
set(install_smoke_build_type "${CMAKE_BUILD_TYPE}")
set(install_smoke_toolchain_file "${CMAKE_TOOLCHAIN_FILE}")
set(install_smoke_vcpkg_target_triplet "${VCPKG_TARGET_TRIPLET}")
set(install_smoke_vcpkg_host_triplet "${VCPKG_HOST_TRIPLET}")
if(CMAKE_PREFIX_PATH)
  string(APPEND install_smoke_prefix_path ";${CMAKE_PREFIX_PATH}")
endif()

add_test(
  NAME install_package
  COMMAND ${CMAKE_COMMAND}
    "-DINSTALL_PREFIX=${install_smoke_prefix}"
    "-DCORE_BUILD_DIR=${install_smoke_core_build_dir}"
    "-DBACKEND_BUILD_DIR=${install_smoke_backend_build_dir}"
    "-DPROJECT_BUILD_DIR=${CMAKE_BINARY_DIR}"
    "-DINSTALL_CONFIG=$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/install_smoke_setup.cmake"
)
set_tests_properties(install_package PROPERTIES
  FIXTURES_SETUP install_smoke_prefix
)

function(add_expected_install_smoke_test name source_dir build_dir)
  list(JOIN ARGN "|@|" expected_strings)
  add_test(
    NAME "${name}"
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${source_dir}"
      "-DBINARY_DIR=${build_dir}"
      "-DGENERATOR=${install_smoke_generator}"
      "-DGENERATOR_INSTANCE=${install_smoke_generator_instance}"
      "-DGENERATOR_PLATFORM=${install_smoke_generator_platform}"
      "-DGENERATOR_TOOLSET=${install_smoke_generator_toolset}"
      "-DBUILD_TYPE=${install_smoke_build_type}"
      "-DTOOLCHAIN_FILE=${install_smoke_toolchain_file}"
      "-DVCPKG_TARGET_TRIPLET=${install_smoke_vcpkg_target_triplet}"
      "-DVCPKG_HOST_TRIPLET=${install_smoke_vcpkg_host_triplet}"
      "-DPREFIX_PATH=${install_smoke_prefix_path}"
      "-DEXPECTED_STRINGS=${expected_strings}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_configure_failure.cmake"
  )
  # Task 1 intentionally locks these consumer checks red until split package targets exist.
  set_tests_properties("${name}" PROPERTIES
    FIXTURES_REQUIRED install_smoke_prefix
  )
endfunction()

add_expected_install_smoke_test(
  install_smoke_core_configure
  "${CMAKE_CURRENT_SOURCE_DIR}/install_smoke_core"
  "${install_smoke_core_build_dir}"
  "get_target_property() called with non-existent target"
  "\"openrasterpp::openrasterpp-core\""
)

add_expected_install_smoke_test(
  install_smoke_backend_configure
  "${CMAKE_CURRENT_SOURCE_DIR}/install_smoke"
  "${install_smoke_backend_build_dir}"
  "Target \"install_smoke\" links to:"
  "openrasterpp::openrasterpp-png-lodepng"
  "but the target was not found."
)
