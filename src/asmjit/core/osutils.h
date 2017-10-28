// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_CORE_OSUTILS_H
#define _ASMJIT_CORE_OSUTILS_H

// [Dependencies]
#include "../core/globals.h"

ASMJIT_BEGIN_NAMESPACE

//! \addtogroup asmjit_core
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

  #if ASMJIT_OS_EMSCRIPTEN
  typedef uint32_t Handle;

  //! Create a new `Lock` instance.
  inline Lock() noexcept { _handle = 0; }
  //! Destroy the `Lock` instance.
  inline ~Lock() noexcept { ASMJIT_ASSERT(_handle == 0); }

  //! Lock.
  inline void lock() noexcept { _handle++; }
  //! Unlock.
  inline void unlock() noexcept { _handle--; }
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

ASMJIT_END_NAMESPACE

// [Guard]
#endif // _ASMJIT_CORE_OSUTILS_H
