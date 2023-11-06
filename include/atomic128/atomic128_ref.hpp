#pragma once
#include <atomic>
#include <cstdint>
#include <type_traits>

#undef __ATOMIC128__MSVC
#if !defined(__GLIBCXX__) && !defined(__GLIBCPP__)     \
    && !defined(_LIBCPP_VERSION) && defined(_MSC_VER)
#  define __ATOMIC128__MSVC
#endif

#ifdef __ATOMIC128_MSVC
#  include <intrin.h>  // for _InterlockedCompareExchange128
#endif

#ifndef __cpp_lib_atomic_ref
namespace std { template <typename> struct atomic_ref; }
#endif

/**
 * @file
 * A cross-platform implementation of DWCAS mimicking the
 * std::atomic_ref interface
 *
 * @author    patternnoster@github
 * @copyright 2023, under the MIT License (see /LICENSE for details)
 **/

namespace atomic128 {

template <typename T>
concept atomic128_referenceable =
  std::is_trivially_copyable_v<T> && sizeof(T) == 16 && alignof(T) >= 16;

using mo_t = std::memory_order;

/**
 * @brief Applies atomic operations to (properly aligned) 16-byte
 *        types enforcing the use of native DWCAS instructions
 *
 * We're in the middle of 202xs and the DWCAS instructions have been
 * around for decades, yet there is still a nightmare going on with
 * their proper compiler support. This is an attempt to clean things
 * up a little and avoid any implicit substitution of an expensive
 * function call that modern compilers sometimes perform.
 **/
template <atomic128_referenceable T>
class atomic128_ref {
public:
  using value_type = T;

  constexpr static bool is_always_lock_free = true/*, that's why we do this */;
  constexpr static std::size_t required_alignment = alignof(T);

#ifdef __cpp_lib_atomic_ref
  constexpr static bool use_native = std::atomic_ref<T>::is_always_lock_free;
#else
  constexpr static bool use_native = false;
#endif

  explicit atomic128_ref(T& obj) noexcept: obj_(obj) {}
  atomic128_ref(const atomic128_ref& rhs) noexcept: obj_(rhs.obj_) {}

  bool compare_exchange_weak(T& old_val, const T& new_val,
            [[maybe_unused]] const mo_t mo = mo_t::seq_cst) const noexcept {
    if constexpr (use_native) {
      /* The cleanest of all possible solutions. For now, only
       * supported by Clang and Intel on x86 with the -mcx16 flag,
       * Clang on ARM64, and Apple Clang */
      return
        std::atomic_ref(obj_).compare_exchange_weak(old_val, new_val, mo);
    }
    else {  // Here comes the trouble...
#ifdef __ATOMIC128__MSVC
#  define __ATOMIC_128__HAS_BUILTIN
      /* It's still not so bad on Windows, we just need to use the
       * proper intrinsic and be naughty enough to break the strict
       * aliasing rule (which is not that bad because of the
       * trivially_copyable requirement) */
      const auto obj_p64 = reinterpret_cast<int64_t*>(std::addressof(obj_));
      const auto new_p64 =
        reinterpret_cast<const int64_t*>(std::addressof(new_val));

      T old = old_val;  // Non-atomic read here
      const auto old_p64 = reinterpret_cast<int64_t*>(std::addressof(old));

      // The intrinsic acts as a full barrier so we can ignore the
      // specified memory order: It's always seq_cst in Philadelphia.
      if (_InterlockedCompareExchange128(obj_p64,
                                         *(new_p64 + 1), *new_p64, old_p64))
        return true;

      if (std::addressof(old_val) != std::addressof(obj_)) old_val = old;
      return false;
#elif defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16)
#  define __ATOMIC_128__HAS_BUILTIN
      /* GCC for x86 won't emit the cmpxchg16b instruction through the
       * __atomic built-ins or std::atomic interfaces but will do so
       * with a __sync built-in if a proper flag (like -mcx16) was
       * passed, and in that case, the macro in our conditional is
       * always defined */
      __extension__ using int128_t = __int128;  // suppress ISO-C++ warnings

      const auto obj_p128 = reinterpret_cast<int128_t*>(std::addressof(obj_));
      const auto new_p128 =
        reinterpret_cast<const int128_t*>(std::addressof(new_val));

      const auto old_p128 =
        reinterpret_cast<int128_t*>(std::addressof(old_val));
      const int128_t old = *old_p128;

      // Same as above, the built-in is guaranteed to create a full
      // memory barrier so just ignore the memory_order param.
      const int128_t result =
        __sync_val_compare_and_swap(obj_p128, old, *new_p128);

      if (result == old) return true;

      if (old_p128 != obj_p128) *old_p128 = result;
      return false;
#else
#  undef __ATOMIC_128__HAS_BUILTIN
      /* The non-lock-free DWCAS implies a costly library call with a
       * possible runtime capability check and/or locking, which kind
       * of defeats the purpose. Disable that implicit behavior and
       * force the user to pass the proper target architecture flags
       * (or avoid using DWCAS altogether) */
      static_assert(use_native,
                    "The DWCAS operation is not considered lock-free "
                    "on the target architecture. Try signaling the "
                    "availability of DWCAS instruction to the compiler "
                    "(e.g., by passing the -mcx16 or -march flags)");
      return false;
#endif
    }
  }

  bool compare_exchange_weak(T& old_val, const T& new_val,
            [[maybe_unused]] const mo_t mo_succ,
            [[maybe_unused]] const mo_t mo_fail) const noexcept {
    if constexpr (use_native)
      return
        std::atomic_ref(obj_).compare_exchange_weak(old_val, new_val,
                                                    mo_succ, mo_fail);
    else
      /* The intrinsic/built-in alternatives we use always create a
       * full memory barrier, so we can safely ignore the order param */
      return compare_exchange_weak(old_val, new_val);
  }

  bool compare_exchange_strong(T& old_val, const T& new_val,
                               const mo_t mo = mo_t::seq_cst) const noexcept {
    if constexpr (use_native)
      return
        std::atomic_ref(obj_).compare_exchange_strong(old_val, new_val, mo);
    else
      /* Also, the intrinsic/built-in alternatives we use offer
       * guarantees of a successful swap on equality, so we can just
       * do that */
      return compare_exchange_weak(old_val, new_val, mo);
  }

  bool compare_exchange_strong(T& old_val, const T& new_val,
              [[maybe_unused]] const mo_t mo_succ,
              [[maybe_unused]] const mo_t mo_fail) const noexcept {
    if constexpr (use_native)
      return
        std::atomic_ref(obj_).compare_exchange_strong(old_val, new_val,
                                                      mo_succ, mo_fail);
    else  // See above why
      return compare_exchange_weak(old_val, new_val);
  }

  T load(const mo_t mo = mo_t::seq_cst) const noexcept {
    if constexpr (use_native) {
      /* Although on x86 the compiler is probably doing the same
       * cmpxchg16b trick as below under the covers, we call it
       * directly anyway here just in case */
      return std::atomic_ref(obj_).load(mo);
    }
    else {
      T result = std::is_trivially_constructible_v<T> ? T{} : obj_;
      compare_exchange_weak(result, result, mo);
      return result;
    }
  }

  T exchange(const T& val, const mo_t mo = mo_t::seq_cst) const noexcept {
    if constexpr (use_native) {
      /* And please don't think we can just replace these conditions
       * with a using directive with std::conditional, that's just a
       * terrible idea that can lead to all sorts of porting
       * problems */
      return std::atomic_ref(obj_).exchange(val, mo);
    }
    else {
      T result = obj_;  // An additional non-atomic read is better
                        // than a failure of an expensive instruction
      while (!compare_exchange_weak(result, val, mo));
      return result;
    }
  }

  void store(const T& val, const mo_t mo = mo_t::seq_cst) const noexcept {
    if constexpr (use_native)
      std::atomic_ref(obj_).store(val, mo);
    else
      exchange(val, mo);
  }

  operator T() const noexcept {
    return load();
  }

  T operator=(const T& rhs) const noexcept {
    store(rhs);
    return rhs;
  }

  bool is_lock_free() const noexcept {
    return true;  // Because of the static_assert above, the class
                  // simply won't compile if it isn't lock-free
  }

  /* We don't mimic the wait/notify interface so that's it for now */

private:
  T& obj_;
};

} // namespace atomic128
