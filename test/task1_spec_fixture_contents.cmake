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

set(readme_path "${OPENRASTERPP_SOURCE_DIR}/README.md")
assert_not_contains(
  "${readme_path}"
  [=[現時点では backend smoke は install 済み backend 向け header / target が未実装のため失敗する想定です。]=]
)
assert_not_contains(
  "${readme_path}"
  [=[cmake -S test/install_smoke_core -B build/install-smoke-core]=]
)
assert_not_contains(
  "${readme_path}"
  [=[cmake --build build/install-smoke]=]
)

set(test_sh_path "${OPENRASTERPP_SOURCE_DIR}/test.sh")
assert_contains("${test_sh_path}" [=[ctest --test-dir "${BUILD_DIR}" -V]=])
assert_not_contains("${test_sh_path}" [=[install_smoke|install_package]=])

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
