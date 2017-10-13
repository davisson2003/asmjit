// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#if defined(ASMJIT_API_SCOPE)
# undef ASMJIT_API_SCOPE
#else
# error "[asmjit] api-scope not active, forgot to include asmjit_apibegin.h?"
#endif

// ============================================================================
// [Compiler Support]
// ============================================================================

// [Clang]
#if ASMJIT_CC_CLANG
# pragma clang diagnostic pop
#endif

// [GCC]
#if ASMJIT_CC_GCC
# pragma GCC diagnostic pop
#endif
