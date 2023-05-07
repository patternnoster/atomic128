cmake_minimum_required(VERSION 3.18)

project(atomic128 LANGUAGES CXX)

option(ATOMIC128_BUILD_TESTS "Build tests for this library" OFF)

add_library(atomic128 INTERFACE)

target_compile_features(atomic128 INTERFACE cxx_std_20)
target_include_directories(atomic128 INTERFACE include)

if(ATOMIC128_BUILD_TESTS)
  enable_testing()
  add_subdirectory(extern)
  add_subdirectory(test)
endif()
