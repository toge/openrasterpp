if(NOT DEFINED INSTALL_PREFIX OR NOT DEFINED CORE_BUILD_DIR OR NOT DEFINED BACKEND_BUILD_DIR
   OR NOT DEFINED PROJECT_BUILD_DIR)
  message(FATAL_ERROR
    "INSTALL_PREFIX, CORE_BUILD_DIR, BACKEND_BUILD_DIR, and PROJECT_BUILD_DIR are required"
  )
endif()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E rm -rf
    "${INSTALL_PREFIX}"
    "${CORE_BUILD_DIR}"
    "${BACKEND_BUILD_DIR}"
  RESULT_VARIABLE cleanup_result
)

if(NOT cleanup_result EQUAL 0)
  message(FATAL_ERROR "Failed to clean install smoke directories")
endif()

set(install_command
  "${CMAKE_COMMAND}"
  --install "${PROJECT_BUILD_DIR}"
  --prefix "${INSTALL_PREFIX}"
)

if(DEFINED INSTALL_CONFIG AND NOT INSTALL_CONFIG STREQUAL "")
  list(APPEND install_command --config "${INSTALL_CONFIG}")
endif()

execute_process(
  COMMAND ${install_command}
  RESULT_VARIABLE install_result
  OUTPUT_VARIABLE install_stdout
  ERROR_VARIABLE install_stderr
)

if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "Install smoke setup failed:\n${install_stdout}${install_stderr}")
endif()
