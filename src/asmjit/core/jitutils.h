// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_CORE_JITUTILS_H
#define _ASMJIT_CORE_JITUTILS_H

#include "../core/build.h"
#ifndef ASMJIT_DISABLE_JIT

// [Dependencies]
#include "../core/globals.h"

ASMJIT_BEGIN_NAMESPACE

//! \addtogroup asmjit_core
//! \{

// ============================================================================
// [asmjit::JitUtils]
// ============================================================================

namespace JitUtils {

//! Virtual memory flags.
enum AccessFlags : uint32_t {
  kAccessNone             = 0x00000000U, //!< No access flags.
  kAccessWrite            = 0x00000001U, //!< Memory is writable.
  kAccessExecute          = 0x00000002U, //!< Memory is executable.

  kAccessWriteExecute     = kAccessWrite |
                            kAccessExecute
};

//! Information related to virtual memory.
struct MemInfo {
  uint32_t pageSize;                     //!< Virtual memory page size.
  uint32_t pageGranularity;              //!< Virtual memory page granularity.
};

//! Get virtual memory information, see \ref JitUtils::MemInfo for more details.
ASMJIT_API MemInfo getMemInfo() noexcept;

//! Allocate virtual memory.
//!
//! NOTE: `size` should be aligned to page size, use `getMemInfo()` to obtain it.
ASMJIT_API void* virtualAlloc(size_t size, uint32_t accessFlags) noexcept;

//! Release virtual memory previously allocated by `JitUtils::virtualAlloc()`.
ASMJIT_API Error virtualRelease(void* p, size_t size) noexcept;

} // JitUtils

//! \}

ASMJIT_END_NAMESPACE

#endif
#endif
