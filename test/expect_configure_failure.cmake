if(NOT DEFINED SOURCE_DIR OR NOT DEFINED BINARY_DIR OR NOT DEFINED GENERATOR OR NOT DEFINED PREFIX_PATH
   OR NOT DEFINED EXPECTED_STRINGS)
  message(FATAL_ERROR "SOURCE_DIR, BINARY_DIR, GENERATOR, PREFIX_PATH, and EXPECTED_STRINGS are required")
endif()

string(REPLACE ";" "\\;" escaped_prefix_path "${PREFIX_PATH}")
string(REPLACE "|@|" ";" expected_strings "${EXPECTED_STRINGS}")

set(configure_command
  "${CMAKE_COMMAND}"
  -S "${SOURCE_DIR}"
  -B "${BINARY_DIR}"
  -G "${GENERATOR}"
  "-DCMAKE_PREFIX_PATH=${escaped_prefix_path}"
)

if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
  list(APPEND configure_command -A "${GENERATOR_PLATFORM}")
endif()

if(DEFINED GENERATOR_INSTANCE AND NOT GENERATOR_INSTANCE STREQUAL "")
  list(APPEND configure_command "-DCMAKE_GENERATOR_INSTANCE=${GENERATOR_INSTANCE}")
endif()

if(DEFINED GENERATOR_TOOLSET AND NOT GENERATOR_TOOLSET STREQUAL "")
  list(APPEND configure_command -T "${GENERATOR_TOOLSET}")
endif()

if(DEFINED BUILD_TYPE AND NOT BUILD_TYPE STREQUAL "")
  list(APPEND configure_command "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}")
endif()

if(DEFINED TOOLCHAIN_FILE AND NOT TOOLCHAIN_FILE STREQUAL "")
  list(APPEND configure_command "-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}")
endif()

if(DEFINED VCPKG_TARGET_TRIPLET AND NOT VCPKG_TARGET_TRIPLET STREQUAL "")
  list(APPEND configure_command "-DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}")
endif()

if(DEFINED VCPKG_HOST_TRIPLET AND NOT VCPKG_HOST_TRIPLET STREQUAL "")
  list(APPEND configure_command "-DVCPKG_HOST_TRIPLET=${VCPKG_HOST_TRIPLET}")
endif()

execute_process(
  COMMAND ${configure_command}
  RESULT_VARIABLE configure_result
  OUTPUT_VARIABLE configure_stdout
  ERROR_VARIABLE configure_stderr
)

set(configure_output "${configure_stdout}${configure_stderr}")

if(configure_result EQUAL 0)
  message(FATAL_ERROR "Expected configure failure, but configure succeeded:\n${configure_output}")
endif()

foreach(expected_string IN LISTS expected_strings)
  string(FIND "${configure_output}" "${expected_string}" expected_pos)
  if(expected_pos LESS 0)
    message(FATAL_ERROR
      "Expected configure failure containing '${expected_string}', got:\n${configure_output}"
    )
  endif()
endforeach()

message(STATUS "Observed expected configure failure")
