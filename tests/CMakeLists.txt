cmake_minimum_required(VERSION 3.28)

FetchContent_Declare(googletest
  GIT_REPOSITORY    https://github.com/google/googletest.git
  GIT_TAG           v1.12.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

unset(CMAKE_MODULE_PATH CACHE)

include(GoogleTest)
enable_testing()

add_executable(unit_test
  ${STATSIG_SOURCE_FILES}
  ${CMAKE_CURRENT_SOURCE_DIR}/statsig_test.cpp
)

target_include_directories(unit_test PRIVATE "../include")
target_include_directories(unit_test PRIVATE "../src")

target_link_libraries(unit_test Statsig)
target_link_libraries(unit_test GTest::gtest_main)
target_compile_features(unit_test PUBLIC cxx_std_17)

gtest_discover_tests(unit_test)
