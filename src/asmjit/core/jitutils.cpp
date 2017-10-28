// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define ASMJIT_EXPORTS

#include "../core/build.h"
#ifndef ASMJIT_DISABLE_JIT

// [Dependencies]
#include "../core/intutils.h"
#include "../core/jitutils.h"

#if ASMJIT_OS_POSIX
  #include <sys/types.h>
  #include <sys/mman.h>
  #include <unistd.h>
#endif

ASMJIT_BEGIN_NAMESPACE

// ============================================================================
// [asmjit::JitUtils - Virtual Memory (Windows)]
// ============================================================================

// Windows specific implementation using `VirtualAlloc` and `VirtualFree`.
#if ASMJIT_OS_WINDOWS
JitUtils::MemInfo JitUtils::getMemInfo() noexcept {
  MemInfo memInfo;
  SYSTEM_INFO systemInfo;

  ::GetSystemInfo(&systemInfo);
  memInfo.pageSize = IntUtils::alignUpPowerOf2<uint32_t>(systemInfo.dwPageSize);
  memInfo.pageGranularity = systemInfo.dwAllocationGranularity;

  return memInfo;
}

void* JitUtils::virtualAlloc(size_t size, uint32_t flags) noexcept {
  if (size == 0)
    return nullptr;

  // Windows XP-SP2, Vista and newer support data-execution-prevention (DEP).
  DWORD protectFlags = 0;
  if (flags & kAccessExecute)
    protectFlags |= (flags & kAccessWrite) ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
  else
    protectFlags |= (flags & kAccessWrite) ? PAGE_READWRITE : PAGE_READONLY;
  return ::VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protectFlags);
}

Error JitUtils::virtualRelease(void* p, size_t size) noexcept {
  ASMJIT_UNUSED(size);
  if (ASMJIT_UNLIKELY(!::VirtualFree(p, 0, MEM_RELEASE)))
    return DebugUtils::errored(kErrorInvalidState);
  return kErrorOk;
}
#endif

// ============================================================================
// [asmjit::JitUtils - Virtual Memory (Posix)]
// ============================================================================

// Posix specific implementation using `mmap()` and `munmap()`.
#if ASMJIT_OS_POSIX

// BSD/OSX: `MAP_ANONYMOUS` is not defined, `MAP_ANON` is.
#if !defined(MAP_ANONYMOUS)
  #define MAP_ANONYMOUS MAP_ANON
#endif

JitUtils::MemInfo JitUtils::getMemInfo() noexcept {
  MemInfo memInfo;

  uint32_t pageSize = uint32_t(::getpagesize());
  memInfo.pageSize = pageSize;
  memInfo.pageGranularity = std::max<uint32_t>(pageSize, 65536);

  return memInfo;
}

void* JitUtils::virtualAlloc(size_t size, uint32_t flags) noexcept {
  int protection = PROT_READ;
  if (flags & kAccessWrite  ) protection |= PROT_WRITE;
  if (flags & kAccessExecute) protection |= PROT_EXEC;

  void* mbase = ::mmap(nullptr, size, protection, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ASMJIT_UNLIKELY(mbase == MAP_FAILED)) return nullptr;

  return mbase;
}

Error JitUtils::virtualRelease(void* p, size_t size) noexcept {
  if (ASMJIT_UNLIKELY(::munmap(p, size) != 0))
    return DebugUtils::errored(kErrorInvalidState);

  return kErrorOk;
}
#endif

ASMJIT_END_NAMESPACE

#endif
