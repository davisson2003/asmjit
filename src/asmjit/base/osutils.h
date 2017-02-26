// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_BASE_OSUTILS_H
#define _ASMJIT_BASE_OSUTILS_H

// [Dependencies]
#include "../base/globals.h"

// [Api-Begin]
#include "../asmjit_apibegin.h"

namespace asmjit {

//! \addtogroup asmjit_base
//! \{

// ============================================================================
// [asmjit::OSUtils]
// ============================================================================

//! OS utilities.
//!
//! Benchmarking
//! ------------
//!
//! OSUtils also provide a function `getTickCount()` that can be used for
//! benchmarking purposes. It's similar to Windows-only `GetTickCount()`, but
//! it's cross-platform and tries to be the most reliable platform specific
//! calls to make the result usable.
namespace OSUtils {
  //! Get the current CPU tick count, used for benchmarking (1ms resolution).
  ASMJIT_API uint32_t getTickCount() noexcept;
};

// ============================================================================
// [asmjit::Lock]
// ============================================================================

//! \internal
//!
//! Lock.
struct Lock {
  ASMJIT_NONCOPYABLE(Lock)

  // --------------------------------------------------------------------------
  // [Windows]
  // --------------------------------------------------------------------------

#if ASMJIT_OS_WINDOWS
  typedef CRITICAL_SECTION Handle;

  //! Create a new `Lock` instance.
  inline Lock() noexcept { InitializeCriticalSection(&_handle); }
  //! Destroy the `Lock` instance.
  inline ~Lock() noexcept { DeleteCriticalSection(&_handle); }

  //! Lock.
  inline void lock() noexcept { EnterCriticalSection(&_handle); }
  //! Unlock.
  inline void unlock() noexcept { LeaveCriticalSection(&_handle); }
#endif

  // --------------------------------------------------------------------------
  // [Posix]
  // --------------------------------------------------------------------------

#if ASMJIT_OS_POSIX
  typedef pthread_mutex_t Handle;

  //! Create a new `Lock` instance.
  inline Lock() noexcept { pthread_mutex_init(&_handle, nullptr); }
  //! Destroy the `Lock` instance.
  inline ~Lock() noexcept { pthread_mutex_destroy(&_handle); }

  //! Lock.
  inline void lock() noexcept { pthread_mutex_lock(&_handle); }
  //! Unlock.
  inline void unlock() noexcept { pthread_mutex_unlock(&_handle); }
#endif

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Native handle.
  Handle _handle;
};

// ============================================================================
// [asmjit::AutoLock]
// ============================================================================

//! \internal
//!
//! Scoped lock.
struct AutoLock {
  ASMJIT_NONCOPYABLE(AutoLock)

  inline AutoLock(Lock& target) noexcept : _target(target) { _target.lock(); }
  inline ~AutoLock() noexcept { _target.unlock(); }

  Lock& _target;
};

//! \}

} // asmjit namespace

// [Api-End]
#include "../asmjit_apiend.h"

// [Guard]
#endif // _ASMJIT_BASE_OSUTILS_H
