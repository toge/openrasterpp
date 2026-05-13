find_package(Catch2 REQUIRED CONFIG)

add_executable(core_test test_openraster.cpp)
target_link_libraries(core_test PRIVATE
  openrasterpp-core
  Catch2::Catch2WithMain
)
target_compile_features(core_test PRIVATE cxx_std_26)
add_test(NAME core_test COMMAND core_test)

if(OPENRASTERPP_ENABLE_LODEPNG)
  add_executable(lodepng_backend_test test_lodepng_backend.cpp)
  target_link_libraries(lodepng_backend_test PRIVATE
    openrasterpp-png-lodepng
    Catch2::Catch2WithMain
  )
  target_compile_features(lodepng_backend_test PRIVATE cxx_std_26)
  add_test(NAME lodepng_backend_test COMMAND lodepng_backend_test)
endif()

if(OPENRASTERPP_ENABLE_LIBSPNG)
  add_executable(libspng_backend_test test_libspng_backend.cpp)
  target_link_libraries(libspng_backend_test PRIVATE
    openrasterpp-png-libspng
    Catch2::Catch2WithMain
  )
  target_compile_features(libspng_backend_test PRIVATE cxx_std_26)
  add_test(NAME libspng_backend_test COMMAND libspng_backend_test)
endif()

if(OPENRASTERPP_ENABLE_LIBPNG)
  add_executable(libpng_backend_test test_libpng_backend.cpp)
  target_link_libraries(libpng_backend_test PRIVATE
    openrasterpp-png-libpng
    Catch2::Catch2WithMain
  )
  target_compile_features(libpng_backend_test PRIVATE cxx_std_26)
  add_test(NAME libpng_backend_test COMMAND libpng_backend_test)
endif()

if(OPENRASTERPP_ENABLE_STB)
  # stb_backend_test uses libpng for verification
  if(OPENRASTERPP_ENABLE_LIBPNG)
    add_executable(stb_backend_test test_stb_backend.cpp)
    target_link_libraries(stb_backend_test PRIVATE
      openrasterpp-png-stb
      PNG::PNG
      Catch2::Catch2WithMain
    )
    target_compile_features(stb_backend_test PRIVATE cxx_std_26)
    add_test(NAME stb_backend_test COMMAND stb_backend_test)
  endif()
endif()


if(OPENRASTERPP_ENABLE_FPNG)
  add_executable(fpng_backend_test test_fpng_backend.cpp)
  target_link_libraries(fpng_backend_test PRIVATE
    openrasterpp-png-fpng
    Catch2::Catch2WithMain
  )
  target_compile_features(fpng_backend_test PRIVATE cxx_std_26)
  add_test(NAME fpng_backend_test COMMAND fpng_backend_test)
endif()

set(install_smoke_prefix "${CMAKE_BINARY_DIR}/test/install-prefix")
set(install_smoke_core_build_dir "${CMAKE_BINARY_DIR}/test/install-smoke-core")
set(install_smoke_backend_build_dir "${CMAKE_BINARY_DIR}/test/install-smoke")
set(install_smoke_libspng_build_dir "${CMAKE_BINARY_DIR}/test/install-smoke-libspng")
set(install_smoke_libpng_build_dir "${CMAKE_BINARY_DIR}/test/install-smoke-libpng")
set(install_smoke_stb_build_dir "${CMAKE_BINARY_DIR}/test/install-smoke-stb")
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
    "${install_smoke_libspng_build_dir}"
    "${install_smoke_libpng_build_dir}"
    "${install_smoke_stb_build_dir}"
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

function(add_install_smoke_run_test name build_dir fixture_name executable_name)
  if(CMAKE_CONFIGURATION_TYPES)
    set(executable_path "${build_dir}/$<CONFIG>/${executable_name}${CMAKE_EXECUTABLE_SUFFIX}")
  else()
    set(executable_path "${build_dir}/${executable_name}${CMAKE_EXECUTABLE_SUFFIX}")
  endif()

  add_test(
    NAME "${name}"
    COMMAND "${executable_path}"
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

if(OPENRASTERPP_ENABLE_LODEPNG)
  add_install_smoke_configure_test(
    install_smoke_backend_configure
    "${CMAKE_CURRENT_SOURCE_DIR}/install_smoke"
    "${install_smoke_backend_build_dir}"
    install_smoke_backend_configured
  )
endif()

if(OPENRASTERPP_ENABLE_LIBSPNG)
  add_install_smoke_configure_test(
    install_smoke_libspng_configure
    "${CMAKE_CURRENT_SOURCE_DIR}/install_smoke_libspng"
    "${install_smoke_libspng_build_dir}"
    install_smoke_libspng_configured
  )
endif()

if(OPENRASTERPP_ENABLE_LIBPNG)
  add_install_smoke_configure_test(
    install_smoke_libpng_configure
    "${CMAKE_CURRENT_SOURCE_DIR}/install_smoke_libpng"
    "${install_smoke_libpng_build_dir}"
    install_smoke_libpng_configured
  )
endif()

if(OPENRASTERPP_ENABLE_STB)
  add_install_smoke_configure_test(
    install_smoke_stb_configure
    "${CMAKE_CURRENT_SOURCE_DIR}/install_smoke_stb"
    "${install_smoke_stb_build_dir}"
    install_smoke_stb_configured
  )
endif()

add_install_smoke_build_test(
  install_smoke_core_build
  "${install_smoke_core_build_dir}"
  install_smoke_core_configured
)

if(OPENRASTERPP_ENABLE_LODEPNG)
  add_install_smoke_build_test(
    install_smoke_backend_build
    "${install_smoke_backend_build_dir}"
    install_smoke_backend_configured
  )
endif()

if(OPENRASTERPP_ENABLE_LIBSPNG)
  add_install_smoke_build_test(
    install_smoke_libspng_build
    "${install_smoke_libspng_build_dir}"
    install_smoke_libspng_configured
  )
endif()

if(OPENRASTERPP_ENABLE_LIBPNG)
  add_install_smoke_build_test(
    install_smoke_libpng_build
    "${install_smoke_libpng_build_dir}"
    install_smoke_libpng_configured
  )
endif()

if(OPENRASTERPP_ENABLE_STB)
  add_install_smoke_build_test(
    install_smoke_stb_build
    "${install_smoke_stb_build_dir}"
    install_smoke_stb_configured
  )
  set_tests_properties(install_smoke_stb_build PROPERTIES
    FIXTURES_SETUP install_smoke_stb_built
  )

  add_install_smoke_run_test(
    install_smoke_stb_run
    "${install_smoke_stb_build_dir}"
    install_smoke_stb_built
    install_smoke_stb
  )
endif()

