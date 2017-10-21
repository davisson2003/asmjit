// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// AsmJit Static Builds and Embedding
// ----------------------------------
//
// These definitions can be used to enable static library build. Embed is used
// when AsmJit's source code is embedded directly in another project, implies
// static build as well.
//
// #define ASMJIT_EMBED              // Asmjit is embedded (implies ASMJIT_STATIC).
// #define ASMJIT_STATIC             // Define to enable static-library build.

// AsmJit Build Mode
// -----------------
//
// These definitions control the build mode and tracing support. The build mode
// should be auto-detected at compile time, but it's possible to override it in
// case that the auto-detection fails.
//
// Tracing is a feature that is never compiled by default and it's only used to
// debug AsmJit itself.
//
// #define ASMJIT_BUILD_DEBUG        // Define to always use debug-mode (ASMJIT_ASSERT enabled).
// #define ASMJIT_BUILD_RELEASE      // Define to always use release-mode (ASMJIT_ASSERT disabled).

// AsmJit Build Backends
// ---------------------
//
// These definitions control which backends to compile. If none of these is
// defined AsmJit will use host architecture by default (for JIT code generation).
//
// #define ASMJIT_BUILD_X86          // Define to enable X86 and X64 code-generation.
// #define ASMJIT_BUILD_ARM          // Define to enable ARM32 and ARM64 code-generation.
// #define ASMJIT_BUILD_HOST         // Define to enable host instruction set.

// AsmJit Build Options
// --------------------
//
// Flags can be defined to disable standard features. These are handy especially
// when building AsmJit statically and some features are not needed or unwanted
// (like CodeCompiler).
//
// AsmJit features are enabled by default.
// #define ASMJIT_DISABLE_COMPILER   // Disable CodeCompiler (completely).
// #define ASMJIT_DISABLE_LOGGING    // Disable logging and formatting (completely).
// #define ASMJIT_DISABLE_TEXT       // Disable everything that contains text
//                                   // representation (instructions, errors, ...).
// #define ASMJIT_DISABLE_INST_API   // Disable API related to instruction database
//                                      (validation, cpu features, rw-info, etc).

// [Guard]
#ifndef _ASMJIT_CORE_BUILD_H
#define _ASMJIT_CORE_BUILD_H

// ============================================================================
// [asmjit::Build - Globals - Version]
// ============================================================================

#define ASMJIT_LIBRARY_VERSION ((1 << 16) | (0 << 8) | (0))

// ============================================================================
// [asmjit::Build - Globals - Target Operating System]
// ============================================================================

#if defined(_WIN32)
  #define ASMJIT_OS_WINDOWS   1
#else
  #define ASMJIT_OS_WINDOWS   0
#endif

#if defined(__linux__) || defined(__ANDROID__)
  #define ASMJIT_OS_LINUX     1
#else
  #define ASMJIT_OS_LINUX     0
#endif

#if defined(__ANDROID__)
  #define ASMJIT_OS_ANDROID   1
#else
  #define ASMJIT_OS_ANDROID   0
#endif

#if defined(__APPLE__)
  #define ASMJIT_OS_DARWIN    1
#else
  #define ASMJIT_OS_DARWIN    0
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__)    || \
    defined(__OpenBSD__) || defined(__DragonFly__) || \
    defined(__bsdi__)
  #define ASMJIT_OS_BSD       1
#else
  #define ASMJIT_OS_BSD       0
#endif

#define ASMJIT_OS_POSIX       (!ASMJIT_OS_WINDOWS)

// ============================================================================
// [asmjit::Build - Globals - Target Architecture]
// ============================================================================

#if defined(_M_X64) || defined(__amd64) || defined(__x86_64) || defined(__x86_64__)
  #define ASMJIT_ARCH_X86     64
#elif defined(_M_IX86) || defined(__i386) || defined(__i386__)
  #define ASMJIT_ARCH_X86     32
#else
  #define ASMJIT_ARCH_X86     0
#endif

#if defined(__ARM64__) || defined(__aarch64__)
  #define ASMJIT_ARCH_ARM     64
#elif (defined(_M_ARM  ) || defined(__arm    ) || defined(__thumb__ ) || \
       defined(_M_ARMT ) || defined(__arm__  ) || defined(__thumb2__))
  #define ASMJIT_ARCH_ARM     32
#else
  #define ASMJIT_ARCH_ARM     0
#endif

#if defined(_MIPS_ARCH_MIPS64) || defined(__mips64)
  #define ASMJIT_ARCH_MIPS    64
#elif defined(_MIPS_ARCH_MIPS32) || defined(_M_MRX000) || defined(__mips) || defined(__mips__)
  #define ASMJIT_ARCH_MIPS    32
#else
  #define ASMJIT_ARCH_MIPS    0
#endif

#define ASMJIT_ARCH_BITS (ASMJIT_ARCH_X86 | ASMJIT_ARCH_ARM | ASMJIT_ARCH_MIPS)
#if ASMJIT_ARCH_BITS == 0
  #undef ASMJIT_ARCH_BITS
  #if defined (__LP64__) || defined(_LP64)
    #define ASMJIT_ARCH_BITS  64
  #else
    #define ASMJIT_ARCH_BITS  32
  #endif
#endif

#if (defined(__ARMEB__)) || \
    (defined(__MIPSEB__)) || \
    (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
  #define ASMJIT_ARCH_LE      0
  #define ASMJIT_ARCH_BE      1
#else
  #define ASMJIT_ARCH_LE      1
  #define ASMJIT_ARCH_BE      0
#endif

// ============================================================================
// [asmjit::Build - Globals - C++ Compiler and Features Detection]
// ============================================================================

#define ASMJIT_CXX_CLANG      0
#define ASMJIT_CXX_INTEL      0
#define ASMJIT_CXX_GNU_ONLY   0
#define ASMJIT_CXX_MSC_ONLY   0
#define ASMJIT_CXX_VER(MAJOR, MINOR, PATCH) ((MAJOR) * 10000000 + (MINOR) * 100000 + (PATCH))

#if defined(__INTEL_COMPILER)
  // Intel compiler pretends to be GNU or MSC, so it must be checked first.
  //   https://software.intel.com/en-us/articles/c0x-features-supported-by-intel-c-compiler
  //   https://software.intel.com/en-us/articles/c14-features-supported-by-intel-c-compiler
  //   https://software.intel.com/en-us/articles/c17-features-supported-by-intel-c-compiler
  #undef ASMJIT_CXX_INTEL
  #define ASMJIT_CXX_INTEL ASMJIT_CXX_VER(__INTEL_COMPILER / 100, (__INTEL_COMPILER / 10) % 10, __INTEL_COMPILER % 10)
#elif defined(_MSC_VER) && defined(_MSC_FULL_VER)
  // MSC compiler:
  //   https://msdn.microsoft.com/en-us/library/hh567368.aspx
  //
  // Version List:
  //   16.00.0 == VS2010
  //   17.00.0 == VS2012
  //   18.00.0 == VS2013
  //   19.00.0 == VS2015
  //   19.10.0 == VS2017
  #undef ASMJIT_CXX_MSC_ONLY
  #if _MSC_VER == _MSC_FULL_VER / 10000
    #define ASMJIT_CXX_MSC_ONLY ASMJIT_CXX_VER(_MSC_VER / 100, _MSC_VER % 100, _MSC_FULL_VER % 10000)
  #else
    #define ASMJIT_CXX_MSC_ONLY ASMJIT_CXX_VER(_MSC_VER / 100, (_MSC_FULL_VER / 100000) % 100, _MSC_FULL_VER % 100000)
  #endif
#elif defined(__clang_major__) && defined(__clang_minor__) && defined(__clang_patchlevel__)
  // Clang compiler:
  #undef ASMJIT_CXX_CLANG
  #define ASMJIT_CXX_CLANG ASMJIT_CXX_VER(__clang_major__, __clang_minor__, __clang_patchlevel__)
#elif defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
  // GNU compiler:
  //   https://gcc.gnu.org/projects/cxx-status.html
  #undef ASMJIT_CXX_GNU_ONLY
  #define ASMJIT_CXX_GNU_ONLY ASMJIT_CXX_VER(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#endif

// GNU [Compatibility] - GNU compiler or compiler that is compatible with it.
#if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
  #define ASMJIT_CXX_GNU ASMJIT_CXX_VER(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#else
  #define ASMJIT_CXX_GNU 0
#endif

// MSC [Compatibility] - MSC compiler or compiler that is compatible with it
#if !ASMJIT_CXX_MSC_ONLY && defined(_MSC_VER)
  #define ASMJIT_CXX_MSC ASMJIT_CXX_VER(_MSC_VER / 100, _MSC_VER % 100, 0)
#else
  #define ASMJIT_CXX_MSC 0
#endif

// Compiler features detection macros.
#if ASMJIT_CXX_CLANG && defined(__has_builtin)
  #define ASMJIT_CXX_HAS_BUILTIN(NAME, ALTERNATIVE) (__has_builtin(NAME))
#else
  #define ASMJIT_CXX_HAS_BUILTIN(NAME, ALTERNATIVE) (!(!(ALTERNATIVE)))
#endif

#if ASMJIT_CXX_CLANG && defined(__has_extension)
  #define ASMJIT_CXX_HAS_FEATURE(NAME, ALTERNATIVE) (__has_extension(NAME))
#elif ASMJIT_CXX_CLANG && defined(__has_feature)
  #define ASMJIT_CXX_HAS_FEATURE(NAME, ALTERNATIVE) (__has_feature(NAME))
#else
  #define ASMJIT_CXX_HAS_FEATURE(NAME, ALTERNATIVE) (!(!(ALTERNATIVE)))
#endif

#if ASMJIT_CXX_CLANG && defined(__has_attribute)
  #define ASMJIT_CXX_HAS_ATTRIBUTE(NAME, ALTERNATIVE) (__has_attribute(NAME))
#else
  #define ASMJIT_CXX_HAS_ATTRIBUTE(NAME, ALTERNATIVE) (!(!(ALTERNATIVE)))
#endif

#if ASMJIT_CXX_CLANG && defined(__has_cpp_attribute)
  #define ASMJIT_CXX_HAS_CPP_ATTRIBUTE(NAME, ALTERNATIVE) (__has_cpp_attribute(NAME))
#else
  #define ASMJIT_CXX_HAS_CPP_ATTRIBUTE(NAME, ALTERNATIVE) (!(!(ALTERNATIVE)))
#endif

// Compiler features by vendor.
#if defined(ASMJIT_CXX_MSC_ONLY) && !defined(_NATIVE_WCHAR_T_DEFINED)
  #define ASMJIT_CXX_HAS_NATIVE_WCHAR_T 0
#else
  #define ASMJIT_CXX_HAS_NATIVE_WCHAR_T 1
#endif

#define ASMJIT_CXX_HAS_UNICODE_LITERALS \
  ASMJIT_CXX_HAS_FEATURE(__cxx_unicode_literals__, ( \
                        (ASMJIT_CXX_INTEL    >= ASMJIT_CXX_VER(14, 0, 0)) || \
                        (ASMJIT_CXX_MSC_ONLY >= ASMJIT_CXX_VER(19, 0, 0)) || \
                        (ASMJIT_CXX_GNU_ONLY >= ASMJIT_CXX_VER(4 , 5, 0) && __cplusplus >= 201103L) ))

// ============================================================================
// [asmjit::Build - Globals - API Decorators & Language Extensions]
// ============================================================================

// ASMJIT_EMBED implies ASMJIT_STATIC.
#if defined(ASMJIT_EMBED) && !defined(ASMJIT_STATIC)
  #define ASMJIT_STATIC
#endif

// API (Export / Import).
#if !defined(ASMJIT_API) && !defined(ASMJIT_STATIC)
  #if ASMJIT_OS_WINDOWS && (ASMJIT_CXX_MSC || defined(__MINGW32__))
    #if defined(ASMJIT_EXPORTS)
      #define ASMJIT_API __declspec(dllexport)
    #else
      #define ASMJIT_API __declspec(dllimport)
    #endif
  #elif ASMJIT_OS_WINDOWS && ASMJIT_CXX_GNU
    #if defined(ASMJIT_EXPORTS)
      #define ASMJIT_API __attribute__((dllexport))
    #else
      #define ASMJIT_API __attribute__((dllimport))
    #endif
  #elif ASMJIT_CXX_GNU >= ASMJIT_CXX_VER(4, 0, 0)
    #define ASMJIT_API __attribute__((visibility("default")))
  #endif
#endif

#if !defined(ASMJIT_API)
  #define ASMJIT_API
#endif

#if !defined(ASMJIT_VARAPI)
  #define ASMJIT_VARAPI extern ASMJIT_API
#endif

// This is basically a workaround. When using MSVC and marking class as DLL
// export everything gets exported, which is unwanted in most projects. MSVC
// automatically exports typeinfo and vtable if at least one symbol of the
// class is exported. However, GCC has some strange behavior that even if
// one or more symbol is exported it doesn't export typeinfo unless the
// class itself is decorated with "visibility(default)" (i.e. asmjit_API).
#if ASMJIT_CXX_GNU && !ASMJIT_OS_WINDOWS
  #define ASMJIT_VIRTAPI ASMJIT_API
#else
  #define ASMJIT_VIRTAPI
#endif

// Function attributes.
#if (ASMJIT_CXX_GNU >= ASMJIT_CXX_VER(4, 4, 0) && !defined(__MINGW32__))
  #define ASMJIT_FORCEINLINE inline __attribute__((always_inline))
#elif ASMJIT_CXX_MSC
  #define ASMJIT_FORCEINLINE __forceinline
#else
  #define ASMJIT_FORCEINLINE inline
#endif

#if ASMJIT_CXX_GNU
  #define ASMJIT_NOINLINE __attribute__((noinline))
  #define ASMJIT_NORETURN __attribute__((noreturn))
#elif ASMJIT_CXX_MSC
  #define ASMJIT_NOINLINE __declspec(noinline)
  #define ASMJIT_NORETURN __declspec(noreturn)
#else
  #define ASMJIT_NOINLINE
  #define ASMJIT_NORETURN
#endif

// Calling conventions.
#if ASMJIT_ARCH_X86 == 32 && ASMJIT_CXX_GNU
  #define ASMJIT_CDECL __attribute__((cdecl))
  #define ASMJIT_STDCALL __attribute__((stdcall))
  #define ASMJIT_FASTCALL __attribute__((fastcall))
  #define ASMJIT_REGPARM(N) __attribute__((regparm(N)))
#elif ASMJIT_ARCH_X86 == 32 && ASMJIT_CXX_MSC
  #define ASMJIT_CDECL __cdecl
  #define ASMJIT_STDCALL __stdcall
  #define ASMJIT_FASTCALL __fastcall
  #define ASMJIT_REGPARM(N)
#else
  #define ASMJIT_CDECL
  #define ASMJIT_STDCALL
  #define ASMJIT_FASTCALL
  #define ASMJIT_REGPARM(N)
#endif

// Type alignment (not allowed by C++11 'alignas' keyword).
#if ASMJIT_CXX_GNU
  #define ASMJIT_ALIGN_TYPE(TYPE, N) __attribute__((aligned(N))) TYPE
#elif ASMJIT_CXX_MSC
  #define ASMJIT_ALIGN_TYPE(TYPE, N) __declspec(align(N)) TYPE
#else
  #define ASMJIT_ALIGN_TYPE(TYPE, N) TYPE
#endif

// Annotations.
#if ASMJIT_CXX_HAS_BUILTIN(__builtin_expect, ASMJIT_CXX_GNU >= ASMJIT_CXX_VER(3, 0, 0))
  #define ASMJIT_LIKELY(EXP) __builtin_expect(!!(EXP), 1)
  #define ASMJIT_UNLIKELY(EXP) __builtin_expect(!!(EXP), 0)
#else
  #define ASMJIT_LIKELY(EXP) (EXP)
  #define ASMJIT_UNLIKELY(EXP) (EXP)
#endif

#if ASMJIT_CXX_CLANG && __cplusplus >= 201103L
  #define ASMJIT_FALLTHROUGH [[clang::fallthrough]]
#elif ASMJIT_CXX_GNU_ONLY >= ASMJIT_CXX_VER(7, 0, 0)
  #define ASMJIT_FALLTHROUGH __attribute__((fallthrough))
#else
  #define ASMJIT_FALLTHROUGH ((void)0) /* fallthrough */
#endif

#define ASMJIT_UNUSED(x) (void)(x)

// Utilities.
#define ASMJIT_UINT64_C(VALUE) static_cast<uint64_t>(VALUE##ull)
#define ASMJIT_OFFSET_OF(STRUCT, MEMBER) ((int)(intptr_t)((const char*)&((const STRUCT*)0x1)->MEMBER) - 1)
#define ASMJIT_ARRAY_SIZE(VAR) uint32_t(sizeof(VAR) / sizeof(VAR[0]))

// ============================================================================
// [asmjit::Build - Globals - Begin-Namespace / End-Namespace]
// ============================================================================

#if ASMJIT_CXX_CLANG
  #define ASMJIT_BEGIN_NAMESPACE                                               \
    namespace asmjit {                                                         \
      _Pragma("clang diagnostic push")                                         \
      _Pragma("clang diagnostic ignored \"-Wconstant-logical-operand\"")       \
      _Pragma("clang diagnostic ignored \"-Wunnamed-type-template-args\"")
  #define ASMJIT_END_NAMESPACE                                                 \
      _Pragma("clang diagnostic pop")                                          \
    }
#endif

#if ASMJIT_CXX_GNU_ONLY
  // Nothing atm.
#endif

#if ASMJIT_CXX_MSC_ONLY
  #define ASMJIT_BEGIN_NAMESPACE                                               \
    namespace asmjit {                                                         \
      __pragma(warning(push))                                                  \
      __pragma(warning(disable: 4127)) /* conditional expression is constant*/ \
      __pragma(warning(disable: 4201)) /* nameless struct/union             */
  #define ASMJIT_END_NAMESPACE                                                 \
      __pragma(warning(pop))                                                   \
    }
#endif

#if !defined(ASMJIT_BEGIN_NAMESPACE) && !defined(ASMJIT_END_NAMESPACE)
  #define ASMJIT_BEGIN_NAMESPACE namespace asmjit {
  #define ASMJIT_END_NAMESPACE }
#endif

// ============================================================================
// [asmjit::Build - Globals - Utilities]
// ============================================================================

#define ASMJIT_NONCOPYABLE(...)                                               \
  private:                                                                    \
    __VA_ARGS__(const __VA_ARGS__& other) = delete;                           \
    __VA_ARGS__& operator=(const __VA_ARGS__& other) = delete;                \
  public:

#define ASMJIT_NONCONSTRUCTIBLE(...)                                          \
  private:                                                                    \
    __VA_ARGS__() = delete;                                                   \
    __VA_ARGS__(const __VA_ARGS__& other) = delete;                           \
    __VA_ARGS__& operator=(const __VA_ARGS__& other) = delete;                \
  public:

// ============================================================================
// [asmjit::Build - Globals - Build]
// ============================================================================

// Internal macros that are only used when building AsmJit itself.
#if defined(ASMJIT_EXPORTS)
  // Code marked `ASMJIT_FAVOR_SIZE` can be compiled for size instead of speed.
  #if !defined(ASMJIT_BUILD_DEBUG) && ASMJIT_CXX_HAS_ATTRIBUTE(optimize, ASMJIT_CXX_GNU >= ASMJIT_CXX_VER(4, 4, 0))
    #define ASMJIT_FAVOR_SIZE __attribute__((optimize("Os")))
  #else
    #define ASMJIT_FAVOR_SIZE
  #endif

  // Only turn-off these warnings when building asmjit itself.
  #if defined(_MSC_VER)
    #if !defined(_CRT_SECURE_NO_DEPRECATE)
      #define _CRT_SECURE_NO_DEPRECATE
    #endif
    #if !defined(_CRT_SECURE_NO_WARNINGS)
      #define _CRT_SECURE_NO_WARNINGS
    #endif
  #endif
#endif

// ============================================================================
// [asmjit::Build - Globals - Configuration]
// ============================================================================

// Prevent compile-time errors caused by misconfiguration.
#if defined(ASMJIT_DISABLE_TEXT) && !defined(ASMJIT_DISABLE_LOGGING)
  #error "[asmjit] ASMJIT_DISABLE_TEXT requires ASMJIT_DISABLE_LOGGING to be defined."
#endif

// Detect ASMJIT_BUILD_DEBUG and ASMJIT_BUILD_RELEASE if not defined.
#if !defined(ASMJIT_BUILD_DEBUG) && !defined(ASMJIT_BUILD_RELEASE)
  #if !defined(NDEBUG)
    #define ASMJIT_BUILD_DEBUG
  #else
    #define ASMJIT_BUILD_RELEASE
  #endif
#endif

// Build host architecture if no architecture is selected.
#if !defined(ASMJIT_BUILD_HOST) && \
    !defined(ASMJIT_BUILD_X86)  && \
    !defined(ASMJIT_BUILD_ARM)
  #define ASMJIT_BUILD_HOST
#endif

// Detect host architecture if building only for host.
#if ASMJIT_ARCH_X86 && defined(ASMJIT_BUILD_HOST) && !defined(ASMJIT_BUILD_X86)
  #define ASMJIT_BUILD_X86
#endif

#if ASMJIT_ARCH_ARM && defined(ASMJIT_BUILD_HOST) && !defined(ASMJIT_BUILD_ARM)
  #define ASMJIT_BUILD_ARM
#endif

// ============================================================================
// [asmjit::Build - Globals - Dependencies]
// ============================================================================

// We really want std-types as globals.
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>

#if ASMJIT_OS_WINDOWS
  #if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
    #define ASMJIT_UNDEF_WIN32_LEAN_AND_MEAN
  #endif
  #if !defined(NOMINMAX)
    #define NOMINMAX
    #define ASMJIT_UNDEF_NOMINMAX
  #endif
  #include <windows.h>
  #if defined(ASMJIT_UNDEF_NOMINMAX)
    #undef NOMINMAX
    #undef ASMJIT_UNDEF_NOMINMAX
  #endif
  #if defined(ASMJIT_UNDEF_WIN32_LEAN_AND_MEAN)
    #undef WIN32_LEAN_AND_MEAN
    #undef ASMJIT_UNDEF_WIN32_LEAN_AND_MEAN
 #endif
#endif

#if ASMJIT_OS_POSIX
  #include <pthread.h>
#endif

// ============================================================================
// [asmjit::Build - Globals - Unit-Tests]
// ============================================================================

// VS-ONLY: Allows to see all tests highlighted instead of gray.
#if defined(__INTELLISENSE__) && !defined(ASMJIT_TEST)
  #define ASMJIT_TEST
#endif

// Include a unit testing package if this is a `asmjit_test` build.
#if defined(ASMJIT_TEST)
  #include "../../../test/broken.h"
#endif

// [Guard]
#endif // _ASMJIT_CORE_BUILD_H