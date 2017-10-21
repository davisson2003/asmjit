// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_CORE_INTUTILS_H
#define _ASMJIT_CORE_INTUTILS_H

// [Dependencies]
#include "../core/globals.h"

#if ASMJIT_CXX_MSC
  #include <intrin.h>
#endif

ASMJIT_BEGIN_NAMESPACE

//! \addtogroup asmjit_base
//! \{

//! Utilities related to integers and bitwords.
namespace IntUtils {

// ============================================================================
// [asmjit::IntUtils::FastUInt8]
// ============================================================================

#if ASMJIT_ARCH_X86
typedef uint8_t FastUInt8;
#else
typedef unsigned int FastUInt8;
#endif

// ============================================================================
// [asmjit::IntUtils::ParametrizedInt / NormalizedInt]
// ============================================================================

template<size_t SIZE, int IS_SIGNED>
struct ParametrizedInt {}; // Fail if not specialized.

template<> struct ParametrizedInt<1, 0> { typedef uint8_t  Type; };
template<> struct ParametrizedInt<1, 1> { typedef int8_t   Type; };
template<> struct ParametrizedInt<2, 0> { typedef uint16_t Type; };
template<> struct ParametrizedInt<2, 1> { typedef int16_t  Type; };
template<> struct ParametrizedInt<4, 0> { typedef uint32_t Type; };
template<> struct ParametrizedInt<4, 1> { typedef int32_t  Type; };
template<> struct ParametrizedInt<8, 0> { typedef uint64_t Type; };
template<> struct ParametrizedInt<8, 1> { typedef int64_t  Type; };

template<typename T, int IS_SIGNED = std::is_signed<T>::value>
struct NormalizedInt : public ParametrizedInt<sizeof(T) <= 4 ? size_t(4) : sizeof(T), IS_SIGNED> {};

//! Cast an integer `x` of type `T` to either `int32_t`, uint32_t`, `int64_t`,
//! or `uint64_t` depending on T. Used to cast any integer to something that
//! is always 32-bit or wider.
template<typename T>
static constexpr typename NormalizedInt<T>::Type asNormalized(T x) noexcept { return (typename NormalizedInt<T>::Type)x; }

//! Cast an integer `x` of type `T` to either `int32_t` or `int64_t` depending
//! on T. Used internally by AsmJit to dispatch arguments that can be of arbitrary
//! integer type into a function argument that is either `int32_t` or `int64_t`.
template<typename T>
static constexpr typename NormalizedInt<T, 1>::Type asInt(T x) noexcept { return (typename NormalizedInt<T, 1>::Type)x; }

//! Cast an integer `x` of type `T` to either `uint32_t` or `uint64_t` depending
//! on T. Used internally by AsmJit to dispatch arguments that can be of arbitrary
//! integer type into a function argument that is either `uint32_t` or `uint64_t`.
template<typename T>
static constexpr typename NormalizedInt<T, 0>::Type asUInt(T x) noexcept { return (typename NormalizedInt<T, 0>::Type)x; }

// ============================================================================
// [asmjit::IntUtils::Bit Cast]
// ============================================================================

template<typename SRC, typename DST>
union BitCastImpl {
  constexpr BitCastImpl(SRC src) noexcept : src(src) {}
  SRC src;
  DST dst;
};

//! Bit-cast `SRC` to `DST`.
//!
//! Useful to bitcast between integer and floating point.
template<typename DST, typename SRC>
static constexpr DST bit_cast(const SRC& x) noexcept { return BitCastImpl<DST, SRC>(x).dst; }

// ============================================================================
// [asmjit::IntUtils::Bit Manipulation]
// ============================================================================

//! Returns `0 - x` in a safe way (no undefined behavior), works for unsigned numbers as well.
template<typename T>
static constexpr T neg(const T& x) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return T(U(0) - U(x));
}

//! Returns `x << y` (shift left logical) by explicitly casting `x` to unsigned type and back.
template<typename X, typename Y>
static constexpr X shl(const X& x, const Y& y) noexcept {
  typedef typename std::make_unsigned<X>::type U;
  return X(U(x) << y);
}

//! Returns `x >> y` (shift right logical) by explicitly casting `x` to unsigned type and back.
template<typename X, typename Y>
static constexpr X shr(const X& x, const Y& y) noexcept {
  typedef typename std::make_unsigned<X>::type U;
  return X(U(x) >> y);
}

//! Returns `x | (x << y)` - helper used by some bit manipulation helpers.
template<typename X, typename Y>
static constexpr X or_shr(const X& x, const Y& y) noexcept { return X(x | shr(x, y)); }

//! Returns `x & -x` - extracts lowest set isolated bit (like BLSI instruction).
template<typename T>
static constexpr T blsi(T x) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return T(U(x) & neg(U(x)));
}

//! Returns `x & (x - 1)` - resets lowest set bit (like BLSR instruction).
template<typename T>
static constexpr T blsr(T x) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return T(U(x) & (U(x) - U(1)));
}

//! Get whether `x` has Nth bit set.
template<typename T, typename INDEX>
static constexpr bool bitTest(T x, INDEX n) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return (U(x) & (U(1) << n)) != 0;
}

//! Generate a trailing bit-mask that has `n` least significant (trailing) bits set.
template<typename T, typename COUNT>
static constexpr T lsbMask(COUNT n) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return (sizeof(U) < sizeof(uintptr_t))
    ? T(U((uintptr_t(1) << n) - uintptr_t(1)))
    // Shifting more bits than the type provides is in UNDEFINED BEHAVIOR.
    // In such case we trash the result by ORing it with a mask that has
    // all bits set and discards the UNDEFINED RESULT of the shift.
    : T(((U(1) << n) - U(1U)) | neg(U(n >= COUNT(sizeof(T) * 8))));
}

//! Get whether the `x` is a power of two (only one bit is set).
template<typename T>
static constexpr bool isPowerOf2(T x) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return x && !(U(x) & (U(x) - U(1)));
}

// Fill all trailing bits right from the first most significant bit set.
static constexpr uint8_t _fillTrailingBitsImpl(uint8_t x) noexcept { return or_shr(or_shr(or_shr(x, 1), 2), 4); }
// Fill all trailing bits right from the first most significant bit set.
static constexpr uint16_t _fillTrailingBitsImpl(uint16_t x) noexcept { return or_shr(or_shr(or_shr(or_shr(x, 1), 2), 4), 8); }
// Fill all trailing bits right from the first most significant bit set.
static constexpr uint32_t _fillTrailingBitsImpl(uint32_t x) noexcept { return or_shr(or_shr(or_shr(or_shr(or_shr(x, 1), 2), 4), 8), 16); }
// Fill all trailing bits right from the first most significant bit set.
static constexpr uint64_t _fillTrailingBitsImpl(uint64_t x) noexcept { return or_shr(or_shr(or_shr(or_shr(or_shr(or_shr(x, 1), 2), 4), 8), 16), 32); }

// Fill all trailing bits right from the first most significant bit set.
template<typename T>
static constexpr T fillTrailingBits(const T& x) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return T(_fillTrailingBitsImpl(U(x)));
}

// ============================================================================
// [asmjit::IntUtils::CTZ]
// ============================================================================

//! \internal
static ASMJIT_FORCEINLINE uint32_t _ctzGeneric(uint32_t x) noexcept {
  x &= neg(x);

  uint32_t c = 31;
  if (x & 0x0000FFFFU) c -= 16;
  if (x & 0x00FF00FFU) c -= 8;
  if (x & 0x0F0F0F0FU) c -= 4;
  if (x & 0x33333333U) c -= 2;
  if (x & 0x55555555U) c -= 1;

  return c;
}

//! \internal
static ASMJIT_FORCEINLINE uint32_t _ctzGeneric(uint64_t x) noexcept {
  x &= uint64_t(0) - x;

  uint32_t c = 63;
  if (x & ASMJIT_UINT64_C(0x00000000FFFFFFFF)) c -= 32;
  if (x & ASMJIT_UINT64_C(0x0000FFFF0000FFFF)) c -= 16;
  if (x & ASMJIT_UINT64_C(0x00FF00FF00FF00FF)) c -= 8;
  if (x & ASMJIT_UINT64_C(0x0F0F0F0F0F0F0F0F)) c -= 4;
  if (x & ASMJIT_UINT64_C(0x3333333333333333)) c -= 2;
  if (x & ASMJIT_UINT64_C(0x5555555555555555)) c -= 1;

  return c;
}

//! \internal
static ASMJIT_FORCEINLINE uint32_t _ctzImpl(uint32_t x) noexcept {
  #if ASMJIT_CXX_MSC && (ASMJIT_ARCH_X86 || ASMJIT_ARCH_ARM)
    unsigned long i;
    _BitScanForward(&i, x);
    return uint32_t(i);
  #elif ASMJIT_CXX_GNU
    return uint32_t(__builtin_ctz(x));
  #else
    return _ctzGeneric(x);
  #endif
}

//! \internal
static ASMJIT_FORCEINLINE uint32_t _ctzImpl(uint64_t x) noexcept {
  #if ASMJIT_CXX_MSC && (ASMJIT_ARCH_X86 == 64 || ASMJIT_ARCH_ARM == 64)
    unsigned long i;
    _BitScanForward64(&i, x);
    return uint32_t(i);
  #elif ASMJIT_CXX_GNU
    return uint32_t(__builtin_ctzll(x));
  #else
    return _ctzGeneric(x);
  #endif
}

//! Count trailing zeros in `x` (returns a position of a first bit set in `x`).
//!
//! NOTE: The input MUST NOT be zero, otherwise the result is undefined.
template<typename T>
static inline uint32_t ctz(T x) noexcept { return _ctzImpl(asUInt(x)); }

template<uint64_t N>
struct StaticCtz {
  enum {
    _kTmp1 = 0      + (((N            ) & uint64_t(0xFFFFFFFFU)) == 0 ? 32 : 0),
    _kTmp2 = _kTmp1 + (((N >> (_kTmp1)) & uint64_t(0x0000FFFFU)) == 0 ? 16 : 0),
    _kTmp3 = _kTmp2 + (((N >> (_kTmp2)) & uint64_t(0x000000FFU)) == 0 ?  8 : 0),
    _kTmp4 = _kTmp3 + (((N >> (_kTmp3)) & uint64_t(0x0000000FU)) == 0 ?  4 : 0),
    _kTmp5 = _kTmp4 + (((N >> (_kTmp4)) & uint64_t(0x00000003U)) == 0 ?  2 : 0),
    kValue = _kTmp5 + (((N >> (_kTmp5)) & uint64_t(0x00000001U)) == 0 ?  1 : 0)
  };
};

template<>
struct StaticCtz<0> {}; // Undefined.

template<uint64_t N>
static inline uint32_t staticCtz() noexcept { return StaticCtz<N>::kValue; }

// ============================================================================
// [asmjit::IntUtils::Popcnt]
// ============================================================================

// Based on the following resource:
//   http://graphics.stanford.edu/~seander/bithacks.html
//
// Alternatively, for a very small number of bits in `x`:
//   uint32_t n = 0;
//   while (x) {
//     x &= x - 1;
//     n++;
//   }
//   return n;

//! \internal
static inline uint32_t _popcntGeneric(uint32_t x) noexcept {
  x = x - ((x >> 1) & 0x55555555U);
  x = (x & 0x33333333U) + ((x >> 2) & 0x33333333U);
  return (((x + (x >> 4)) & 0x0F0F0F0FU) * 0x01010101U) >> 24;
}

//! \internal
static inline uint32_t _popcntGeneric(uint64_t x) noexcept {
  if (ASMJIT_ARCH_BITS == 64) {
    x = x - ((x >> 1) & ASMJIT_UINT64_C(0x5555555555555555));
    x = (x & ASMJIT_UINT64_C(0x3333333333333333)) + ((x >> 2) & ASMJIT_UINT64_C(0x3333333333333333));
    return uint32_t((((x + (x >> 4)) & ASMJIT_UINT64_C(0x0F0F0F0F0F0F0F0F)) * ASMJIT_UINT64_C(0x0101010101010101)) >> 56);
  }
  else {
    return _popcntGeneric(uint32_t(x >> 32)) +
           _popcntGeneric(uint32_t(x & 0xFFFFFFFFU));
  }
}

//! \internal
static inline uint32_t _popcntImpl(uint32_t x) noexcept {
  #if ASMJIT_CXX_GNU
    return uint32_t(__builtin_popcount(x));
  #else
    return _popcntGeneric(asUInt(x));
  #endif
}

//! \internal
static inline uint32_t _popcntImpl(uint64_t x) noexcept {
  #if ASMJIT_CXX_GNU
    return uint32_t(__builtin_popcountll(x));
  #else
    return _popcntGeneric(asUInt(x));
  #endif
}

//! Get count of bits in `x`.
template<typename T>
static inline uint32_t popcnt(T x) noexcept { return _popcntImpl(asUInt(x)); }

// ============================================================================
// [asmjit::IntUtils::Alignment]
// ============================================================================

template<typename X, typename Y>
static constexpr bool isAligned(X base, Y alignment) noexcept {
  typedef typename ParametrizedInt<sizeof(X), 0>::Type U;
  return ((U)base % (U)alignment) == 0;
}

template<typename X, typename Y>
static constexpr X alignUp(X x, Y alignment) noexcept {
  typedef typename ParametrizedInt<sizeof(X), 0>::Type U;
  return (X)( ((U)x + ((U)(alignment) - 1U)) & ~((U)(alignment) - 1U) );
}

//! Get zero or a positive difference between `base` and `base` aligned to `alignment`.
template<typename X, typename Y>
static constexpr X alignUpDiff(X base, Y alignment) noexcept {
  typedef typename ParametrizedInt<sizeof(X), 0>::Type U;
  return (X)(alignUp(U(base), alignment) - U(base));
}

template<typename T>
static constexpr T alignUpPowerOf2(T x) noexcept {
  typedef typename ParametrizedInt<sizeof(T), 0>::Type U;
  return (T)(fillTrailingBits(U(x) - 1U) + 1U);
}

// ============================================================================
// [asmjit::IntUtils::IsBetween]
// ============================================================================

//! Get whether `x` is greater than or equal to `a` and lesses than or equal to `b`.
template<typename T>
static constexpr bool isBetween(const T& x, const T& a, const T& b) noexcept {
  return x >= a && x <= b;
}

// ============================================================================
// [asmjit::IntUtils::IsInt / IsUInt]
// ============================================================================

//! Get whether the given integer `x` can be casted to a 4-bit signed integer.
template<typename T>
static constexpr bool isInt4(T x) noexcept {
  typedef typename std::make_signed<T>::type S;
  typedef typename std::make_unsigned<T>::type U;

  return std::is_signed<T>::value ? isBetween<S>(S(x), -8, 7)
                                  : U(x) <= U(7U);
}

//! Get whether the given integer `x` can be casted to an 8-bit signed integer.
template<typename T>
static constexpr bool isInt8(T x) noexcept {
  typedef typename std::make_signed<T>::type S;
  typedef typename std::make_unsigned<T>::type U;

  return std::is_signed<T>::value ? sizeof(T) <= 1 || isBetween<S>(S(x), -128, 127)
                                  : U(x) <= U(127U);
}

//! Get whether the given integer `x` can be casted to a 16-bit signed integer.
template<typename T>
static constexpr bool isInt16(T x) noexcept {
  typedef typename std::make_signed<T>::type S;
  typedef typename std::make_unsigned<T>::type U;

  return std::is_signed<T>::value ? sizeof(T) <= 2 || isBetween<S>(S(x), -32768, 32767)
                                  : sizeof(T) <= 1 || U(x) <= U(32767U);
}

//! Get whether the given integer `x` can be casted to a 32-bit signed integer.
template<typename T>
static constexpr bool isInt32(T x) noexcept {
  typedef typename std::make_signed<T>::type S;
  typedef typename std::make_unsigned<T>::type U;

  return std::is_signed<T>::value ? sizeof(T) <= 4 || isBetween<S>(S(x), -2147483647 - 1, 2147483647)
                                  : sizeof(T) <= 2 || U(x) <= U(2147483647U);
}

//! Get whether the given integer `x` can be casted to a 4-bit unsigned integer.
template<typename T>
static constexpr bool isUInt4(T x) noexcept {
  typedef typename std::make_unsigned<T>::type U;

  return std::is_signed<T>::value ? x >= T(0) && x <= T(15)
                                  : U(x) <= U(15U);
}

//! Get whether the given integer `x` can be casted to an 8-bit unsigned integer.
template<typename T>
static constexpr bool isUInt8(T x) noexcept {
  typedef typename std::make_unsigned<T>::type U;

  return std::is_signed<T>::value ? (sizeof(T) <= 1 || T(x) <= T(255)) && x >= T(0)
                                  : (sizeof(T) <= 1 || U(x) <= U(255U));
}

//! Get whether the given integer `x` can be casted to a 12-bit unsigned integer (ARM specific).
template<typename T>
static constexpr bool isUInt12(T x) noexcept {
  typedef typename std::make_unsigned<T>::type U;

  return std::is_signed<T>::value ? (sizeof(T) <= 1 || T(x) <= T(4095)) && x >= T(0)
                                  : (sizeof(T) <= 1 || U(x) <= U(4095U));
}

//! Get whether the given integer `x` can be casted to a 16-bit unsigned integer.
template<typename T>
static constexpr bool isUInt16(T x) noexcept {
  typedef typename std::make_unsigned<T>::type U;

  return std::is_signed<T>::value ? (sizeof(T) <= 2 || T(x) <= T(65535)) && x >= T(0)
                                  : (sizeof(T) <= 2 || U(x) <= U(65535U));
}

//! Get whether the given integer `x` can be casted to a 32-bit unsigned integer.
template<typename T>
static constexpr bool isUInt32(T x) noexcept {
  typedef typename std::make_unsigned<T>::type U;

  return std::is_signed<T>::value ? (sizeof(T) <= 4 || T(x) <= T(4294967295U)) && x >= T(0)
                                  : (sizeof(T) <= 4 || U(x) <= U(4294967295U));
}

// ============================================================================
// [asmjit::IntUtils::Mask]
// ============================================================================

//! Return a bit-mask that has `x` bit set.
template<typename T>
static constexpr uint32_t mask(T x) noexcept { return (1U << x); }

//! Return a bit-mask that has `x` bit set (multiple arguments).
template<typename T, typename... Args>
static constexpr uint32_t mask(T x, Args... args) noexcept { return mask(x) | mask(args...); }

// ============================================================================
// [asmjit::IntUtils::Bits]
// ============================================================================

//! Convert a boolean value `b` to zero or full mask (all bits set).
template<typename DST, typename SRC>
static constexpr DST maskFromBool(SRC b) noexcept {
  typedef typename std::make_unsigned<DST>::type U;
  return DST(U(0) - U(b));
}

// ============================================================================
// [asmjit::IntUtils::ByteSwap]
// ============================================================================

static inline uint32_t byteswap32(uint32_t x) noexcept {
  #if ASMJIT_CXX_MSC_ONLY
    return uint32_t(_byteswap_ulong(x));
  #elif ASMJIT_CXX_GNU
    return __builtin_bswap32(x);
  #else
    return (x << 24) | (x >> 24) | ((x << 8) & 0x00FF0000U) | ((x >> 8) & 0x0000FF00);
  #endif
}

// ============================================================================
// [asmjit::IntUtils::BytePack / Unpack]
// ============================================================================

//! Pack four 8-bit integer into a 32-bit integer as it is an array of `{b0,b1,b2,b3}`.
static constexpr uint32_t bytepack32_4x8(uint32_t a, uint32_t b, uint32_t c, uint32_t d) noexcept {
  return ASMJIT_ARCH_LE ? (a | (b << 8) | (c << 16) | (d << 24))
                        : (d | (c << 8) | (b << 16) | (a << 24)) ;
}

template<typename T>
static constexpr uint32_t unpackU32At0(T x) noexcept { return ASMJIT_ARCH_LE ? uint32_t(uint64_t(x) & 0xFFFFFFFFU) : uint32_t(uint64_t(x) >> 32); }
template<typename T>
static constexpr uint32_t unpackU32At1(T x) noexcept { return ASMJIT_ARCH_BE ? uint32_t(uint64_t(x) & 0xFFFFFFFFU) : uint32_t(uint64_t(x) >> 32); }

// ============================================================================
// [asmjit::IntUtils::Position of byte (in bit-shift)]
// ============================================================================

static inline uint32_t byteShiftOfDWordStruct(uint32_t index) noexcept {
  return ASMJIT_ARCH_LE ? index * 8 : (uint32_t(sizeof(uint32_t)) - 1U - index) * 8;
}

// ============================================================================
// [asmjit::IntUtils::Operators]
// ============================================================================

struct And    { template<typename T> static inline T op(T x, T y) noexcept { return  x &  y; } };
struct AndNot { template<typename T> static inline T op(T x, T y) noexcept { return  x & ~y; } };
struct NotAnd { template<typename T> static inline T op(T x, T y) noexcept { return ~x &  y; } };
struct Or     { template<typename T> static inline T op(T x, T y) noexcept { return  x |  y; } };
struct Xor    { template<typename T> static inline T op(T x, T y) noexcept { return  x ^  y; } };
struct Add    { template<typename T> static inline T op(T x, T y) noexcept { return  x +  y; } };
struct Sub    { template<typename T> static inline T op(T x, T y) noexcept { return  x -  y; } };
struct Min    { template<typename T> static inline T op(T x, T y) noexcept { return std::min<T>(x, y); } };
struct Max    { template<typename T> static inline T op(T x, T y) noexcept { return std::max<T>(x, y); } };

// ============================================================================
// [asmjit::IntUtils::Iterators]
// ============================================================================

template<typename BitWordT>
inline uint32_t _ctzPlusOneAndShift(BitWordT& bitWord) {
  uint32_t x = ctz(bitWord);

  if (sizeof(BitWordT) < sizeof(Globals::BitWord)) {
    // One instruction less on most architectures, no undefined behavior.
    bitWord = BitWordT(Globals::BitWord(bitWord) >> ++x);
  }
  else {
    bitWord >>= x++;
    bitWord >>= 1U;
  }

  return x;
}

//! Iterates over each bit in a number which is set to 1.
//!
//! Example of use:
//!
//! ```
//! uint32_t bitsToIterate = 0x110F;
//! IntUtils::BitWordIterator<uint32_t> it(bitsToIterate);
//!
//! while (it.hasNext()) {
//!   uint32_t bitIndex = it.next();
//!   std::printf("Bit at %u is set\n", unsigned(bitIndex));
//! }
//! ```
template<typename BitWordT>
class BitWordIterator {
public:
  explicit constexpr BitWordIterator(BitWordT bitWord) noexcept
    : _bitWord(bitWord),
      _index(~uint32_t(0)) {}

  inline void init(BitWordT bitWord) noexcept {
    _bitWord = bitWord;
    _index = ~uint32_t(0);
  }

  inline bool hasNext() const noexcept { return _bitWord != 0; }
  inline uint32_t next() noexcept {
    ASMJIT_ASSERT(_bitWord != 0);
    _index += _ctzPlusOneAndShift(_bitWord);
    return _index;
  }

  BitWordT _bitWord;
  uint32_t _index;
};

template<typename BitWordT>
class BitArrayIterator {
public:
  static constexpr uint32_t kBitWordSizeInBits = uint32_t(sizeof(BitWordT)) * 8;

  ASMJIT_FORCEINLINE BitArrayIterator(const BitWordT* data, uint32_t count) noexcept {
    init(data, count);
  }

  ASMJIT_FORCEINLINE void init(const BitWordT* data, uint32_t count) noexcept {
    const BitWordT* ptr = data;
    const BitWordT* end = data + count;

    BitWordT bitWord = BitWordT(0);
    uint32_t bitIndex = ~uint32_t(0);

    while (ptr != end) {
      bitWord = *ptr++;
      if (bitWord)
        break;
      bitIndex += kBitWordSizeInBits;
    }

    _ptr = ptr;
    _end = end;
    _current = bitWord;
    _bitIndex = bitIndex;
  }

  ASMJIT_FORCEINLINE bool hasNext() noexcept {
    return _current != BitWordT(0);
  }

  ASMJIT_FORCEINLINE uint32_t next() noexcept {
    BitWordT bitWord = _current;
    uint32_t bitIndex = _bitIndex;
    ASMJIT_ASSERT(bitWord != BitWordT(0));

    bitIndex += _ctzPlusOneAndShift(bitWord);
    uint32_t retIndex = bitIndex;

    if (!bitWord) {
      bitIndex |= uint32_t(kBitWordSizeInBits - 1);
      while (_ptr != _end) {
        bitWord = *_ptr++;
        if (bitWord)
          break;
        bitIndex += kBitWordSizeInBits;
      }
    }

    _current = bitWord;
    _bitIndex = bitIndex;
    return retIndex;
  }

  const BitWordT* _ptr;
  const BitWordT* _end;
  BitWordT _current;
  uint32_t _bitIndex;
};

template<typename BitWordT, class Operator>
class BitArrayOpIterator {
public:
  static constexpr uint32_t kBitWordSizeInBits = uint32_t(sizeof(BitWordT)) * 8;

  ASMJIT_FORCEINLINE BitArrayOpIterator(const BitWordT* aData, const BitWordT* bData, uint32_t count) noexcept {
    init(aData, bData, count);
  }

  ASMJIT_FORCEINLINE void init(const BitWordT* aData, const BitWordT* bData, uint32_t count) noexcept {
    const BitWordT* aPtr = aData;
    const BitWordT* bPtr = bData;
    const BitWordT* aEnd = aData + count;

    BitWordT bitWord = BitWordT(0);
    uint32_t bitIndex = ~uint32_t(0);

    while (aPtr != aEnd) {
      bitWord = Operator::op(*aPtr++, *bPtr++);
      if (bitWord)
        break;
      bitIndex += kBitWordSizeInBits;
    }

    _aPtr = aPtr;
    _bPtr = bPtr;
    _aEnd = aEnd;
    _current = bitWord;
    _bitIndex = bitIndex;
  }

  ASMJIT_FORCEINLINE bool hasNext() noexcept {
    return _current != BitWordT(0);
  }

  ASMJIT_FORCEINLINE uint32_t next() noexcept {
    BitWordT bitWord = _current;
    uint32_t bitIndex = _bitIndex;
    ASMJIT_ASSERT(bitWord != BitWordT(0));

    bitIndex += _ctzPlusOneAndShift(bitWord);
    uint32_t retIndex = bitIndex;

    if (!bitWord) {
      bitIndex |= uint32_t(kBitWordSizeInBits - 1);
      while (_aPtr != _aEnd) {
        bitWord = Operator::op(*_aPtr++, *_bPtr++);
        if (bitWord)
          break;
        bitIndex += kBitWordSizeInBits;
      }
    }

    _current = bitWord;
    _bitIndex = bitIndex;
    return retIndex;
  }

  const BitWordT* _aPtr;
  const BitWordT* _bPtr;
  const BitWordT* _aEnd;
  BitWordT _current;
  uint32_t _bitIndex;
};

} // IntUtils namespace

//! \}

ASMJIT_END_NAMESPACE

// [Guard]
#endif // _ASMJIT_CORE_INTUTILS_H
