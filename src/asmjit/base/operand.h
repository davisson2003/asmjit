// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_BASE_OPERAND_H
#define _ASMJIT_BASE_OPERAND_H

// [Dependencies]
#include "../base/intutils.h"

// [Api-Begin]
#include "../asmjit_apibegin.h"

namespace asmjit {

//! \addtogroup asmjit_base
//! \{

// ============================================================================
// [asmjit::Operand_]
// ============================================================================

//! Constructorless \ref Operand.
//!
//! Contains no initialization code and can be used safely to define an array
//! of operands that won't be initialized. This is a \ref Operand compatible
//! data structure designed to be statically initialized, static const, or to
//! be used by the user to define an array of operands without having them
//! default initialized.
//!
//! The difference between `Operand` and `Operand_`:
//!
//! ```
//! Operand_ xArray[10]; // Not initialized, contains garbage.
//! Operand  yArray[10]; // All operands initialized to none.
//! ```
struct Operand_ {
  // --------------------------------------------------------------------------
  // [Operand Type]
  // --------------------------------------------------------------------------

  //! Operand types that can be encoded in \ref Operand.
  enum OpType : uint32_t {
    kOpNone  = 0,                        //!< Not an operand or not initialized.
    kOpReg   = 1,                        //!< Operand is a register.
    kOpMem   = 2,                        //!< Operand is a memory.
    kOpImm   = 3,                        //!< Operand is an immediate value.
    kOpLabel = 4                         //!< Operand is a label.
  };

  static_assert(kOpMem - kOpReg == 1, "asmjit::Operand requires `kOpMem` to be `kOpReg+1`.");

  // --------------------------------------------------------------------------
  // [Operand Signature (Bits)]
  // --------------------------------------------------------------------------

  enum SignatureBits : uint32_t {
    // Operand type (3 least significant bits).
    // |........|........|........|.....XXX|
    kSignatureOpShift           = 0,
    kSignatureOpBits            = 0x07U,
    kSignatureOpMask            = kSignatureOpBits << kSignatureOpShift,

    // Register type (5 bits).
    // |........|........|........|XXXXX...|
    kSignatureRegTypeShift      = 3,
    kSignatureRegTypeBits       = 0x1FU,
    kSignatureRegTypeMask       = kSignatureRegTypeBits << kSignatureRegTypeShift,

    // Register group (4 bits).
    // |........|........|....XXXX|........|
    kSignatureRegGroupShift     = 8,
    kSignatureRegGroupBits      = 0x0FU,
    kSignatureRegGroupMask      = kSignatureRegGroupBits << kSignatureRegGroupShift,

    // Memory base type (5 bits).
    // |........|........|........|XXXXX...|
    kSignatureMemBaseTypeShift  = 3,
    kSignatureMemBaseTypeBits   = 0x1FU,
    kSignatureMemBaseTypeMask   = kSignatureMemBaseTypeBits << kSignatureMemBaseTypeShift,

    // Memory index type (5 bits).
    // |........|........|...XXXXX|........|
    kSignatureMemIndexTypeShift = 8,
    kSignatureMemIndexTypeBits  = 0x1FU,
    kSignatureMemIndexTypeMask  = kSignatureMemIndexTypeBits << kSignatureMemIndexTypeShift,

    // Memory base+index combined (10 bits).
    // |........|........|...XXXXX|XXXXX...|
    kSignatureMemBaseIndexShift = 3,
    kSignatureMemBaseIndexBits  = 0x3FFU,
    kSignatureMemBaseIndexMask  = kSignatureMemBaseIndexBits << kSignatureMemBaseIndexShift,

    // Memory address type (2 bits).
    // |........|........|.XX.....|........|
    kSignatureMemAddrTypeShift  = 13,
    kSignatureMemAddrTypeBits   = 0x03U,
    kSignatureMemAddrTypeMask   = kSignatureMemAddrTypeBits << kSignatureMemAddrTypeShift,

    // This memory operand represents a home-slot or stack (CodeCompiler).
    // |........|........|X.......|........|
    kSignatureMemRegHomeShift   = 15,
    kSignatureMemRegHomeBits    = 0x01U,
    kSignatureMemRegHomeFlag    = kSignatureMemRegHomeBits << kSignatureMemRegHomeShift,

    // Operand size (8 most significant bits).
    // |XXXXXXXX|........|........|........|
    kSignatureSizeShift         = 24,
    kSignatureSizeBits          = 0xFFU,
    kSignatureSizeMask          = kSignatureSizeBits << kSignatureSizeShift
  };

  // --------------------------------------------------------------------------
  // [Operand Id]
  // --------------------------------------------------------------------------

  //! Operand id helpers useful for id <-> index translation.
  enum PackedId : uint32_t {
    //! Minimum valid packed-id.
    kPackedIdMin    = 0x00000100U,
    //! Maximum valid packed-id.
    kPackedIdMax    = 0xFFFFFFFFU,
    //! Count of valid packed-ids.
    kPackedIdCount  = uint32_t(kPackedIdMax - kPackedIdMin + 1)
  };

  // --------------------------------------------------------------------------
  // [Operand Utilities]
  // --------------------------------------------------------------------------

  //! Get whether the given `id` is a valid packed-id that can be used by Operand.
  //! Packed ids are those equal or greater than `kPackedIdMin` and lesser or
  //! equal to `kPackedIdMax`. This concept was created to support virtual
  //! registers and to make them distinguishable from physical ones. It allows
  //! a single uint32_t to contain either physical register id or virtual
  //! register id represented as `packed-id`. This concept is used also for
  //! labels to make the API consistent.
  static ASMJIT_INLINE bool isPackedId(uint32_t id) noexcept { return id - kPackedIdMin < uint32_t(kPackedIdCount); }
  //! Convert a real-id into a packed-id that can be stored in Operand.
  static ASMJIT_INLINE uint32_t packId(uint32_t id) noexcept { return id + kPackedIdMin; }
  //! Convert a packed-id back to real-id.
  static ASMJIT_INLINE uint32_t unpackId(uint32_t id) noexcept { return id - kPackedIdMin; }

  // --------------------------------------------------------------------------
  // [Operand Data]
  // --------------------------------------------------------------------------

  //! Any operand.
  struct AnyData {
    uint32_t signature;                  //!< Type of the operand (see \ref OpType) and other data.
    uint32_t id;                         //!< Operand id or `0`.
    uint32_t p32_2;                      //!< \internal
    uint32_t p32_3;                      //!< \internal
  };

  //! Register operand data.
  struct RegData {
    uint32_t signature;                  //!< Type of the operand (always \ref kOpReg) and other data.
    uint32_t id;                         //!< Physical or virtual register id.
    uint32_t p32_2;                      //!< \internal
    uint32_t p32_3;                      //!< \internal
  };

  //! Memory operand data.
  struct MemData {
    uint32_t signature;                  //!< Type of the operand (always \ref kOpMem) and other data.
    uint32_t base;                       //!< BASE register id or HIGH part of 64-bit offset.
    uint32_t index;                      //!< INDEX register id.
    uint32_t offsetLo32;                 //!< LOW part of 64-bit offset (or 32-bit offset).
  };

  //! Immediate operand data.
  struct ImmData {
    uint32_t signature;                  //!< Type of the operand (always \ref kOpImm) and other data.
    uint32_t id;                         //!< Immediate id (always `0`).

    union {
      uint64_t u64;                      //!< 64-bit unsigned integer.
      int64_t  i64;                      //!< 64-bit signed integer.
      double   f64;                      //!< 64-bit floating point.

      uint32_t u32[2];                   //!< 32-bit unsigned integer (2x).
      int32_t  i32[2];                   //!< 32-bit signed integer (2x).
      float    f32[2];                   //!< 32-bit floating point (2x).

#if ASMJIT_ARCH_LE
      struct { float    f32Lo, f32Hi; };
      struct { int32_t  i32Lo, i32Hi; };
      struct { uint32_t u32Lo, u32Hi; };
#else
      struct { float    f32Hi, f32Lo; };
      struct { int32_t  i32Hi, i32Lo; };
      struct { uint32_t u32Hi, u32Lo; };
#endif
    } value;
  };

  //! Label operand data.
  struct LabelData {
    uint32_t signature;                  //!< Type of the operand (always \ref kOpLabel) and other data.
    uint32_t id;                         //!< Label id (`0` if not initialized).
    uint32_t p32_2;                      //!< \internal
    uint32_t p32_3;                      //!< \internal
  };

  // --------------------------------------------------------------------------
  // [Init & Reset]
  // --------------------------------------------------------------------------

  //! \internal
  inline void _initReg(uint32_t signature, uint32_t rId) {
    _reg.signature = signature;
    _reg.id = rId;
    _p64[1] = 0;
  }

  //! \internal
  //!
  //! Initialize the operand from `other` (used by operator overloads).
  inline void copyFrom(const Operand_& other) noexcept { std::memcpy(this, &other, sizeof(Operand_)); }

  //! Reset the `Operand` to none.
  //!
  //! None operand is defined the following way:
  //!   - Its signature is zero (kOpNone, and the rest zero as well).
  //!   - Its id is `0`.
  //!   - The reserved8_4 field is set to `0`.
  //!   - The reserved12_4 field is set to zero.
  //!
  //! In other words, reset operands have all members set to zero. Reset operand
  //! must match the Operand state right after its construction. Alternatively,
  //! if you have an array of operands, you can simply use `std::memset()`.
  //!
  //! ```
  //! using namespace asmjit;
  //!
  //! Operand a;
  //! Operand b;
  //! assert(a == b);
  //!
  //! b = x86::eax;
  //! assert(a != b);
  //!
  //! b.reset();
  //! assert(a == b);
  //!
  //! std::memset(&b, 0, sizeof(Operand));
  //! assert(a == b);
  //! ```
  inline void reset() noexcept {
    _p64[0] = 0;
    _p64[1] = 0;
  }

  // --------------------------------------------------------------------------
  // [Cast]
  // --------------------------------------------------------------------------

  //! Cast this operand to `T` type.
  template<typename T>
  inline T& as() noexcept { return static_cast<T&>(*this); }

  //! Cast this operand to `T` type (const).
  template<typename T>
  inline const T& as() const noexcept { return static_cast<const T&>(*this); }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get whether the operand matches the given signature `sign`.
  constexpr bool hasSignature(uint32_t signature) const noexcept { return _any.signature == signature; }
  //! Get whether the operand matches a signature of the `other` operand.
  constexpr bool hasSignature(const Operand_& other) const noexcept { return _any.signature == other.getSignature(); }

  //! Get a 32-bit operand signature.
  //!
  //! Signature is first 4 bytes of the operand data. It's used mostly for
  //! operand checking as it's much faster to check 4 bytes at once than having
  //! to check these bytes individually.
  constexpr uint32_t getSignature() const noexcept { return _any.signature; }

  //! Set the operand signature (see \ref getSignature).
  //!
  //! Improper use of `setSignature()` can lead to hard-to-debug errors.
  inline void setSignature(uint32_t signature) noexcept { _any.signature = signature; }

  //! Checks if the signature contains at least one bit set of `bits`.
  constexpr bool _hasSignatureData(uint32_t bits) const noexcept { return (_any.signature & bits) != 0; }
  //! Unpacks information from operand's signature.
  constexpr uint32_t _getSignatureData(uint32_t bits, uint32_t shift) const noexcept { return (_any.signature >> shift) & bits; }

  //! \internal
  //!
  //! Packs information to operand's signature.
  inline void _setSignatureData(uint32_t value, uint32_t bits, uint32_t shift) noexcept {
    ASMJIT_ASSERT(value <= bits);
    _any.signature = (_any.signature & ~(bits << shift)) | (value << shift);
  }

  inline void _addSignatureData(uint32_t data) noexcept { _any.signature |= data; }

  //! Clears specified bits in operand's signature.
  inline void _clearSignatureData(uint32_t bits, uint32_t shift) noexcept { _any.signature &= ~(bits << shift); }

  //! Get type of the operand, see \ref OpType.
  constexpr uint32_t getOp() const noexcept { return _getSignatureData(kSignatureOpBits, kSignatureOpShift); }
  //! Get whether the operand is none (\ref kOpNone).
  constexpr bool isNone() const noexcept { return _any.signature == 0; }
  //! Get whether the operand is a register (\ref kOpReg).
  constexpr bool isReg() const noexcept { return getOp() == kOpReg; }
  //! Get whether the operand is a memory location (\ref kOpMem).
  constexpr bool isMem() const noexcept { return getOp() == kOpMem; }
  //! Get whether the operand is an immediate (\ref kOpImm).
  constexpr bool isImm() const noexcept { return getOp() == kOpImm; }
  //! Get whether the operand is a label (\ref kOpLabel).
  constexpr bool isLabel() const noexcept { return getOp() == kOpLabel; }

  //! Get whether the operand is a physical register.
  constexpr bool isPhysReg() const noexcept { return isReg() && _reg.id < 0xFFU; }
  //! Get whether the operand is a virtual register.
  constexpr bool isVirtReg() const noexcept { return isReg() && isPackedId(_reg.id); }

  //! Get whether the operand specifies a size (i.e. the size is not zero).
  constexpr bool hasSize() const noexcept { return _hasSignatureData(kSignatureSizeMask); }
  //! Get whether the size of the operand matches `size`.
  constexpr bool hasSize(uint32_t size) const noexcept { return getSize() == size; }

  //! Get size of the operand (in bytes).
  //!
  //! The value returned depends on the operand type:
  //!   * None  - Should always return zero size.
  //!   * Reg   - Should always return the size of the register. If the register
  //!             size depends on architecture (like \ref X86CReg and \ref X86DReg)
  //!             the size returned should be the greatest possible (so it should
  //!             return 64-bit size in such case).
  //!   * Mem   - Size is optional and will be in most cases zero.
  //!   * Imm   - Should always return zero size.
  //!   * Label - Should always return zero size.
  constexpr uint32_t getSize() const noexcept { return _getSignatureData(kSignatureSizeBits, kSignatureSizeShift); }

  //! Get the operand id.
  //!
  //! The value returned should be interpreted accordingly to the operand type:
  //!   * None  - Should be `0`.
  //!   * Reg   - Physical or virtual register id.
  //!   * Mem   - Multiple meanings - BASE address (register or label id), or
  //!             high value of a 64-bit absolute address.
  //!   * Imm   - Should be `0`.
  //!   * Label - Label id if it was created by using `newLabel()` or `0`
  //!             if the label is invalid or uninitialized.
  constexpr uint32_t getId() const noexcept { return _any.id; }

  //! Get whether the operand is 100% equal to `other`.
  constexpr bool isEqual(const Operand_& other) const noexcept {
    return (_p64[0] == other._p64[0]) && (_p64[1] == other._p64[1]);
  }

  //! Get whether the operand is a register matching `rType`.
  constexpr bool isReg(uint32_t rType) const noexcept {
    return (_any.signature & (kSignatureOpMask | kSignatureRegTypeMask)) ==
           ((kOpReg << kSignatureOpShift) | (rType << kSignatureRegTypeShift));
  }

  //! Get whether the operand is register and of `rType` and `rId`.
  constexpr bool isReg(uint32_t rType, uint32_t rId) const noexcept {
    return isReg(rType) && getId() == rId;
  }

  //! Get whether the operand is a register or memory.
  constexpr bool isRegOrMem() const noexcept {
    return IntUtils::isBetween<uint32_t>(getOp(), kOpReg, kOpMem);
  }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  template<typename T> constexpr bool operator==(const T& other) const noexcept { return  isEqual(other); }
  template<typename T> constexpr bool operator!=(const T& other) const noexcept { return !isEqual(other); }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  union {
    uint32_t _p32[4];                    //!< Operand packed into four 32-bit integers.
    uint64_t _p64[2];                    //!< Operand packed into two 64-bit integers.

    AnyData _any;                        //!< Generic data.
    RegData _reg;                        //!< Physical or virtual register data.
    MemData _mem;                        //!< Memory address data.
    ImmData _imm;                        //!< Immediate value data.
    LabelData _label;                    //!< Label data.
  };
};

// ============================================================================
// [asmjit::Operand]
// ============================================================================

//! Operand can contain register, memory location, immediate, or label.
class Operand : public Operand_ {
public:
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create \ref Operand_::kOpNone operand (all values initialized to zeros).
  constexpr Operand() noexcept
    : Operand_{{{ kOpNone, 0U, 0U, 0U }}} {}
  //! Create a cloned `other` operand.
  constexpr Operand(const Operand& other) noexcept
    : Operand_(other) {}
  //! Create a cloned `other` operand.
  explicit constexpr Operand(const Operand_& other)
    : Operand_(other) {}

  //! Create an operand initialized to raw `[p0, p1, p2, p3]` values.
  constexpr Operand(Globals::Init_, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3) noexcept
    : Operand_{{{ p0, p1, p2, p3 }}} {}

  //! Create an uninitialized operand (dangerous).
  explicit inline Operand(Globals::NoInit_) noexcept {}

  // --------------------------------------------------------------------------
  // [Clone]
  // --------------------------------------------------------------------------

  //! Clone the `Operand`.
  constexpr Operand clone() const noexcept { return Operand(*this); }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  inline Operand& operator=(const Operand_& other) noexcept { copyFrom(other); return *this; }
};

static_assert(sizeof(Operand) == 16, "asmjit::Operand must be exactly 16 bytes long");

// ============================================================================
// [asmjit::Label]
// ============================================================================

//! Label (jump target or data location).
//!
//! Label represents a location in code typically used as a jump target, but
//! may be also a reference to some data or a static variable. Label has to be
//! explicitly created by CodeEmitter.
//!
//! Example of using labels:
//!
//! ~~~
//! // Create a CodeEmitter (for example X86Assembler).
//! X86Assembler a;
//!
//! // Create Label instance.
//! Label L1 = a.newLabel();
//!
//! // ... your code ...
//!
//! // Using label.
//! a.jump(L1);
//!
//! // ... your code ...
//!
//! // Bind label to the current position, see `CodeEmitter::bind()`.
//! a.bind(L1);
//! ~~~
class Label : public Operand {
public:
  //! Type of the Label.
  enum Type : uint32_t {
    kTypeAnonymous = 0,                  //!< Anonymous (unnamed) label.
    kTypeLocal     = 1,                  //!< Local label (always has parentId).
    kTypeGlobal    = 2,                  //!< Global label (never has parentId).
    kTypeCount     = 3                   //!< Number of label types.
  };

  // TODO: Find a better place, find a better name.
  enum {
    //! Label tag is used as a sub-type, forming a unique signature across all
    //! operand types as 0x1 is never associated with any register (reg-type).
    //! This means that a memory operand's BASE register can be constructed
    //! from virtually any operand (register vs. label) by just assigning its
    //! type (reg type or label-tag) and operand id.
    kLabelTag = 0x1
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create a label operand without ID (you must set the ID to make it valid).
  constexpr Label() noexcept : Operand(Globals::Init, kOpLabel, 0, 0, 0) {}
  //! Create a cloned label operand of `other` .
  constexpr Label(const Label& other) noexcept : Operand(other) {}
  //! Create a label operand of the given `id`.
  explicit constexpr Label(uint32_t id) noexcept : Operand(Globals::Init, kOpLabel, id, 0, 0) {}

  explicit inline Label(Globals::NoInit_) noexcept : Operand(Globals::NoInit) {}

  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  //! Reset label, will reset all properties and set its ID to `0`.
  ASMJIT_INLINE void reset() noexcept {
    _label.signature = kOpLabel;
    _label.id = 0;
    _p64[1] = 0;
  }

  // --------------------------------------------------------------------------
  // [Label Specific]
  // --------------------------------------------------------------------------

  //! Get whether the label was created by CodeEmitter and has an assigned id.
  constexpr bool isValid() const noexcept { return _label.id != 0; }
  //! Set label id.
  inline void setId(uint32_t id) { _label.id = id; }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  ASMJIT_INLINE Label& operator=(const Label& other) noexcept { copyFrom(other); return *this; }
};

// ============================================================================
// [asmjit::Reg]
// ============================================================================

#define ASMJIT_DEFINE_REG_TRAITS(TRAITS_T, REG_T, TYPE, GROUP, SIZE, COUNT, TYPE_ID) \
template<>                                                                    \
struct TRAITS_T < TYPE > {                                                    \
  typedef REG_T RegT;                                                         \
                                                                              \
  enum : uint32_t {                                                           \
    kValid     = 1,                                                           \
    kCount     = COUNT,                                                       \
    kTypeId    = TYPE_ID,                                                     \
                                                                              \
    kType      = TYPE,                                                        \
    kGroup     = GROUP,                                                       \
    kSize      = SIZE,                                                        \
    kSignature = (Operand::kOpReg << Operand::kSignatureOpShift      ) |      \
                 (kType           << Operand::kSignatureRegTypeShift ) |      \
                 (kGroup          << Operand::kSignatureRegGroupShift) |      \
                 (kSize           << Operand::kSignatureSizeShift    )        \
  };                                                                          \
}

#define ASMJIT_DEFINE_ABSTRACT_REG(REG_T, BASE_T)                             \
public:                                                                       \
  /*! Default constructor doesn't setup anything, it's like `Operand()`. */   \
  constexpr REG_T() noexcept                                                  \
    : BASE_T() {}                                                             \
                                                                              \
  /*! Copy the `other` REG_T register operand. */                             \
  constexpr REG_T(const REG_T& other) noexcept                                \
    : BASE_T(other) {}                                                        \
                                                                              \
  /*! Copy the `other` REG_T register operand having its id set to `rId` */   \
  constexpr REG_T(const Reg& other, uint32_t rId) noexcept                    \
    : BASE_T(other, rId) {}                                                   \
                                                                              \
  /*! Create a REG_T register operand based on `signature` and `rId`. */      \
  constexpr REG_T(uint32_t signature, uint32_t rId) noexcept                  \
    : BASE_T(signature, rId) {}                                               \
                                                                              \
  /*! Create a completely uninitialized REG_T register operand (garbage). */  \
  explicit inline REG_T(Globals::NoInit_) noexcept                            \
    : BASE_T(Globals::NoInit) {}                                              \
                                                                              \
  /*! Create a new register from register type and id. */                     \
  static inline REG_T fromTypeAndId(uint32_t rType, uint32_t rId) noexcept {  \
    return REG_T(signatureOf(rType), rId);                                    \
  }                                                                           \
                                                                              \
  /*! Clone the register operand. */                                          \
  constexpr REG_T clone() const noexcept { return REG_T(*this); }             \
                                                                              \
  inline REG_T& operator=(const REG_T& other) noexcept {                      \
    copyFrom(other); return *this;                                            \
  }

#define ASMJIT_DEFINE_FINAL_REG(REG_T, BASE_T, TRAITS_T)                      \
  ASMJIT_DEFINE_ABSTRACT_REG(REG_T, BASE_T)                                   \
                                                                              \
  enum {                                                                      \
    kThisType  = TRAITS_T::kType,                                             \
    kThisGroup = TRAITS_T::kGroup,                                            \
    kThisSize  = TRAITS_T::kSize,                                             \
    kSignature = TRAITS_T::kSignature                                         \
  };                                                                          \
                                                                              \
  /*! Create a REG_T register with `rId`. */                                  \
  explicit constexpr REG_T(uint32_t rId) noexcept                             \
    : BASE_T(kSignature, rId) {}

//! Structure that allows to extract a register information based on the signature.
//!
//! This information is compatible with operand's signature (32-bit integer)
//! and `RegInfo` just provides easy way to access it.
struct RegInfo {
  inline void reset() noexcept { _signature = 0; }
  inline void setSignature(uint32_t signature) noexcept { _signature = signature; }

  constexpr uint32_t isValid() const noexcept { return _signature != 0; }
  constexpr uint32_t getSignature() const noexcept { return _signature; }
  constexpr uint32_t getOp() const noexcept { return (_signature >> Operand::kSignatureOpShift) & Operand::kSignatureOpBits; }
  constexpr uint32_t getType() const noexcept { return (_signature >> Operand::kSignatureRegTypeShift) & Operand::kSignatureRegTypeBits; }
  constexpr uint32_t getGroup() const noexcept { return (_signature >> Operand::kSignatureRegGroupShift) & Operand::kSignatureRegGroupBits; }
  constexpr uint32_t getSize() const noexcept { return (_signature >> Operand::kSignatureSizeShift) & Operand::kSignatureSizeBits; }

  uint32_t _signature;
};

//! Physical/Virtual register operand.
class Reg : public Operand {
public:
  //! Architecture neutral register types.
  //!
  //! These must be reused by any platform that contains that types. All GP
  //! and VEC registers are also allowed by design to be part of a BASE|INDEX
  //! of a memory operand.
  enum RegType : uint32_t {
    kRegNone       = 0,                  //!< No register - unused, invalid, multiple meanings.
    // (1 is used as a LabelTag)
    kRegGp8Lo      = 2,                  //!< 8-bit low general purpose register (X86).
    kRegGp8Hi      = 3,                  //!< 8-bit high general purpose register (X86).
    kRegGp16       = 4,                  //!< 16-bit general purpose register (X86).
    kRegGp32       = 5,                  //!< 32-bit general purpose register (X86|ARM).
    kRegGp64       = 6,                  //!< 64-bit general purpose register (X86|ARM).
    kRegVec32      = 7,                  //!< 32-bit view of a vector register (ARM).
    kRegVec64      = 8,                  //!< 64-bit view of a vector register (ARM).
    kRegVec128     = 9,                  //!< 128-bit view of a vector register (X86|ARM).
    kRegVec256     = 10,                 //!< 256-bit view of a vector register (X86).
    kRegVec512     = 11,                 //!< 512-bit view of a vector register (X86).
    kRegVec1024    = 12,                 //!< 1024-bit view of a vector register (future).
    kRegOther0     = 13,                 //!< Other0 register, should match `kOther0` group.
    kRegOther1     = 14,                 //!< Other1 register, should match `kOther1` group.
    kRegIP         = 15,                 //!< Universal id of IP/PC register (if separate).
    kRegCustom     = 16,                 //!< Start of platform dependent register types (must be honored).
    kRegMax        = 31                  //!< Maximum possible register id of all architectures.
  };

  //! Register group (architecture neutral), and some limits.
  enum Group : uint32_t {
    kGroupGp        = 0,                  //!< General purpose register group compatible with all backends.
    kGroupVec       = 1,                  //!< Vector register group compatible with all backends.
    kGroupOther0    = 2,
    kGroupOther1    = 3,
    kGroupVirt      = 4,                  //!< Count of register classes used by virtual registers.
    kGroupCount     = 16                  //!< Count of register classes used by physical registers.
  };

  enum Id : uint32_t {
    kIdBad          = 0xFFU               //!< None or any register (mostly internal).
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create a dummy register operand.
  constexpr Reg() noexcept : Operand() {}
  //! Create a new register operand which is the same as `other` .
  constexpr Reg(const Reg& other) noexcept : Operand(other) {}

  //! Create a new register operand compatible with `other`, but with a different `rId`.
  constexpr Reg(const Reg& other, uint32_t rId) noexcept : Operand(Globals::Init, other._any.signature, rId, 0, 0) {}
  //! Create a register initialized to `signature` and `rId`.
  constexpr Reg(uint32_t signature, uint32_t rId) noexcept : Operand(Globals::Init, signature, rId, 0, 0) {}

  explicit inline Reg(Globals::NoInit_) noexcept : Operand(Globals::NoInit) {}

  // --------------------------------------------------------------------------
  // [Reg Specific]
  // --------------------------------------------------------------------------

  //! Get whether the register is valid (either virtual or physical).
  constexpr bool isValid() const noexcept { return _any.signature != 0; }
  //! Get whether this is a physical register.
  constexpr bool isPhysReg() const noexcept { return _reg.id < kIdBad; }
  //! Get whether this is a virtual register (used by \ref CodeCompiler).
  ASMJIT_INLINE bool isVirtReg() const noexcept { return isPackedId(_reg.id); }

  //! Get whether this register is the same as `other`.
  //!
  //! This is just an optimization. Registers by default only use the first
  //! 8 bytes of the Operand, so this method takes advantage of this knowledge
  //! and only compares these 8 bytes. If both operands were created correctly
  //! then `isEqual()` and `isSame()` should give the same answer, however, if
  //! some one of the two operand contains a garbage or other metadata in the
  //! upper 8 bytes then `isSame()` may return `true` in cases where `isEqual()`
  //! returns false.
  constexpr bool isSame(const Reg& other) const noexcept { return _p64[0] == other._p64[0]; }

  //! Get whether the register type matches `type` - same as `isReg(type)`, provided for convenience.
  constexpr bool isType(uint32_t type) const noexcept { return (_any.signature & kSignatureRegTypeMask) == (type << kSignatureRegTypeShift); }
  //! Get whether the register group matches `group`.
  constexpr bool isGroup(uint32_t group) const noexcept { return (_any.signature & kSignatureRegGroupMask) == (group << kSignatureRegGroupShift); }

  //! Get whether the register is a general purpose register (any size).
  constexpr bool isGp() const noexcept { return isGroup(kGroupGp); }
  //! Get whether the register is a vector register.
  constexpr bool isVec() const noexcept { return isGroup(kGroupVec); }

  using Operand_::isReg;

  //! Same as `isType()`, provided for convenience.
  constexpr bool isReg(uint32_t rType) const noexcept { return isType(rType); }
  //! Get whether the register type matches `type` and register id matches `rId`.
  constexpr bool isReg(uint32_t rType, uint32_t rId) const noexcept { return isType(rType) && getId() == rId; }

  //! Get the register type.
  constexpr uint32_t getType() const noexcept { return _getSignatureData(kSignatureRegTypeBits, kSignatureRegTypeShift); }
  //! Get the register group.
  constexpr uint32_t getGroup() const noexcept { return _getSignatureData(kSignatureRegGroupBits, kSignatureRegGroupShift); }

  //! Clone the register operand.
  constexpr Reg clone() const noexcept { return Reg(*this); }

  //! Cast this register to `RegT` by also changing its signature.
  //!
  //! NOTE: Improper use of `cloneAs()` can lead to hard-to-debug errors.
  template<typename RegT>
  constexpr RegT cloneAs() const noexcept { return RegT(RegT::kSignature, getId()); }

  //! Cast this register to `other` by also changing its signature.
  //!
  //! NOTE: Improper use of `cloneAs()` can lead to hard-to-debug errors.
  template<typename RegT>
  constexpr RegT cloneAs(const RegT& other) const noexcept { return RegT(other.getSignature(), getId()); }

  //! Set the register id to `rId`.
  inline void setId(uint32_t rId) noexcept { _reg.id = rId; }

  //! Set a 32-bit operand signature based on traits of `RegT`.
  template<typename RegT>
  inline void setSignatureT() noexcept { _any.signature = RegT::kSignature; }

  //! Set register's `signature` and `rId`.
  inline void setSignatureAndId(uint32_t signature, uint32_t rId) noexcept {
    _reg.signature = signature;
    _reg.id = rId;
  }

  // --------------------------------------------------------------------------
  // [Reg Statics]
  // --------------------------------------------------------------------------

  static ASMJIT_INLINE bool isGp(const Operand_& op) noexcept {
    // Check operand type and register group. Not interested in register type and size.
    const uint32_t kSgn = (kOpReg   << kSignatureOpShift      ) |
                          (kGroupGp << kSignatureRegGroupShift) ;
    return (op.getSignature() & (kSignatureOpMask | kSignatureRegGroupMask)) == kSgn;
  }

  //! Get whether the `op` operand is either a low or high 8-bit GPB register.
  static ASMJIT_INLINE bool isVec(const Operand_& op) noexcept {
    // Check operand type and register group. Not interested in register type and size.
    const uint32_t kSgn = (kOpReg    << kSignatureOpShift      ) |
                          (kGroupVec << kSignatureRegGroupShift) ;
    return (op.getSignature() & (kSignatureOpMask | kSignatureRegGroupMask)) == kSgn;
  }

  static ASMJIT_INLINE bool isGp(const Operand_& op, uint32_t rId) noexcept { return isGp(op) & (op.getId() == rId); }
  static ASMJIT_INLINE bool isVec(const Operand_& op, uint32_t rId) noexcept { return isVec(op) & (op.getId() == rId); }
};

// ============================================================================
// [asmjit::RegOnly]
// ============================================================================

//! RegOnly is 8-byte version of `Reg` that only allows to store either `Reg`
//! or nothing. This class was designed to decrease the space consumed by each
//! extra "operand" in `CodeEmitter` and `CBInst` classes.
struct RegOnly {
  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  //! Initialize the `RegOnly` instance to hold register `signature` and `id`.
  inline void init(uint32_t signature, uint32_t id) noexcept {
    _signature = signature;
    _id = id;
  }

  inline void init(const Reg& reg) noexcept { init(reg.getSignature(), reg.getId()); }
  inline void init(const RegOnly& reg) noexcept { init(reg.getSignature(), reg.getId()); }

  //! Reset the `RegOnly` to none.
  inline void reset() noexcept { init(0, 0); }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get whether the `ExtraReg` is none (same as calling `Operand_::isNone()`).
  constexpr bool isNone() const noexcept { return _signature == 0; }
  //! Get whether the register is valid (either virtual or physical).
  constexpr bool isValid() const noexcept { return _signature != 0; }

  //! Get whether this is a physical register.
  ASMJIT_INLINE bool isPhysReg() const noexcept { return _id < Reg::kIdBad; }
  //! Get whether this is a virtual register (used by \ref CodeCompiler).
  ASMJIT_INLINE bool isVirtReg() const noexcept { return Operand::isPackedId(_id); }

  //! Get the register signature or 0.
  ASMJIT_INLINE uint32_t getSignature() const noexcept { return _signature; }

  //! Get the register id or 0.
  constexpr uint32_t getId() const noexcept { return _id; }
  //! Set the register id.
  ASMJIT_INLINE void setId(uint32_t id) noexcept { _id = id; }

  //! \internal
  //!
  //! Unpacks information from operand's signature.
  constexpr uint32_t _getSignatureData(uint32_t bits, uint32_t shift) const noexcept { return (_signature >> shift) & bits; }

  //! Get the register type.
  constexpr uint32_t getType() const noexcept { return _getSignatureData(Operand::kSignatureRegTypeBits, Operand::kSignatureRegTypeShift); }
  //! Get constexpr register group.
  constexpr uint32_t getGroup() const noexcept { return _getSignatureData(Operand::kSignatureRegGroupBits, Operand::kSignatureRegGroupShift); }

  // --------------------------------------------------------------------------
  // [ToReg]
  // --------------------------------------------------------------------------

  //! Convert back to `RegT` operand.
  template<typename RegT>
  constexpr RegT toReg() const noexcept { return RegT(_signature, _id); }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Type of the operand, either `kOpNone` or `kOpReg`.
  uint32_t _signature;
  //! Physical or virtual register id.
  uint32_t _id;
};

// ============================================================================
// [asmjit::Mem]
// ============================================================================

//! Base class for all memory operands.
//!
//! NOTE: It's tricky to pack all possible cases that define a memory operand
//! into just 16 bytes. The `Mem` splits data into the following parts:
//!
//!   BASE - Base register or label - requires 36 bits total. 4 bits are used
//!     to encode the type of the BASE operand (label vs. register type) and
//!     the remaining 32 bits define the BASE id, which can be a physical or
//!     virtual register index. If BASE type is zero, which is never used as
//!     a register-type and label doesn't use it as well then BASE field
//!     contains a high DWORD of a possible 64-bit absolute address, which is
//!     possible on X64.
//!
//!   INDEX - Index register (or theoretically Label, which doesn't make sense).
//!     Encoding is similar to BASE - it also requires 36 bits and splits the
//!     encoding to INDEX type (4 bits defining the register type) and id (32-bits).
//!
//!   OFFSET - A relative offset of the address. Basically if BASE is specified
//!     the relative displacement adjusts BASE and an optional INDEX. if BASE is
//!     not specified then the OFFSET should be considered as ABSOLUTE address
//!     (at least on X86/X64). In that case its low 32 bits are stored in
//!     DISPLACEMENT field and the remaining high 32 bits are stored in BASE.
//!
//!   OTHER FIELDS - There is rest 8 bits that can be used for whatever purpose.
//!          The X86Mem operand uses these bits to store segment override
//!          prefix and index shift (scale).
class Mem : public Operand {
public:
  enum AddrType : uint32_t {
    kAddrTypeDefault = 0,
    kAddrTypeAbs     = 1,
    kAddrTypeRel     = 2,
    kAddrTypeWrt     = 3
  };

  // Shortcuts.
  enum SignatureMem : uint32_t {
    kSignatureMemAbs = kAddrTypeAbs << kSignatureMemAddrTypeShift,
    kSignatureMemRel = kAddrTypeRel << kSignatureMemAddrTypeShift,
    kSignatureMemWrt = kAddrTypeWrt << kSignatureMemAddrTypeShift
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Construct a default `Mem` operand, that points to [0].
  constexpr Mem() noexcept : Operand(Globals::Init, kOpMem, 0, 0, 0) {}
  //! Construct a `Mem` operand that is a clone of `other`.
  constexpr Mem(const Mem& other) noexcept : Operand(other) {}

  constexpr Mem(Globals::Init_, uint32_t baseType, uint32_t baseId, uint32_t indexType, uint32_t indexId, int32_t off, uint32_t size, uint32_t flags) noexcept
    : Operand(Globals::Init,
              kOpMem | (baseType  << kSignatureMemBaseTypeShift )
                     | (indexType << kSignatureMemIndexTypeShift)
                     | (size      << kSignatureSizeShift        )
                     | flags,
              baseId,
              indexId,
              uint32_t(off)) {}
  explicit inline Mem(Globals::NoInit_) noexcept : Operand(Globals::NoInit) {}

  // --------------------------------------------------------------------------
  // [Mem Specific]
  // --------------------------------------------------------------------------

  //! Clone `Mem` operand.
  constexpr Mem clone() const noexcept { return Mem(*this); }

  //! Reset the memory operand - after reset the memory points to [0].
  ASMJIT_INLINE void reset() noexcept {
    _any.signature = kOpMem;
    _any.id = 0;
    _p64[1] = 0;
  }

  constexpr bool hasAddrType() const noexcept { return _hasSignatureData(kSignatureMemAddrTypeMask); }
  constexpr uint32_t getAddrType() const noexcept { return _getSignatureData(kSignatureMemAddrTypeBits, kSignatureMemAddrTypeShift); }
  inline void setAddrType(uint32_t addrType) noexcept { return _setSignatureData(addrType, kSignatureMemAddrTypeBits, kSignatureMemAddrTypeShift); }
  inline void resetAddrType() noexcept { return _clearSignatureData(kSignatureMemAddrTypeBits, kSignatureMemAddrTypeShift); }

  constexpr bool isAbs() const noexcept { return getAddrType() == kAddrTypeAbs; }
  inline void setAbs() noexcept { setAddrType(kAddrTypeAbs); }

  constexpr bool isRel() const noexcept { return getAddrType() == kAddrTypeRel; }
  inline void setRel() noexcept { setAddrType(kAddrTypeRel); }

  constexpr bool isWrt() const noexcept { return getAddrType() == kAddrTypeWrt; }
  inline void setWrt() noexcept { setAddrType(kAddrTypeWrt); }

  constexpr bool isRegHome() const noexcept { return _hasSignatureData(kSignatureMemRegHomeFlag); }
  inline void setRegHome() noexcept { _any.signature |= kSignatureMemRegHomeFlag; }
  inline void clearRegHome() noexcept { _any.signature &= ~kSignatureMemRegHomeFlag; }

  //! Get whether the memory operand has a BASE register or label specified.
  constexpr bool hasBase() const noexcept { return (_any.signature & kSignatureMemBaseTypeMask) != 0; }
  //! Get whether the memory operand has an INDEX register specified.
  constexpr bool hasIndex() const noexcept { return (_any.signature & kSignatureMemIndexTypeMask) != 0; }
  //! Get whether the memory operand has BASE and INDEX register.
  constexpr bool hasBaseOrIndex() const noexcept { return (_any.signature & kSignatureMemBaseIndexMask) != 0; }
  //! Get whether the memory operand has BASE and INDEX register.
  constexpr bool hasBaseAndIndex() const noexcept { return (_any.signature & kSignatureMemBaseTypeMask) != 0 && (_any.signature & kSignatureMemIndexTypeMask) != 0; }

  //! Get whether the BASE operand is a register (registers start after `kLabelTag`).
  constexpr bool hasBaseReg() const noexcept { return (_any.signature & kSignatureMemBaseTypeMask) > (Label::kLabelTag << kSignatureMemBaseTypeShift); }
  //! Get whether the BASE operand is a label.
  constexpr bool hasBaseLabel() const noexcept { return (_any.signature & kSignatureMemBaseTypeMask) == (Label::kLabelTag << kSignatureMemBaseTypeShift); }
  //! Get whether the INDEX operand is a register (registers start after `kLabelTag`).
  constexpr bool hasIndexReg() const noexcept { return (_any.signature & kSignatureMemIndexTypeMask) > (Label::kLabelTag << kSignatureMemIndexTypeShift); }

  //! Get type of a BASE register (0 if this memory operand doesn't use the BASE register).
  //!
  //! NOTE: If the returned type is one (a value never associated to a register
  //! type) the BASE is not register, but it's a label. One equals to `kLabelTag`.
  //! You should always check `hasBaseLabel()` before using `getBaseId()` result.
  constexpr uint32_t getBaseType() const noexcept { return _getSignatureData(kSignatureMemBaseTypeBits, kSignatureMemBaseTypeShift); }
  //! Get type of an INDEX register (0 if this memory operand doesn't use the INDEX register).
  constexpr uint32_t getIndexType() const noexcept { return _getSignatureData(kSignatureMemIndexTypeBits, kSignatureMemIndexTypeShift); }
  //! This is used internally for BASE+INDEX validation.
  constexpr uint32_t getBaseAndIndexTypes() const noexcept { return _getSignatureData(kSignatureMemBaseIndexBits, kSignatureMemBaseIndexShift); }

  //! Get both BASE (4:0 bits) and INDEX (9:5 bits) types combined into a single integer.
  //!
  //! Get id of the BASE register or label (if the BASE was specified as label).
  constexpr uint32_t getBaseId() const noexcept { return _mem.base; }
  //! Get id of the INDEX register.
  constexpr uint32_t getIndexId() const noexcept { return _mem.index; }

  //! Set id of the BASE register (without modifying its type).
  inline void setBaseId(uint32_t rId) noexcept { _mem.base = rId; }
  //! Set id of the INDEX register (without modifying its type).
  inline void setIndexId(uint32_t rId) noexcept { _mem.index = rId; }

  inline void setBase(const Reg& base) noexcept { return _setBase(base.getType(), base.getId()); }
  inline void setIndex(const Reg& index) noexcept { return _setIndex(index.getType(), index.getId()); }

  inline void _setBase(uint32_t rType, uint32_t rId) noexcept {
    _setSignatureData(rType, kSignatureMemBaseTypeBits, kSignatureMemBaseTypeShift);
    _mem.base = rId;
  }

  inline void _setIndex(uint32_t rType, uint32_t rId) noexcept {
    _setSignatureData(rType, kSignatureMemIndexTypeBits, kSignatureMemIndexTypeShift);
    _mem.index = rId;
  }

  //! Reset the memory operand's BASE register / label.
  inline void resetBase() noexcept { _setBase(0, 0); }
  //! Reset the memory operand's INDEX register.
  inline void resetIndex() noexcept { _setIndex(0, 0); }

  //! Set memory operand size.
  inline void setSize(uint32_t size) noexcept {
    _setSignatureData(size, kSignatureSizeBits, kSignatureSizeShift);
  }

  //! Get whether the memory operand has a 64-bit offset or absolute address.
  //!
  //! If this is true then `hasBase()` must always report false.
  constexpr bool isOffset64Bit() const noexcept { return getBaseType() == 0; }

  //! Get whether the memory operand has a non-zero offset or absolute address.
  constexpr bool hasOffset() const noexcept {
    return (_mem.offsetLo32 | uint32_t(_mem.base & IntUtils::maskFromBool<uint32_t>(isOffset64Bit()))) != 0;
  }

  //! Get a 64-bit offset or absolute address.
  constexpr int64_t getOffset() const noexcept {
    return isOffset64Bit() ? int64_t(uint64_t(_mem.offsetLo32) | (uint64_t(_mem.base) << 32))
                            : int64_t(int32_t(_mem.offsetLo32)); // Sign extend 32-bit offset.
  }

  //! Get a lower part of a 64-bit offset or absolute address.
  constexpr int32_t getOffsetLo32() const noexcept { return int32_t(_mem.offsetLo32); }
  //! Get a higher part of a 64-bit offset or absolute address.
  //!
  //! NOTE: This function is UNSAFE and returns garbage if `isOffset64Bit()`
  //! returns false. Never use it blindly without checking it first.
  constexpr int32_t getOffsetHi32() const noexcept { return int32_t(_mem.base); }

  //! Set a 64-bit offset or an absolute address to `offset`.
  //!
  //! NOTE: This functions attempts to set both high and low parts of a 64-bit
  //! offset, however, if the operand has a BASE register it will store only the
  //! low 32 bits of the offset / address as there is no way to store both BASE
  //! and 64-bit offset, and there is currently no architecture that has such
  //! capability targeted by AsmJit.
  inline void setOffset(int64_t offset) noexcept {
    uint32_t lo = uint32_t(uint64_t(offset) & 0xFFFFFFFFU);
    uint32_t hi = uint32_t(uint64_t(offset) >> 32);
    uint32_t hiMsk = IntUtils::maskFromBool<uint32_t>(isOffset64Bit());

    _mem.offsetLo32 = lo;
    _mem.base = (hi & hiMsk) | (_mem.base & ~hiMsk);
  }
  //! Set a low 32-bit offset to `offset` (don't use without knowing how Mem works).
  inline void setOffsetLo32(int32_t offset) noexcept { _mem.offsetLo32 = uint32_t(offset); }

  //! Adjust the offset by `offset`.
  //!
  //! NOTE: This is a fast function that doesn't use the HI 32-bits of a
  //! 64-bit offset. Use it only if you know that there is a BASE register
  //! and the offset is only 32 bits anyway.

  //! Adjust the offset by a 64-bit `offset`.
  inline void addOffset(int64_t offset) noexcept {
    if (isOffset64Bit()) {
      int64_t result = offset + int64_t(uint64_t(_mem.offsetLo32) | (uint64_t(_mem.base) << 32));
      _mem.offsetLo32 = uint32_t(uint64_t(result) & 0xFFFFFFFFU);
      _mem.base       = uint32_t(uint64_t(result) >> 32);
    }
    else {
      _mem.offsetLo32 += uint32_t(uint64_t(offset) & 0xFFFFFFFFU);
    }
  }
  //! Add `offset` to a low 32-bit offset part (don't use without knowing how Mem works).
  inline void addOffsetLo32(int32_t offset) noexcept { _mem.offsetLo32 += uint32_t(offset); }

  //! Reset the memory offset to zero.
  inline void resetOffset() noexcept { setOffset(0); }
  //! Reset the lo part of memory offset to zero (don't use without knowing how Mem works).
  inline void resetOffsetLo32() noexcept { setOffsetLo32(0); }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  inline Mem& operator=(const Mem& other) noexcept { copyFrom(other); return *this; }
};

// ============================================================================
// [asmjit::Imm]
// ============================================================================

//! Immediate operand.
//!
//! Immediate operand is usually part of instruction itself. It's inlined after
//! or before the instruction opcode. Immediates can be only signed or unsigned
//! integers.
//!
//! To create immediate operand use `imm()` or `imm_u()` non-members or `Imm`
//! constructors.
class Imm : public Operand {
public:
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create a new immediate value (initial value is 0).
  constexpr Imm() noexcept : Operand(Globals::Init, kOpImm, 0, 0, 0) {}
  //! Create a new immediate value from `other`.
  constexpr Imm(const Imm& other) noexcept : Operand(other) {}
  //! Create a new signed immediate value, assigning the value to `val`.
  explicit constexpr Imm(int64_t val) noexcept : Operand(Globals::Init, kOpImm, 0, IntUtils::unpackU32At0(val), IntUtils::unpackU32At1(val)) {}

  explicit inline Imm(Globals::NoInit_) noexcept : Operand(Globals::NoInit) {}

  // --------------------------------------------------------------------------
  // [Immediate Specific]
  // --------------------------------------------------------------------------

  //! Clone `Imm` operand.
  constexpr Imm clone() const noexcept { return Imm(*this); }

  //! Get whether the immediate can be casted to 8-bit signed integer.
  constexpr bool isInt8() const noexcept { return IntUtils::isInt8(_imm.value.i64); }
  //! Get whether the immediate can be casted to 8-bit unsigned integer.
  constexpr bool isUInt8() const noexcept { return IntUtils::isUInt8(_imm.value.i64); }

  //! Get whether the immediate can be casted to 16-bit signed integer.
  constexpr bool isInt16() const noexcept { return IntUtils::isInt16(_imm.value.i64); }
  //! Get whether the immediate can be casted to 16-bit unsigned integer.
  constexpr bool isUInt16() const noexcept { return IntUtils::isUInt16(_imm.value.i64); }

  //! Get whether the immediate can be casted to 32-bit signed integer.
  constexpr bool isInt32() const noexcept { return IntUtils::isInt32(_imm.value.i64); }
  //! Get whether the immediate can be casted to 32-bit unsigned integer.
  constexpr bool isUInt32() const noexcept { return IntUtils::isUInt32(_imm.value.i64); }

  //! Get immediate value as 8-bit signed integer.
  constexpr int8_t getInt8() const noexcept { return int8_t(_imm.value.i32Lo & 0xFF); }
  //! Get immediate value as 8-bit unsigned integer.
  constexpr uint8_t getUInt8() const noexcept { return uint8_t(_imm.value.u32Lo & 0xFFU); }
  //! Get immediate value as 16-bit signed integer.
  constexpr int16_t getInt16() const noexcept { return int16_t(_imm.value.i32Lo & 0xFFFF);}
  //! Get immediate value as 16-bit unsigned integer.
  constexpr uint16_t getUInt16() const noexcept { return uint16_t(_imm.value.u32Lo & 0xFFFFU);}

  //! Get immediate value as 32-bit signed integer.
  constexpr int32_t getInt32() const noexcept { return _imm.value.i32Lo; }
  //! Get low 32-bit signed integer.
  constexpr int32_t getInt32Lo() const noexcept { return _imm.value.i32Lo; }
  //! Get high 32-bit signed integer.
  constexpr int32_t getInt32Hi() const noexcept { return _imm.value.i32Hi; }

  //! Get immediate value as 32-bit unsigned integer.
  constexpr uint32_t getUInt32() const noexcept { return _imm.value.u32Lo; }
  //! Get low 32-bit signed integer.
  constexpr uint32_t getUInt32Lo() const noexcept { return _imm.value.u32Lo; }
  //! Get high 32-bit signed integer.
  constexpr uint32_t getUInt32Hi() const noexcept { return _imm.value.u32Hi; }

  //! Get immediate value as 64-bit signed integer.
  constexpr int64_t getInt64() const noexcept { return _imm.value.i64; }
  //! Get immediate value as 64-bit unsigned integer.
  constexpr uint64_t getUInt64() const noexcept { return _imm.value.u64; }

  //! Get immediate value as `intptr_t`.
  constexpr intptr_t getIntPtr() const noexcept { return (sizeof(intptr_t) == sizeof(int64_t)) ? intptr_t(getInt64()) : intptr_t(getInt32()); }
  //! Get immediate value as `uintptr_t`.
  constexpr uintptr_t getUIntPtr() const noexcept { return (sizeof(uintptr_t) == sizeof(uint64_t)) ? uintptr_t(getUInt64()) : uintptr_t(getUInt32()); }

  //! Set immediate value to 8-bit signed integer `val`.
  inline void setInt8(int8_t val) noexcept { _imm.value.i64 = int64_t(val); }
  //! Set immediate value to 8-bit unsigned integer `val`.
  inline void setUInt8(uint8_t val) noexcept { _imm.value.u64 = uint64_t(val); }

  //! Set immediate value to 16-bit signed integer `val`.
  inline void setInt16(int16_t val) noexcept { _imm.value.i64 = int64_t(val); }
  //! Set immediate value to 16-bit unsigned integer `val`.
  inline void setUInt16(uint16_t val) noexcept { _imm.value.u64 = uint64_t(val); }

  //! Set immediate value to 32-bit signed integer `val`.
  inline void setInt32(int32_t val) noexcept { _imm.value.i64 = int64_t(val); }
  //! Set immediate value to 32-bit unsigned integer `val`.
  inline void setUInt32(uint32_t val) noexcept { _imm.value.u64 = uint64_t(val); }

  //! Set immediate value to 64-bit signed integer `val`.
  inline void setInt64(int64_t val) noexcept { _imm.value.i64 = val; }
  //! Set immediate value to 64-bit unsigned integer `val`.
  inline void setUInt64(uint64_t val) noexcept { _imm.value.u64 = val; }
  //! Set immediate value to intptr_t `val`.
  inline void setIntPtr(intptr_t val) noexcept { _imm.value.i64 = int64_t(val); }
  //! Set immediate value to uintptr_t `val`.
  inline void setUIntPtr(uintptr_t val) noexcept { _imm.value.u64 = uint64_t(val); }

  //! Set immediate value as unsigned type to `val`.
  inline void setPtr(void* p) noexcept { setUIntPtr((uintptr_t)p); }
  //! Set immediate value to `val`.
  template<typename T>
  inline void setValue(T val) noexcept { setInt64((int64_t)val); }

  // --------------------------------------------------------------------------
  // [Float]
  // --------------------------------------------------------------------------

  inline void setFloat(float f) noexcept {
    _imm.value.f32Lo = f;
    _imm.value.u32Hi = 0;
  }

  inline void setDouble(double d) noexcept {
    _imm.value.f64 = d;
  }

  // --------------------------------------------------------------------------
  // [Sign Extend / Zero Extend]
  // --------------------------------------------------------------------------

  inline void signExtend8Bits() noexcept { _imm.value.i64 = int64_t(int8_t(_imm.value.u64 & 0x000000FFU)); }
  inline void signExtend16Bits() noexcept { _imm.value.i64 = int64_t(int16_t(_imm.value.u64 & 0x0000FFFFU)); }
  inline void signExtend32Bits() noexcept { _imm.value.i64 = int64_t(int32_t(_imm.value.u64 & 0xFFFFFFFFU)); }

  inline void zeroExtend8Bits() noexcept { _imm.value.u64 = _imm.value.u64 & 0x000000FFU; }
  inline void zeroExtend16Bits() noexcept { _imm.value.u64 = _imm.value.u64 & 0x0000FFFFU; }
  inline void zeroExtend32Bits() noexcept { _imm.value.u64 = _imm.value.u64 & 0xFFFFFFFFU; }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  //! Assign `other` to the immediate operand.
  inline Imm& operator=(const Imm& other) noexcept { copyFrom(other); return *this; }
};

//! Create a signed immediate operand.
static constexpr Imm imm(int64_t val) noexcept { return Imm(val); }
//! Create an unsigned immediate operand.
static constexpr Imm imm_u(uint64_t val) noexcept { return Imm(int64_t(val)); }
//! Create an immediate operand from `p`.
template<typename T>
static constexpr Imm imm_ptr(T p) noexcept { return Imm(int64_t((intptr_t)p)); }

// ============================================================================
// [asmjit::TypeId]
// ============================================================================

//! Type-id.
//!
//! This is an additional information that can be used to describe a physical
//! or virtual register. it's used mostly by CodeCompiler to describe register
//! representation (the group of data stored in the register and the width used)
//! and it's also used by APIs that allow to describe and work with function
//! signatures.
struct TypeId {
  // --------------------------------------------------------------------------
  // [Id]
  // --------------------------------------------------------------------------

  enum Id : uint32_t {
    kVoid         = 0,

    _kIntStart    = 32,
    _kIntEnd      = 41,

    kIntPtr       = 32,
    kUIntPtr      = 33,

    kI8           = 34,
    kU8           = 35,
    kI16          = 36,
    kU16          = 37,
    kI32          = 38,
    kU32          = 39,
    kI64          = 40,
    kU64          = 41,

    _kFloatStart  = 42,
    _kFloatEnd    = 44,

    kF32          = 42,
    kF64          = 43,
    kF80          = 44,

    _kMaskStart   = 45,
    _kMaskEnd     = 48,

    kMask8        = 45,
    kMask16       = 46,
    kMask32       = 47,
    kMask64       = 48,

    _kMmxStart    = 49,
    _kMmxEnd      = 50,

    kMmx32        = 49,
    kMmx64        = 50,

    _kVec32Start  = 51,
    _kVec32End    = 60,

    kI8x4         = 51,
    kU8x4         = 52,
    kI16x2        = 53,
    kU16x2        = 54,
    kI32x1        = 55,
    kU32x1        = 56,
    kF32x1        = 59,

    _kVec64Start  = 61,
    _kVec64End    = 70,

    kI8x8         = 61,
    kU8x8         = 62,
    kI16x4        = 63,
    kU16x4        = 64,
    kI32x2        = 65,
    kU32x2        = 66,
    kI64x1        = 67,
    kU64x1        = 68,
    kF32x2        = 69,
    kF64x1        = 70,

    _kVec128Start = 71,
    _kVec128End   = 80,

    kI8x16        = 71,
    kU8x16        = 72,
    kI16x8        = 73,
    kU16x8        = 74,
    kI32x4        = 75,
    kU32x4        = 76,
    kI64x2        = 77,
    kU64x2        = 78,
    kF32x4        = 79,
    kF64x2        = 80,

    _kVec256Start = 81,
    _kVec256End   = 90,

    kI8x32        = 81,
    kU8x32        = 82,
    kI16x16       = 83,
    kU16x16       = 84,
    kI32x8        = 85,
    kU32x8        = 86,
    kI64x4        = 87,
    kU64x4        = 88,
    kF32x8        = 89,
    kF64x4        = 90,

    _kVec512Start = 91,
    _kVec512End   = 100,

    kI8x64        = 91,
    kU8x64        = 92,
    kI16x32       = 93,
    kU16x32       = 94,
    kI32x16       = 95,
    kU32x16       = 96,
    kI64x8        = 97,
    kU64x8        = 98,
    kF32x16       = 99,
    kF64x8        = 100,

    kCount        = 101
  };

  // --------------------------------------------------------------------------
  // [TypeName - Used by Templates]
  // --------------------------------------------------------------------------

  struct Int8    {};                     //!< int8_t as C++ type-name.
  struct UInt8   {};                     //!< uint8_t as C++ type-name.
  struct Int16   {};                     //!< int16_t as C++ type-name.
  struct UInt16  {};                     //!< uint16_t as C++ type-name.
  struct Int32   {};                     //!< int32_t as C++ type-name.
  struct UInt32  {};                     //!< uint32_t as C++ type-name.
  struct Int64   {};                     //!< int64_t as C++ type-name.
  struct UInt64  {};                     //!< uint64_t as C++ type-name.
  struct IntPtr  {};                     //!< intptr_t as C++ type-name.
  struct UIntPtr {};                     //!< uintptr_t as C++ type-name.
  struct Float   {};                     //!< float as C++ type-name.
  struct Double  {};                     //!< double as C++ type-name.
  struct MmxReg  {};                     //!< MMX register as C++ type-name.
  struct Vec128  {};                     //!< SIMD128/XMM register as C++ type-name.
  struct Vec256  {};                     //!< SIMD256/YMM register as C++ type-name.
  struct Vec512  {};                     //!< SIMD512/ZMM register as C++ type-name.

  // --------------------------------------------------------------------------
  // [Utilities]
  // --------------------------------------------------------------------------

  struct Info {
    uint8_t sizeOf[128];
    uint8_t elementOf[128];
  };

  ASMJIT_API static const Info _info;

  static ASMJIT_INLINE bool isVoid(uint32_t typeId) noexcept { return typeId == 0; }
  static ASMJIT_INLINE bool isValid(uint32_t typeId) noexcept { return typeId >= _kIntStart && typeId <= _kVec512End; }
  static ASMJIT_INLINE bool isAbstract(uint32_t typeId) noexcept { return typeId >= kIntPtr && typeId <= kUIntPtr; }
  static ASMJIT_INLINE bool isInt(uint32_t typeId) noexcept { return typeId >= _kIntStart && typeId <= _kIntEnd; }
  static ASMJIT_INLINE bool isGpb(uint32_t typeId) noexcept { return typeId >= kI8 && typeId <= kU8; }
  static ASMJIT_INLINE bool isGpw(uint32_t typeId) noexcept { return typeId >= kI16 && typeId <= kU16; }
  static ASMJIT_INLINE bool isGpd(uint32_t typeId) noexcept { return typeId >= kI32 && typeId <= kU32; }
  static ASMJIT_INLINE bool isGpq(uint32_t typeId) noexcept { return typeId >= kI64 && typeId <= kU64; }
  static ASMJIT_INLINE bool isFloat(uint32_t typeId) noexcept { return typeId >= _kFloatStart && typeId <= _kFloatEnd; }
  static ASMJIT_INLINE bool isMask(uint32_t typeId) noexcept { return typeId >= _kMaskStart && typeId <= _kMaskEnd; }
  static ASMJIT_INLINE bool isMmx(uint32_t typeId) noexcept { return typeId >= _kMmxStart && typeId <= _kMmxEnd; }

  static ASMJIT_INLINE bool isVec(uint32_t typeId) noexcept { return typeId >= _kVec32Start && typeId <= _kVec512End; }
  static ASMJIT_INLINE bool isVec32(uint32_t typeId) noexcept { return typeId >= _kVec32Start && typeId <= _kVec32End; }
  static ASMJIT_INLINE bool isVec64(uint32_t typeId) noexcept { return typeId >= _kVec64Start && typeId <= _kVec64End; }
  static ASMJIT_INLINE bool isVec128(uint32_t typeId) noexcept { return typeId >= _kVec128Start && typeId <= _kVec128End; }
  static ASMJIT_INLINE bool isVec256(uint32_t typeId) noexcept { return typeId >= _kVec256Start && typeId <= _kVec256End; }
  static ASMJIT_INLINE bool isVec512(uint32_t typeId) noexcept { return typeId >= _kVec512Start && typeId <= _kVec512End; }

  static ASMJIT_INLINE uint32_t sizeOf(uint32_t typeId) noexcept {
    ASMJIT_ASSERT(typeId < ASMJIT_ARRAY_SIZE(_info.sizeOf));
    return _info.sizeOf[typeId];
  }

  static ASMJIT_INLINE uint32_t elementOf(uint32_t typeId) noexcept {
    ASMJIT_ASSERT(typeId < ASMJIT_ARRAY_SIZE(_info.elementOf));
    return _info.elementOf[typeId];
  }

  //! Get an offset to convert a `kIntPtr` and `kUIntPtr` TypeId into a
  //! type that matches `gpSize` (general-purpose register size). If you
  //! find such TypeId it's then only about adding the offset to it.
  //!
  //! For example:
  //! ~~~
  //! uint32_t gpSize = '4' or '8';
  //! uint32_t deabstractDelta = TypeId::deabstractDeltaOfSize(gpSize);
  //!
  //! uint32_t typeId = 'some type-id';
  //!
  //! // Normalize some typeId into a non-abstract typeId.
  //! if (TypeId::isAbstract(typeId)) typeId += deabstractDelta;
  //!
  //! // The same, but by using TypeId::deabstract() function.
  //! typeId = TypeId::deabstract(typeId, deabstractDelta);
  //! ~~~
  static ASMJIT_INLINE uint32_t deabstractDeltaOfSize(uint32_t gpSize) noexcept {
    return gpSize >= 8 ? kI64 - kIntPtr : kI32 - kIntPtr;
  }

  static ASMJIT_INLINE uint32_t deabstract(uint32_t typeId, uint32_t deabstractDelta) noexcept {
    return TypeId::isAbstract(typeId) ? typeId += deabstractDelta : typeId;
  }
};

//! TypeIdOf<> template allows to get a TypeId of a C++ type.
template<typename T> struct TypeIdOf {
  // Don't provide anything if not specialized.
};
template<typename T> struct TypeIdOf<T*> {
  enum : uint32_t {
    kTypeId = TypeId::kUIntPtr
  };
};

template<typename T>
struct TypeIdOfInt {
  enum : uint32_t {
    kSignature = uint32_t(~T(0) < T(0)),
    kTypeId = (sizeof(T) == 1) ? uint32_t(kSignature ? TypeId::kI8  : TypeId::kU8 ) :
              (sizeof(T) == 2) ? uint32_t(kSignature ? TypeId::kI16 : TypeId::kU16) :
              (sizeof(T) == 4) ? uint32_t(kSignature ? TypeId::kI32 : TypeId::kU32) :
              (sizeof(T) == 8) ? uint32_t(kSignature ? TypeId::kI64 : TypeId::kU64) : uint32_t(TypeId::kVoid)
  };
};

#define ASMJIT_DEFINE_TYPE_ID(T, TYPE_ID) \
  template<> \
  struct TypeIdOf<T> { enum { kTypeId = TYPE_ID}; }

ASMJIT_DEFINE_TYPE_ID(signed char       , TypeIdOfInt< signed char        >::kTypeId);
ASMJIT_DEFINE_TYPE_ID(unsigned char     , TypeIdOfInt< unsigned char      >::kTypeId);
ASMJIT_DEFINE_TYPE_ID(short             , TypeIdOfInt< short              >::kTypeId);
ASMJIT_DEFINE_TYPE_ID(unsigned short    , TypeIdOfInt< unsigned short     >::kTypeId);
ASMJIT_DEFINE_TYPE_ID(int               , TypeIdOfInt< int                >::kTypeId);
ASMJIT_DEFINE_TYPE_ID(unsigned int      , TypeIdOfInt< unsigned int       >::kTypeId);
ASMJIT_DEFINE_TYPE_ID(long              , TypeIdOfInt< long               >::kTypeId);
ASMJIT_DEFINE_TYPE_ID(unsigned long     , TypeIdOfInt< unsigned long      >::kTypeId);

#if ASMJIT_CC_MSC && !ASMJIT_CC_MSC_GE(16, 0, 0)
ASMJIT_DEFINE_TYPE_ID(__int64           , TypeIdOfInt< __int64            >::kTypeId);
ASMJIT_DEFINE_TYPE_ID(unsigned __int64  , TypeIdOfInt< unsigned __int64   >::kTypeId);
#else
ASMJIT_DEFINE_TYPE_ID(long long         , TypeIdOfInt< long long          >::kTypeId);
ASMJIT_DEFINE_TYPE_ID(unsigned long long, TypeIdOfInt< unsigned long long >::kTypeId);
#endif

#if ASMJIT_CC_HAS_NATIVE_CHAR
ASMJIT_DEFINE_TYPE_ID(char              , TypeIdOfInt< char               >::kTypeId);
#endif

#if ASMJIT_CC_HAS_NATIVE_CHAR16_T
ASMJIT_DEFINE_TYPE_ID(char16_t          , TypeIdOfInt< char16_t           >::kTypeId);
#endif

#if ASMJIT_CC_HAS_NATIVE_CHAR32_T
ASMJIT_DEFINE_TYPE_ID(char32_t          , TypeIdOfInt< char32_t           >::kTypeId);
#endif

#if ASMJIT_CC_HAS_NATIVE_WCHAR_T
ASMJIT_DEFINE_TYPE_ID(wchar_t           , TypeIdOfInt< wchar_t            >::kTypeId);
#endif

ASMJIT_DEFINE_TYPE_ID(void              , TypeId::kVoid);
ASMJIT_DEFINE_TYPE_ID(float             , TypeId::kF32);
ASMJIT_DEFINE_TYPE_ID(double            , TypeId::kF64);

ASMJIT_DEFINE_TYPE_ID(TypeId::Int8      , TypeId::kI8);
ASMJIT_DEFINE_TYPE_ID(TypeId::UInt8     , TypeId::kU8);
ASMJIT_DEFINE_TYPE_ID(TypeId::Int16     , TypeId::kI16);
ASMJIT_DEFINE_TYPE_ID(TypeId::UInt16    , TypeId::kU16);
ASMJIT_DEFINE_TYPE_ID(TypeId::Int32     , TypeId::kI32);
ASMJIT_DEFINE_TYPE_ID(TypeId::UInt32    , TypeId::kU32);
ASMJIT_DEFINE_TYPE_ID(TypeId::Int64     , TypeId::kI64);
ASMJIT_DEFINE_TYPE_ID(TypeId::UInt64    , TypeId::kU64);
ASMJIT_DEFINE_TYPE_ID(TypeId::IntPtr    , TypeId::kIntPtr);
ASMJIT_DEFINE_TYPE_ID(TypeId::UIntPtr   , TypeId::kUIntPtr);
ASMJIT_DEFINE_TYPE_ID(TypeId::Float     , TypeId::kF32);
ASMJIT_DEFINE_TYPE_ID(TypeId::Double    , TypeId::kF64);
ASMJIT_DEFINE_TYPE_ID(TypeId::MmxReg    , TypeId::kMmx64);
ASMJIT_DEFINE_TYPE_ID(TypeId::Vec128    , TypeId::kI32x4);
ASMJIT_DEFINE_TYPE_ID(TypeId::Vec256    , TypeId::kI32x8);
ASMJIT_DEFINE_TYPE_ID(TypeId::Vec512    , TypeId::kI32x16);

//! \}

} // asmjit namespace

// [Api-End]
#include "../asmjit_apiend.h"

// [Guard]
#endif // _ASMJIT_BASE_OPERAND_H
