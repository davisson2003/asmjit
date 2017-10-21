// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_CORE_FUNC_H
#define _ASMJIT_CORE_FUNC_H

// [Dependencies]
#include "../core/arch.h"
#include "../core/intutils.h"
#include "../core/operand.h"
#include "../core/type.h"

ASMJIT_BEGIN_NAMESPACE

//! \addtogroup asmjit_base
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

class CodeEmitter;

// ============================================================================
// [asmjit::CallConv]
// ============================================================================

//! Function calling convention.
//!
//! Function calling convention is a scheme that defines how function parameters
//! are passed and how function returns its result. AsmJit defines a variety of
//! architecture and OS specific calling conventions and also provides a compile
//! time detection to make the code-generation easier.
struct CallConv {
  //! Calling convention id.
  enum Id : uint32_t {
    //! None or invalid (can't be used).
    kIdNone = 0,

    // ------------------------------------------------------------------------
    // [Universal]
    // ------------------------------------------------------------------------

    // TODO: To make this possible we need to know target ARCH and ABI.

    /*

    // Universal calling conventions are applicable to any target and are
    // converted to target dependent conventions at runtime. The purpose of
    // these conventions is to make using functions less target dependent.

    kIdCDecl = 1,
    kIdStdCall = 2,
    kIdFastCall = 3,

    //! AsmJit specific calling convention designed for calling functions
    //! inside a multimedia code like that don't use many registers internally,
    //! but are long enough to be called and not inlined. These functions are
    //! usually used to calculate trigonometric functions, logarithms, etc...
    kIdLightCall2 = 10,
    kIdLightCall3 = 11,
    kIdLightCall4 = 12,
    */

    // ------------------------------------------------------------------------
    // [X86]
    // ------------------------------------------------------------------------

    //! X86 `__cdecl` calling convention (used by C runtime and libraries).
    kIdX86CDecl = 16,
    //! X86 `__stdcall` calling convention (used mostly by WinAPI).
    kIdX86StdCall = 17,
    //! X86 `__thiscall` calling convention (MSVC/Intel).
    kIdX86MsThisCall = 18,
    //! X86 `__fastcall` convention (MSVC/Intel).
    kIdX86MsFastCall = 19,
    //! X86 `__fastcall` convention (GCC and Clang).
    kIdX86GccFastCall = 20,
    //! X86 `regparm(1)` convention (GCC and Clang).
    kIdX86GccRegParm1 = 21,
    //! X86 `regparm(2)` convention (GCC and Clang).
    kIdX86GccRegParm2 = 22,
    //! X86 `regparm(3)` convention (GCC and Clang).
    kIdX86GccRegParm3 = 23,

    kIdX86LightCall2 = 29,
    kIdX86LightCall3 = 30,
    kIdX86LightCall4 = 31,

    //! X64 calling convention - WIN64-ABI.
    kIdX86Win64 = 32,
    //! X64 calling convention - SystemV / AMD64-ABI.
    kIdX86SysV64 = 33,

    kIdX64LightCall2 = 45,
    kIdX64LightCall3 = 46,
    kIdX64LightCall4 = 47,

    // ------------------------------------------------------------------------
    // [ARM]
    // ------------------------------------------------------------------------

    //! Legacy calling convention, floating point arguments are passed via GP registers.
    kIdArm32SoftFP = 48,
    //! Modern calling convention, uses VFP registers to pass floating point arguments.
    kIdArm32HardFP = 49,

    // ------------------------------------------------------------------------
    // [Internal]
    // ------------------------------------------------------------------------

    _kIdX86Start = 16,  //!< \internal
    _kIdX86End = 31,    //!< \internal

    _kIdX64Start = 32,  //!< \internal
    _kIdX64End = 47,    //!< \internal

    _kIdArmStart = 48,  //!< \internal
    _kIdArmEnd = 49,    //!< \internal

    // ------------------------------------------------------------------------
    // [Host]
    // ------------------------------------------------------------------------

    #if defined(ASMJIT_DOCGEN)
      //! Default calling convention based on the current C++ compiler's settings.
      //!
      //! NOTE: This should be always the same as `kIdHostCDecl`, but some
      //! compilers allow to override the default calling convention. Overriding
      //! is not detected at the moment.
      kIdHost          = DETECTED_AT_COMPILE_TIME,
      //! Default CDECL calling convention based on the current C++ compiler's settings.
      kIdHostCDecl     = DETECTED_AT_COMPILE_TIME,
      //! Default STDCALL calling convention based on the current C++ compiler's settings.
      //!
      //! NOTE: If not defined by the host then it's the same as `kIdHostCDecl`.
      kIdHostStdCall   = DETECTED_AT_COMPILE_TIME,
      //! Compatibility for `__fastcall` calling convention.
      //!
      //! NOTE: If not defined by the host then it's the same as `kIdHostCDecl`.
      kIdHostFastCall  = DETECTED_AT_COMPILE_TIME
    #elif ASMJIT_ARCH_X86 == 32
      kIdHost          = kIdX86CDecl,
      kIdHostCDecl     = kIdX86CDecl,
      kIdHostStdCall   = kIdX86StdCall,
      kIdHostFastCall  = ASMJIT_CXX_MSC ? kIdX86MsFastCall  :
                        ASMJIT_CXX_GNU ? kIdX86GccFastCall : kIdNone,
      kIdHostLightCall2 = kIdX86LightCall2,
      kIdHostLightCall3 = kIdX86LightCall3,
      kIdHostLightCall4 = kIdX86LightCall4
    #elif ASMJIT_ARCH_X86 == 64
      kIdHost          = ASMJIT_OS_WINDOWS ? kIdX86Win64 : kIdX86SysV64,
      kIdHostCDecl     = kIdHost, // Doesn't exist, redirected to host.
      kIdHostStdCall   = kIdHost, // Doesn't exist, redirected to host.
      kIdHostFastCall  = kIdHost, // Doesn't exist, redirected to host.
      kIdHostLightCall2 = kIdX64LightCall2,
      kIdHostLightCall3 = kIdX64LightCall3,
      kIdHostLightCall4 = kIdX64LightCall4
    #elif ASMJIT_ARCH_ARM == 32
      #if defined(__SOFTFP__)
      kIdHost          = kIdArm32SoftFP,
      #else
      kIdHost          = kIdArm32HardFP,
      #endif
      // These don't exist on ARM.
      kIdHostCDecl     = kIdHost, // Doesn't exist, redirected to host.
      kIdHostStdCall   = kIdHost, // Doesn't exist, redirected to host.
      kIdHostFastCall  = kIdHost  // Doesn't exist, redirected to host.
    #else
      #error "[asmjit] Couldn't determine the target's calling convention."
    #endif
  };

  //! Strategy used to assign registers to function arguments.
  //!
  //! This is AsmJit specific. It basically describes how should AsmJit convert
  //! the function arguments defined by `FuncSignature` into register IDs or
  //! stack offsets. The default strategy assigns registers and then stack.
  //! The Win64 strategy does register shadowing as defined by `WIN64` calling
  //! convention - it applies to 64-bit calling conventions only.
  enum Strategy : uint32_t {
    kStrategyDefault     = 0,            //!< Default register assignment strategy.
    kStrategyWin64       = 1             //!< WIN64 specific register assignment strategy.
  };

  //! Calling convention flags.
  enum Flags : uint32_t {
    kFlagCalleePopsStack = 0x01,         //!< Callee is responsible for cleaning up the stack.
    kFlagPassFloatsByVec = 0x02,         //!< Pass F32 and F64 arguments by VEC128 register.
    kFlagVectorCall      = 0x04,         //!< This is a '__vectorcall' calling convention.
    kFlagIndirectVecArgs = 0x08          //!< Pass vector arguments indirectly (as a pointer).
  };

  //! Internal limits of AsmJit/CallConv.
  enum Limits : uint32_t {
    kMaxRegArgsPerGroup  = 16
  };

  //! Passed registers' order.
  union RegOrder {
    uint8_t id[kMaxRegArgsPerGroup];     //!< Passed registers, ordered.
    uint32_t packed[(kMaxRegArgsPerGroup + 3) / 4];
  };

  // --------------------------------------------------------------------------
  // [Utilities]
  // --------------------------------------------------------------------------

  static inline bool isX86Family(uint32_t ccId) noexcept { return ccId >= _kIdX86Start && ccId <= _kIdX64End; }
  static inline bool isArmFamily(uint32_t ccId) noexcept { return ccId >= _kIdArmStart && ccId <= _kIdArmEnd; }

  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  ASMJIT_API Error init(uint32_t ccId) noexcept;

  inline void reset() noexcept {
    std::memset(this, 0, sizeof(*this));
    std::memset(_passedOrder, 0xFF, sizeof(_passedOrder));
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get calling convention id, see \ref Id.
  inline uint32_t getId() const noexcept { return _id; }
  //! Set calling convention id, see \ref Id.
  inline void setId(uint32_t id) noexcept { _id = uint8_t(id); }

  //! Get architecture type.
  inline uint32_t getArchType() const noexcept { return _archType; }
  //! Set architecture type.
  inline void setArchType(uint32_t archType) noexcept { _archType = uint8_t(archType); }

  //! Get a strategy used to assign registers to arguments, see \ref Strategy.
  inline uint32_t getStrategy() const noexcept { return _strategy; }
  //! Set a strategy used to assign registers to arguments, see \ref Strategy.
  inline void setStrategy(uint32_t strategy) noexcept { _strategy = uint8_t(strategy); }

  //! Get whether the calling convention has the given `flag` set.
  inline bool hasFlag(uint32_t flag) const noexcept { return (uint32_t(_flags) & flag) != 0; }
  //! Get calling convention flags, see \ref Flags.
  inline uint32_t getFlags() const noexcept { return _flags; }
  //! Add calling convention flags, see \ref Flags.
  inline void setFlags(uint32_t flag) noexcept { _flags = uint8_t(flag); };
  //! Add calling convention flags, see \ref Flags.
  inline void addFlags(uint32_t flags) noexcept { _flags = uint8_t(_flags | flags); };

  //! Get whether this calling convention specifies 'RedZone'.
  inline bool hasRedZone() const noexcept { return _redZoneSize != 0; }
  //! Get size of 'RedZone'.
  inline uint32_t getRedZoneSize() const noexcept { return _redZoneSize; }
  //! Set size of 'RedZone'.
  inline void setRedZoneSize(uint32_t size) noexcept { _redZoneSize = uint8_t(size); }

  //! Get whether this calling convention specifies 'SpillZone'.
  inline bool hasSpillZone() const noexcept { return _spillZoneSize != 0; }
  //! Get size of 'SpillZone'.
  inline uint32_t getSpillZoneSize() const noexcept { return _spillZoneSize; }
  //! Set size of 'SpillZone'.
  inline void setSpillZoneSize(uint32_t size) noexcept { _spillZoneSize = uint8_t(size); }

  //! Get a natural stack alignment.
  inline uint32_t getNaturalStackAlignment() const noexcept { return _naturalStackAlignment; }
  //! Set a natural stack alignment.
  //!
  //! This function can be used to override the default stack alignment in case
  //! that you know that it's alignment is different. For example it allows to
  //! implement custom calling conventions that guarantee higher stack alignment.
  inline void setNaturalStackAlignment(uint32_t value) noexcept { _naturalStackAlignment = uint8_t(value); }

  inline const uint8_t* getPassedOrder(uint32_t group) const noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    return _passedOrder[group].id;
  }

  inline uint32_t getPassedRegs(uint32_t group) const noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    return _passedRegs[group];
  }

  inline void _setPassedPacked(uint32_t group, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3) noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);

    _passedOrder[group].packed[0] = p0;
    _passedOrder[group].packed[1] = p1;
    _passedOrder[group].packed[2] = p2;
    _passedOrder[group].packed[3] = p3;
  }

  inline void setPassedToNone(uint32_t group) noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);

    _setPassedPacked(group, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU);
    _passedRegs[group] = 0U;
  }

  inline void setPassedOrder(uint32_t group, uint32_t a0, uint32_t a1 = 0xFF, uint32_t a2 = 0xFF, uint32_t a3 = 0xFF, uint32_t a4 = 0xFF, uint32_t a5 = 0xFF, uint32_t a6 = 0xFF, uint32_t a7 = 0xFF) noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);

    // NOTE: This should always be called with all arguments known at compile time,
    // so even if it looks scary it should be translated into few instructions.
    _setPassedPacked(group, IntUtils::bytepack32_4x8(a0, a1, a2, a3),
                            IntUtils::bytepack32_4x8(a4, a5, a6, a7),
                            0xFFFFFFFFU,
                            0xFFFFFFFFU);

    _passedRegs[group] = (a0 != 0xFF ? 1U << a0 : 0U) |
                         (a1 != 0xFF ? 1U << a1 : 0U) |
                         (a2 != 0xFF ? 1U << a2 : 0U) |
                         (a3 != 0xFF ? 1U << a3 : 0U) |
                         (a4 != 0xFF ? 1U << a4 : 0U) |
                         (a5 != 0xFF ? 1U << a5 : 0U) |
                         (a6 != 0xFF ? 1U << a6 : 0U) |
                         (a7 != 0xFF ? 1U << a7 : 0U) ;
  }

  inline uint32_t getPreservedRegs(uint32_t group) const noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    return _preservedRegs[group];
  }

  inline void setPreservedRegs(uint32_t group, uint32_t regs) noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    _preservedRegs[group] = regs;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint8_t _id;                           //!< Calling convention id, see \ref Id.
  uint8_t _archType;                     //!< Architecture type (see \ref ArchInfo::Type).
  uint8_t _strategy;                     //!< Register assignment strategy.
  uint8_t _flags;                        //!< Flags.

  uint8_t _redZoneSize;                  //!< Red zone size (AMD64 == 128 bytes).
  uint8_t _spillZoneSize;                //!< Spill zone size (WIN64 == 32 bytes).
  uint8_t _naturalStackAlignment;        //!< Natural stack alignment as defined by OS/ABI.
  uint8_t _reserved[1];

  uint32_t _passedRegs[Reg::kGroupVirt];    //!< Mask of all passed registers, per group.
  uint32_t _preservedRegs[Reg::kGroupVirt]; //!< Mask of all preserved registers, per group.
  RegOrder _passedOrder[Reg::kGroupVirt];   //!< Passed registers' order, per group.
};

// ============================================================================
// [asmjit::FuncArgIndex]
// ============================================================================

//! Function argument index (lo/hi).
enum FuncArgIndex : uint32_t {
  //! Maximum number of function arguments supported by AsmJit.
  kFuncArgCount = 16,
  //! Extended maximum number of arguments (used internally).
  kFuncArgCountLoHi = kFuncArgCount * 2,

  //! Index to the LO part of function argument (default).
  //!
  //! This value is typically omitted and added only if there is HI argument
  //! accessed.
  kFuncArgLo = 0,

  //! Index to the HI part of function argument.
  //!
  //! HI part of function argument depends on target architecture. On x86 it's
  //! typically used to transfer 64-bit integers (they form a pair of 32-bit
  //! integers).
  kFuncArgHi = kFuncArgCount
};

// ============================================================================
// [asmjit::FuncSignature]
// ============================================================================

//! Function signature.
//!
//! Contains information about function return type, count of arguments and
//! their TypeIds. Function signature is a low level structure which doesn't
//! contain platform specific or calling convention specific information.
struct FuncSignature {
  enum {
    //! Doesn't have variable number of arguments (`...`).
    kNoVarArgs = 0xFF
  };

  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  //! Initialize the function signature.
  inline void init(uint32_t ccId, uint32_t ret, const uint8_t* args, uint32_t argCount) noexcept {
    ASMJIT_ASSERT(ccId <= 0xFF);
    ASMJIT_ASSERT(argCount <= 0xFF);

    _callConv = uint8_t(ccId);
    _argCount = uint8_t(argCount);
    _vaIndex = kNoVarArgs;
    _ret = uint8_t(ret);
    _args = args;
  }

  inline void reset() noexcept { std::memset(this, 0, sizeof(*this)); }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the function's calling convention.
  inline uint32_t getCallConv() const noexcept { return _callConv; }

  //! Get whether the function has variable number of arguments (...).
  inline bool hasVarArgs() const noexcept { return _vaIndex != kNoVarArgs; }
  //! Get the variable arguments (...) index, `kNoVarArgs` if none.
  inline uint32_t getVAIndex() const noexcept { return _vaIndex; }

  //! Get the number of function arguments.
  inline uint32_t getArgCount() const noexcept { return _argCount; }

  inline bool hasRet() const noexcept { return _ret != Type::kIdVoid; }
  //! Get the return value type.
  inline uint32_t getRet() const noexcept { return _ret; }

  //! Get the type of the argument at index `i`.
  inline uint32_t getArg(uint32_t i) const noexcept {
    ASMJIT_ASSERT(i < _argCount);
    return _args[i];
  }
  //! Get the array of function arguments' types.
  inline const uint8_t* getArgs() const noexcept { return _args; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint8_t _callConv;                     //!< Calling convention id.
  uint8_t _argCount;                     //!< Count of arguments.
  uint8_t _vaIndex;                      //!< Index of a first VA or `kNoVarArgs`.
  uint8_t _ret;                          //!< Return value TypeId.
  const uint8_t* _args;                  //!< Function arguments TypeIds.
};

// ============================================================================
// [asmjit::FuncSignatureT]
// ============================================================================

template<typename... RET_ARGS>
class FuncSignatureT : public FuncSignature {
public:
  inline FuncSignatureT(uint32_t ccId = CallConv::kIdHost) noexcept {
    static const uint8_t ret_args[] = { (uint8_t(Type::IdOfT<RET_ARGS>::kTypeId))... };
    init(ccId, ret_args[0], ret_args + 1, uint32_t(ASMJIT_ARRAY_SIZE(ret_args) - 1));
  }
};

// ============================================================================
// [asmjit::FuncSignatureX]
// ============================================================================

//! Dynamic function signature.
class FuncSignatureX : public FuncSignature {
public:
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline FuncSignatureX(uint32_t ccId = CallConv::kIdHost) noexcept {
    init(ccId, Type::kIdVoid, _builderArgList, 0);
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline void setCallConv(uint32_t ccId) noexcept { _callConv = uint8_t(ccId); }

  //! Set the return type to `retType`.
  inline void setRet(uint32_t retType) noexcept { _ret = uint8_t(retType); }
  //! Set the return type based on `T`.
  template<typename T>
  inline void setRetT() noexcept { setRet(Type::IdOfT<T>::kTypeId); }

  //! Set the argument at index `i` to `argType`.
  inline void setArg(uint32_t i, uint32_t argType) noexcept {
    ASMJIT_ASSERT(i < _argCount);
    _builderArgList[i] = uint8_t(argType);
  }
  //! Set the argument at index `i` to the type based on `T`.
  template<typename T>
  inline void setArgT(uint32_t i) noexcept { setArg(i, Type::IdOfT<T>::kTypeId); }

  //! Append an argument of `type` to the function prototype.
  inline void addArg(uint32_t type) noexcept {
    ASMJIT_ASSERT(_argCount < kFuncArgCount);
    _builderArgList[_argCount++] = uint8_t(type);
  }
  //! Append an argument of type based on `T` to the function prototype.
  template<typename T>
  inline void addArgT() noexcept { addArg(Type::IdOfT<T>::kTypeId); }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint8_t _builderArgList[kFuncArgCount];
};

// ============================================================================
// [asmjit::FuncValue]
// ============================================================================

//! Argument or return value as defined by `FuncSignature`, but with register
//! or stack address (and other metadata) assigned to it.
struct FuncValue {
  enum Parts : uint32_t {
    kTypeIdShift      = 0,             //!< TypeId shift.
    kTypeIdMask       = 0x000000FFU,   //!< TypeId mask.

    kFlagIsReg        = 0x00000100U,   //!< Passed by register.
    kFlagIsStack      = 0x00000200U,   //!< Passed by stack.
    kFlagIsIndirect   = 0x00000400U,   //!< Passed indirectly by reference (internally a pointer).
    kFlagIsDone       = 0x00000800U,   //!< Used internally by arguments allocator.

    kStackOffsetShift = 12,            //!< Stack offset shift.
    kStackOffsetMask  = 0xFFFFF000U,   //!< Stack offset mask (must occupy MSB bits).

    kRegIdShift       = 16,            //!< RegId shift.
    kRegIdMask        = 0x00FF0000U,   //!< RegId mask.

    kRegTypeShift     = 24,            //!< RegType shift.
    kRegTypeMask      = 0xFF000000U    //!< RegType mask.
  };

  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  // These initialize the whole `FuncValue` to either Reg or Stack. Useful when
  // you know all of these properties and wanna just set it up.

  //! Initialize this in/out by a given `typeId`.
  inline void initTypeId(uint32_t typeId) noexcept {
    _data = typeId << kTypeIdShift;
  }

  inline void initReg(uint32_t regType, uint32_t regId, uint32_t typeId, uint32_t flags = 0) noexcept {
    _data = (regType << kRegTypeShift) | (regId << kRegIdShift) | (typeId << kTypeIdShift) | kFlagIsReg | flags;
  }

  inline void initStack(int32_t offset, uint32_t typeId) noexcept {
    _data = (uint32_t(offset) << kStackOffsetShift) | (typeId << kTypeIdShift) | kFlagIsStack;
  }

  //! Reset the value to its unassigned state.
  inline void reset() noexcept { _data = 0; }

  // --------------------------------------------------------------------------
  // [Assign]
  // --------------------------------------------------------------------------

  // These initialize only part of `FuncValue`, useful when building `FuncValue`
  // incrementally. The caller should first init the type-id by caliing `initTypeId`
  // and then continue building either Reg or Stack.

  inline void assignRegData(uint32_t regType, uint32_t regId) noexcept {
    ASMJIT_ASSERT((_data & (kRegTypeMask | kRegIdMask)) == 0);
    _data |= (regType << kRegTypeShift) | (regId << kRegIdShift) | kFlagIsReg;
  }

  inline void assignStackOffset(int32_t offset) noexcept {
    ASMJIT_ASSERT((_data & kStackOffsetMask) == 0);
    _data |= (uint32_t(offset) << kStackOffsetShift) | kFlagIsStack;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline void _replaceValue(uint32_t mask, uint32_t value) noexcept { _data = (_data & ~mask) | value; }

  //! Get whether the `FuncValue` has a flag `flag` set.
  inline bool hasFlag(uint32_t flag) const noexcept { return (_data & flag) != 0; }
  //! Add `flags` to `FuncValue`.
  inline void addFlags(uint32_t flags) noexcept { _data |= flags; }
  //! Clear `flags` of `FuncValue`.
  inline void clearFlags(uint32_t flags) noexcept { _data &= ~flags; }

  //! Get whether this value is initialized (i.e. contains a valid data).
  inline bool isInitialized() const noexcept { return _data != 0; }
  //! Get whether this argument is passed by register.
  inline bool isReg() const noexcept { return hasFlag(kFlagIsReg); }
  //! Get whether this argument is passed by stack.
  inline bool isStack() const noexcept { return hasFlag(kFlagIsStack); }
  //! Get whether this argument is passed by register.
  inline bool isAssigned() const noexcept { return hasFlag(kFlagIsReg | kFlagIsStack); }
  //! Get whether this argument is passed through a pointer (used by WIN64 to pass XMM|YMM|ZMM).
  inline bool isIndirect() const noexcept { return hasFlag(kFlagIsIndirect); }

  inline bool isDone() const noexcept { return hasFlag(kFlagIsDone); }

  //! Get a register type of the register used to pass function argument or return value.
  inline uint32_t getRegType() const noexcept { return (_data & kRegTypeMask) >> kRegTypeShift; }
  //! Set a register type of the register used to pass function argument or return value.
  inline void setRegType(uint32_t regType) noexcept { _replaceValue(kRegTypeMask, regType << kRegTypeShift); }

  //! Get a physical id of the register used to pass function argument or return value.
  inline uint32_t getRegId() const noexcept { return (_data & kRegIdMask) >> kRegIdShift; }
  //! Set a physical id of the register used to pass function argument or return value.
  inline void setRegId(uint32_t regId) noexcept { _replaceValue(kRegIdMask, regId << kRegIdShift); }

  //! Get a stack offset of this argument.
  inline int32_t getStackOffset() const noexcept { return int32_t(_data & kStackOffsetMask) >> kStackOffsetShift; }
  //! Set a stack offset of this argument.
  inline void setStackOffset(int32_t offset) noexcept { _replaceValue(kStackOffsetMask, uint32_t(offset) << kStackOffsetShift); }

  //! Get a TypeId of this argument or return value.
  inline bool hasTypeId() const noexcept { return (_data & kTypeIdMask) != 0; }
  //! Get a TypeId of this argument or return value.
  inline uint32_t getTypeId() const noexcept { return (_data & kTypeIdMask) >> kTypeIdShift; }
  //! Set a TypeId of this argument or return value.
  inline void setTypeId(uint32_t typeId) noexcept { _replaceValue(kTypeIdMask, typeId << kTypeIdShift); }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _data;
};

// ============================================================================
// [asmjit::FuncDetail]
// ============================================================================

//! Function detail - CallConv and expanded FuncSignature.
//!
//! Function details is architecture and OS dependent representation of function.
//! It contains calling convention and expanded function signature so all
//! arguments have assigned either register type & id or stack address.
class FuncDetail {
public:
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline FuncDetail() noexcept { reset(); }
  inline FuncDetail(const FuncDetail& other) noexcept { std::memcpy(this, &other, sizeof(*this)); }

  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  //! Initialize this `FuncDetail` to the given signature.
  ASMJIT_API Error init(const FuncSignature& sign);
  inline void reset() noexcept { std::memset(this, 0, sizeof(*this)); }

  // --------------------------------------------------------------------------
  // [Accessors - Calling Convention]
  // --------------------------------------------------------------------------

  //! Get the function's calling convention, see `CallConv`.
  inline const CallConv& getCallConv() const noexcept { return _callConv; }

  //! Get CallConv flags, see \ref CallConv::Flags.
  inline uint32_t getFlags() const noexcept { return _callConv.getFlags(); }
  //! Check if a CallConv `flag` is set, see \ref CallConv::Flags.
  inline bool hasFlag(uint32_t ccFlag) const noexcept { return _callConv.hasFlag(ccFlag); }

  // --------------------------------------------------------------------------
  // [Accessors - Arguments and Return]
  // --------------------------------------------------------------------------

  //! Get count of function return values.
  inline uint32_t getRetCount() const noexcept { return _retCount; }
  //! Get the number of function arguments.
  inline uint32_t getArgCount() const noexcept { return _argCount; }

  //! Get whether the function has a return value.
  inline bool hasRet() const noexcept { return _retCount != 0; }
  //! Get function return value.
  inline FuncValue& getRet(uint32_t index = 0) noexcept {
    ASMJIT_ASSERT(index < ASMJIT_ARRAY_SIZE(_rets));
    return _rets[index];
  }
  //! Get function return value (const).
  inline const FuncValue& getRet(uint32_t index = 0) const noexcept {
    ASMJIT_ASSERT(index < ASMJIT_ARRAY_SIZE(_rets));
    return _rets[index];
  }

  //! Get function arguments array.
  inline FuncValue* getArgs() noexcept { return _args; }
  //! Get function arguments array (const).
  inline const FuncValue* getArgs() const noexcept { return _args; }

  inline bool hasArg(uint32_t index) const noexcept {
    ASMJIT_ASSERT(index < kFuncArgCountLoHi);
    return _args[index].isInitialized();
  }

  //! Get function argument at index `index`.
  inline FuncValue& getArg(uint32_t index) noexcept {
    ASMJIT_ASSERT(index < kFuncArgCountLoHi);
    return _args[index];
  }

  //! Get function argument at index `index`.
  inline const FuncValue& getArg(uint32_t index) const noexcept {
    ASMJIT_ASSERT(index < kFuncArgCountLoHi);
    return _args[index];
  }

  inline void resetArg(uint32_t index) noexcept {
    ASMJIT_ASSERT(index < kFuncArgCountLoHi);
    _args[index].reset();
  }

  //! Get whether the function passes one or more argument by stack.
  inline bool hasStackArgs() const noexcept { return _argStackSize != 0; }
  //! Get stack size needed for function arguments passed on the stack.
  inline uint32_t getArgStackSize() const noexcept { return _argStackSize; }

  inline uint32_t getRedZoneSize() const noexcept { return _callConv.getRedZoneSize(); }
  inline uint32_t getSpillZoneSize() const noexcept { return _callConv.getSpillZoneSize(); }
  inline uint32_t getNaturalStackAlignment() const noexcept { return _callConv.getNaturalStackAlignment(); }

  inline uint32_t getPassedRegs(uint32_t group) const noexcept { return _callConv.getPassedRegs(group); }
  inline uint32_t getPreservedRegs(uint32_t group) const noexcept { return _callConv.getPreservedRegs(group); }

  inline uint32_t getUsedRegs(uint32_t group) const noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    return _usedRegs[group];
  }

  inline void addUsedRegs(uint32_t group, uint32_t regs) noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    _usedRegs[group] |= regs;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  CallConv _callConv;                    //!< Calling convention.
  uint8_t _argCount;                     //!< Number of function arguments.
  uint8_t _retCount;                     //!< Number of function return values.
  uint16_t _reserved;                    //!< Reserved.
  uint32_t _usedRegs[Reg::kGroupVirt];   //!< Registers that contains arguments.
  uint32_t _argStackSize;                //!< Size of arguments passed by stack.
  FuncValue _rets[2];                    //!< Function return values.
  FuncValue _args[kFuncArgCountLoHi];    //!< Function arguments.
};

// ============================================================================
// [asmjit::FuncFrame]
// ============================================================================

//! Function frame.
//!
//! Function frame is used directly by prolog and epilog insertion (PEI) utils.
//! It provides information necessary to insert a proper and ABI comforming
//! prolog and epilog. Function frame calculation is based on \ref CallConv and
//! other function attributes.
//!
//! Function Frame Structure
//! ------------------------
//!
//! Various properties can contribute to the size and structure of the function
//! frame. The function frame in most cases won't use all of the properties
//! illustrated (for example Spill Zone and Red Zone are never used together).
//!
//!   +-----------------------------+
//!   | Arguments Passed by Stack   |
//!   +-----------------------------+
//!   | Spill Zone                  |
//!   +-----------------------------+ <- Stack offset (args) starts from here.
//!   | Return Address if Pushed    |
//!   +-----------------------------+ <- Stack pointer (SP) upon entry.
//!   | Save/Restore Stack.         |
//!   +-----------------------------+-----------------------------+
//!   | Local Stack                 |                             |
//!   +-----------------------------+          Final Stack        |
//!   | Call Stack                  |                             |
//!   +-----------------------------+-----------------------------+
//!   | Red Zone                    |
//!   +-----------------------------+
class FuncFrame {
public:
  enum Group : uint32_t {
    kGroupVirt = Reg::kGroupVirt
  };

  enum Tag : uint32_t {
    kTagInvalidOffset     = 0xFFFFFFFFU  //!< Tag used to inform that some offset is invalid.
  };

  //! Attributes are designed in a way that all are initially false, and user
  //! or FuncFrame finalizer adds them when necessary.
  enum Attributes : uint32_t {
    kAttrHasPreservedFP   = 0x00000001U, //!< Preserve frame pointer (don't omit FP).
    kAttrHasFuncCalls     = 0x00000002U, //!< Function calls other functions (is not leaf).

    kAttrX86AvxEnabled    = 0x00010000U, //!< Use AVX instead of SSE for all operations (X86).
    kAttrX86AvxCleanup    = 0x00020000U, //!< Emit VZEROUPPER instruction in epilog (X86).
    kAttrX86MmxCleanup    = 0x00040000U, //!< Emit EMMS instruction in epilog (X86).

    kAttrAlignedVecSR     = 0x40000000U, //!< Function has aligned save/restore of vector registers.
    kAttrIsFinalized      = 0x80000000U  //!< FuncFrame is finalized and can be used by PEI.
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline FuncFrame() noexcept { reset(); }
  inline FuncFrame(const FuncFrame& other) noexcept { std::memcpy(this, &other, sizeof(FuncFrame)); }

  // --------------------------------------------------------------------------
  // [Init / Reset / Finalize]
  // --------------------------------------------------------------------------

  ASMJIT_API Error init(const FuncDetail& func) noexcept;
  ASMJIT_API Error finalize() noexcept;

  inline void reset() noexcept {
    std::memset(this, 0, sizeof(FuncFrame));
    _spRegId = Reg::kIdBad;
    _saRegId = Reg::kIdBad;
    _daOffset = kTagInvalidOffset;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the target architecture of the function frame.
  inline uint32_t getArchType() const noexcept { return _archType; }

  //! Get FuncFrame attributes, see \ref Attributes.
  inline uint32_t getAttributes() const noexcept { return _attributes; }
  //! Check if the FuncFame contains an attribute `attr`.
  inline bool hasAttribute(uint32_t attr) const noexcept { return (_attributes & attr) != 0; }
  //! Add attributes `attrs` to the FuncFrame.
  inline void addAttributes(uint32_t attrs) noexcept { _attributes |= attrs; }
  //! Clear attributes `attrs` from the FrameFrame.
  inline void clearAttributes(uint32_t attrs) noexcept { _attributes &= ~attrs; }

  //! Get whether the function preserves frame pointer (EBP|ESP on X86).
  inline bool hasPreservedFP() const noexcept { return hasAttribute(kAttrHasPreservedFP); }
  //! Enable preserved frame pointer.
  inline void setPreservedFP() noexcept { addAttributes(kAttrHasPreservedFP); }
  //! Disable preserved frame pointer.
  inline void resetPreservedFP() noexcept { clearAttributes(kAttrHasPreservedFP); }

  //! Get whether the function calls other functions.
  inline bool hasFuncCalls() const noexcept { return hasAttribute(kAttrHasFuncCalls); }
  //! Set `kFlagHasCalls` to true.
  inline void setFuncCalls() noexcept { addAttributes(kAttrHasFuncCalls); }
  //! Set `kFlagHasCalls` to false.
  inline void resetFuncCalls() noexcept { clearAttributes(kAttrHasFuncCalls); }

  //! Get whether the function contains AVX cleanup - 'vzeroupper' instruction in epilog.
  inline bool hasAvxCleanup() const noexcept { return hasAttribute(kAttrX86AvxCleanup); }
  //! Enable AVX cleanup.
  inline void setAvxCleanup() noexcept { addAttributes(kAttrX86AvxCleanup); }
  //! Disable AVX cleanup.
  inline void resetAvxCleanup() noexcept { clearAttributes(kAttrX86AvxCleanup); }

  //! Get whether the function contains AVX cleanup - 'vzeroupper' instruction in epilog.
  inline bool isAvxEnabled() const noexcept { return hasAttribute(kAttrX86AvxEnabled); }
  //! Enable AVX cleanup.
  inline void setAvxEnabled() noexcept { addAttributes(kAttrX86AvxEnabled); }
  //! Disable AVX cleanup.
  inline void resetAvxEnabled() noexcept { clearAttributes(kAttrX86AvxEnabled); }

  //! Get whether the function contains MMX cleanup - 'emms' instruction in epilog.
  inline bool hasMmxCleanup() const noexcept { return hasAttribute(kAttrX86MmxCleanup); }
  //! Enable MMX cleanup.
  inline void setMmxCleanup() noexcept { addAttributes(kAttrX86MmxCleanup); }
  //! Disable MMX cleanup.
  inline void resetMmxCleanup() noexcept { clearAttributes(kAttrX86MmxCleanup); }

  //! Get whether the function uses call stack.
  inline bool hasCallStack() const noexcept { return _callStackSize != 0; }
  //! Get whether the function uses local stack.
  inline bool hasLocalStack() const noexcept { return _localStackSize != 0; }
  //! Get whether vector registers can be saved and restored by using aligned writes and reads.
  inline bool hasAlignedVecSR() const noexcept { return hasAttribute(kAttrAlignedVecSR); }
  //! Get whether the function has to align stack dynamically.
  inline bool hasDynamicAlignment() const noexcept { return _finalStackAlignment >= _minimumDynamicAlignment; }

  //! Get whether this calling convention specifies 'RedZone'.
  inline bool hasRedZone() const noexcept { return _redZoneSize != 0; }
  //! Get whether this calling convention specifies 'SpillZone'.
  inline bool hasSpillZone() const noexcept { return _spillZoneSize != 0; }

  //! Get size of 'RedZone'.
  inline uint32_t getRedZoneSize() const noexcept { return _redZoneSize; }
  //! Get size of 'SpillZone'.
  inline uint32_t getSpillZoneSize() const noexcept { return _spillZoneSize; }
  //! Get natural stack alignment (guaranteed stack alignment upon entry).
  inline uint32_t getNaturalStackAlignment() const noexcept { return _naturalStackAlignment; }
  //! Get natural stack alignment (guaranteed stack alignment upon entry).
  inline uint32_t getMinimumDynamicAlignment() const noexcept { return _minimumDynamicAlignment; }

  //! Get whether the callee must adjust SP before returning (X86-STDCALL only)
  inline bool hasCalleeStackCleanup() const noexcept { return _calleeStackCleanup != 0; }
  //! Get home many bytes of the stack the the callee must adjust before returning (X86-STDCALL only)
  inline uint32_t getCalleeStackCleanup() const noexcept { return _calleeStackCleanup; }

  //! Get call stack alignment.
  inline uint32_t getCallStackAlignment() const noexcept { return _callStackAlignment; }
  //! Get local stack alignment.
  inline uint32_t getLocalStackAlignment() const noexcept { return _localStackAlignment; }
  //! Get final stack alignment (the maximum value of call, local, and natural stack alignments).
  inline uint32_t getFinalStackAlignment() const noexcept { return _finalStackAlignment; }

  //! Set call stack alignment.
  //!
  //! NOTE: This also updates the final stack alignment.
  inline void setCallStackAlignment(uint32_t alignment) noexcept {
    _callStackAlignment = uint8_t(alignment);
    _finalStackAlignment = std::max(_naturalStackAlignment, std::max(_callStackAlignment, _localStackAlignment));
  }

  //! Set local stack alignment.
  //!
  //! NOTE: This also updates the final stack alignment.
  inline void setLocalStackAlignment(uint32_t value) noexcept {
    _localStackAlignment = uint8_t(value);
    _finalStackAlignment = std::max(_naturalStackAlignment, std::max(_callStackAlignment, _localStackAlignment));
  }

  //! Combine call stack alignment with `alignment`, updating it to the greater value.
  //!
  //! NOTE: This also updates the final stack alignment.
  inline void updateCallStackAlignment(uint32_t alignment) noexcept {
    _callStackAlignment = uint8_t(std::max<uint32_t>(_callStackAlignment, alignment));
    _finalStackAlignment = std::max(_finalStackAlignment, _callStackAlignment);
  }

  //! Combine local stack alignment with `alignment`, updating it to the greater value.
  //!
  //! NOTE: This also updates the final stack alignment.
  inline void updateLocalStackAlignment(uint32_t alignment) noexcept {
    _localStackAlignment = uint8_t(std::max<uint32_t>(_localStackAlignment, alignment));
    _finalStackAlignment = std::max(_finalStackAlignment, _localStackAlignment);
  }

  //! Get call stack size.
  inline uint32_t getCallStackSize() const noexcept { return _callStackSize; }
  //! Get local stack size.
  inline uint32_t getLocalStackSize() const noexcept { return _localStackSize; }

  //! Set call stack size.
  inline void setCallStackSize(uint32_t size) noexcept { _callStackSize = size; }
  //! Set local stack size.
  inline void setLocalStackSize(uint32_t size) noexcept { _localStackSize = size; }

  //! Combine call stack size with `size`, updating it to the greater value.
  inline void updateCallStackSize(uint32_t size) noexcept { _callStackSize = std::max(_callStackSize, size); }
  //! Combine local stack size with `size`, updating it to the greater value.
  inline void updateLocalStackSize(uint32_t size) noexcept { _localStackSize = std::max(_localStackSize, size); }

  //! Get final stack size (only valid after the FuncFrame is finalized).
  inline uint32_t getFinalStackSize() const noexcept { return _finalStackSize; }

  //! Get an offset to access the local stack (non-zero only if call stack is used).
  inline uint32_t getLocalStackOffset() const noexcept { return _localStackOffset; }

  //! Get whether the function prolog/epilog requires a memory slot for storing unaligned SP.
  inline bool hasDAOffset() const noexcept { return _daOffset != kTagInvalidOffset; }
  //! Get a memory offset used to store DA (dynamic alignment) slot (relative to SP).
  inline uint32_t getDAOffset() const noexcept { return _daOffset; }

  inline uint32_t getSAOffset(uint32_t regId) const noexcept {
    return regId == _spRegId ? getSAOffsetFromSP()
                             : getSAOffsetFromSA();
  }

  inline uint32_t getSAOffsetFromSP() const noexcept { return _saOffsetFromSP; }
  inline uint32_t getSAOffsetFromSA() const noexcept { return _saOffsetFromSA; }

  //! Get which registers (by `group`) are saved/restored in prolog/epilog, respectively.
  inline uint32_t getDirtyRegs(uint32_t group) const noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    return _dirtyRegs[group];
  }

  //! Set which registers (by `group`) are saved/restored in prolog/epilog, respectively.
  inline void setDirtyRegs(uint32_t group, uint32_t regs) noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    _dirtyRegs[group] = regs;
  }

  //! Add registers (by `group`) to saved/restored registers.
  inline void addDirtyRegs(uint32_t group, uint32_t regs) noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    _dirtyRegs[group] |= regs;
  }

  inline void setAllDirty() noexcept {
    _dirtyRegs[0] = 0xFFFFFFFFU;
    _dirtyRegs[1] = 0xFFFFFFFFU;
    _dirtyRegs[2] = 0xFFFFFFFFU;
    _dirtyRegs[3] = 0xFFFFFFFFU;
  }

  inline void setAllDirty(uint32_t group) noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    _dirtyRegs[group] = 0xFFFFFFFFU;
  }

  inline uint32_t getSavedRegs(uint32_t group) const noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    return _dirtyRegs[group] & _preservedRegs[group];
  }

  inline uint32_t getPreservedRegs(uint32_t group) const noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    return _preservedRegs[group];
  }

  inline bool hasSARegId() const noexcept { return _saRegId != Reg::kIdBad; }
  inline uint32_t getSARegId() const noexcept { return _saRegId; }
  inline void setSARegId(uint32_t regId) { _saRegId = uint8_t(regId); }
  inline void resetSARegId() { setSARegId(Reg::kIdBad); }

  //! Get stack size required to save GP registers.
  inline uint32_t getGpSaveSize() const noexcept { return _gpSaveSize; }
  //! Get stack size required to save other than GP registers (MM, XMM|YMM|ZMM, K, VFP, etc...).
  inline uint32_t getNonGpSaveSize() const noexcept { return _nonGpSaveSize; }

  inline uint32_t getGpSaveOffset() const noexcept { return _gpSaveOffset; }
  inline uint32_t getNonGpSaveOffset() const noexcept { return _nonGpSaveOffset; }

  inline bool hasStackAdjustment() const noexcept { return _stackAdjustment != 0; }
  inline uint32_t getStackAdjustment() const noexcept { return _stackAdjustment; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _attributes;                  //!< Function attributes.

  uint8_t _archType;                     //!< Architecture.
  uint8_t _spRegId;                      //!< SP register ID (to access call stack and local stack).
  uint8_t _saRegId;                      //!< SA register ID (to access stack arguments).

  uint8_t _redZoneSize;                  //!< Red zone size (copied from CallConv).
  uint8_t _spillZoneSize;                //!< Spill zone size (copied from CallConv).
  uint8_t _naturalStackAlignment;        //!< Natural stack alignment (copied from CallConv).
  uint8_t _minimumDynamicAlignment;      //!< Minimum stack alignment to turn on dynamic alignment.

  uint8_t _callStackAlignment;           //!< Call stack alignment.
  uint8_t _localStackAlignment;          //!< Local stack alignment.
  uint8_t _finalStackAlignment;          //!< Final stack alignment.

  uint16_t _calleeStackCleanup;          //!< Adjustment of the stack before returning (X86-STDCALL).

  uint32_t _callStackSize;               //!< Call stack size.
  uint32_t _localStackSize;              //!< Local stack size.
  uint32_t _finalStackSize;              //!< Final stack size (sum of call stack and local stack).

  uint32_t _localStackOffset;            //!< Local stack offset (non-zero only if call stack is used).
  uint32_t _daOffset;                    //!< Offset relative to SP that contains previous SP (before alignment).
  uint32_t _saOffsetFromSP;              //!< Offset of the first stack argument relative to SP.
  uint32_t _saOffsetFromSA;              //!< Offset of the first stack argument relative to SA (_saRegId or FP).

  uint32_t _stackAdjustment;             //!< Local stack adjustment in prolog/epilog.

  uint32_t _dirtyRegs[Reg::kGroupVirt];     //!< Registers that are dirty.
  uint32_t _preservedRegs[Reg::kGroupVirt]; //!< Registers that must be preserved (copied from CallConv).

  uint16_t _gpSaveSize;                  //!< Final stack size required to save GP regs.
  uint16_t _nonGpSaveSize;               //!< Final Stack size required to save other than GP regs.
  uint32_t _gpSaveOffset;                //!< Final offset where saved GP regs are stored.
  uint32_t _nonGpSaveOffset;             //!< Final offset where saved other than GP regs are stored.
};

// ============================================================================
// [asmjit::FuncArgsAssignment]
// ============================================================================

//! A helper class that can be used to assign a physical register for each
//! function argument. Use with `CodeEmitter::emitArgsAssignment()`.
class FuncArgsAssignment {
public:
  enum {
    kArgCount = kFuncArgCountLoHi
  };

  explicit inline FuncArgsAssignment(const FuncDetail* fd = nullptr) noexcept { reset(fd); }

  inline FuncArgsAssignment(const FuncArgsAssignment& other) noexcept {
    std::memcpy(this, &other, sizeof(*this));
  }

  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  inline void reset(const FuncDetail* fd = nullptr) noexcept {
    _funcDetail = fd;
    _saRegId = uint8_t(Reg::kIdBad);
    std::memset(_reserved, 0, sizeof(_reserved));
    std::memset(_args, 0, sizeof(_args));
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline const FuncDetail* getFuncDetail() const noexcept { return _funcDetail; }
  inline void setFuncDetail(const FuncDetail* fd) noexcept { _funcDetail = fd; }

  inline bool hasSARegId() const noexcept { return _saRegId != Reg::kIdBad; }
  inline uint32_t getSARegId() const noexcept { return _saRegId; }
  inline void setSARegId(uint32_t regId) { _saRegId = uint8_t(regId); }
  inline void resetSARegId() { _saRegId = uint8_t(Reg::kIdBad); }

  inline FuncValue& getArg(uint32_t index) noexcept {
    ASMJIT_ASSERT(index < ASMJIT_ARRAY_SIZE(_args));
    return _args[index];
  }
  inline const FuncValue& getArg(uint32_t index) const noexcept {
    ASMJIT_ASSERT(index < ASMJIT_ARRAY_SIZE(_args));
    return _args[index];
  }

  inline bool isAssigned(uint32_t argIndex) const noexcept {
    ASMJIT_ASSERT(argIndex < ASMJIT_ARRAY_SIZE(_args));
    return _args[argIndex].isAssigned();
  }

  inline void assignReg(uint32_t argIndex, const Reg& reg, uint32_t typeId = Type::kIdVoid) noexcept {
    ASMJIT_ASSERT(argIndex < ASMJIT_ARRAY_SIZE(_args));
    ASMJIT_ASSERT(reg.isPhysReg());
    _args[argIndex].initReg(reg.getType(), reg.getId(), typeId);
  }

  inline void assignReg(uint32_t argIndex, uint32_t regType, uint32_t regId, uint32_t typeId = Type::kIdVoid) noexcept {
    ASMJIT_ASSERT(argIndex < ASMJIT_ARRAY_SIZE(_args));
    _args[argIndex].initReg(regType, regId, typeId);
  }

  inline void assignStack(uint32_t argIndex, int32_t offset, uint32_t typeId = Type::kIdVoid) {
    ASMJIT_ASSERT(argIndex < ASMJIT_ARRAY_SIZE(_args));
    _args[argIndex].initStack(offset, typeId);
  }

  // NOTE: All `assignAll()` methods are shortcuts to assign all arguments at
  // once, however, since registers are passed all at once these initializers
  // don't provide any way to pass TypeId and/or to keep any argument between
  // the arguments passed unassigned.
  inline void assignAll(const Reg& a0) noexcept {
    assignReg(0, a0);
  }

  inline void assignAll(const Reg& a0, const Reg& a1) noexcept {
    assignReg(0, a0);
    assignReg(1, a1);
  }

  inline void assignAll(const Reg& a0, const Reg& a1, const Reg& a2) noexcept {
    assignReg(0, a0);
    assignReg(1, a1);
    assignReg(2, a2);
  }

  inline void assignAll(const Reg& a0, const Reg& a1, const Reg& a2, const Reg& a3) noexcept {
    assignReg(0, a0);
    assignReg(1, a1);
    assignReg(2, a2);
    assignReg(3, a3);
  }

  inline void assignAll(const Reg& a0, const Reg& a1, const Reg& a2, const Reg& a3, const Reg& a4) noexcept {
    assignReg(0, a0);
    assignReg(1, a1);
    assignReg(2, a2);
    assignReg(3, a3);
    assignReg(4, a4);
  }

  inline void assignAll(const Reg& a0, const Reg& a1, const Reg& a2, const Reg& a3, const Reg& a4, const Reg& a5) noexcept {
    assignReg(0, a0);
    assignReg(1, a1);
    assignReg(2, a2);
    assignReg(3, a3);
    assignReg(4, a4);
    assignReg(5, a5);
  }

  inline void assignAll(const Reg& a0, const Reg& a1, const Reg& a2, const Reg& a3, const Reg& a4, const Reg& a5, const Reg& a6) noexcept {
    assignReg(0, a0);
    assignReg(1, a1);
    assignReg(2, a2);
    assignReg(3, a3);
    assignReg(4, a4);
    assignReg(5, a5);
    assignReg(6, a6);
  }

  inline void assignAll(const Reg& a0, const Reg& a1, const Reg& a2, const Reg& a3, const Reg& a4, const Reg& a5, const Reg& a6, const Reg& a7) noexcept {
    assignReg(0, a0);
    assignReg(1, a1);
    assignReg(2, a2);
    assignReg(3, a3);
    assignReg(4, a4);
    assignReg(5, a5);
    assignReg(6, a6);
    assignReg(7, a7);
  }

  // --------------------------------------------------------------------------
  // [Utilities]
  // --------------------------------------------------------------------------

  //! Update `FuncFrame` based on function's arguments assignment.
  //!
  //! NOTE: You MUST call this in orher to use `CodeEmitter::emitArgsAssignment()`,
  //! otherwise the FuncFrame would not contain the information necessary to
  //! assign all arguments into the registers and/or stack specified.
  ASMJIT_API Error updateFuncFrame(FuncFrame& frame) const noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  const FuncDetail* _funcDetail;         //!< Function detail.
  uint8_t _saRegId;                      //!< Register that can be used to access arguments passed by stack.
  uint8_t _reserved[3];                  //!< \internal
  FuncValue _args[kArgCount];            //!< Mapping of each function argument.
};

//! \}

ASMJIT_END_NAMESPACE

// [Guard]
#endif // _ASMJIT_CORE_FUNC_H
