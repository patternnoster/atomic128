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

TEST(Atomic128RefTests, cas_alternative) {
  vec m1{ nullptr, 0 }, m2{ &m1, 1 };

  // Common case: old_value == ref
  ASSERT_TRUE(atomic128_ref(m1).compare_exchange_strong(m1, m2));
  ASSERT_EQ(m1, m2);

  // Weak interfaces
  const vec new_val{ &m2, 2 };
  while (!atomic128_ref(m1).compare_exchange_weak(m2, { &m2, 2 },
                                                  std::memory_order_relaxed));
  ASSERT_EQ(m1, new_val);

  while (!atomic128_ref(m1).compare_exchange_weak(m1, m2,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_relaxed));
  ASSERT_EQ(m1, m2);
}

TEST(Atomic128RefTests, lse) {
  constexpr size_t Cycles = 100;

  vec mem = { &mem, 0 };
  for (size_t i = 0; i < Cycles; ++i) {
    atomic128_ref ref(mem);

    // I'm just having fun with the memory order args, don't look too
    // much into it...

    vec obj = i % 2 ? ref.load(std::memory_order_relaxed) : ref;
    ASSERT_EQ(obj.ptr, &mem + 2*i);
    ASSERT_EQ(obj.ctr, 2*i);

    const vec next = { obj.ptr + 1, obj.ctr + 1 };
    const auto prev = ref.exchange(next, std::memory_order_acq_rel);
    ASSERT_EQ(prev, obj);

    obj = ref.load(std::memory_order_seq_cst);
    ASSERT_EQ(obj.ptr, next.ptr);
    ASSERT_EQ(obj.ctr, next.ctr);

    const vec next_next{next.ptr + 1, next.ctr + 1};
    if (i % 2) ref.store(next_next, std::memory_order_release);
    else ref = next_next;
  }
}
