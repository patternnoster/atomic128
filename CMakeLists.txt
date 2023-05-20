cmake_minimum_required(VERSION 3.23)

project(atomic128 LANGUAGES CXX)

option(ATOMIC128_BUILD_TESTS "Build tests for this library" OFF)

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(atomic128 INTERFACE)
target_compile_features(atomic128 INTERFACE cxx_std_20)
target_include_directories(atomic128 INTERFACE $<BUILD_INTERFACE:${INCLUDE_DIR}>)

if(ATOMIC128_BUILD_TESTS)
  enable_testing()
  add_subdirectory(extern)

  add_subdirectory(test)
  add_subdirectory(example)
endif()

target_sources(atomic128 INTERFACE
  FILE_SET HEADERS
    BASE_DIRS ${INCLUDE_DIR}
    FILES ${INCLUDE_DIR}/atomic128/atomic128_ref.hpp)

include(GNUInstallDirs)
install(TARGETS atomic128 EXPORT atomic128 FILE_SET HEADERS)
install(EXPORT atomic128
  FILE atomic128Config.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atomic128)
