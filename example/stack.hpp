#pragma once
#include <atomic>
#include <concepts>

#include "atomic128/atomic128_ref.hpp"

#if !(defined(__cpp_lib_atomic_ref) || defined(__GCC_ATOMIC_POINTER_LOCK_FREE))
#  error Either std::atomic_ref or the __atomic_* built-ins must be available
#endif

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

  auto read_head(const mo_t mo) const noexcept {
    return head_t {  // No need to use the 16b instruction for reads
#ifdef __cpp_lib_atomic_ref
      .ptr = std::atomic_ref(head_.ptr).load(mo),
      .aba_counter = std::atomic_ref(head_.aba_counter).load(mo_t::relaxed)
#else
      .ptr = __atomic_load_n(&head_.ptr,
                             (mo == mo_t::acquire
                              ? __ATOMIC_ACQUIRE : __ATOMIC_RELAXED)),
      .aba_counter = __atomic_load_n(&head_.aba_counter, __ATOMIC_RELAXED)
#endif
    };
  }

  head_t head_;
};

template <stackable T>
void stack<T>::push(T* const ptr) noexcept {
  // Since we don't really access the head pointer, a relaxed read is
  // perfectly fine (including that in case of a failed CAS)
  head_t old_head = read_head(mo_t::relaxed);

  head_t new_head;
  new_head.ptr = ptr;

  do {
#ifdef __cpp_lib_atomic_ref
    std::atomic_ref(ptr->next).store(old_head.ptr, mo_t::relaxed);
#else
    __atomic_store_n(&ptr->next, old_head.ptr, __ATOMIC_RELAXED);
#endif
    new_head.aba_counter = old_head.aba_counter + 1;
  }
  while (!atomic128_ref(head_).compare_exchange_weak(old_head, new_head,
                                                     mo_t::release,
                                                     mo_t::relaxed));
  /* A release is enough here: all we need is for our changes to the
   * *ptr to become visible after a read of this new head value (or a
   * value following it in the release sequence) */
}

template <stackable T>
T* stack<T>::pop() noexcept {
  // We need to always acquire the head since we're planning to read
  // from the corresponding pointer
  head_t old_head = read_head(mo_t::acquire);
  head_t new_head;

  do {
    if (!old_head.ptr) return nullptr;  // Empty stack

    new_head = {
#ifdef __cpp_lib_atomic_ref
      .ptr = std::atomic_ref(old_head.ptr->next).load(mo_t::relaxed),
#else
      .ptr = __atomic_load_n(&old_head.ptr->next, __ATOMIC_RELAXED),
#endif
      .aba_counter = old_head.aba_counter + 1
    };
  }
  while (!atomic128_ref(head_).compare_exchange_weak(old_head, new_head,
                                                     mo_t::relaxed,
                             /* C++20 allows this: */mo_t::acquire));
  /* Relaxed ordering on success is enough here because the written
   * pointer was put on the top of the stack (with a release) before,
   * and all following RMWs (even relaxed) form a release sequence */

  return old_head.ptr;
}

} // namespace atomic128
