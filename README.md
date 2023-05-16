### About the library

Atomic128 enforces the use of the proper DWCAS instructions (e.g., `cmpxchg16b` on x86_64) where modern compilers fail to do so through the standard `std::atomic` interface, avoiding unnecessary function calls, runtime checks, and the possibility of a lock-based implementation. It works with GCC (12+), Clang (15+), MSVC (17+) and Intel (2023+) compilers using the C++20 language standard.

### Installation

The library is header-only. To install the headers along with the CMake configuration file (for `find_package`) one can use the standard CMake procedure:
```sh
cmake $SOURCE_PATH
cmake --install . --prefix=$INSTALL_PATH
```

### Usage

The `atomic128_ref<T>` class template mimicks the interface of the standard C++20 `std::atomic_ref<T>` from `<atomic>` (excepts for the `wait`/`notify` methods) and is intended to be used in the same way:
```c++
#include <atomic128/atomic128_ref.hpp>
using namespace atomic128;

struct alignas(16) foo {
  void* pointer;
  size_t counter;
};

foo f1{}, f2{&f1, 42};
atomic128_ref(f1).compare_exchange_strong(f1, f2, std::memory_order_acq_rel);

atomic128_ref ref{f2};
ref.store({nullptr, 1}, std::memory_order_relaxed);
```

### Building the tests

Use the `ATOMIC128_BUILD_TESTS=ON` CMake option to build the library tests on your system. Note that some compilers (e.g., GCC) may require additional flags (i.e., `-mcx16` on x86_64) to enable the DWCAS instructions. Thus, one can run the following commands (in the source directory):
```sh
mkdir build && cd build
cmake .. -DATOMIC128_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS='-mcx16'
cmake --build .
```
