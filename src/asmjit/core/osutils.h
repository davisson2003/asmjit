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
class Lock {
public:
  ASMJIT_NONCOPYABLE(Lock)

  #if ASMJIT_OS_WINDOWS

  typedef CRITICAL_SECTION Handle;

  inline Lock() noexcept { InitializeCriticalSection(&_handle); }
  inline ~Lock() noexcept { DeleteCriticalSection(&_handle); }

  inline void lock() noexcept { EnterCriticalSection(&_handle); }
  inline void unlock() noexcept { LeaveCriticalSection(&_handle); }

  Handle _handle;

  #elif ASMJIT_OS_POSIX && !ASMJIT_OS_BROWSER

  typedef pthread_mutex_t Handle;

  inline Lock() noexcept { pthread_mutex_init(&_handle, nullptr); }
  inline ~Lock() noexcept { pthread_mutex_destroy(&_handle); }

  inline void lock() noexcept { pthread_mutex_lock(&_handle); }
  inline void unlock() noexcept { pthread_mutex_unlock(&_handle); }

  Handle _handle;

  #else

  #ifndef ASMJIT_OS_BROWSER
  #pragma message("asmjit::Lock doesn't have implementation for your target OS.")
  #endif

  // Browser or other unsupported OS.
  inline Lock() noexcept {}
  inline ~Lock() noexcept {}

  inline void lock() noexcept {}
  inline void unlock() noexcept {}

  #endif
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
