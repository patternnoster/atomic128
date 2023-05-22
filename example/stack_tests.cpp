#include <algorithm>
#include <concepts>
#include <cstdlib>
#include <gtest/gtest.h>
#include <set>
#include <thread>
#include <vector>

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

struct stack_item {
  uint64_t val;
  stack_item* next;
};

template <size_t _cnt, std::invocable<size_t> Func>
void run_test_threads(Func&& func) {
  std::vector<std::thread> threads;
  threads.reserve(_cnt);

  for (size_t i = 0; i < _cnt; ++i)
    threads.emplace_back(std::forward<Func>(func), i);

  for (size_t i = 0; i < _cnt; ++i)
    threads[i].join();
}

TEST(StackTests, base_multi_thread) {
  constexpr size_t ThreadsCount = 8;
  constexpr size_t CyclesPerThread = 100000;

  stack<stack_item> stack;

  const auto items = new stack_item[ThreadsCount];
  for (size_t i = 0; i < ThreadsCount; ++i) {
    items[i].val = 0;
    stack.push(items + i);
  }

  const auto results = new size_t[ThreadsCount]{};
  const auto routine = [&stack, results](const size_t id) {
    size_t ctr = 0;

    for (size_t i = 0; i < CyclesPerThread; ++i) {
      stack_item* popped;
      while (!(popped = stack.pop()));

      ++popped->val;
      stack.push(popped);
      ++ctr;
    }

    results[id] = ctr;
  };

  run_test_threads<ThreadsCount>(routine);

  std::set<stack_item*> ret_items;
  size_t items_ctr = 0;
  size_t threads_ctr = 0;

  for (size_t i = 0; i < ThreadsCount; ++i) {
    const auto item = stack.pop();
    ASSERT_NE(item, nullptr);
    ASSERT_TRUE(item >= items && item <= (items + ThreadsCount));
    ret_items.insert(item);

    threads_ctr+= results[i];
  }
  ASSERT_EQ(ret_items.size(), ThreadsCount);

  for (auto item : ret_items)
    items_ctr+= item->val;

  ASSERT_EQ(items_ctr, threads_ctr);
}

TEST(StackTests, randomized_multi_thread) {
  constexpr size_t ThreadsCount = 8;
  constexpr size_t NodesPerThread = 100000;
  constexpr size_t NodesTotal = ThreadsCount * NodesPerThread;

  stack<stack_item> stack;

  const auto items = new stack_item[NodesTotal];

  const auto results = new uint64_t[NodesTotal]{};
  const auto retrieved = new stack_item*[NodesTotal]{};

  const auto routine = [&stack, items,
                        results, retrieved](const size_t id) {
    const size_t base_offset = NodesPerThread * id;

    size_t stored = 0;
    size_t loaded = 0;

    size_t last_item = 0;
    size_t last_retrieved = 0;

    for (;;) {
      const bool can_store = stored < NodesPerThread;
      const bool can_load = loaded < NodesPerThread;
      if (!can_store && !can_load) break;

      const auto rand_val = rand();
      if (!can_load || (can_store && rand_val & 1)) {  // Push
        stack_item* item;

        if (!last_retrieved || rand_val & 2) {  // Use new node
          item = items + base_offset + last_item;
          ++last_item;  // never runs out: stored < NodesPerThread
        }
        else { // Use popped node
          --last_retrieved;  // >= 0
          item = retrieved[base_offset + last_retrieved];
        }

        item->val = base_offset + stored;
        stack.push(item);
        ++stored;
      }
      else {  // Pop
        const auto item = stack.pop();
        if (!item) continue;

        // last_retrieved <= loaded
        retrieved[base_offset + last_retrieved++] = item;
        results[base_offset + loaded++] = item->val;
      }
    }

    // last_retrieved == last_item now
    for (; last_item < NodesPerThread; ++last_item)
      retrieved[base_offset + last_item] = items + base_offset + last_item;
  };

  run_test_threads<ThreadsCount>(routine);

  std::sort(results, results + NodesTotal);
  std::sort(retrieved, retrieved + NodesTotal);

  ASSERT_EQ(results[0], 0);
  ASSERT_EQ(retrieved[0], items);

  for (size_t i = 1; i < NodesTotal; ++i) {
    ASSERT_EQ(results[i], results[i-1] + 1);
    ASSERT_EQ(retrieved[i], retrieved[i-1] + 1);
  }
}
