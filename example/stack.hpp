#pragma once
#include <concepts>

#include "atomic128/atomic128_ref.hpp"

/**
 * @file
 * A simplistic lock-free stack implementation that uses our interface
 * for the DWCAS instruction to avoid the ABA problem
 *
 * @author    patternnoster@github
 * @copyright 2023, under the MIT License (see /LICENSE for details)
 **/

namespace atomic128 {

template <typename T>
concept stackable = requires(T obj) {
  { obj.next } -> std::same_as<T*&>;
};

/**
 * @brief A simple lock-free stack of pointers
 **/
template <stackable T>
class stack {
public:
  stack() noexcept: head_{nullptr, 0} {}

  stack(const stack&) = delete;
  stack(stack&&) = delete;
  stack& operator=(const stack&) = delete;
  stack& operator=(stack&&) = delete;

  /**
   * @brief Pushes an element on top of the stack
   **/
  inline void push(T*) noexcept;

  /**
   * @brief   Pops the element from the top of the stack.
   * @return  A pointer to the popped element or nullptr if the stack
   *          was empty
   * @warning Other threads can still access the returned pointer even
   *          after the call is complete. This simplistic
   *          implementation of a stack does not use advanced memory
   *          management techniques like hazard pointers, therefore,
   *          deletion of T*, or any non-atomic modification of
   *          T::next, before all the concurrent readers return from
   *          pop() leads to undefined behaviour.
   **/
  inline T* pop() noexcept;

private:
  struct alignas(16) head_t {
    T* ptr;
    std::size_t aba_counter;
  };

  head_t head_;
};

} // namespace atomic128
