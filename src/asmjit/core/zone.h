// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_CORE_ZONE_H
#define _ASMJIT_CORE_ZONE_H

// [Dependencies]
#include "../core/algorithm.h"
#include "../core/intutils.h"

ASMJIT_BEGIN_NAMESPACE

//! \addtogroup asmjit_core
//! \{

// ============================================================================
// [asmjit::Zone]
// ============================================================================

//! Zone memory.
//!
//! Zone is an incremental memory allocator that allocates memory by simply
//! incrementing a pointer. It allocates blocks of memory by using C's `malloc()`,
//! but divides these blocks into smaller segments requested by calling
//! `Zone::alloc()` and friends.
//!
//! Zone has no function to release the allocated memory. It has to be released
//! all at once by calling `reset()`. If you need a more friendly allocator that
//! also supports `release()`, consider using \ref Zone with \ref ZoneAllocator.
class Zone {
public:
  //! \internal
  //!
  //! A single block of memory.
  struct Block {
    Block* prev;                         //!< Link to the previous block.
    Block* next;                         //!< Link to the next block.
    size_t size;                         //!< Size of the block.
    uint8_t data[sizeof(void*)];         //!< Data.
  };

  enum : uint32_t {
    //! Zone allocator overhead.
    kZoneOverhead = Globals::kAllocOverhead + uint32_t(sizeof(Block))
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create a new instance of `Zone` allocator.
  //!
  //! The `blockSize` parameter describes the default size of the block. If the
  //! `size` parameter passed to `alloc()` is greater than the default size
  //! `Zone` will allocate and use a larger block, but it will not change the
  //! default `blockSize`.
  //!
  //! It's not required, but it's good practice to set `blockSize` to a
  //! reasonable value that depends on the usage of `Zone`. Greater block sizes
  //! are generally safer and perform better than unreasonably low values.
  ASMJIT_API Zone(uint32_t blockSize, uint32_t blockAlignment = 32) noexcept;

  //! Destroy the `Zone` instance.
  //!
  //! This will destroy the `Zone` instance and release all blocks of memory
  //! allocated by it. It performs implicit `reset(true)`.
  ASMJIT_API ~Zone() noexcept;

  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  //! Reset the `Zone` invalidating all blocks allocated.
  //!
  //! If `releaseMemory` is true all buffers will be released to the system.
  ASMJIT_API void reset(bool releaseMemory = false) noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the default block size.
  inline uint32_t getBlockSize() const noexcept { return _blockSize; }
  //! Get the default block alignment.
  inline uint32_t getBlockAlignment() const noexcept { return (uint32_t)1 << _blockAlignmentShift; }
  //! Get remaining size of the current block.
  inline size_t getRemainingSize() const noexcept { return (size_t)(_end - _ptr); }

  //! Get the current zone cursor (dangerous).
  //!
  //! This is a function that can be used to get exclusive access to the current
  //! block's memory buffer.
  inline uint8_t* getCursor() noexcept { return _ptr; }
  //! Get the end of the current zone block, only useful if you use `getCursor()`.
  inline uint8_t* getEnd() noexcept { return _end; }

  //! Set the current zone cursor to `p` (must match the current block).
  inline void setCursor(uint8_t* p) noexcept {
    ASMJIT_ASSERT(p >= _ptr && p <= _end);
    _ptr = p;
  }

  // --------------------------------------------------------------------------
  // [Align the current pointer to `alignment` and return it]
  // --------------------------------------------------------------------------

  inline uint8_t* align(size_t alignment) noexcept {
    _ptr = std::min<uint8_t*>(IntUtils::alignUp(_ptr, alignment), _end);
    ASMJIT_ASSERT(_ptr >= _block->data && _ptr <= _end);
    return _ptr;
  }

  // --------------------------------------------------------------------------
  // [Alloc]
  // --------------------------------------------------------------------------

  //! Allocate `size` bytes of memory.
  //!
  //! Pointer returned is valid until the `Zone` instance is destroyed or reset
  //! by calling `reset()`. If you plan to make an instance of C++ from the
  //! given pointer use placement `new` and `delete` operators:
  //!
  //! ~~~
  //! using namespace asmjit;
  //!
  //! class Object { ... };
  //!
  //! // Create Zone with default block size of approximately 65536 bytes.
  //! Zone zone(65536 - Zone::kZoneOverhead);
  //!
  //! // Create your objects using zone object allocating, for example:
  //! Object* obj = static_cast<Object*>( zone.alloc(sizeof(Object)) );
  //
  //! if (!obj) {
  //!   // Handle out of memory error.
  //! }
  //!
  //! // Placement `new` and `delete` operators can be used to instantiate it.
  //! new(obj) Object();
  //!
  //! // ... lifetime of your objects ...
  //!
  //! // To destroy the instance (if required).
  //! obj->~Object();
  //!
  //! // Reset or destroy `Zone`.
  //! zone.reset();
  //! ~~~
  ASMJIT_FORCEINLINE void* alloc(size_t size) noexcept {
    uint8_t* ptr = _ptr;
    size_t remainingBytes = (size_t)(_end - ptr);

    if (ASMJIT_UNLIKELY(remainingBytes < size))
      return _alloc(size);

    _ptr += size;
    ASMJIT_ASSERT(_ptr <= _end);

    return static_cast<void*>(ptr);
  }

  //! Allocate `size` bytes of memory aligned to the given `alignment`.
  inline void* allocAligned(size_t size, size_t alignment) noexcept {
    align(alignment);
    return alloc(size);
  }

  //! Allocate `size` bytes without any checks.
  //!
  //! Can only be called if `getRemainingSize()` returns size at least equal
  //! to `size`.
  inline void* allocNoCheck(size_t size) noexcept {
    ASMJIT_ASSERT((size_t)(_end - _ptr) >= size);

    uint8_t* ptr = _ptr;
    _ptr += size;
    return static_cast<void*>(ptr);
  }

  //! Allocate `size` bytes of zeroed memory.
  //!
  //! See \ref alloc() for more details.
  ASMJIT_API void* allocZeroed(size_t size) noexcept;

  //! Like `alloc()`, but the return pointer is casted to `T*`.
  template<typename T>
  inline T* allocT(size_t size = sizeof(T)) noexcept {
    return static_cast<T*>(alloc(size));
  }

  //! Like `alloc()`, but the return pointer is casted to `T*`.
  template<typename T>
  inline T* allocAlignedT(size_t size, size_t alignment) noexcept {
    return static_cast<T*>(allocAligned(size, alignment));
  }

  //! Like `allocNoCheck()`, but the return pointer is casted to `T*`.
  template<typename T>
  inline T* allocNoCheckT(size_t size = sizeof(T)) noexcept {
    return static_cast<T*>(allocNoCheck(size));
  }

  //! Like `allocZeroed()`, but the return pointer is casted to `T*`.
  template<typename T>
  inline T* allocZeroedT(size_t size = sizeof(T)) noexcept {
    return static_cast<T*>(allocZeroed(size));
  }

  //! Like `new(std::nothrow) T(...)`, but allocated by `Zone`.
  template<typename T>
  inline T* newT() noexcept {
    void* p = alloc(sizeof(T));
    if (ASMJIT_UNLIKELY(!p))
      return nullptr;
    return new(p) T();
  }
  //! Like `new(std::nothrow) T(...)`, but allocated by `Zone`.
  template<typename T, typename P1>
  inline T* newT(P1 p1) noexcept {
    void* p = alloc(sizeof(T));
    if (ASMJIT_UNLIKELY(!p))
      return nullptr;
    return new(p) T(p1);
  }

  //! \internal
  ASMJIT_API void* _alloc(size_t size) noexcept;

  //! Helper to duplicate data.
  ASMJIT_API void* dup(const void* data, size_t size, bool nullTerminate = false) noexcept;

  //! Helper to duplicate data.
  inline void* dupAligned(const void* data, size_t size, size_t alignment, bool nullTerminate = false) noexcept {
    align(alignment);
    return dup(data, size, nullTerminate);
  }

  //! Helper to duplicate a formatted string, maximum length is 256 bytes.
  ASMJIT_API char* sformat(const char* str, ...) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint8_t* _ptr;                         //!< Pointer in the current block's buffer.
  uint8_t* _end;                         //!< End of the current block's buffer.
  Block* _block;                         //!< Current block.

#if ASMJIT_ARCH_BITS == 64
  uint32_t _blockSize;                   //!< Default size of a newly allocated block.
  uint32_t _blockAlignmentShift;         //!< Minimum alignment of each block.
#else
  uint32_t _blockSize : 29;              //!< Default size of a newly allocated block.
  uint32_t _blockAlignmentShift : 3;     //!< Minimum alignment of each block.
#endif
};

// ============================================================================
// [asmjit::ZoneAllocator]
// ============================================================================

//! Zone-based memory allocator that uses an existing \ref Zone and provides
//! a `release()` functionality on top of it. It uses \ref Zone only for chunks
//! that can be pooled, and uses libc `malloc()` for chunks that are large.
//!
//! The advantage of ZoneAllocator is that it can allocate small chunks of memory
//! really fast, and these chunks, when released, will be reused by consecutive
//! calls to `alloc()`. Also, since ZoneAllocator uses \ref Zone, you can turn
//! any \ref Zone into a \ref ZoneAllocator, and use it in your \ref Pass when
//! necessary.
//!
//! ZoneAllocator is used by AsmJit containers to make containers having only
//! few elements fast (and lightweight) and to allow them to grow and use
//! dynamic blocks when require more storage.
class ZoneAllocator {
public:
  ASMJIT_NONCOPYABLE(ZoneAllocator)

  enum {
    // In short, we pool chunks of these sizes:
    //   [32, 64, 96, 128, 192, 256, 320, 384, 448, 512]

    //! How many bytes per a low granularity pool (has to be at least 16).
    kLoGranularity = 32,
    //! Number of slots of a low granularity pool.
    kLoCount = 4,
    //! Maximum size of a block that can be allocated in a low granularity pool.
    kLoMaxSize = kLoGranularity * kLoCount,

    //! How many bytes per a high granularity pool.
    kHiGranularity = 64,
    //! Number of slots of a high granularity pool.
    kHiCount = 6,
    //! Maximum size of a block that can be allocated in a high granularity pool.
    kHiMaxSize = kLoMaxSize + kHiGranularity * kHiCount,

    //! Alignment of every pointer returned by `alloc()`.
    kBlockAlignment = kLoGranularity
  };

  //! Single-linked list used to store unused chunks.
  struct Slot {
    //! Link to a next slot in a single-linked list.
    Slot* next;
  };

  //! A block of memory that has been allocated dynamically and is not part of
  //! block-list used by the allocator. This is used to keep track of all these
  //! blocks so they can be freed by `reset()` if not freed explicitly.
  struct DynamicBlock {
    DynamicBlock* prev;
    DynamicBlock* next;
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create a new `ZoneAllocator`.
  //!
  //! NOTE: To use it, you must first `init()` it.
  inline ZoneAllocator() noexcept {
    std::memset(this, 0, sizeof(*this));
  }
  //! Create a new `ZoneAllocator` initialized to use `zone`.
  explicit inline ZoneAllocator(Zone* zone) noexcept {
    std::memset(this, 0, sizeof(*this));
    _zone = zone;
  }
  //! Destroy the `ZoneAllocator`.
  inline ~ZoneAllocator() noexcept { reset(); }

  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  //! Get whether the `ZoneAllocator` is initialized (i.e. has `Zone`).
  inline bool isInitialized() const noexcept { return _zone != nullptr; }

  //! Convenience method to initialize the `ZoneAllocator` with `zone`.
  //!
  //! It's the same as calling `reset(zone)`.
  inline void init(Zone* zone) noexcept { reset(zone); }

  //! Reset this `ZoneAllocator` and also forget about the current `Zone` which
  //! is attached (if any). Reset optionally attaches a new `zone` passed, or
  //! keeps the `ZoneAllocator` in an uninitialized state, if `zone` is null.
  ASMJIT_API void reset(Zone* zone = nullptr) noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the `Zone` the allocator is using, or null if it's not initialized.
  inline Zone* getZone() const noexcept { return _zone; }

  // --------------------------------------------------------------------------
  // [Utilities]
  // --------------------------------------------------------------------------

  //! \internal
  //!
  //! Get the slot index to be used for `size`. Returns `true` if a valid slot
  //! has been written to `slot` and `allocatedSize` has been filled with slot
  //! exact size (`allocatedSize` can be equal or slightly greater than `size`).
  static ASMJIT_FORCEINLINE bool _getSlotIndex(size_t size, uint32_t& slot) noexcept {
    ASMJIT_ASSERT(size > 0);
    if (size > kHiMaxSize)
      return false;

    if (size <= kLoMaxSize)
      slot = uint32_t((size - 1) / kLoGranularity);
    else
      slot = uint32_t((size - kLoMaxSize - 1) / kHiGranularity) + kLoCount;

    return true;
  }

  //! \overload
  static ASMJIT_FORCEINLINE bool _getSlotIndex(size_t size, uint32_t& slot, size_t& allocatedSize) noexcept {
    ASMJIT_ASSERT(size > 0);
    if (size > kHiMaxSize)
      return false;

    if (size <= kLoMaxSize) {
      slot = uint32_t((size - 1) / kLoGranularity);
      allocatedSize = IntUtils::alignUp(size, kLoGranularity);
    }
    else {
      slot = uint32_t((size - kLoMaxSize - 1) / kHiGranularity) + kLoCount;
      allocatedSize = IntUtils::alignUp(size, kHiGranularity);
    }

    return true;
  }

  // --------------------------------------------------------------------------
  // [Alloc / Release]
  // --------------------------------------------------------------------------

  ASMJIT_API void* _alloc(size_t size, size_t& allocatedSize) noexcept;
  ASMJIT_API void* _allocZeroed(size_t size, size_t& allocatedSize) noexcept;
  ASMJIT_API void _releaseDynamic(void* p, size_t size) noexcept;

  //! Allocate `size` bytes of memory, ideally from an available pool.
  //!
  //! NOTE: `size` can't be zero, it will assert in debug mode in such case.
  inline void* alloc(size_t size) noexcept {
    ASMJIT_ASSERT(isInitialized());
    size_t allocatedSize;
    return _alloc(size, allocatedSize);
  }

  //! Like `alloc(size)`, but provides a second argument `allocatedSize` that
  //! provides a way to know how big the block returned actually is. This is
  //! useful for containers to prevent growing too early.
  inline void* alloc(size_t size, size_t& allocatedSize) noexcept {
    ASMJIT_ASSERT(isInitialized());
    return _alloc(size, allocatedSize);
  }

  //! Like `alloc()`, but the return pointer is casted to `T*`.
  template<typename T>
  inline T* allocT(size_t size = sizeof(T)) noexcept {
    return static_cast<T*>(alloc(size));
  }

  //! Like `alloc(size)`, but returns zeroed memory.
  inline void* allocZeroed(size_t size) noexcept {
    ASMJIT_ASSERT(isInitialized());
    size_t allocatedSize;
    return _allocZeroed(size, allocatedSize);
  }

  //! Like `alloc(size, allocatedSize)`, but returns zeroed memory.
  inline void* allocZeroed(size_t size, size_t& allocatedSize) noexcept {
    ASMJIT_ASSERT(isInitialized());
    return _allocZeroed(size, allocatedSize);
  }

  //! Like `allocZeroed()`, but the return pointer is casted to `T*`.
  template<typename T>
  inline T* allocZeroedT(size_t size = sizeof(T)) noexcept {
    return static_cast<T*>(allocZeroed(size));
  }

  //! Release the memory previously allocated by `alloc()`. The `size` argument
  //! has to be the same as used to call `alloc()` or `allocatedSize` returned
  //! by `alloc()`.
  inline void release(void* p, size_t size) noexcept {
    ASMJIT_ASSERT(isInitialized());
    ASMJIT_ASSERT(p != nullptr);
    ASMJIT_ASSERT(size != 0);

    uint32_t slot;
    if (_getSlotIndex(size, slot)) {
      static_cast<Slot*>(p)->next = static_cast<Slot*>(_slots[slot]);
      _slots[slot] = static_cast<Slot*>(p);
    }
    else {
      _releaseDynamic(p, size);
    }
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  Zone* _zone;                           //!< Zone used to allocate memory that fits into slots.
  Slot* _slots[kLoCount + kHiCount];     //!< Indexed slots containing released memory.
  DynamicBlock* _dynamicBlocks;          //!< Dynamic blocks for larger allocations (no slots).
};

// ============================================================================
// [asmjit::ZoneList<T>]
// ============================================================================

//! \internal
template <typename T>
class ZoneList {
public:
  ASMJIT_NONCOPYABLE(ZoneList<T>)

  // --------------------------------------------------------------------------
  // [Link]
  // --------------------------------------------------------------------------

  //! ZoneList node.
  struct Link {
    //! Get next node.
    inline Link* getNext() const noexcept { return _next; }
    //! Get value.
    inline T getValue() const noexcept { return _value; }
    //! Set value to `value`.
    inline void setValue(const T& value) noexcept { _value = value; }

    Link* _next;
    T _value;
  };

  // --------------------------------------------------------------------------
  // [Appender]
  // --------------------------------------------------------------------------

  //! Specialized appender that takes advantage of ZoneList structure. You must
  //! initialize it and then call done().
  struct Appender {
    inline Appender(ZoneList<T>& list) noexcept { init(list); }

    inline void init(ZoneList<T>& list) noexcept {
      pPrev = &list._first;
    }

    inline void done(ZoneList<T>& list) noexcept {
      list._last = *pPrev;
      *pPrev = nullptr;
    }

    inline void append(Link* node) noexcept {
      *pPrev = node;
      pPrev = &node->_next;
    }

    Link** pPrev;
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline ZoneList() noexcept : _first(nullptr), _last(nullptr) {}
  inline ~ZoneList() noexcept {}

  // --------------------------------------------------------------------------
  // [Data]
  // --------------------------------------------------------------------------

  inline bool isEmpty() const noexcept { return _first != nullptr; }
  inline Link* getFirst() const noexcept { return _first; }
  inline Link* getLast() const noexcept { return _last; }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  inline void reset() noexcept {
    _first = nullptr;
    _last = nullptr;
  }

  inline void prepend(Link* link) noexcept {
    link->_next = _first;
    if (!_first) _last = link;
    _first = link;
  }

  inline void append(Link* link) noexcept {
    link->_next = nullptr;
    if (!_first)
      _first = link;
    else
      _last->_next = link;
    _last = link;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  Link* _first;
  Link* _last;
};

// ============================================================================
// [asmjit::ZoneBitVector]
// ============================================================================

class ZoneBitVector {
public:
  ASMJIT_NONCOPYABLE(ZoneBitVector)

  typedef Globals::BitWord BitWord;
  enum { kBitWordSize = Globals::kBitWordSize };

  static inline uint32_t _wordsPerBits(uint32_t nBits) noexcept {
    return ((nBits + kBitWordSize - 1) / kBitWordSize);
  }

  static inline void _zeroBits(BitWord* dst, uint32_t nBitWords) noexcept {
    for (uint32_t i = 0; i < nBitWords; i++)
      dst[i] = 0;
  }

  static inline void _copyBits(BitWord* dst, const BitWord* src, uint32_t nBitWords) noexcept {
    for (uint32_t i = 0; i < nBitWords; i++)
      dst[i] = src[i];
  }

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  explicit inline ZoneBitVector() noexcept
    : _data(nullptr),
      _length(0),
      _capacity(0) {}

  inline ZoneBitVector(ZoneBitVector&& other) noexcept
    : _data(other._data),
      _length(other._length),
      _capacity(other._capacity) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get whether the bit-vector is empty (has no bits).
  inline bool isEmpty() const noexcept { return _length == 0; }
  //! Get a length of this bit-vector (in bits).
  inline uint32_t getLength() const noexcept { return _length; }
  //! Get a capacity of this bit-vector (in bits).
  inline uint32_t getCapacity() const noexcept { return _capacity; }

  //! Get a count of `BitWord[]` array need to store all bits.
  inline uint32_t getBitWordLength() const noexcept { return _wordsPerBits(_length); }
  //! Get a count of `BitWord[]` array need to store all bits.
  inline uint32_t getBitWordCapacity() const noexcept { return _wordsPerBits(_capacity); }

  //! Get data.
  inline BitWord* getData() noexcept { return _data; }
  //! \overload
  inline const BitWord* getData() const noexcept { return _data; }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  inline void clear() noexcept {
    _length = 0;
  }

  inline void reset() noexcept {
    _data = nullptr;
    _length = 0;
    _capacity = 0;
  }

  inline void truncate(uint32_t newLength) noexcept {
    _length = std::min(_length, newLength);
    _clearUnusedBits();
  }

  inline bool getAt(uint32_t index) const noexcept {
    ASMJIT_ASSERT(index < _length);

    uint32_t idx = index / kBitWordSize;
    uint32_t bit = index % kBitWordSize;
    return bool((_data[idx] >> bit) & 1);
  }

  inline void setAt(uint32_t index, bool value) noexcept {
    ASMJIT_ASSERT(index < _length);

    uint32_t idx = index / kBitWordSize;
    uint32_t bit = index % kBitWordSize;
    if (value)
      _data[idx] |= BitWord(1) << bit;
    else
      _data[idx] &= ~(BitWord(1) << bit);
  }

  inline void toggleAt(uint32_t index) noexcept {
    ASMJIT_ASSERT(index < _length);

    uint32_t idx = index / kBitWordSize;
    uint32_t bit = index % kBitWordSize;
    _data[idx] ^= BitWord(1) << bit;
  }

  ASMJIT_FORCEINLINE Error append(ZoneAllocator* allocator, bool value) noexcept {
    uint32_t index = _length;
    if (ASMJIT_UNLIKELY(index >= _capacity))
      return _append(allocator, value);

    uint32_t idx = index / kBitWordSize;
    uint32_t bit = index % kBitWordSize;

    if (bit == 0)
      _data[idx] = BitWord(value) << bit;
    else
      _data[idx] |= BitWord(value) << bit;

    _length++;
    return kErrorOk;
  }

  ASMJIT_API Error copyFrom(ZoneAllocator* allocator, const ZoneBitVector& other) noexcept;
  ASMJIT_API Error fill(uint32_t fromIndex, uint32_t toIndex, bool value) noexcept;
  inline void zero() noexcept { _zeroBits(_data, _wordsPerBits(_length)); }

  inline void and_(const ZoneBitVector& other) noexcept {
    BitWord* dst = _data;
    const BitWord* src = other._data;

    uint32_t numWords = (std::min(_length, other._length) + kBitWordSize - 1) / kBitWordSize;
    for (uint32_t i = 0; i < numWords; i++)
      dst[i] = dst[i] & src[i];
    _clearUnusedBits();
  }

  inline void andNot(const ZoneBitVector& other) noexcept {
    BitWord* dst = _data;
    const BitWord* src = other._data;

    uint32_t numWords = _wordsPerBits(std::min(_length, other._length));
    for (uint32_t i = 0; i < numWords; i++)
      dst[i] = dst[i] & ~src[i];
    _clearUnusedBits();
  }

  inline void or_(const ZoneBitVector& other) noexcept {
    BitWord* dst = _data;
    const BitWord* src = other._data;

    uint32_t numWords = _wordsPerBits(std::min(_length, other._length));
    for (uint32_t i = 0; i < numWords; i++)
      dst[i] = dst[i] | src[i];
    _clearUnusedBits();
  }

  inline void _clearUnusedBits() noexcept {
    uint32_t idx = _length / kBitWordSize;
    uint32_t bit = _length % kBitWordSize;

    if (!bit) return;
    _data[idx] &= (BitWord(1) << bit) - 1U;
  }

  inline bool eq(const ZoneBitVector& other) const noexcept {
    uint32_t len = _length;

    if (len != other._length)
      return false;

    const BitWord* aData = _data;
    const BitWord* bData = other._data;

    uint32_t numBitWords = _wordsPerBits(len);
    for (uint32_t i = 0; i < numBitWords; i++) {
      if (aData[i] != bData[i])
        return false;
    }
    return true;
  }

  inline bool operator==(const ZoneBitVector& other) const noexcept { return  eq(other); }
  inline bool operator!=(const ZoneBitVector& other) const noexcept { return !eq(other); }

  // --------------------------------------------------------------------------
  // [Memory Management]
  // --------------------------------------------------------------------------

  inline void release(ZoneAllocator* allocator) noexcept {
    if (!_data) return;
    allocator->release(_data, _capacity / 8);
    reset();
  }

  inline Error resize(ZoneAllocator* allocator, uint32_t newLength, bool newBitsValue = false) noexcept {
    return _resize(allocator, newLength, newLength, newBitsValue);
  }

  ASMJIT_API Error _resize(ZoneAllocator* allocator, uint32_t newLength, uint32_t idealCapacity, bool newBitsValue) noexcept;
  ASMJIT_API Error _append(ZoneAllocator* allocator, bool value) noexcept;

  // --------------------------------------------------------------------------
  // [Iterators]
  // --------------------------------------------------------------------------

  class ForEachBitSet : public IntUtils::BitArrayIterator<BitWord> {
  public:
    explicit ASMJIT_FORCEINLINE ForEachBitSet(const ZoneBitVector& bitVector) noexcept
      : IntUtils::BitArrayIterator<BitWord>(bitVector.getData(), bitVector.getBitWordLength()) {}
  };

  template<class Operator>
  class ForEachBitOp : public IntUtils::BitArrayOpIterator<BitWord, Operator> {
  public:
    ASMJIT_FORCEINLINE ForEachBitOp(const ZoneBitVector& a, const ZoneBitVector& b) noexcept
      : IntUtils::BitArrayOpIterator<BitWord, Operator>(a.getData(), b.getData(), a.getBitWordLength()) {
      ASMJIT_ASSERT(a.getLength() == b.getLength());
    }
  };

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  BitWord* _data;                        //!< Bits.
  uint32_t _length;                      //!< Length of the bit-vector (in bits).
  uint32_t _capacity;                    //!< Capacity of the bit-vector (in bits).
};

// ============================================================================
// [asmjit::ZoneVectorBase]
// ============================================================================

//! \internal
class ZoneVectorBase {
public:
  ASMJIT_NONCOPYABLE(ZoneVectorBase)

protected:
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create a new instance of `ZoneVectorBase`.
  explicit inline ZoneVectorBase() noexcept
    : _data(nullptr),
      _length(0),
      _capacity(0) {}

  inline ZoneVectorBase(ZoneVectorBase&& other) noexcept
    : _data(other._data),
      _length(other._length),
      _capacity(other._capacity) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

public:
  //! Get whether the vector is empty.
  inline bool isEmpty() const noexcept { return _length == 0; }
  //! Get vector length.
  inline uint32_t getLength() const noexcept { return _length; }
  //! Get vector capacity.
  inline uint32_t getCapacity() const noexcept { return _capacity; }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  //! Makes the vector empty (won't change the capacity or data pointer).
  inline void clear() noexcept { _length = 0; }
  //! Reset the vector data and set its `length` to zero.
  inline void reset() noexcept {
    _data = nullptr;
    _length = 0;
    _capacity = 0;
  }

  //! Truncate the vector to at most `n` items.
  inline void truncate(uint32_t n) noexcept {
    _length = std::min(_length, n);
  }

  //! Set length of the vector to `n`. Used internally by some algorithms.
  inline void _setLength(uint32_t n) noexcept {
    ASMJIT_ASSERT(n <= _capacity);
    _length = n;
  }

  // --------------------------------------------------------------------------
  // [Memory Management]
  // --------------------------------------------------------------------------

protected:
  inline void _release(ZoneAllocator* allocator, uint32_t sizeOfT) noexcept {
    if (_data != nullptr) {
      allocator->release(_data, _capacity * sizeOfT);
      reset();
    }
  }

  ASMJIT_API Error _grow(ZoneAllocator* allocator, uint32_t sizeOfT, uint32_t n) noexcept;
  ASMJIT_API Error _resize(ZoneAllocator* allocator, uint32_t sizeOfT, uint32_t n) noexcept;
  ASMJIT_API Error _reserve(ZoneAllocator* allocator, uint32_t sizeOfT, uint32_t n) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

public:
  void* _data;                           //!< Vector data (untyped).
  uint32_t _length;                      //!< Length of the vector.
  uint32_t _capacity;                    //!< Capacity of the vector.
};

// ============================================================================
// [asmjit::ZoneVector<T>]
// ============================================================================

//! Template used to store and manage array of Zone allocated data.
//!
//! This template has these advantages over other std::vector<>:
//! - Always non-copyable (designed to be non-copyable, we want it).
//! - No copy-on-write (some implementations of STL can use it).
//! - Optimized for working only with POD types.
//! - Uses ZoneAllocator, thus small vectors are basically for free.
template <typename T>
class ZoneVector : public ZoneVectorBase {
public:
  ASMJIT_NONCOPYABLE(ZoneVector<T>)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline ZoneVector() noexcept : ZoneVectorBase() {}
  inline ZoneVector(ZoneVector&& other) noexcept : ZoneVector(other) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get data.
  inline T* getData() noexcept { return static_cast<T*>(_data); }
  //! \overload
  inline const T* getData() const noexcept { return static_cast<const T*>(_data); }

  //! Get item at index `i` (const).
  inline const T& getAt(uint32_t i) const noexcept {
    ASMJIT_ASSERT(i < _length);
    return getData()[i];
  }

  inline void _setEndPtr(T* p) noexcept {
    ASMJIT_ASSERT(p >= getData() && p <= getData() + _capacity);
    _setLength(uint32_t((uintptr_t)(p - getData())));
  }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  //! Prepend `item` to the vector.
  inline Error prepend(ZoneAllocator* allocator, const T& item) noexcept {
    if (ASMJIT_UNLIKELY(_length == _capacity))
      ASMJIT_PROPAGATE(grow(allocator, 1));

    std::memmove(static_cast<T*>(_data) + 1, _data, size_t(_length) * sizeof(T));
    std::memcpy(_data, &item, sizeof(T));

    _length++;
    return kErrorOk;
  }

  //! Insert an `item` at the specified `index`.
  inline Error insert(ZoneAllocator* allocator, uint32_t index, const T& item) noexcept {
    ASMJIT_ASSERT(index <= _length);

    if (ASMJIT_UNLIKELY(_length == _capacity))
      ASMJIT_PROPAGATE(grow(allocator, 1));

    T* dst = static_cast<T*>(_data) + index;
    std::memmove(dst + 1, dst, size_t(_length - index) * sizeof(T));
    std::memcpy(dst, &item, sizeof(T));
    _length++;

    return kErrorOk;
  }

  //! Append `item` to the vector.
  inline Error append(ZoneAllocator* allocator, const T& item) noexcept {
    if (ASMJIT_UNLIKELY(_length == _capacity))
      ASMJIT_PROPAGATE(grow(allocator, 1));

    std::memcpy(static_cast<T*>(_data) + _length, &item, sizeof(T));
    _length++;

    return kErrorOk;
  }

  inline Error concat(ZoneAllocator* allocator, const ZoneVector<T>& other) noexcept {
    uint32_t count = other._length;
    if (_capacity - _length < count)
      ASMJIT_PROPAGATE(grow(allocator, count));

    if (count) {
      std::memcpy(static_cast<T*>(_data) + _length, other._data, size_t(count) * sizeof(T));
      _length += count;
    }

    return kErrorOk;
  }

  //! Prepend `item` to the vector (unsafe case).
  //!
  //! Can only be used together with `willGrow()`. If `willGrow(N)` returns
  //! `kErrorOk` then N elements can be added to the vector without checking
  //! if there is a place for them. Used mostly internally.
  inline void prependUnsafe(const T& item) noexcept {
    ASMJIT_ASSERT(_length < _capacity);
    T* data = static_cast<T*>(_data);

    if (_length)
      std::memmove(data + 1, data, size_t(_length) * sizeof(T));

    std::memcpy(data, &item, sizeof(T));
    _length++;
  }

  //! Append `item` to the vector (unsafe case).
  //!
  //! Can only be used together with `willGrow()`. If `willGrow(N)` returns
  //! `kErrorOk` then N elements can be added to the vector without checking
  //! if there is a place for them. Used mostly internally.
  inline void appendUnsafe(const T& item) noexcept {
    ASMJIT_ASSERT(_length < _capacity);

    std::memcpy(static_cast<T*>(_data) + _length, &item, sizeof(T));
    _length++;
  }

  //! Concatenate all items of `other` at the end of the vector.
  inline void concatUnsafe(const ZoneVector<T>& other) noexcept {
    uint32_t count = other._length;
    ASMJIT_ASSERT(_capacity - _length >= count);

    if (count) {
      std::memcpy(static_cast<T*>(_data) + _length, other._data, size_t(count) * sizeof(T));
      _length += count;
    }
  }

  //! Get index of `val` or `Globals::kNotFound` if not found.
  inline uint32_t indexOf(const T& val) const noexcept {
    const T* data = static_cast<const T*>(_data);
    uint32_t length = _length;

    for (uint32_t i = 0; i < length; i++)
      if (data[i] == val)
        return i;

    return Globals::kNotFound;
  }

  //! Get whether the vector contains `val`.
  inline bool contains(const T& val) const noexcept {
    return indexOf(val) != Globals::kNotFound;
  }

  //! Remove item at index `i`.
  inline void removeAt(uint32_t i) noexcept {
    ASMJIT_ASSERT(i < _length);

    T* data = static_cast<T*>(_data) + i;
    uint32_t count = --_length - i;

    if (count)
      std::memmove(data, data + 1, size_t(count) * sizeof(T));
  }

  inline T pop() noexcept {
    ASMJIT_ASSERT(_length > 0);

    uint32_t index = --_length;
    return getData()[index];
  }

  //! Swap this pod-vector with `other`.
  inline void swap(ZoneVector<T>& other) noexcept {
    std::swap(_length, other._length);
    std::swap(_capacity, other._capacity);
    std::swap(_data, other._data);
  }

  template<typename CMP = Algorithm::Compare<Algorithm::kOrderAscending>>
  inline void sort(const CMP& cmp = CMP()) noexcept {
    Algorithm::qSort<T, CMP>(getData(), getLength(), cmp);
  }

  //! Get item at index `i`.
  inline T& operator[](uint32_t i) noexcept {
    ASMJIT_ASSERT(i < _length);
    return getData()[i];
  }

  //! Get item at index `i`.
  inline const T& operator[](uint32_t i) const noexcept {
    ASMJIT_ASSERT(i < _length);
    return getData()[i];
  }

  inline T& getFirst() noexcept { return operator[](0); }
  inline const T& getFirst() const noexcept { return operator[](0); }

  inline T& getLast() noexcept { return operator[](_length - 1); }
  inline const T& getLast() const noexcept { return operator[](_length - 1); }

  // --------------------------------------------------------------------------
  // [Memory Management]
  // --------------------------------------------------------------------------

  //! Release the memory held by `ZoneVector<T>` back to the `allocator`.
  inline void release(ZoneAllocator* allocator) noexcept {
    _release(allocator, sizeof(T));
  }

  //! Called to grow the buffer to fit at least `n` elements more.
  inline Error grow(ZoneAllocator* allocator, uint32_t n) noexcept {
    return ZoneVectorBase::_grow(allocator, sizeof(T), n);
  }

  //! Resize the vector to hold `n` elements.
  //!
  //! If `n` is greater than the current length then the additional elements'
  //! content will be initialized to zero. If `n` is less than the current
  //! length then the vector will be truncated to exactly `n` elements.
  inline Error resize(ZoneAllocator* allocator, uint32_t n) noexcept {
    return ZoneVectorBase::_resize(allocator, sizeof(T), n);
  }

  //! Realloc internal array to fit at least `n` items.
  inline Error reserve(ZoneAllocator* allocator, uint32_t n) noexcept {
    return n > _capacity ? ZoneVectorBase::_reserve(allocator, sizeof(T), n) : Error(kErrorOk);
  }

  inline Error willGrow(ZoneAllocator* allocator, uint32_t n = 1) noexcept {
    return _capacity - _length < n ? grow(allocator, n) : Error(kErrorOk);
  }
};

// ============================================================================
// [asmjit::ZoneStackBase]
// ============================================================================

class ZoneStackBase {
public:
  enum Side : uint32_t {
    kSideLeft  = 0,
    kSideRight = 1
  };

  static constexpr uint32_t kBlockSize = ZoneAllocator::kHiMaxSize;

  struct Block {
    inline bool isEmpty() const noexcept { return _start == _end; }

    inline Block* getPrev() const noexcept { return _link[kSideLeft]; }
    inline Block* getNext() const noexcept { return _link[kSideRight]; }

    inline void setPrev(Block* block) noexcept { _link[kSideLeft] = block; }
    inline void setNext(Block* block) noexcept { _link[kSideRight] = block; }

    template<typename T>
    inline T* getStart() const noexcept { return static_cast<T*>(_start); }
    template<typename T>
    inline void setStart(T* start) noexcept { _start = static_cast<void*>(start); }

    template<typename T>
    inline T* getEnd() const noexcept { return (T*)_end; }
    template<typename T>
    inline void setEnd(T* end) noexcept { _end = (void*)end; }

    template<typename T>
    inline T* getData() const noexcept { return (T*)((uint8_t*)(this) + sizeof(Block)); }

    template<typename T>
    inline bool canPrepend() const noexcept { return _start > getData<void>(); }

    template<typename T>
    inline bool canAppend() const noexcept {
      size_t kNumBlockItems = (kBlockSize - sizeof(Block)) / sizeof(T);
      size_t kStartBlockIndex = sizeof(Block);
      size_t kEndBlockIndex = kStartBlockIndex + kNumBlockItems * sizeof(T);

      return (uintptr_t)_end <= ((uintptr_t)this + kEndBlockIndex - sizeof(T));
    }

    Block* _link[2];                     //!< Next and previous blocks.
    void* _start;                        //!< Pointer to the start of the array.
    void* _end;                          //!< Pointer to the end of the array.
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline ZoneStackBase() noexcept {
    _allocator = nullptr;
    _block[0] = nullptr;
    _block[1] = nullptr;
  }
  inline ~ZoneStackBase() noexcept { reset(); }

  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  inline bool isInitialized() const noexcept { return _allocator != nullptr; }
  ASMJIT_API Error _init(ZoneAllocator* allocator, size_t middleIndex) noexcept;
  inline Error reset() noexcept { return _init(nullptr, 0); }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get a `ZoneAllocator` attached to this container.
  inline ZoneAllocator* getAllocator() const noexcept { return _allocator; }

  inline bool isEmpty() const noexcept {
    ASMJIT_ASSERT(isInitialized());
    return _block[0]->getStart<void>() == _block[1]->getEnd<void>();
  }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  ASMJIT_API Error _prepareBlock(uint32_t side, size_t initialIndex) noexcept;
  ASMJIT_API void _cleanupBlock(uint32_t side, size_t middleIndex) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  ZoneAllocator* _allocator;             //!< Allocator used to allocate data.
  Block* _block[2];                      //!< First and last blocks.
};

// ============================================================================
// [asmjit::ZoneStack<T>]
// ============================================================================

template<typename T>
class ZoneStack : public ZoneStackBase {
public:
  enum : uint32_t {
    kNumBlockItems   = uint32_t((kBlockSize - sizeof(Block)) / sizeof(T)),
    kStartBlockIndex = uint32_t(sizeof(Block)),
    kMidBlockIndex   = uint32_t(kStartBlockIndex + (kNumBlockItems / 2) * sizeof(T)),
    kEndBlockIndex   = uint32_t(kStartBlockIndex + (kNumBlockItems    ) * sizeof(T))
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline ZoneStack() noexcept {}
  inline ~ZoneStack() noexcept {}

  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  inline Error init(ZoneAllocator* allocator) noexcept { return _init(allocator, kMidBlockIndex); }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  ASMJIT_FORCEINLINE Error prepend(T item) noexcept {
    ASMJIT_ASSERT(isInitialized());
    Block* block = _block[kSideLeft];

    if (!block->canPrepend<T>()) {
      ASMJIT_PROPAGATE(_prepareBlock(kSideLeft, kEndBlockIndex));
      block = _block[kSideLeft];
    }

    T* ptr = block->getStart<T>() - 1;
    ASMJIT_ASSERT(ptr >= block->getData<T>() && ptr <= block->getData<T>() + (kNumBlockItems - 1));
    *ptr = item;
    block->setStart<T>(ptr);
    return kErrorOk;
  }

  ASMJIT_FORCEINLINE Error append(T item) noexcept {
    ASMJIT_ASSERT(isInitialized());
    Block* block = _block[kSideRight];

    if (!block->canAppend<T>()) {
      ASMJIT_PROPAGATE(_prepareBlock(kSideRight, kStartBlockIndex));
      block = _block[kSideRight];
    }

    T* ptr = block->getEnd<T>();
    ASMJIT_ASSERT(ptr >= block->getData<T>() && ptr <= block->getData<T>() + (kNumBlockItems - 1));

    *ptr++ = item;
    block->setEnd(ptr);
    return kErrorOk;
  }

  ASMJIT_FORCEINLINE T popFirst() noexcept {
    ASMJIT_ASSERT(isInitialized());
    ASMJIT_ASSERT(!isEmpty());

    Block* block = _block[kSideLeft];
    ASMJIT_ASSERT(!block->isEmpty());

    T* ptr = block->getStart<T>();
    T item = *ptr++;

    block->setStart(ptr);
    if (block->isEmpty())
      _cleanupBlock(kSideLeft, kMidBlockIndex);

    return item;
  }

  ASMJIT_FORCEINLINE T pop() noexcept {
    ASMJIT_ASSERT(isInitialized());
    ASMJIT_ASSERT(!isEmpty());

    Block* block = _block[kSideRight];
    ASMJIT_ASSERT(!block->isEmpty());

    T* ptr = block->getEnd<T>();
    T item = *--ptr;
    ASMJIT_ASSERT(ptr >= block->getData<T>());
    ASMJIT_ASSERT(ptr >= block->getStart<T>());

    block->setEnd(ptr);
    if (block->isEmpty())
      _cleanupBlock(kSideRight, kMidBlockIndex);

    return item;
  }
};

// ============================================================================
// [asmjit::ZoneHashNode]
// ============================================================================

//! Node used by \ref ZoneHash<> template.
//!
//! You must provide function `bool eq(const Key& key)` in order to make
//! `ZoneHash::get()` working.
class ZoneHashNode {
public:
  inline ZoneHashNode(uint32_t hVal = 0) noexcept
    : _hashNext(nullptr),
      _hVal(hVal) {}

  ZoneHashNode* _hashNext;               //!< Next node in the chain, null if it terminates the chain.
  uint32_t _hVal;                        //!< Key hash value.
  uint32_t _customData;                  //!< Padding, can be reused by any Node that inherits `ZoneHashNode`.
};

// ============================================================================
// [asmjit::ZoneHashBase]
// ============================================================================

class ZoneHashBase {
public:
  ASMJIT_NONCOPYABLE(ZoneHashBase)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline ZoneHashBase() noexcept {
    _size = 0;
    _bucketsCount = 1;
    _bucketsGrow = 1;
    _data = _embedded;
    _embedded[0] = nullptr;
  }

  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  inline void reset() noexcept {
    _size = 0;
    _bucketsCount = 1;
    _bucketsGrow = 1;
    _data = _embedded;
    _embedded[0] = nullptr;
  }

  inline void release(ZoneAllocator* allocator) noexcept {
    ZoneHashNode** oldData = _data;
    if (oldData != _embedded)
      allocator->release(oldData, _bucketsCount * sizeof(ZoneHashNode*));
    reset();
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline size_t getSize() const noexcept { return _size; }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  ASMJIT_API void _rehash(ZoneAllocator* allocator, uint32_t newCount) noexcept;
  ASMJIT_API ZoneHashNode* _put(ZoneAllocator* allocator, ZoneHashNode* node) noexcept;
  ASMJIT_API ZoneHashNode* _del(ZoneAllocator* allocator, ZoneHashNode* node) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  size_t _size;                          //!< Count of records inserted into the hash table.
  uint32_t _bucketsCount;                //!< Count of hash buckets.
  uint32_t _bucketsGrow;                 //!< When buckets array should grow.

  ZoneHashNode** _data;                  //!< Buckets data.
  ZoneHashNode* _embedded[1];            //!< Embedded data, used by empty hash tables.
};

// ============================================================================
// [asmjit::ZoneHash<Key, Node>]
// ============================================================================

//! Low-level hash table specialized for storing string keys and POD values.
//!
//! This hash table allows duplicates to be inserted (the API is so low
//! level that it's up to you if you allow it or not, as you should first
//! `get()` the node and then modify it or insert a new node by using `put()`,
//! depending on the intention).
template<typename Node>
class ZoneHash : public ZoneHashBase {
public:
  inline ZoneHash() noexcept : ZoneHashBase() {}

  template<typename Key>
  inline Node* get(const Key& key) const noexcept {
    uint32_t hMod = key.hVal % _bucketsCount;
    Node* node = static_cast<Node*>(_data[hMod]);

    while (node && !key.matches(node))
      node = static_cast<Node*>(node->_hashNext);
    return node;
  }

  inline Node* put(ZoneAllocator* allocator, Node* node) noexcept { return static_cast<Node*>(_put(allocator, node)); }
  inline Node* del(ZoneAllocator* allocator, Node* node) noexcept { return static_cast<Node*>(_del(allocator, node)); }
};

//! \}

ASMJIT_END_NAMESPACE

// [Guard]
#endif // _ASMJIT_CORE_ZONE_H
