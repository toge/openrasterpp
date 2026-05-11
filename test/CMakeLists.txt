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

set(task1_spec_fixture_contents_script
  "${CMAKE_CURRENT_BINARY_DIR}/task1_spec_fixture_contents.cmake"
)
file(WRITE "${task1_spec_fixture_contents_script}" [==[
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED OPENRASTERPP_SOURCE_DIR)
  message(FATAL_ERROR "OPENRASTERPP_SOURCE_DIR is required")
endif()

function(assert_contains file_path needle)
  file(READ "${file_path}" content)
  string(FIND "${content}" "${needle}" needle_index)
  if(needle_index EQUAL -1)
    message(FATAL_ERROR "Expected '${needle}' in ${file_path}")
  endif()
endfunction()

function(assert_not_contains file_path needle)
  file(READ "${file_path}" content)
  string(FIND "${content}" "${needle}" needle_index)
  if(NOT needle_index EQUAL -1)
    message(FATAL_ERROR "Did not expect '${needle}' in ${file_path}")
  endif()
endfunction()

set(source_fixture_path
  "${OPENRASTERPP_SOURCE_DIR}/test/task1_spec_fixture_contents.cmake"
)
if(EXISTS "${source_fixture_path}")
  message(FATAL_ERROR "Out-of-scope source fixture must be removed: ${source_fixture_path}")
endif()

set(install_smoke_core_cmake
  "${OPENRASTERPP_SOURCE_DIR}/test/install_smoke_core/CMakeLists.txt"
)
assert_contains(
  "${install_smoke_core_cmake}"
  "find_package(openrasterpp CONFIG REQUIRED COMPONENTS core)"
)
assert_contains(
  "${install_smoke_core_cmake}"
  "target_link_libraries(install_smoke_core PRIVATE openrasterpp::openrasterpp-core)"
)
assert_contains(
  "${install_smoke_core_cmake}"
  "get_target_property(core_links openrasterpp::openrasterpp-core INTERFACE_LINK_LIBRARIES)"
)
assert_contains(
  "${install_smoke_core_cmake}"
  "if(core_links MATCHES \"lodepng\")"
)
assert_contains(
  "${install_smoke_core_cmake}"
  "message(FATAL_ERROR \"openrasterpp-core must not leak lodepng\")"
)

set(install_smoke_backend_cmake
  "${OPENRASTERPP_SOURCE_DIR}/test/install_smoke/CMakeLists.txt"
)
assert_contains(
  "${install_smoke_backend_cmake}"
  "find_package(openrasterpp CONFIG REQUIRED COMPONENTS png-lodepng)"
)
assert_contains(
  "${install_smoke_backend_cmake}"
  "target_link_libraries(install_smoke PRIVATE openrasterpp::openrasterpp-png-lodepng)"
)
assert_not_contains(
  "${install_smoke_backend_cmake}"
  "OPENRASTERPP_INSTALL_SMOKE_EXPECT_BACKEND_TARGET_MISSING"
)
assert_not_contains(
  "${install_smoke_backend_cmake}"
  "target_link_libraries(install_smoke PRIVATE openrasterpp::openrasterpp)"
)

set(test_cmakelists_path "${OPENRASTERPP_SOURCE_DIR}/test/CMakeLists.txt")
assert_contains("${test_cmakelists_path}" [=[NAME install_package]=])
assert_contains("${test_cmakelists_path}" [=[FIXTURES_REQUIRED install_smoke_clean]=])
assert_contains("${test_cmakelists_path}" [=[FIXTURES_SETUP install_smoke_prefix]=])
assert_contains("${test_cmakelists_path}" [=[add_install_smoke_configure_test(]=])
assert_contains("${test_cmakelists_path}" [=[FIXTURES_REQUIRED install_smoke_prefix]=])
assert_contains("${test_cmakelists_path}" [=[FIXTURES_SETUP "${fixture_name}"]=])
assert_contains("${test_cmakelists_path}" [=[install_smoke_core_configure]=])
assert_contains("${test_cmakelists_path}" [=[install_smoke_backend_configure]=])
assert_contains("${test_cmakelists_path}" [=[add_install_smoke_build_test(]=])
assert_contains("${test_cmakelists_path}" [=[FIXTURES_REQUIRED "${fixture_name}"]=])
assert_contains("${test_cmakelists_path}" [=[install_smoke_core_build]=])
assert_contains("${test_cmakelists_path}" [=[install_smoke_backend_build]=])
]==])

add_test(
  NAME task1_spec_fixture_contents
  COMMAND ${CMAKE_COMMAND}
    "-DOPENRASTERPP_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${task1_spec_fixture_contents_script}"
)

set(task2_package_split_fixture_contents_script
  "${CMAKE_CURRENT_BINARY_DIR}/task2_package_split_fixture_contents.cmake"
)
file(WRITE "${task2_package_split_fixture_contents_script}" [==[
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED OPENRASTERPP_SOURCE_DIR)
  message(FATAL_ERROR "OPENRASTERPP_SOURCE_DIR is required")
endif()

function(assert_contains file_path needle)
  file(READ "${file_path}" content)
  string(FIND "${content}" "${needle}" needle_index)
  if(needle_index EQUAL -1)
    message(FATAL_ERROR "Expected '${needle}' in ${file_path}")
  endif()
endfunction()

function(assert_not_contains file_path needle)
  file(READ "${file_path}" content)
  string(FIND "${content}" "${needle}" needle_index)
  if(NOT needle_index EQUAL -1)
    message(FATAL_ERROR "Did not expect '${needle}' in ${file_path}")
  endif()
endfunction()

set(root_cmakelists "${OPENRASTERPP_SOURCE_DIR}/CMakeLists.txt")
assert_contains("${root_cmakelists}" "add_library(openrasterpp-core")
assert_contains("${root_cmakelists}" "openrasterpp-png-lodepng")
assert_contains("${root_cmakelists}" "openraster_core.cpp")
assert_contains("${root_cmakelists}" "src/backends/lodepng_backend.cpp")
assert_contains("${root_cmakelists}" "EXPORT openrasterppCoreTargets")
assert_contains("${root_cmakelists}" "FILE openrasterppCoreTargets.cmake")
assert_contains("${root_cmakelists}" "COMPONENT core")
assert_contains("${root_cmakelists}" "EXPORT openrasterppPngLodepngTargets")
assert_contains("${root_cmakelists}" "FILE openrasterppPngLodepngTargets.cmake")
assert_contains("${root_cmakelists}" "COMPONENT png-lodepng")
assert_contains("${root_cmakelists}" "openraster_lodepng.hpp")
assert_not_contains("${root_cmakelists}" "install(\n    EXPORT openrasterppTargets")

set(config_template "${OPENRASTERPP_SOURCE_DIR}/cmake/openrasterppConfig.cmake.in")
assert_contains("${config_template}" "include(\"\${CMAKE_CURRENT_LIST_DIR}/openrasterppCoreTargets.cmake\")")
assert_contains("${config_template}" "if(NOT openrasterpp_FIND_COMPONENTS)")
assert_contains("${config_template}" "set(openrasterpp_FIND_COMPONENTS core)")
assert_contains("${config_template}" "\"png-lodepng\" IN_LIST openrasterpp_FIND_COMPONENTS")
assert_contains("${config_template}" "include(\"\${CMAKE_CURRENT_LIST_DIR}/openrasterppConfig-png-lodepng.cmake\")")
assert_contains("${config_template}" "check_required_components(openrasterpp)")

set(backend_config_template
  "${OPENRASTERPP_SOURCE_DIR}/cmake/openrasterppConfig-png-lodepng.cmake.in"
)
assert_contains("${backend_config_template}" "find_dependency(lodepng CONFIG REQUIRED)")
assert_contains("${backend_config_template}" "include(\"\${CMAKE_CURRENT_LIST_DIR}/openrasterppPngLodepngTargets.cmake\")")

set(core_header "${OPENRASTERPP_SOURCE_DIR}/openraster.hpp")
assert_not_contains("${core_header}" "class DefaultOraProvider;")
assert_not_contains("${core_header}" "auto encode_png(const ImageBuffer& image) -> std::expected<std::vector<uint8_t>, Error>;")
assert_not_contains("${core_header}" "auto render_preview_and_thumbnail(OraDocument& doc) -> std::expected<void, Error>;")
assert_not_contains("${core_header}" "auto read(std::string_view filename) -> std::expected<OraDocument, Error>;")
assert_not_contains("${core_header}" "auto write(std::string_view filename, const OraDocument& doc) -> std::expected<void, Error>;")

set(backend_header "${OPENRASTERPP_SOURCE_DIR}/openraster_lodepng.hpp")
assert_contains("${backend_header}" "namespace ora::util {")
assert_contains("${backend_header}" "auto encode_png(ImageBuffer const& image) -> std::expected<std::vector<uint8_t>, Error>;")
assert_contains("${backend_header}" "auto render_preview_and_thumbnail(OraDocument& doc) -> std::expected<void, Error>;")
assert_contains("${backend_header}" "namespace ora {")
assert_contains("${backend_header}" "auto read(std::string_view filename) -> std::expected<OraDocument, Error>;")
assert_contains("${backend_header}" "auto write(std::string_view filename, OraDocument const& doc) -> std::expected<void, Error>;")

set(core_source "${OPENRASTERPP_SOURCE_DIR}/openraster_core.cpp")
assert_not_contains("${core_source}" "auto escape_xml(")
assert_not_contains("${core_source}" "auto generate_node_xml(")
]==])

add_test(
  NAME task2_package_split_fixture_contents
  COMMAND ${CMAKE_COMMAND}
    "-DOPENRASTERPP_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${task2_package_split_fixture_contents_script}"
)

set(install_smoke_prefix "${CMAKE_BINARY_DIR}/test/install-prefix")
set(install_smoke_core_build_dir "${CMAKE_BINARY_DIR}/test/install-smoke-core")
set(install_smoke_backend_build_dir "${CMAKE_BINARY_DIR}/test/install-smoke")
set(install_smoke_prefix_path "${install_smoke_prefix}")
if(CMAKE_PREFIX_PATH)
  string(APPEND install_smoke_prefix_path ";${CMAKE_PREFIX_PATH}")
endif()
string(REPLACE ";" "\\;" install_smoke_prefix_path_escaped "${install_smoke_prefix_path}")

add_test(
  NAME install_smoke_cleanup
  COMMAND ${CMAKE_COMMAND} -E rm -rf
    "${install_smoke_prefix}"
    "${install_smoke_core_build_dir}"
    "${install_smoke_backend_build_dir}"
)
set_tests_properties(install_smoke_cleanup PROPERTIES
  FIXTURES_SETUP install_smoke_clean
  RESOURCE_LOCK install_smoke
)

set(install_package_command
  ${CMAKE_COMMAND}
  --install "${CMAKE_BINARY_DIR}"
  --prefix "${install_smoke_prefix}"
)
if(CMAKE_CONFIGURATION_TYPES)
  list(APPEND install_package_command --config "$<CONFIG>")
endif()

add_test(
  NAME install_package
  COMMAND ${install_package_command}
)
set_tests_properties(install_package PROPERTIES
  FIXTURES_REQUIRED install_smoke_clean
  FIXTURES_SETUP install_smoke_prefix
  RESOURCE_LOCK install_smoke
)

set(task2_installed_package_layout_script
  "${CMAKE_CURRENT_BINARY_DIR}/task2_installed_package_layout.cmake"
)
file(WRITE "${task2_installed_package_layout_script}" [==[
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED OPENRASTERPP_INSTALL_PREFIX)
  message(FATAL_ERROR "OPENRASTERPP_INSTALL_PREFIX is required")
endif()

function(assert_exists file_path)
  if(NOT EXISTS "${file_path}")
    message(FATAL_ERROR "Expected file to exist: ${file_path}")
  endif()
endfunction()

set(package_dir "${OPENRASTERPP_INSTALL_PREFIX}/lib/cmake/openrasterpp")
if(NOT EXISTS "${package_dir}")
  set(package_dir "${OPENRASTERPP_INSTALL_PREFIX}/lib64/cmake/openrasterpp")
endif()
assert_exists("${package_dir}/openrasterppConfig.cmake")
assert_exists("${package_dir}/openrasterppConfigVersion.cmake")
assert_exists("${package_dir}/openrasterppConfig-png-lodepng.cmake")
assert_exists("${package_dir}/openrasterppCoreTargets.cmake")
assert_exists("${package_dir}/openrasterppPngLodepngTargets.cmake")
assert_exists("${OPENRASTERPP_INSTALL_PREFIX}/include/openraster.hpp")
assert_exists("${OPENRASTERPP_INSTALL_PREFIX}/include/openraster_lodepng.hpp")
]==])

add_test(
  NAME task2_installed_package_layout
  COMMAND ${CMAKE_COMMAND}
    "-DOPENRASTERPP_INSTALL_PREFIX=${install_smoke_prefix}"
    -P "${task2_installed_package_layout_script}"
)
set_tests_properties(task2_installed_package_layout PROPERTIES
  FIXTURES_REQUIRED install_smoke_prefix
  RESOURCE_LOCK install_smoke
)

function(add_install_smoke_configure_test name source_dir build_dir fixture_name)
  set(configure_command
    ${CMAKE_COMMAND}
    -S "${source_dir}"
    -B "${build_dir}"
    -G "${CMAKE_GENERATOR}"
    "-DCMAKE_PREFIX_PATH=${install_smoke_prefix_path_escaped}"
  )

  if(CMAKE_GENERATOR_PLATFORM)
    list(APPEND configure_command -A "${CMAKE_GENERATOR_PLATFORM}")
  endif()

  if(CMAKE_GENERATOR_INSTANCE)
    list(APPEND configure_command "-DCMAKE_GENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}")
  endif()

  if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND configure_command -T "${CMAKE_GENERATOR_TOOLSET}")
  endif()

  if(CMAKE_BUILD_TYPE)
    list(APPEND configure_command "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
  endif()

  if(CMAKE_TOOLCHAIN_FILE)
    list(APPEND configure_command "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
  endif()

  if(VCPKG_TARGET_TRIPLET)
    list(APPEND configure_command "-DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}")
  endif()

  if(VCPKG_HOST_TRIPLET)
    list(APPEND configure_command "-DVCPKG_HOST_TRIPLET=${VCPKG_HOST_TRIPLET}")
  endif()

  add_test(
    NAME "${name}"
    COMMAND ${configure_command}
  )
  set_tests_properties("${name}" PROPERTIES
    FIXTURES_REQUIRED install_smoke_prefix
    FIXTURES_SETUP "${fixture_name}"
    RESOURCE_LOCK install_smoke
  )
endfunction()

function(add_install_smoke_build_test name build_dir fixture_name)
  set(build_command
    ${CMAKE_COMMAND}
    --build "${build_dir}"
  )

  if(CMAKE_CONFIGURATION_TYPES)
    list(APPEND build_command --config "$<CONFIG>")
  endif()

  add_test(
    NAME "${name}"
    COMMAND ${build_command}
  )
  set_tests_properties("${name}" PROPERTIES
    FIXTURES_REQUIRED "${fixture_name}"
    RESOURCE_LOCK install_smoke
  )
endfunction()

add_install_smoke_configure_test(
  install_smoke_core_configure
  "${CMAKE_CURRENT_SOURCE_DIR}/install_smoke_core"
  "${install_smoke_core_build_dir}"
  install_smoke_core_configured
)

add_install_smoke_configure_test(
  install_smoke_backend_configure
  "${CMAKE_CURRENT_SOURCE_DIR}/install_smoke"
  "${install_smoke_backend_build_dir}"
  install_smoke_backend_configured
)

add_install_smoke_build_test(
  install_smoke_core_build
  "${install_smoke_core_build_dir}"
  install_smoke_core_configured
)

add_install_smoke_build_test(
  install_smoke_backend_build
  "${install_smoke_backend_build_dir}"
  install_smoke_backend_configured
)
