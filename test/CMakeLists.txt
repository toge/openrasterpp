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
