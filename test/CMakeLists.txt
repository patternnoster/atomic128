cmake_minimum_required(VERSION 3.18)

add_executable(atomic128_tests atomic128_tests.cpp)
target_link_libraries(atomic128_tests PRIVATE atomic128 gtest_main)

# Show all warnings because we're pedantic (and also all and extra)
target_compile_options(atomic128_tests PRIVATE
  $<IF:$<BOOL:${MSVC}>, /W3, -Wall -Wpedantic -Wextra>)

include(GoogleTest)
gtest_discover_tests(atomic128_tests)
