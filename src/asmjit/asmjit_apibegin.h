// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Dependencies]
#if !defined(_ASMJIT_BUILD_H)
# include "./build.h"
#endif

// [Guard]
#if !defined(ASMJIT_API_SCOPE)
# define ASMJIT_API_SCOPE
#else
# error "[asmjit] api-scope is already active, previous scope not closed by asmjit_apiend.h?"
#endif

// ============================================================================
// [Compiler Support]
// ============================================================================

// [Clang]
#if ASMJIT_CC_CLANG
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wconstant-logical-operand"
# pragma clang diagnostic ignored "-Wunnamed-type-template-args"
#endif

// [GCC]
#if ASMJIT_CC_GCC
# pragma GCC diagnostic push
#endif

// [MSC]
#if ASMJIT_CC_MSC
# pragma warning(push)
# pragma warning(disable: 4127) // conditional expression is constant
# pragma warning(disable: 4201) // nameless struct/union
# pragma warning(disable: 4244) // '+=' : conversion from 'int' to 'x', possible loss of data
# pragma warning(disable: 4251) // struct needs to have dll-interface to be used by clients of struct ...
# pragma warning(disable: 4275) // non dll-interface struct ... used as base for dll-interface struct
# pragma warning(disable: 4355) // this used in base member initializer list
# pragma warning(disable: 4480) // specifying underlying type for enum
# pragma warning(disable: 4800) // forcing value to bool 'true' or 'false'
# if _MSC_VER < 1900
#  if !defined(vsnprintf)
#   define ASMJIT_UNDEF_VSNPRINTF
#   define vsnprintf _vsnprintf
#  endif
#  if !defined(snprintf)
#   define ASMJIT_UNDEF_SNPRINTF
#   define snprintf _snprintf
#  endif
# endif
#endif
