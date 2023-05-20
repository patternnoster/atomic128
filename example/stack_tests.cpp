#include <cstdint>
#include <gtest/gtest.h>

#include "stack.hpp"

using namespace atomic128;
using std::size_t;

TEST(StackTests, base_single_thread) {
  constexpr size_t ConsRuns = 100;

  struct elt {
    elt* next;
    uint64_t val;
  };

  const auto elts = new elt[ConsRuns];
  for (size_t i = 0; i < ConsRuns; ++i)
    elts[i].val = i;

  stack<elt> stack;
  for (size_t i = 0; i <= ConsRuns; ++i) {
    for (size_t j = 0; j < i; ++j)
      stack.push(elts + j);

    for (size_t k = i; k > 0; --k) {
      auto elt_ptr = stack.pop();
      ASSERT_NE(elt_ptr, nullptr);
      ASSERT_EQ(elt_ptr->val, k-1);
    }

    ASSERT_EQ(stack.pop(), nullptr);
  }

  delete[] elts;
}
