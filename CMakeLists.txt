cmake_minimum_required(VERSION 3.23)

project(atomic128 LANGUAGES CXX)

option(ATOMIC128_BUILD_TESTS   "Build tests for this library" OFF)
option(ATOMIC128_INSTALL_STACK "Install the example lock-free stack" OFF)

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(EXAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/example)

add_library(atomic128 INTERFACE)
target_compile_features(atomic128 INTERFACE cxx_std_20)
target_include_directories(atomic128 INTERFACE
  $<BUILD_INTERFACE:${INCLUDE_DIR}>)

if(ATOMIC128_BUILD_TESTS)
  enable_testing()
  add_subdirectory(extern)

  add_subdirectory(test)
  add_subdirectory(example)
endif()

target_sources(atomic128 INTERFACE
  FILE_SET HEADERS
    BASE_DIRS ${INCLUDE_DIR}/atomic128
    FILES ${INCLUDE_DIR}/atomic128/atomic128_ref.hpp)

if(ATOMIC128_INSTALL_STACK)
  target_sources(atomic128 INTERFACE
    FILE_SET HEADERS
      BASE_DIRS ${EXAMPLE_DIR}
      FILES ${EXAMPLE_DIR}/stack.hpp)
endif()

include(GNUInstallDirs)
install(TARGETS atomic128 EXPORT atomic128
  FILE_SET HEADERS
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atomic128)
install(EXPORT atomic128
  FILE atomic128Config.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atomic128)
