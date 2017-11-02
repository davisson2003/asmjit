// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_CORE_JITALLOCATOR_H
#define _ASMJIT_CORE_JITALLOCATOR_H

#include "../core/build.h"
#ifndef ASMJIT_DISABLE_JIT

// [Dependencies]
#include "../core/globals.h"
#include "../core/osutils.h"
#include "../core/zonerbtree.h"

ASMJIT_BEGIN_NAMESPACE

//! \addtogroup asmjit_core
//! \{

// ============================================================================
// [asmjit::JitAllocator]
// ============================================================================

//! Reference implementation of memory manager that uses `JitUtils::virtualAlloc()`
//! and `JitUtils::virtualRelease()` to manage executable memory.
//!
//! Internally the allocator doesn't store any information into the executable
//! memory it allocates, it uses bit vectors to mark allocated memory and RB
//! tree to be able to find block for any address.
class JitAllocator {
public:
  ASMJIT_NONCOPYABLE(JitAllocator)

  enum Flags : uint32_t {
    kFlagSecure = 0x80000000U            //!< Always clean non-occupied memory.
  };

  //! Number of slots that contain blocks.
  //!
  //! Each slot increases granularity twice to make memory management more
  //! efficient. Ideal number of slots appears to be `3` as it distributes
  //! small and large functions properly.
  static constexpr uint32_t kSlotCount = 3;
  //! Minimum granularity (and the default granularity for slot #0).
  static constexpr uint32_t kMinimumGranularity = 64;

  //! Minimum block size.
  static constexpr uint32_t kMinBlockSize = 65536;   // 64kB.
  //! Maximum block size.
  static constexpr uint32_t kMaxBlockSize = 4194304; //  4MB.

  struct Slot;

  class Block : public ZoneRBNode {
  public:
    inline Block(Slot* slot, uint8_t* virtMem, size_t blockSize) noexcept
      : ZoneRBNode(),
        slot(slot),
        prev(nullptr),
        next(nullptr),
        mem(virtMem),
        blockSize(blockSize),
        usedSize(0),
        largestBlock(0),
        bvLength(0),
        bvOccupied(nullptr),
        bvSentinel(nullptr) {}

    // Get available space.
    inline size_t getAvailable() const noexcept { return blockSize - usedSize; }
    inline bool lt(const Block* other) const noexcept { return mem < other->mem; }

    inline int cmpKey(const uint8_t* key) const noexcept {
      return key >= mem ? int(key >= mem + blockSize) : -1;
    }

    Slot* slot;                          //!< Link to the slot that owns this block.
    Block* prev;                         //!< Prev node in list.
    Block* next;                         //!< Next node in list.
    uint8_t* mem;                        //!< Virtual memory address.

    size_t blockSize;                    //!< Number of bytes this block represents (size of the block).
    size_t usedSize;                     //!< Number of occupied bytes in this block.
    size_t largestBlock;                 //!< Contains largest block that can be allocated.
    size_t bvLength;                     //!< Count of bits in each bit-vector (`bvOccupied` and `bvSentinel`).

    Globals::BitWord* bvOccupied;        //!< Occupied bits (0 = unused        , 1 = used).
    Globals::BitWord* bvSentinel;        //!< Sentinel bits (0 = unused || used, 1 = used && sentinel).
  };

  struct Slot {
    inline void resetBlocks() noexcept {
      _first = nullptr;
      _last = nullptr;
      _optimal = nullptr;
      _blockCount = 0;
    }

    inline void resetGranularity(uint32_t granularity) noexcept {
      ASMJIT_ASSERT(granularity < 65536U);
      _granularity = uint16_t(granularity);
      _granularityLog2 = uint8_t(IntUtils::ctz(granularity));
    }

    inline size_t byteSizeToBitLength(size_t size) const noexcept {
      return (size + _granularity - 1) >> _granularityLog2;
    }

    inline size_t bitLengthToByteSize(size_t bitLength) const noexcept {
      return bitLength * _granularity;
    }

    Block* _first;                       //!< First node in node-list.
    Block* _last;                        //!< Last node in node-list.
    Block* _optimal;                     //!< Where to start look first.

    uint32_t _blockCount;                //!< Count of blocks.
    uint16_t _granularity;               //!< Allocation granularity.
    uint8_t _granularityLog2;            //!< Log2(_granularity).
    uint8_t _reserved;
  };

  struct Statistics {
    inline void reset() noexcept {
      _usedSize = 0;
      _reservedSize = 0;
      _overheadSize = 0;
    }

    //! Get how many bytes are currently used.
    inline size_t getUsedSize() const noexcept { return _usedSize; }
    //! Total number of bytes bytes reserved by the allocator (sum of sizes of all blocks).
    inline size_t getReservedSize() const noexcept { return _reservedSize; }
    //! Number of bytes the allocator needs to manage the allocated memory.
    inline size_t getOverheadSize() const noexcept { return _overheadSize; }

    inline double getUsedAsPercent() const noexcept {
      return 100.0 * double(_usedSize) / (double(_reservedSize) + 1e-16);
    }

    inline double getOverheadAsPercent() const noexcept {
      return 100.0 * double(_overheadSize) / (double(_reservedSize) + 1e-16);
    }

    size_t _usedSize;                    //!< How many bytes are currently used / allocated.
    size_t _reservedSize;                //!< How many bytes are currently reserved by the allocator.
    size_t _overheadSize;                //!< Allocation overhead (in bytes) required to maintain all blocks.
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create a `JitAllocator` instance.
  ASMJIT_API JitAllocator() noexcept;
  //! Destroy the `JitAllocator` instance and free all blocks.
  ASMJIT_API ~JitAllocator() noexcept;

  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  //! Free all allocated memory - makes all pointers returned by `alloc()` invalid.
  ASMJIT_API void reset() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get allocation flags, see \ref Flags.
  inline uint32_t getFlags() const noexcept { return _flags; }

  //! Get a page size (smallest possible allocable chunk of virtual memory).
  inline uint32_t getPageSize() const noexcept { return _pageSize; }
  //! Get a minimum block size (a minimum size of block that the allocator would allocate).
  inline uint32_t getBlockSize() const noexcept { return _blockSize; }

  //! Get allocation statistics.
  ASMJIT_API Statistics getStatistics() const noexcept;

  // --------------------------------------------------------------------------
  // [Alloc / Release]
  // --------------------------------------------------------------------------

  //! Allocate a `size` bytes of virtual memory.
  ASMJIT_API void* alloc(size_t size) noexcept;
  //! Free previously allocated memory at a given `address`.
  ASMJIT_API Error release(void* p) noexcept;
  //! Free extra memory allocated with `p`.
  ASMJIT_API Error shrink(void* p, size_t used) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _flags;                       //!< Allocator flags, see \ref Flags.
  uint32_t _pageSize;                    //!< System page size (also a minimum block size).
  uint32_t _blockSize;                   //!< Default block size.
  uint32_t _fillPattern;                 //!< A pattern that is used to fill unused memory if secure mode is enabled.

  mutable Lock _lock;                    //!< Lock for thread safety.
  ZoneRBTree<Block> _tree;                   //!< Block tree.
  Statistics _statistics;                //!< Allocator statistics.
  Slot _slots[kSlotCount];               //!< Allocator slots.
};

//! \}

ASMJIT_END_NAMESPACE

#endif
#endif
