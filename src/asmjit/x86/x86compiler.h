// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_X86_X86COMPILER_H
#define _ASMJIT_X86_X86COMPILER_H

#include "../asmjit_build.h"
#if !defined(ASMJIT_DISABLE_COMPILER)

// [Dependencies]
#include "../base/codecompiler.h"
#include "../base/simdtypes.h"
#include "../x86/x86emitter.h"

// [Api-Begin]
#include "../asmjit_apibegin.h"

namespace asmjit {

//! \addtogroup asmjit_x86
//! \{

// ============================================================================
// [asmjit::X86Compiler]
// ============================================================================

//! Architecture-dependent \ref CodeCompiler targeting X86 and X64.
class ASMJIT_VIRTAPI X86Compiler
  : public CodeCompiler,
    public X86EmitterExplicitT<X86Compiler> {
public:
  ASMJIT_NONCOPYABLE(X86Compiler)
  typedef CodeCompiler Base;

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create a `X86Compiler` instance.
  ASMJIT_API X86Compiler(CodeHolder* code = nullptr) noexcept;
  //! Destroy the `X86Compiler` instance.
  ASMJIT_API ~X86Compiler() noexcept;

  // --------------------------------------------------------------------------
  // [X86Emitter]
  // --------------------------------------------------------------------------

  //! Implicit cast to `X86Emitter&`.
  inline operator X86Emitter&() noexcept { return *as<X86Emitter>(); }
  //! Implicit cast to `X86Emitter&` (const).
  inline operator const X86Emitter&() const noexcept { return *as<X86Emitter>(); }

  // --------------------------------------------------------------------------
  // [Finalize]
  // --------------------------------------------------------------------------

  ASMJIT_API Error finalize() override;

  // --------------------------------------------------------------------------
  // [VirtReg]
  // --------------------------------------------------------------------------

#if !defined(ASMJIT_DISABLE_LOGGING)
# define ASMJIT_NEW_REG(OUT, PARAM, NAME_FMT)                 \
    std::va_list ap;                                          \
    va_start(ap, NAME_FMT);                                   \
    _newReg(OUT, PARAM, NAME_FMT, ap);                        \
    va_end(ap)
#else
# define ASMJIT_NEW_REG(OUT, PARAM, NAME_FMT)                 \
    ASMJIT_UNUSED(NAME_FMT);                                  \
    _newReg(OUT, PARAM, nullptr)
#endif

#define ASMJIT_NEW_REG_USER(FUNC, REG)                        \
    inline REG FUNC(uint32_t typeId) {                        \
      REG reg(Globals::NoInit);                               \
      _newReg(reg, typeId, nullptr);                          \
      return reg;                                             \
    }                                                         \
                                                              \
    inline REG FUNC(uint32_t typeId, const char* fmt, ...) {  \
      REG reg(Globals::NoInit);                               \
      ASMJIT_NEW_REG(reg, typeId, fmt);                       \
      return reg;                                             \
    }

#define ASMJIT_NEW_REG_AUTO(FUNC, REG, TYPE_ID)               \
    inline REG FUNC() {                                       \
      REG reg(Globals::NoInit);                               \
      _newReg(reg, TYPE_ID, nullptr);                         \
      return reg;                                             \
    }                                                         \
                                                              \
    inline REG FUNC(const char* fmt, ...) {                   \
      REG reg(Globals::NoInit);                               \
      ASMJIT_NEW_REG(reg, TYPE_ID, fmt);                      \
      return reg;                                             \
    }

  template<typename RegT>
  inline RegT newSimilarReg(const RegT& ref) {
    RegT reg(Globals::NoInit);
    _newReg(reg, ref, nullptr);
    return reg;
  }

  template<typename RegT>
  inline RegT newSimilarReg(const RegT& ref, const char* fmt, ...) {
    RegT reg(Globals::NoInit);
    ASMJIT_NEW_REG(reg, ref, fmt);
    return reg;
  }

  ASMJIT_NEW_REG_USER(newReg    , X86Reg )
  ASMJIT_NEW_REG_USER(newGpReg  , X86Gp  )
  ASMJIT_NEW_REG_USER(newMmReg  , X86Mm  )
  ASMJIT_NEW_REG_USER(newKReg   , X86KReg)
  ASMJIT_NEW_REG_USER(newVecReg , X86Vec )
  ASMJIT_NEW_REG_USER(newXmmReg , X86Xmm )
  ASMJIT_NEW_REG_USER(newYmmReg , X86Ymm )
  ASMJIT_NEW_REG_USER(newZmmReg , X86Zmm )

  ASMJIT_NEW_REG_AUTO(newI8     , X86Gp  , TypeId::kI8     )
  ASMJIT_NEW_REG_AUTO(newU8     , X86Gp  , TypeId::kU8     )
  ASMJIT_NEW_REG_AUTO(newI16    , X86Gp  , TypeId::kI16    )
  ASMJIT_NEW_REG_AUTO(newU16    , X86Gp  , TypeId::kU16    )
  ASMJIT_NEW_REG_AUTO(newI32    , X86Gp  , TypeId::kI32    )
  ASMJIT_NEW_REG_AUTO(newU32    , X86Gp  , TypeId::kU32    )
  ASMJIT_NEW_REG_AUTO(newI64    , X86Gp  , TypeId::kI64    )
  ASMJIT_NEW_REG_AUTO(newU64    , X86Gp  , TypeId::kU64    )
  ASMJIT_NEW_REG_AUTO(newInt8   , X86Gp  , TypeId::kI8     )
  ASMJIT_NEW_REG_AUTO(newUInt8  , X86Gp  , TypeId::kU8     )
  ASMJIT_NEW_REG_AUTO(newInt16  , X86Gp  , TypeId::kI16    )
  ASMJIT_NEW_REG_AUTO(newUInt16 , X86Gp  , TypeId::kU16    )
  ASMJIT_NEW_REG_AUTO(newInt32  , X86Gp  , TypeId::kI32    )
  ASMJIT_NEW_REG_AUTO(newUInt32 , X86Gp  , TypeId::kU32    )
  ASMJIT_NEW_REG_AUTO(newInt64  , X86Gp  , TypeId::kI64    )
  ASMJIT_NEW_REG_AUTO(newUInt64 , X86Gp  , TypeId::kU64    )
  ASMJIT_NEW_REG_AUTO(newIntPtr , X86Gp  , TypeId::kIntPtr )
  ASMJIT_NEW_REG_AUTO(newUIntPtr, X86Gp  , TypeId::kUIntPtr)

  ASMJIT_NEW_REG_AUTO(newGpb    , X86Gp  , TypeId::kU8     )
  ASMJIT_NEW_REG_AUTO(newGpw    , X86Gp  , TypeId::kU16    )
  ASMJIT_NEW_REG_AUTO(newGpd    , X86Gp  , TypeId::kU32    )
  ASMJIT_NEW_REG_AUTO(newGpq    , X86Gp  , TypeId::kU64    )
  ASMJIT_NEW_REG_AUTO(newGpz    , X86Gp  , TypeId::kUIntPtr)
  ASMJIT_NEW_REG_AUTO(newKb     , X86KReg, TypeId::kMask8  )
  ASMJIT_NEW_REG_AUTO(newKw     , X86KReg, TypeId::kMask16 )
  ASMJIT_NEW_REG_AUTO(newKd     , X86KReg, TypeId::kMask32 )
  ASMJIT_NEW_REG_AUTO(newKq     , X86KReg, TypeId::kMask64 )
  ASMJIT_NEW_REG_AUTO(newMm     , X86Mm  , TypeId::kMmx64  )
  ASMJIT_NEW_REG_AUTO(newXmm    , X86Xmm , TypeId::kI32x4  )
  ASMJIT_NEW_REG_AUTO(newXmmSs  , X86Xmm , TypeId::kF32x1  )
  ASMJIT_NEW_REG_AUTO(newXmmSd  , X86Xmm , TypeId::kF64x1  )
  ASMJIT_NEW_REG_AUTO(newXmmPs  , X86Xmm , TypeId::kF32x4  )
  ASMJIT_NEW_REG_AUTO(newXmmPd  , X86Xmm , TypeId::kF64x2  )
  ASMJIT_NEW_REG_AUTO(newYmm    , X86Ymm , TypeId::kI32x8  )
  ASMJIT_NEW_REG_AUTO(newYmmPs  , X86Ymm , TypeId::kF32x8  )
  ASMJIT_NEW_REG_AUTO(newYmmPd  , X86Ymm , TypeId::kF64x4  )
  ASMJIT_NEW_REG_AUTO(newZmm    , X86Zmm , TypeId::kI32x16 )
  ASMJIT_NEW_REG_AUTO(newZmmPs  , X86Zmm , TypeId::kF32x16 )
  ASMJIT_NEW_REG_AUTO(newZmmPd  , X86Zmm , TypeId::kF64x8  )

#undef ASMJIT_NEW_REG_AUTO
#undef ASMJIT_NEW_REG_USER
#undef ASMJIT_NEW_REG

  // --------------------------------------------------------------------------
  // [Stack]
  // --------------------------------------------------------------------------

  //! Create a new memory chunk allocated on the current function's stack.
  ASMJIT_INLINE X86Mem newStack(uint32_t size, uint32_t alignment, const char* name = nullptr) {
    X86Mem m(Globals::NoInit);
    _newStack(m, size, alignment, name);
    return m;
  }

  // --------------------------------------------------------------------------
  // [Const]
  // --------------------------------------------------------------------------

  //! Put data to a constant-pool and get a memory reference to it.
  ASMJIT_INLINE X86Mem newConst(uint32_t scope, const void* data, size_t size) {
    X86Mem m(Globals::NoInit);
    _newConst(m, scope, data, size);
    return m;
  }

  //! Put a BYTE `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newByteConst(uint32_t scope, uint8_t val) noexcept { return newConst(scope, &val, 1); }
  //! Put a WORD `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newWordConst(uint32_t scope, uint16_t val) noexcept { return newConst(scope, &val, 2); }
  //! Put a DWORD `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newDWordConst(uint32_t scope, uint32_t val) noexcept { return newConst(scope, &val, 4); }
  //! Put a QWORD `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newQWordConst(uint32_t scope, uint64_t val) noexcept { return newConst(scope, &val, 8); }

  //! Put a WORD `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newInt16Const(uint32_t scope, int16_t val) noexcept { return newConst(scope, &val, 2); }
  //! Put a WORD `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newUInt16Const(uint32_t scope, uint16_t val) noexcept { return newConst(scope, &val, 2); }
  //! Put a DWORD `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newInt32Const(uint32_t scope, int32_t val) noexcept { return newConst(scope, &val, 4); }
  //! Put a DWORD `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newUInt32Const(uint32_t scope, uint32_t val) noexcept { return newConst(scope, &val, 4); }
  //! Put a QWORD `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newInt64Const(uint32_t scope, int64_t val) noexcept { return newConst(scope, &val, 8); }
  //! Put a QWORD `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newUInt64Const(uint32_t scope, uint64_t val) noexcept { return newConst(scope, &val, 8); }

  //! Put a SP-FP `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newFloatConst(uint32_t scope, float val) noexcept { return newConst(scope, &val, 4); }
  //! Put a DP-FP `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newDoubleConst(uint32_t scope, double val) noexcept { return newConst(scope, &val, 8); }

  //! Put a MMX `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newMmConst(uint32_t scope, const Data64& val) noexcept { return newConst(scope, &val, 8); }
  //! Put a XMM `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newXmmConst(uint32_t scope, const Data128& val) noexcept { return newConst(scope, &val, 16); }
  //! Put a YMM `val` to a constant-pool.
  ASMJIT_INLINE X86Mem newYmmConst(uint32_t scope, const Data256& val) noexcept { return newConst(scope, &val, 32); }

  // --------------------------------------------------------------------------
  // [Instruction Options]
  // --------------------------------------------------------------------------

  //! Force the compiler to not follow the conditional or unconditional jump.
  ASMJIT_INLINE X86Compiler& unfollow() noexcept { _instOptions |= Inst::kOptionUnfollow; return *this; }
  //! Tell the compiler that the destination variable will be overwritten.
  ASMJIT_INLINE X86Compiler& overwrite() noexcept { _instOptions |= Inst::kOptionOverwrite; return *this; }

  // --------------------------------------------------------------------------
  // [Emit]
  // --------------------------------------------------------------------------

  //! Call a function.
  ASMJIT_INLINE CCFuncCall* call(const X86Gp& dst, const FuncSignature& sign) { return addCall(X86Inst::kIdCall, dst, sign); }
  //! \overload
  ASMJIT_INLINE CCFuncCall* call(const X86Mem& dst, const FuncSignature& sign) { return addCall(X86Inst::kIdCall, dst, sign); }
  //! \overload
  ASMJIT_INLINE CCFuncCall* call(const Label& label, const FuncSignature& sign) { return addCall(X86Inst::kIdCall, label, sign); }
  //! \overload
  ASMJIT_INLINE CCFuncCall* call(const Imm& dst, const FuncSignature& sign) { return addCall(X86Inst::kIdCall, dst, sign); }
  //! \overload
  ASMJIT_INLINE CCFuncCall* call(uint64_t dst, const FuncSignature& sign) { return addCall(X86Inst::kIdCall, Imm(int64_t(dst)), sign); }

  //! Return.
  ASMJIT_INLINE CCFuncRet* ret() { return addRet(Operand(), Operand()); }
  //! \overload
  ASMJIT_INLINE CCFuncRet* ret(const Reg& o0) { return addRet(o0, Operand()); }
  //! \overload
  ASMJIT_INLINE CCFuncRet* ret(const Reg& o0, const Reg& o1) { return addRet(o0, o1); }

  // --------------------------------------------------------------------------
  // [Events]
  // --------------------------------------------------------------------------

  ASMJIT_API Error onAttach(CodeHolder* code) noexcept override;
};

//! \}

} // asmjit namespace

// [Api-End]
#include "../asmjit_apiend.h"

// [Guard]
#endif // !ASMJIT_DISABLE_COMPILER
#endif // _ASMJIT_X86_X86COMPILER_H
