// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define ASMJIT_EXPORTS

// [Dependencies]
#include "../base/intutils.h"
#include "../base/osutils.h"

#if ASMJIT_OS_POSIX
# include <time.h>
# include <unistd.h>
#endif

#if ASMJIT_OS_MAC
# include <mach/mach_time.h>
#endif

#if ASMJIT_OS_WINDOWS
# if defined(_MSC_VER) && _MSC_VER >= 1400
#  include <intrin.h>
# else
#  define _InterlockedCompareExchange InterlockedCompareExchange
# endif
#endif

// [Api-Begin]
#include "../asmjit_apibegin.h"

namespace asmjit {

// ============================================================================
// [asmjit::OSUtils - GetTickCount]
// ============================================================================

#if ASMJIT_OS_WINDOWS
static ASMJIT_INLINE uint32_t OSUtils_calcHiRes(const LARGE_INTEGER& now, double freq) noexcept {
  return uint32_t((int64_t)(double(now.QuadPart) / freq) & 0xFFFFFFFF);
}

uint32_t OSUtils::getTickCount() noexcept {
  static volatile uint32_t _hiResTicks;
  static volatile double _hiResFreq;

  do {
    uint32_t hiResOk = _hiResTicks;
    LARGE_INTEGER qpf, now;

    // If for whatever reason this fails, bail to `GetTickCount()`.
    if (!::QueryPerformanceCounter(&now)) break;

    // Expected - if we ran through this at least once `hiResTicks` will be
    // either 1 or 0xFFFFFFFF. If it's '1' then the Hi-Res counter is available
    // and `QueryPerformanceCounter()` can be used.
    if (hiResOk == 1) return OSUtils_calcHiRes(now, _hiResFreq);

    // Hi-Res counter is not available, bail to `GetTickCount()`.
    if (hiResOk != 0) break;

    // Detect availability of Hi-Res counter, if not available, bail to `GetTickCount()`.
    if (!::QueryPerformanceFrequency(&qpf)) {
      _InterlockedCompareExchange((LONG*)&_hiResTicks, 0xFFFFFFFF, 0);
      break;
    }

    double freq = double(qpf.QuadPart) / 1000.0;
    _hiResFreq = freq;

    _InterlockedCompareExchange((LONG*)&_hiResTicks, 1, 0);
    return OSUtils_calcHiRes(now, freq);
  } while (0);

  return ::GetTickCount();
}
#elif ASMJIT_OS_MAC
uint32_t OSUtils::getTickCount() noexcept {
  static mach_timebase_info_data_t _machTime;

  // See Apple's QA1398.
  if (ASMJIT_UNLIKELY(_machTime.denom == 0) || mach_timebase_info(&_machTime) != KERN_SUCCESS)
    return 0;

  // `mach_absolute_time()` returns nanoseconds, we want milliseconds.
  uint64_t t = mach_absolute_time() / 1000000;

  t = t * _machTime.numer / _machTime.denom;
  return uint32_t(t & 0xFFFFFFFFU);
}
#elif defined(_POSIX_MONOTONIC_CLOCK) && _POSIX_MONOTONIC_CLOCK >= 0
uint32_t OSUtils::getTickCount() noexcept {
  struct timespec ts;

  if (ASMJIT_UNLIKELY(clock_gettime(CLOCK_MONOTONIC, &ts) != 0))
    return 0;

  uint64_t t = (uint64_t(ts.tv_sec ) * 1000) + (uint64_t(ts.tv_nsec) / 1000000);
  return uint32_t(t & 0xFFFFFFFFU);
}
#else
#pragma message("asmjit::OSUtils::getTickCount() doesn't have implementation for your target OS.")
uint32_t OSUtils::getTickCount() noexcept { return 0; }
#endif

} // asmjit namespace

// [Api-End]
#include "../asmjit_apiend.h"
