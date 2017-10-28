// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_CORE_RADEFS_P_H
#define _ASMJIT_CORE_RADEFS_P_H

#include "../core/build.h"
#ifndef ASMJIT_DISABLE_COMPILER

// [Dependencies]
#include "../core/codecompiler.h"
#include "../core/intutils.h"
#include "../core/logging.h"
#include "../core/zone.h"

ASMJIT_BEGIN_NAMESPACE

//! \addtogroup asmjit_ra
//! \{

// ============================================================================
// [Logging]
// ============================================================================

#ifndef ASMJIT_DISABLE_LOGGING
# define ASMJIT_RA_LOG_INIT(...) __VA_ARGS__
# define ASMJIT_RA_LOG_FORMAT(...)  \
  do {                              \
    if (logger)                     \
      logger->logf(__VA_ARGS__);    \
  } while (0)
# define ASMJIT_RA_LOG_COMPLEX(...) \
  do {                              \
    if (logger) {                   \
      __VA_ARGS__                   \
    }                               \
  } while (0)
#else
# define ASMJIT_RA_LOG_INIT(...) ((void)0)
# define ASMJIT_RA_LOG_FORMAT(...) ((void)0)
# define ASMJIT_RA_LOG_COMPLEX(...) ((void)0)
#endif

// ============================================================================
// [Forward Declarations]
// ============================================================================

class RAPass;
class RABlock;
struct RAStackSlot;

typedef ZoneVector<RABlock*> RABlocks;
typedef ZoneVector<RAWorkReg*> RAWorkRegs;

// ============================================================================
// [asmjit::RAStrategy]
// ============================================================================

struct RAStrategy {
  enum StrategyType : uint32_t {
    kStrategySimple  = 0,
    kStrategyComplex = 1
  };

  inline RAStrategy() noexcept { reset(); }
  inline void reset() noexcept { std::memset(this, 0, sizeof(*this)); }

  inline uint32_t getType() const noexcept { return _type; }
  inline void setType(uint32_t type) noexcept { _type = uint8_t(type); }

  inline bool isSimple() const noexcept { return _type == kStrategySimple; }
  inline bool isComplex() const noexcept { return _type >= kStrategyComplex; }

  uint8_t _type;
};

// ============================================================================
// [asmjit::RAArchTraits]
// ============================================================================

//! Traits.
struct RAArchTraits {
  enum Flags : uint32_t {
    //! Registers can be swapped by a single instruction.
    kHasSwap = 0x01U
  };

  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  inline RAArchTraits() noexcept { reset(); }
  inline void reset() noexcept { std::memset(_flags, 0, sizeof(_flags)); }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline bool hasFlag(uint32_t group, uint32_t flag) const noexcept { return (_flags[group] & flag) != 0; }
  inline bool hasSwap(uint32_t group) const noexcept { return hasFlag(group, kHasSwap); }

  inline uint8_t& operator[](uint32_t group) noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    return _flags[group];
  }

  inline const uint8_t& operator[](uint32_t group) const noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    return _flags[group];
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint8_t _flags[Reg::kGroupVirt];
};

// ============================================================================
// [asmjit::RARegCount]
// ============================================================================

//! Count of virtual or physical registers per group.
//!
//! NOTE: This class uses 8-bit integers to represent counters, it's only used
//! in places where this is sufficient - for example total count of machine's
//! physical registers, count of virtual registers per instruction, etc. There
//! is also `RALiveCount`, which uses 32-bit integers and is indeed much safer.
struct RARegCount {
  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  //! Reset all counters to zero.
  inline void reset() noexcept { _packed = 0; }

  // --------------------------------------------------------------------------
  // [Operators]
  // --------------------------------------------------------------------------

  //! Get register count by a register `group`.
  inline uint32_t get(uint32_t group) const noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);

    uint32_t shift = IntUtils::byteShiftOfDWordStruct(group);
    return (_packed >> shift) & uint32_t(0xFF);
  }

  //! Set register count by a register `group`.
  inline void set(uint32_t group, uint32_t n) noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    ASMJIT_ASSERT(n <= 0xFF);

    uint32_t shift = IntUtils::byteShiftOfDWordStruct(group);
    _packed = (_packed & ~uint32_t(0xFF << shift)) + (n << shift);
  }

  //! Add register count by a register `group`.
  inline void add(uint32_t group, uint32_t n = 1) noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    ASMJIT_ASSERT(0xFF - uint32_t(_regs[group]) >= n);

    uint32_t shift = IntUtils::byteShiftOfDWordStruct(group);
    _packed += n << shift;
  }

  // --------------------------------------------------------------------------
  // [Operator OVerload]
  // --------------------------------------------------------------------------

  inline uint8_t& operator[](uint32_t index) noexcept {
    ASMJIT_ASSERT(index < Reg::kGroupVirt);
    return _regs[index];
  }

  inline const uint8_t& operator[](uint32_t index) const noexcept {
    ASMJIT_ASSERT(index < Reg::kGroupVirt);
    return _regs[index];
  }

  inline RARegCount& operator=(const RARegCount& other) noexcept {
    _packed = other._packed;
    return *this;
  }

  inline bool operator==(const RARegCount& other) const noexcept { return _packed == other._packed; }
  inline bool operator!=(const RARegCount& other) const noexcept { return _packed != other._packed; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  union {
    uint8_t _regs[4];
    uint32_t _packed;
  };
};

struct RARegIndex : public RARegCount {
  //! Build register indexes based on the given `count` of registers.
  inline void buildIndexes(const RARegCount& count) noexcept {
    uint32_t x = uint32_t(count._regs[0]);
    uint32_t y = uint32_t(count._regs[1]) + x;
    uint32_t z = uint32_t(count._regs[2]) + y;

    ASMJIT_ASSERT(y <= 0xFF);
    ASMJIT_ASSERT(z <= 0xFF);
    _packed = IntUtils::bytepack32_4x8(0, x, y, z);
  }
};

// ============================================================================
// [asmjit::RARegMask]
// ============================================================================

//! Registers mask.
struct RARegMask {
  // --------------------------------------------------------------------------
  // [Consturction / Destruction]
  // --------------------------------------------------------------------------

  inline void init(const RARegMask& other) noexcept {
    for (uint32_t i = 0; i < Reg::kGroupVirt; i++)
      _masks[i] = other._masks[i];
  }

  //! Reset all register masks to zero.
  inline void reset() noexcept {
    for (uint32_t i = 0; i < Reg::kGroupVirt; i++)
      _masks[i] = 0;
  }

  // --------------------------------------------------------------------------
  // [IsEmpty / Has]
  // --------------------------------------------------------------------------

  //! Get whether all register masks are zero (empty).
  inline bool isEmpty() const noexcept {
    uint32_t m = 0;
    for (uint32_t i = 0; i < Reg::kGroupVirt; i++)
      m |= _masks[i];
    return m == 0;
  }

  inline bool has(uint32_t group, uint32_t mask = 0xFFFFFFFFU) const noexcept {
    ASMJIT_ASSERT(group < Reg::kGroupVirt);
    return (_masks[group] & mask) != 0;
  }

  // --------------------------------------------------------------------------
  // [Operators]
  // --------------------------------------------------------------------------

  template<class Operator>
  inline void op(const RARegMask& other) noexcept {
    for (uint32_t i = 0; i < Reg::kGroupVirt; i++)
      _masks[i] = Operator::op(_masks[i], other._masks[i]);
  }

  template<class Operator>
  inline void op(uint32_t group, uint32_t input) noexcept {
    _masks[group] = Operator::op(_masks[group], input);
  }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  inline uint32_t& operator[](uint32_t index) noexcept {
    ASMJIT_ASSERT(index < Reg::kGroupVirt);
    return _masks[index];
  }

  inline const uint32_t& operator[](uint32_t index) const noexcept {
    ASMJIT_ASSERT(index < Reg::kGroupVirt);
    return _masks[index];
  }

  inline RARegMask& operator=(const RARegMask& other) noexcept {
    init(other);
    return *this;
  }

  inline bool operator==(const RARegMask& other) const noexcept {
    return _masks[0] == other._masks[0] &&
           _masks[1] == other._masks[1] &&
           _masks[2] == other._masks[2] &&
           _masks[3] == other._masks[3] ;
  }

  inline bool operator!=(const RARegMask& other) const noexcept {
    return !operator==(other);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _masks[Reg::kGroupVirt];
};

// ============================================================================
// [asmjit::RARegsStats]
// ============================================================================

//! Information associated with each instruction, propagated to blocks, loops,
//! and the whole function. This information can be used to do minor decisions
//! before the register allocator tries to do its job. For example to use fast
//! register allocation inside a block or loop it cannot have clobbered and/or
//! fixed registers, etc...
struct RARegsStats {
  enum Index : uint32_t {
    kIndexUsed       = 0,
    kIndexFixed      = 8,
    kIndexClobbered  = 16
  };

  enum Mask : uint32_t {
    kMaskUsed        = 0xFFU << kIndexUsed,
    kMaskFixed       = 0xFFU << kIndexFixed,
    kMaskClobbered   = 0xFFU << kIndexClobbered
  };

  inline void reset() noexcept { _packed = 0; }
  inline void combineWith(const RARegsStats& other) noexcept { _packed |= other._packed; }

  inline bool hasUsed() const noexcept { return (_packed & kMaskUsed) != 0U; }
  inline bool hasUsed(uint32_t group) const noexcept { return (_packed & IntUtils::mask(kIndexUsed + group)) != 0U; }
  inline void makeUsed(uint32_t group) noexcept { _packed |= IntUtils::mask(kIndexUsed + group); }

  inline bool hasFixed() const noexcept { return (_packed & kMaskFixed) != 0U; }
  inline bool hasFixed(uint32_t group) const noexcept { return (_packed & IntUtils::mask(kIndexFixed + group)) != 0U; }
  inline void makeFixed(uint32_t group) noexcept { _packed |= IntUtils::mask(kIndexFixed + group); }

  inline bool hasClobbered() const noexcept { return (_packed & kMaskClobbered) != 0U; }
  inline bool hasClobbered(uint32_t group) const noexcept { return (_packed & IntUtils::mask(kIndexClobbered + group)) != 0U; }
  inline void makeClobbered(uint32_t group) noexcept { _packed |= IntUtils::mask(kIndexClobbered + group); }

  uint32_t _packed;
};

// ============================================================================
// [asmjit::RALiveCount]
// ============================================================================

//! Count of live register, per group.
class RALiveCount {
public:
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline RALiveCount() noexcept { reset(); }
  inline RALiveCount(const RALiveCount& other) noexcept { init(other); }

  inline void init(const RALiveCount& other) noexcept {
    for (uint32_t group = 0; group < Reg::kGroupVirt; group++)
      n[group] = other.n[group];
  }

  inline void reset() noexcept {
    for (uint32_t group = 0; group < Reg::kGroupVirt; group++)
      n[group] = 0;
  }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  template<class Operator>
  inline void op(const RALiveCount& other) noexcept {
    for (uint32_t group = 0; group < Reg::kGroupVirt; group++)
      n[group] = Operator::op(n[group], other.n[group]);
  }

  inline RALiveCount& operator=(const RALiveCount& other) noexcept { init(other); return *this; }
  inline uint32_t& operator[](uint32_t group) noexcept { return n[group]; }
  inline const uint32_t& operator[](uint32_t group) const noexcept { return n[group]; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t n[Reg::kGroupVirt];
};

// ============================================================================
// [asmjit::LiveInterval]
// ============================================================================

struct LiveInterval {
  enum Misc : uint32_t {
    kNaN = 0,
    kInf = 0xFFFFFFFFU
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline LiveInterval() noexcept : a(0), b(0) {}
  inline LiveInterval(uint32_t a, uint32_t b) noexcept : a(a), b(b) {}
  inline LiveInterval(const LiveInterval& other) noexcept : a(other.a), b(other.b) {}

  inline void init(uint32_t aVal, uint32_t bVal) noexcept {
    a = aVal;
    b = bVal;
  }
  inline void init(const LiveInterval& other) noexcept { init(other.a, other.b); }
  inline void reset() noexcept { init(0, 0); }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline bool isValid() const noexcept { return a < b; }
  inline uint32_t getWidth() const noexcept { return b - a; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t a, b;
};

// ============================================================================
// [asmjit::RALiveSpan<T>]
// ============================================================================

template<typename T>
class RALiveSpan : public LiveInterval, public T {
public:
  typedef T DataType;

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline RALiveSpan() noexcept : LiveInterval(), T() {}
  inline RALiveSpan(const RALiveSpan<T>& other) noexcept : LiveInterval(other), T() {}
  inline RALiveSpan(const LiveInterval& interval, const T& data) noexcept : LiveInterval(interval), T(data) {}
  inline RALiveSpan(uint32_t a, uint32_t b) noexcept : LiveInterval(a, b), T() {}
  inline RALiveSpan(uint32_t a, uint32_t b, const T& data) noexcept : LiveInterval(a, b), T(data) {}

  inline void init(const RALiveSpan<T>& other) noexcept {
    LiveInterval::init(static_cast<const LiveInterval&>(other));
    T::init(static_cast<const T&>(other));
  }

  inline void init(const RALiveSpan<T>& span, const T& data) noexcept {
    LiveInterval::init(static_cast<const LiveInterval&>(span));
    T::init(data);
  }

  inline void init(const LiveInterval& interval, const T& data) noexcept {
    LiveInterval::init(interval);
    T::init(data);
  }
};

// ============================================================================
// [asmjit::RALiveSpans<T>]
// ============================================================================

template<typename T>
class RALiveSpans {
public:
  ASMJIT_NONCOPYABLE(RALiveSpans<T>)

  typedef typename T::DataType DataType;

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline RALiveSpans() noexcept : _data() {}

  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  inline void reset() noexcept { _data.reset(); }
  inline void release(ZoneAllocator* allocator) noexcept { _data.release(allocator); }

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  inline bool isEmpty() const noexcept { return _data.isEmpty(); }
  inline uint32_t getLength() const noexcept { return _data.getLength(); }

  inline T* getData() noexcept { return _data.getData(); }
  inline const T* getData() const noexcept { return _data.getData(); }

  inline void swap(RALiveSpans<T>& other) noexcept { _data.swap(other._data); }

  inline bool isOpen() const noexcept {
    uint32_t len = _data.getLength();
    return len > 0 && _data[len - 1].b == LiveInterval::kInf;
  }

  //! Open the current live span.
  ASMJIT_FORCEINLINE Error openAt(ZoneAllocator* allocator, uint32_t start, uint32_t end) noexcept {
    bool wasOpen;
    return openAt(allocator, start, end, wasOpen);
  }

  ASMJIT_FORCEINLINE Error openAt(ZoneAllocator* allocator, uint32_t start, uint32_t end, bool& wasOpen) noexcept {
    uint32_t len = _data.getLength();
    wasOpen = false;

    if (len > 0) {
      T& last = _data[len - 1];
      if (last.b >= start) {
        wasOpen = last.b > start;
        last.b = end;
        return kErrorOk;
      }
    }

    return _data.append(allocator, T(start, end));
  }

  inline void closeAt(uint32_t end) noexcept {
    ASMJIT_ASSERT(!isEmpty());

    uint32_t len = _data.getLength();
    _data[len - 1].b = end;
  }

  //! Returns the sum of width of all spans.
  //!
  //! NOTE: Don't overuse, this iterates over all spans so it's O(N).
  //! It should be only called once and then cached.
  ASMJIT_FORCEINLINE uint32_t calcWidth() const noexcept {
    uint32_t width = 0;
    const T* spans = _data.getData();
    for (uint32_t i = 0, len = _data.getLength(); i < len; i++)
      width += spans[i].getWidth();
    return width;
  }

  inline T& operator[](uint32_t index) noexcept { return _data[index]; }
  inline const T& operator[](uint32_t index) const noexcept { return _data[index]; }

  inline bool intersects(const RALiveSpans<T>& other) const noexcept {
    return intersects(*this, other);
  }

  ASMJIT_FORCEINLINE Error nonOverlappingUnionOf(ZoneAllocator* allocator, const RALiveSpans<T>& x, const RALiveSpans<T>& y, const DataType& yData) noexcept {
    uint32_t finalLength = x.getLength() + y.getLength();
    ASMJIT_PROPAGATE(_data.reserve(allocator, finalLength));

    T* dstPtr = _data.getData();

    const T* xSpan = x.getData();
    const T* ySpan = y.getData();

    const T* xEnd = xSpan + x.getLength();
    const T* yEnd = ySpan + y.getLength();

    // Loop until we have intersection or either `xSpan == xEnd` or `ySpan == yEnd`,
    // which means that there is no intersection. We advance either `xSpan` or `ySpan`
    // depending on their ranges.
    if (xSpan != xEnd && ySpan != yEnd) {
      uint32_t xa, ya;
      xa = xSpan->a;
      for (;;) {
        while (ySpan->b <= xa) {
          dstPtr->init(*ySpan, yData);
          dstPtr++;
          if (++ySpan == yEnd)
            goto Done;
        }

        ya = ySpan->a;
        while (xSpan->b <= ya) {
          *dstPtr++ = *xSpan;
          if (++xSpan == xEnd)
            goto Done;
        }

        // We know that `xSpan->b > ySpan->a`, so check if `ySpan->b > xSpan->a`.
        xa = xSpan->a;
        if (ySpan->b > xa)
          return 0xFFFFFFFFU;
      }
    }

  Done:
    while (xSpan != xEnd) {
      *dstPtr++ = *xSpan++;
    }

    while (ySpan != yEnd) {
      dstPtr->init(*ySpan, yData);
      dstPtr++;
      ySpan++;
    }

    _data._setEndPtr(dstPtr);
    return kErrorOk;
  }

  static ASMJIT_FORCEINLINE bool intersects(const RALiveSpans<T>& x, const RALiveSpans<T>& y) noexcept {
    const T* xSpan = x.getData();
    const T* ySpan = y.getData();

    const T* xEnd = xSpan + x.getLength();
    const T* yEnd = ySpan + y.getLength();

    // Loop until we have intersection or either `xSpan == xEnd` or `ySpan == yEnd`,
    // which means that there is no intersection. We advance either `xSpan` or `ySpan`
    // depending on their end positions.
    if (xSpan == xEnd || ySpan == yEnd)
      return false;

    uint32_t xa, ya;
    xa = xSpan->a;

    for (;;) {
      while (ySpan->b <= xa)
        if (++ySpan == yEnd)
          return false;

      ya = ySpan->a;
      while (xSpan->b <= ya)
        if (++xSpan == xEnd)
          return false;

      // We know that `xSpan->b > ySpan->a`, so check if `ySpan->b > xSpan->a`.
      xa = xSpan->a;
      if (ySpan->b > xa)
        return true;
    }
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  ZoneVector<T> _data;
};

// ============================================================================
// [asmjit::RALiveStats]
// ============================================================================

//! Statistics about a register liveness.
class RALiveStats {
public:
  inline RALiveStats()
    : _width(0),
      _freq(0.0f) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline uint32_t getWidth() const noexcept { return _width; }
  inline float getFreq() const noexcept { return _freq; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _width;
  float _freq;
};

// ============================================================================
// [asmjit::LiveRegData]
// ============================================================================

struct LiveRegData {
  inline LiveRegData() noexcept : id(Reg::kIdBad) {}
  explicit inline LiveRegData(uint32_t id) noexcept : id(id) {}
  explicit inline LiveRegData(const LiveRegData& other) noexcept : id(other.id) {}

  inline void init(const LiveRegData& other) noexcept { id = other.id; }

  inline bool operator==(const LiveRegData& other) const noexcept { return id == other.id; }
  inline bool operator!=(const LiveRegData& other) const noexcept { return id != other.id; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t id;
};

typedef RALiveSpan<LiveRegData> LiveRegSpan;
typedef RALiveSpans<LiveRegSpan> LiveRegSpans;

// ============================================================================
// [asmjit::RATiedReg]
// ============================================================================

//! Tied register merges one ore more register operand into a single entity. It
//! contains information about its access (Read|Write) and allocation slots
//! (Use|Out) that are used by the register allocator and liveness analysis.
struct RATiedReg {
  //! Flags.
  //!
  //! Register access information is encoded in 4 flags in total:
  //!
  //!   - `kRead`  - Register is Read    (ReadWrite if combined with `kWrite`).
  //!   - `kWrite` - Register is Written (ReadWrite if combined with `kRead`).
  //!   - `kUse`   - Encoded as Read or ReadWrite.
  //!   - `kOut`   - Encoded as WriteOnly.
  //!
  //! Let's describe all of these on two X86 instructions:
  //!
  //!   - ADD x{R|W|Use},  x{R|Use}              -> {x:R|W|Use            }
  //!   - LEA x{  W|Out}, [x{R|Use} + x{R|Out}]  -> {x:R|W|Use|Out        }
  //!   - ADD x{R|W|Use},  y{R|Use}              -> {x:R|W|Use     y:R|Use}
  //!   - LEA x{  W|Out}, [x{R|Use} + y{R|Out}]  -> {x:R|W|Use|Out y:R|Use}
  //!
  //! It should be obvious from the example above how these flags get created.
  //! Each operand contains READ/WRITE information, which is then merged to
  //! RATiedReg's flags. However, we also need to represent the possitility to
  //! use see the operation as two independent operations - USE and OUT, because
  //! the register allocator will first allocate USE registers, and then assign
  //! OUT registers independently of USE registers.
  enum Flags : uint32_t {
    kRead        = OpInfo::kRead,        //!< Register is read.
    kWrite       = OpInfo::kWrite,       //!< Register is written.
    kRW          = OpInfo::kRW,          //!< Register read and written.
    kUse         = OpInfo::kUse,         //!< Register has a USE slot (Read/ReadWrite).
    kOut         = OpInfo::kOut,         //!< Register has an OUT slot (WriteOnly).

    kUseFixed    = 0x00000010U,          //!< Register has a fixed USE slot.
    kOutFixed    = 0x00000020U,          //!< Register has a fixed OUT slot.

    // TODO: Maybe we don't need these at all.
    kUseCall     = 0x00000040U,          //!< Function-call register argument (USE).
    kOutCall     = 0x00000080U,          //!< Function-call register return (OUT).

    kUseDone     = 0x00000100U,          //!< Register USE slot has been allocated.
    kOutDone     = 0x00000200U,          //!< Register OUT slot has been allocated

    kLast        = 0x00000400U,          //!< Last occurrence of this VirtReg in basic block.
    kKill        = 0x00000800U,          //!< Kill this VirtReg after use.

    // Architecture specific flags are used during RATiedReg building to ensure
    // that architecture-specific constraints are handled properly. These flags
    // are not really needed after RATiedReg[] is built and copied to `RAInst`.

    kX86Gpb      = 0x00001000U           //!< This tied references GPB-LO or GPB-HI.
  };

  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  ASMJIT_FORCEINLINE void init(uint32_t workId, uint32_t flags, uint32_t allocableRegs, uint32_t useId, uint32_t useRewriteMask, uint32_t outId, uint32_t outRewriteMask) noexcept {
    _workId = workId;
    _flags = flags;
    _allocableRegs = allocableRegs;
    _useRewriteMask = useRewriteMask;
    _outRewriteMask = outRewriteMask;
    _refCount = 1;
    _useId = uint8_t(useId);
    _outId = uint8_t(outId);
    _reserved = 0;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the associated WorkReg id.
  inline uint32_t getWorkId() const noexcept { return _workId; }

  //! Check if the given `flag` is set, see \ref Flags.
  inline bool hasFlag(uint32_t flag) const noexcept { return (_flags & flag) != 0; }

  //! Get tied register flags, see \ref Flags.
  inline uint32_t getFlags() const noexcept { return _flags; }
  //! Add tied register flags, see \ref Flags.
  inline void addFlags(uint32_t flags) noexcept { _flags |= flags; }

  //! Get whether the register is read (writes `true` also if it's Read/Write).
  inline bool isRead() const noexcept { return hasFlag(kRead); }
  //! Get whether the register is written (writes `true` also if it's Read/Write).
  inline bool isWrite() const noexcept { return hasFlag(kWrite); }
  //! Get whether the register is read only.
  inline bool isReadOnly() const noexcept { return (_flags & kRW) == kRead; }
  //! Get whether the register is write only.
  inline bool isWriteOnly() const noexcept { return (_flags & kRW) == kWrite; }
  //! Get whether the register is read and written.
  inline bool isReadWrite() const noexcept { return (_flags & kRW) == kRW; }

  //! Get whether the tied register has use operand (Read/ReadWrite).
  inline bool isUse() const noexcept { return hasFlag(kUse); }
  //! Get whether the tied register has out operand (Write).
  inline bool isOut() const noexcept { return hasFlag(kOut); }

  inline void makeReadOnly() noexcept {
    _flags = (_flags & ~(kOut | kWrite)) | kUse;
    _useRewriteMask |= _outRewriteMask;
    _outRewriteMask = 0;
  }

  inline void makeWriteOnly() noexcept {
    _flags = (_flags & ~(kUse | kRead)) | kOut;
    _outRewriteMask |= _useRewriteMask;
    _useRewriteMask = 0;
  }

  //! Get whether this register (and the instruction it's part of) appears last in the basic block.
  inline bool isLast() const noexcept { return hasFlag(kLast); }
  //! Get whether this register should be killed after USEd and/or OUTed.
  inline bool isKill() const noexcept { return hasFlag(kKill); }

  //! Get whether this register is OUT or KILL (used internally by local register allocator).
  inline bool isOutOrKill() const noexcept { return hasFlag(kOut | kKill); }

  inline uint32_t getAllocableRegs() const noexcept { return _allocableRegs; }

  inline uint32_t getRefCount() const noexcept { return _refCount; }
  inline void addRefCount(uint32_t n = 1) noexcept { _refCount = uint8_t(_refCount + n); }

  //! Get whether the register must be allocated to a fixed physical register before it's used.
  inline bool hasUseId() const noexcept { return _useId != Reg::kIdBad; }
  //! Get whether the register must be allocated to a fixed physical register before it's written.
  inline bool hasOutId() const noexcept { return _outId != Reg::kIdBad; }

  //! Get a physical register used for 'use' operation.
  inline uint32_t getUseId() const noexcept { return _useId; }
  //! Get a physical register used for 'out' operation.
  inline uint32_t getOutId() const noexcept { return _outId; }

  inline uint32_t getUseRewriteMask() const noexcept { return _useRewriteMask; }
  inline uint32_t getOutRewriteMask() const noexcept { return _outRewriteMask; }

  //! Set a physical register used for 'use' operation.
  inline void setUseId(uint32_t index) noexcept { _useId = uint8_t(index); }
  //! Set a physical register used for 'out' operation.
  inline void setOutId(uint32_t index) noexcept { _outId = uint8_t(index); }

  inline bool isUseDone() const noexcept { return hasFlag(kUseDone); }
  inline bool isOutDone() const noexcept { return hasFlag(kUseDone); }

  inline void markUseDone() noexcept { addFlags(kUseDone); }
  inline void markOutDone() noexcept { addFlags(kUseDone); }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  inline RATiedReg& operator=(const RATiedReg& other) noexcept {
    std::memcpy(this, &other, sizeof(RATiedReg));
    return *this;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _workId;                      //!< WorkReg id.
  uint32_t _flags;                       //!< Allocation flags.
  uint32_t _allocableRegs;               //!< Registers where input {R|X} can be allocated to.
  uint32_t _useRewriteMask;              //!< Indexes used to rewrite USE regs.
  uint32_t _outRewriteMask;              //!< Indexes used to rewrite OUT regs.

  union {
    struct {
      uint8_t _refCount;                 //!< How many times the VirtReg is referenced in all operands.
      uint8_t _useId;                    //!< Physical register for use operation (ReadOnly / ReadWrite).
      uint8_t _outId;                    //!< Physical register for out operation (WriteOnly).
      uint8_t _reserved;                 //!< Index of OUT operand or 0xFF if none.
    };
    uint32_t _packed;                    //!< Packed data.
  };
};

// ============================================================================
// [asmjit::RAWorkReg]
// ============================================================================

class RAWorkReg {
public:
  ASMJIT_NONCOPYABLE(RAWorkReg)

  enum Ids : uint32_t {
    kIdNone               = 0xFFFFFFFFU
  };

  enum Flags : uint32_t {
    kFlagCoalesced        = 0x00000001U, //!< Has been coalesced to another WorkReg.
    kFlagStackUsed        = 0x00000002U, //!< Stack slot has to be allocated.
    kFlagStackPreferred   = 0x00000004U, //!< Stack allocation is preferred.
    kFlagStackArgToStack  = 0x00000008U, //!< Marked for stack argument reassignment.

    // TODO: Used?
    kFlagDirtyStats       = 0x80000000U
  };

  enum ArgIndex : uint32_t {
    kNoArgIndex      = 0xFFU
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  ASMJIT_FORCEINLINE RAWorkReg(VirtReg* vReg, uint32_t workId) noexcept
    : _workId(workId),
      _virtId(vReg->getId()),
      _virtReg(vReg),
      _tiedReg(nullptr),
      _stackSlot(nullptr),
      _info(vReg->getInfo()),
      _flags(kFlagDirtyStats),
      _allocatedMask(0),
      _argIndex(kNoArgIndex),
      _homeId(Reg::kIdBad),
      _liveSpans(),
      _liveStats(),
      _refs() {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline uint32_t getWorkId() const noexcept { return _workId; }
  inline uint32_t getVirtId() const noexcept { return _virtId; }

  inline const char* getName() const noexcept { return _virtReg->getName(); }
  inline uint32_t getNameLength() const noexcept { return _virtReg->getNameLength(); }

  inline uint32_t getTypeId() const noexcept { return _virtReg->getTypeId(); }

  inline bool hasFlag(uint32_t flag) const noexcept { return (_flags & flag) != 0; }
  inline uint32_t getFlags() const noexcept { return _flags; }
  inline void addFlags(uint32_t flags) noexcept { _flags |= flags; }

  inline bool isStackUsed() const noexcept { return hasFlag(kFlagStackUsed); }
  inline void markStackUsed() noexcept { addFlags(kFlagStackUsed); }

  inline bool isStackPreferred() const noexcept { return hasFlag(kFlagStackPreferred); }
  inline void markStackPreferred() noexcept { addFlags(kFlagStackPreferred); }

  //! Get whether this RAWorkReg has been coalesced with another one (cannot be used anymore).
  inline bool isCoalesced() const noexcept { return hasFlag(kFlagCoalesced); }

  inline const RegInfo& getInfo() const noexcept { return _info; }
  inline uint32_t getGroup() const noexcept { return _info.getGroup(); }

  inline VirtReg* getVirtReg() const noexcept { return _virtReg; }

  inline bool hasTiedReg() const noexcept { return _tiedReg != nullptr; }
  inline RATiedReg* getTiedReg() const noexcept { return _tiedReg; }
  inline void setTiedReg(RATiedReg* tiedReg) noexcept { _tiedReg = tiedReg; }
  inline void resetTiedReg() noexcept { _tiedReg = nullptr; }

  inline bool hasStackSlot() const noexcept { return _stackSlot != nullptr; }
  inline RAStackSlot* getStackSlot() const noexcept { return _stackSlot; }

  inline LiveRegSpans& getLiveSpans() noexcept { return _liveSpans; }
  inline const LiveRegSpans& getLiveSpans() const noexcept { return _liveSpans; }

  inline RALiveStats& getLiveStats() noexcept { return _liveStats; }
  inline const RALiveStats& getLiveStats() const noexcept { return _liveStats; }

  inline bool hasArgIndex() const noexcept { return _argIndex != kNoArgIndex; }
  inline uint32_t getArgIndex() const noexcept { return _argIndex; }
  inline void setArgIndex(uint32_t index) noexcept { _argIndex = uint8_t(index); }

  inline bool hasHomeId() const noexcept { return _homeId != Reg::kIdBad; }
  inline uint32_t getHomeId() const noexcept { return _homeId; }
  inline void setHomeId(uint32_t physId) noexcept { _homeId = uint8_t(physId); }

  inline uint32_t getAllocatedMask() const noexcept { return _allocatedMask; }
  inline void addAllocatedMask(uint32_t mask) noexcept { _allocatedMask |= mask; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _workId;                      //!< RAPass specific ID used during analysis and allocation.
  uint32_t _virtId;                      //!< Copy of ID used by `VirtReg`.

  VirtReg* _virtReg;                     //!< Permanent association with `VirtReg`.
  RATiedReg* _tiedReg;                   //!< Temporary association with `RATiedReg`.
  RAStackSlot* _stackSlot;               //!< Stack slot associated with the register.

  RegInfo _info;                         //!< Copy of a signature used by `VirtReg`.
  uint32_t _flags;                       //!< RAPass specific flags used during analysis and allocation.
  uint32_t _allocatedMask;               //!< IDs of all physical registers this WorkReg has been allocated to.

  uint8_t _argIndex;                     //!< Argument index (or kNoStackArgIndex if none).
  uint8_t _homeId;                       //!< Global home register ID (if any).

  LiveRegSpans _liveSpans;               //!< Live spans of the `VirtReg`.
  RALiveStats _liveStats;                //!< Live statistics.

  ZoneVector<CBNode*> _refs;             //!< All nodes that read/write this VirtReg/WorkReg.
  ZoneVector<CBNode*> _writes;           //!< All nodes that write to this VirtReg/WorkReg.
};

//! \}

ASMJIT_END_NAMESPACE

// [Guard]
#endif // !ASMJIT_DISABLE_COMPILER
#endif // _ASMJIT_CORE_RADEFS_P_H
