// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define ASMJIT_EXPORTS

// [Guard]
#include "../asmjit_build.h"
#if defined(ASMJIT_BUILD_X86)

// [Dependencies]
#include "../base/misc_p.h"
#include "../x86/x86operand.h"

// [Api-Begin]
#include "../asmjit_apibegin.h"

namespace asmjit {

// ============================================================================
// [asmjit::X86OpData]
// ============================================================================

const X86OpData x86OpData = {
  // --------------------------------------------------------------------------
  // [ArchRegs]
  // --------------------------------------------------------------------------

  {
    {
#define ASMJIT_X86_REG_SIGNATURE(TYPE) { X86RegTraits<TYPE>::kSignature }
      ASMJIT_TABLE_16(ASMJIT_X86_REG_SIGNATURE,  0),
      ASMJIT_TABLE_16(ASMJIT_X86_REG_SIGNATURE, 16)
#undef ASMJIT_X86_REG_SIGNATURE
    },

    // RegCount[]
    { ASMJIT_TABLE_T_32(X86RegTraits, kCount, 0) },

    // RegTypeToTypeId[]
    { ASMJIT_TABLE_T_32(X86RegTraits, kTypeId, 0) }
  }
};

} // asmjit namespace

// [Api-End]
#include "../asmjit_apiend.h"

// [Guard]
#endif // ASMJIT_BUILD_X86
