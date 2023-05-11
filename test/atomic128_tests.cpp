#include <gtest/gtest.h>

#include "atomic128/atomic128_ref.hpp"

using namespace atomic128;
using std::size_t;

struct alignas(16) vec {
  vec *ptr;
  uint64_t ctr;
  bool operator==(const vec&) const = default;
};

TEST(Atomic128RefTests, cas) {
  constexpr size_t Cycles = 100;

  vec mem = { nullptr, 0 };
  vec prev = { &mem, 42 };
  for (size_t i = 0; i < Cycles; ++i) {
    vec orig_good = mem;  // Non-atomic loads
    vec orig_bad = prev;

    prev = mem;

    vec next = { mem.ptr + 1, mem.ctr + 1 };
    atomic128_ref ref(mem);

    ASSERT_FALSE(ref.compare_exchange_strong(orig_bad, next));
    ASSERT_EQ(mem, orig_good);  // No change
    ASSERT_EQ(orig_bad, prev);  // Load old value

    ASSERT_TRUE(ref.compare_exchange_strong(orig_good, next,
                                            mo_t::acq_rel,
                                            mo_t::relaxed));
    ASSERT_EQ(mem, next);
    ASSERT_EQ(orig_good, prev);
  }
}
