// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// This file contains implementation of virtual memory management for AsmJit
// library. There are several goals I decided to write implementation myself:
//
// - Granularity of allocated blocks is different than granularity for a typical
//   C malloc. It is at least 64-bytes so CodeEmitter can guarantee the alignment
//   up to 64 bytes, which is the size of a cache-line and it's also required by
//   AVX-512 aligned loads and stores. Alignment requirements can grow in the future,
//   but at the moment 64 bytes is safe (we may jump to 128 bytes if necessary or
//   make it configurable).
//
// - Keep memory manager information outside of the allocated virtual memory
//   pages, because these pages allow machine code execution and there should
//   be not data required to keep track of these blocks. Another reason is that
//   some environments (i.e. iOS) allow to generate and run JIT code, but this
//   code has to be set to [Executable AND NOT Writable].
//
// - Keep implementation simple and easy to follow.
//
// Implementation is based on bit arrays and binary trees. Bit arrays contain
// information related to allocated and unused blocks of memory. The size of
// a block is described by `Block::granularity`. Count of blocks is stored in
// `Block::blocks`. For example if granularity is 64 and count of blocks is 20,
// memory node contains 64*20 bytes of memory and the smallest possible allocation
// (and also alignment) is 64 bytes. So granularity is also related to memory
// alignment. Binary trees (RB) are used to enable fast lookup into all addresses
// allocated by memory manager instance. This is used mainly by `VMemPrivate::release()`.
//
// Bit array looks like this (empty = unused, X = used) - Size of block 64:
//
//   -------------------------------------------------------------------------
//   | |X|X| | | | | |X|X|X|X|X|X| | | | | | | | | | | | | | |X| | |X|X|X| | |
//   -------------------------------------------------------------------------
//                                (Maximum unused cont. block)
//
// These bits show that there are 12 allocated blocks (X) of 64 bytes, so total
// size allocated is 768 bytes. Maximum count of continuous memory is 12 * 64.

// [Export]
#define ASMJIT_EXPORTS

#include "../core/build.h"
#ifndef ASMJIT_DISABLE_JIT

// [Dependencies]
#include "../core/arch.h"
#include "../core/intutils.h"
#include "../core/jitallocator.h"
#include "../core/jitutils.h"

#if defined(ASMJIT_BUILD_TEST)
  #include <random>
#endif

ASMJIT_BEGIN_NAMESPACE

// ============================================================================
// [asmjit::JitAllocator::TypeDefs]
// ============================================================================

typedef JitAllocator::Block Block;

// ============================================================================
// [asmjit::JitAllocator - Helpers]
// ============================================================================

static inline uint32_t JitAllocator_getDefaultFillPattern() noexcept {
  // X86 and X86_64 - 4x 'int3' instruction.
  if (ASMJIT_ARCH_X86)
    return 0xCCCCCCCCU;

  // Unknown...
  return 0U;
}

static inline size_t JitAllocator_sizeToSlotIndex(const JitAllocator* self, size_t size) noexcept {
  ASMJIT_UNUSED(self);

  size_t slotIndex = size_t(JitAllocator::kSlotCount - 1);
  size_t granularity = size_t(JitAllocator::kMinimumGranularity) << slotIndex;

  while (slotIndex) {
    if (IntUtils::alignUp(size, granularity) == size)
      break;
    slotIndex--;
    granularity >>= 1;
  }
  return slotIndex;
}

static inline size_t JitAllocator_bitVectorLengthToByteSize(size_t bitLength) noexcept {
  return ((bitLength + Globals::kBitWordSize - 1U) / Globals::kBitWordSize) * sizeof(Globals::BitWord);
}

static size_t JitAllocator_calculateIdealBlockSize(JitAllocator::Slot* slot, size_t allocationSize) noexcept {
  uint32_t kMaxSizeShift = IntUtils::staticCtz<JitAllocator::kMaxBlockSize>() -
                           IntUtils::staticCtz<JitAllocator::kMinBlockSize>() ;

  size_t blockSize = JitAllocator::kMinBlockSize << std::min<uint32_t>(kMaxSizeShift, slot->_blockCount);
  if (blockSize < allocationSize)
    blockSize = IntUtils::alignUp(allocationSize, blockSize);

  return blockSize;
}

// Allocate a new `JitAllocator::Block` for the given `blockSize`.
static Block* JitAllocator_newBlock(JitAllocator::Slot* slot, size_t blockSize) noexcept {
  typedef Globals::BitWord BitWord;

  size_t bitLength = (blockSize + slot->_granularity - 1) >> slot->_granularityLog2;
  size_t bitWordCount = (bitLength + Globals::kBitWordSize - 1U) / Globals::kBitWordSize;

  Block* block = static_cast<Block*>(MemUtils::alloc(sizeof(Block)));
  BitWord* bitWords = static_cast<BitWord*>(MemUtils::alloc(bitWordCount * 2 * sizeof(BitWord)));
  uint8_t* virtMem = static_cast<uint8_t*>(JitUtils::virtualAlloc(blockSize, JitUtils::kVirtMemWriteExecute));

  // Out of memory.
  if (ASMJIT_UNLIKELY(!block || !bitWords || !virtMem)) {
    if (virtMem) JitUtils::virtualRelease(virtMem, blockSize);
    if (bitWords) MemUtils::release(bitWords);
    if (block) MemUtils::release(block);
    return nullptr;
  }

  std::memset(bitWords, 0, bitWordCount * 2 * sizeof(BitWord));

  // Initialize Block data.
  block = new(block) Block(slot, virtMem, blockSize);
  block->blockSize = blockSize;
  block->largestBlock = blockSize;
  block->bvLength = bitLength;
  block->bvOccupied = bitWords;
  block->bvSentinel = bitWords + bitWordCount;
  return block;
}

static void JitAllocator_deleteBlock(JitAllocator* self, JitAllocator::Block* block) noexcept {
  ASMJIT_UNUSED(self);

  JitUtils::virtualRelease(block->mem, block->blockSize);
  MemUtils::release(block->bvOccupied);
  MemUtils::release(block);
}

static void JitAllocator_insertBlock(JitAllocator* self, JitAllocator::Block* block) noexcept {
  JitAllocator::Slot* slot = block->slot;

  // Add to RBTree.
  self->_tree.insert(block);

  // Add to LinkedList.
  block->prev = slot->_last;

  if (!slot->_first) {
    slot->_first = block;
    slot->_last = block;
    slot->_optimal = block;
  }
  else {
    block->prev = slot->_last;
    slot->_last->next = block;
    slot->_last = block;
  }

  // Update statistics.
  self->_statistics._reservedSize += block->blockSize;
  self->_statistics._overheadSize += sizeof(Block) + JitAllocator_bitVectorLengthToByteSize(block->bvLength) * 2U;
  slot->_blockCount++;
}

static void JitAllocator_removeBlock(JitAllocator* self, JitAllocator::Block* block) noexcept {
  JitAllocator::Slot* slot = block->slot;

  // Remove from RBTree
  self->_tree.remove(block);

  // Remove from LinkedList.
  Block* next = block->next;
  Block* prev = block->prev;

  if (prev) { prev->next = next; } else { slot->_first = next; }
  if (next) { next->prev = prev; } else { slot->_last  = prev; }

  if (slot->_optimal == block)
    slot->_optimal = prev ? prev : next;

  // Update statistics.
  self->_statistics._reservedSize -= block->blockSize;
  self->_statistics._overheadSize -= sizeof(Block) + JitAllocator_bitVectorLengthToByteSize(block->bvLength) * 2U;
  slot->_blockCount--;
}

// ============================================================================
// [asmjit::JitAllocator - Construction / Destruction]
// ============================================================================

JitAllocator::JitAllocator() noexcept {
  JitUtils::MemInfo memInfo = JitUtils::getMemInfo();

  _flags = 0;
  _pageSize = memInfo.pageSize;
  _blockSize = memInfo.pageGranularity;
  _fillPattern = JitAllocator_getDefaultFillPattern();
  _statistics.reset();

  for (size_t i = 0; i < kSlotCount; i++) {
    _slots[i].resetBlocks();
    _slots[i].resetGranularity(uint32_t(kMinimumGranularity) << i);
  }
}

JitAllocator::~JitAllocator() noexcept {
  reset();
}

// ============================================================================
// [asmjit::JitAllocator - Reset]
// ============================================================================

void JitAllocator::reset() noexcept {
  for (size_t i = 0; i < kSlotCount; i++) {
    Slot* slot = &_slots[i];
    Block* block = slot->_first;

    while (block) {
      Block* next = block->next;
      JitAllocator_deleteBlock(this, block);
      block = next;
    }

    slot->resetBlocks();
  }

  _tree.reset();
  _statistics.reset();
}

// ============================================================================
// [asmjit::JitAllocator - Statistics]
// ============================================================================

JitAllocator::Statistics JitAllocator::getStatistics() const noexcept {
  ScopedLock locked(_lock);
  return _statistics;
}

// ============================================================================
// [asmjit::JitAllocator - Alloc / Release]
// ============================================================================

void* JitAllocator::alloc(size_t size) noexcept {
  // Align to byte-granularity by default.
  size = IntUtils::alignUp<size_t>(size, kMinimumGranularity);
  if (ASMJIT_UNLIKELY(!size))
    return nullptr;

  ScopedLock locked(_lock);

  Slot* slot = &_slots[JitAllocator_sizeToSlotIndex(this, size)];
  Block* block = slot->_optimal;
  size_t minVSize = _blockSize;

  size_t i;
  size_t need = slot->byteSizeToBitLength(size);

  // Try to find memory block in existing nodes.
  while (block) {
    // Skip this block?
    if ((block->getAvailable() < size) || (block->largestBlock < size && block->largestBlock != 0)) {
      Block* next = block->next;

      if (block->getAvailable() < minVSize && block == slot->_optimal && next)
        slot->_optimal = next;

      block = next;
      continue;
    }

    Globals::BitWord* up = block->bvOccupied; // Current ubits address.
    Globals::BitWord ubits;                   // Current ubits[0] value.
    Globals::BitWord bit;                     // Current bit mask.

    size_t bvLength = block->bvLength;        // BitVector length.
    size_t cont = 0;                          // How many bits are currently freed in find loop.
    size_t maxCont = 0;                       // Largest continuous block (bits count).
    size_t j;

    i = 0;

    // Try to find block that is large enough.
    while (i < bvLength) {
      ubits = *up++;

      // Fast skip used blocks.
      if (ubits == ~Globals::BitWord(0)) {
        if (cont > maxCont)
          maxCont = cont;
        cont = 0;

        i += Globals::kBitWordSize;
        continue;
      }

      size_t max = Globals::kBitWordSize;
      if (i + max > bvLength)
        max = bvLength - i;

      for (j = 0, bit = 1; j < max; bit <<= 1) {
        j++;
        if ((ubits & bit) == 0) {
          if (++cont == need) {
            i += j;
            i -= cont;
            goto Found;
          }

          continue;
        }

        if (cont > maxCont) maxCont = cont;
        cont = 0;
      }

      i += Globals::kBitWordSize;
    }

    // Because we have traversed the entire block, we can set largest
    // block size that will be used to cache next traversing.
    block->largestBlock = maxCont * slot->_granularity;

    block = block->next;
  }

  // If we are here, we failed to find existing memory block and we must
  // allocate a new one.
  {
    size_t blockSize = JitAllocator_calculateIdealBlockSize(slot, size);
    block = JitAllocator_newBlock(slot, blockSize);

    if (ASMJIT_UNLIKELY(!block))
      return nullptr;

    JitAllocator_insertBlock(this, block);
    i = 0; // Alloc first block at the start.
  }

Found:
  // Mark the newly allocated space as occupied and the end-of-region sentinel.
  IntUtils::bitVectorFill(block->bvOccupied, i, need);
  IntUtils::bitVectorSetBit(block->bvSentinel, i + need - 1, true);

  // Update statistics.
  {
    size_t usedSize = slot->bitLengthToByteSize(need);
    block->usedSize += usedSize;
    block->largestBlock = 0;
    _statistics._usedSize += usedSize;
  }

  // And return pointer to allocated memory.
  uint8_t* result = block->mem + slot->bitLengthToByteSize(i);
  ASMJIT_ASSERT(result >= block->mem && result <= block->mem + block->blockSize - size);
  return result;
}

Error JitAllocator::release(void* p) noexcept {
  if (ASMJIT_UNLIKELY(!p))
    return DebugUtils::errored(kErrorInvalidArgument);

  ScopedLock locked(_lock);
  Block* block = _tree.get(static_cast<uint8_t*>(p));

  if (ASMJIT_UNLIKELY(!block))
    return DebugUtils::errored(kErrorInvalidState);

  // Offset relative to the start of the block.
  Slot* slot = block->slot;
  size_t offset = (size_t)((uint8_t*)p - (uint8_t*)block->mem);

  // Bit-index is the first bit representing the allocated region and `bitLength` represents its size.
  size_t bitIndex = offset >> slot->_granularityLog2;
  size_t bitLength = IntUtils::bitVectorIndexOf(block->bvSentinel, bitIndex, true) + 1 - bitIndex;

  // This is the allocated size of `p`.
  size_t allocatedSize = slot->bitLengthToByteSize(bitLength);

  // Clear all occupied bits and also the end-of-region sentinel.
  IntUtils::bitVectorClear(block->bvOccupied, bitIndex, bitLength);
  IntUtils::bitVectorSetBit(block->bvSentinel, bitIndex + bitLength - 1, false);

  // If the freed block is a fully allocated block then it's needed to
  // update the 'optimal' pointer of the slot.
  if (block->usedSize == block->blockSize) {
    Block* cur = slot->_optimal;
    do {
      cur = cur->prev;
      if (cur == block) {
        slot->_optimal = block;
        break;
      }
    } while (cur);
  }

  // Update statistics.
  block->usedSize -= allocatedSize;
  block->largestBlock = std::max(block->largestBlock, allocatedSize);
  _statistics._usedSize -= allocatedSize;

  // Delete the block if it became empty.
  if (block->usedSize == 0) {
    JitAllocator_removeBlock(this, block);
    JitAllocator_deleteBlock(this, block);
  }

  return kErrorOk;
}

Error JitAllocator::shrink(void* p, size_t newSize) noexcept {
  if (ASMJIT_UNLIKELY(!p))
    return DebugUtils::errored(kErrorInvalidArgument);

  if (ASMJIT_UNLIKELY(newSize == 0))
    return release(p);

  ScopedLock locked(_lock);
  Block* node = _tree.get(static_cast<uint8_t*>(p));

  if (ASMJIT_UNLIKELY(!node))
    return DebugUtils::errored(kErrorInvalidState);

  // Offset relative to the start of the block.
  Slot* slot = node->slot;
  size_t offset = (size_t)((uint8_t*)p - (uint8_t*)node->mem);

  // Bit-index is the first bit representing the allocated region and `bitLength` represents its size.
  size_t bitIndex = offset >> slot->_granularityLog2;
  size_t oldBitLength = IntUtils::bitVectorIndexOf(node->bvSentinel, bitIndex, true) + 1 - bitIndex;
  size_t newBitLength = slot->byteSizeToBitLength(newSize);

  if (ASMJIT_UNLIKELY(newBitLength > oldBitLength))
    return DebugUtils::errored(kErrorInvalidState);

  size_t bitLengthDiff = newBitLength - oldBitLength;
  if (!bitLengthDiff)
    return kErrorOk;

  // Mark the released space as not occupied and move the sentinel.
  IntUtils::bitVectorClear(node->bvOccupied, bitIndex + newBitLength, bitLengthDiff);
  IntUtils::bitVectorSetBit(node->bvSentinel, bitIndex + oldBitLength - 1, false);
  IntUtils::bitVectorSetBit(node->bvSentinel, bitIndex + newBitLength - 1, true);

  // Update statistics.
  size_t sizeDiff = slot->bitLengthToByteSize(bitLengthDiff);
  node->usedSize -= sizeDiff;
  node->largestBlock = std::max(node->largestBlock, sizeDiff);
  _statistics._usedSize -= sizeDiff;

  return kErrorOk;
}

// ============================================================================
// [asmjit::JitAllocator - Unit]
// ============================================================================

#if defined(ASMJIT_BUILD_TEST)
static void JitAllocatorTest_fill(void* a, void* b, int i, int pattern) noexcept {
  *(int *)a = i;
  *(int *)b = i;
  std::memset((char*)a + sizeof(int), pattern, unsigned(i) - sizeof(int));
  std::memset((char*)b + sizeof(int), pattern, unsigned(i) - sizeof(int));
}

static void JitAllocatorTest_verify(void* a, void* b) noexcept {
  int ai = *(int*)a;
  int bi = *(int*)b;

  EXPECT(ai == bi, "The length of 'a' (%d) and 'b' (%d) should be same", ai, bi);
  EXPECT(std::memcmp(a, b, size_t(ai)) == 0, "Pattern (%p) doesn't match", a);
}

static void JitAllocatorTest_stats(JitAllocator& allocator) noexcept {
  JitAllocator::Statistics stats = allocator.getStatistics();
  INFO("Reserved (VirtMem): %9llu [Bytes]"         , (unsigned long long)(stats.getReservedSize()));
  INFO("Used     (VirtMem): %9llu [Bytes] (%.1f%%)", (unsigned long long)(stats.getUsedSize()), stats.getUsedAsPercent());
  INFO("Overhead (HeapMem): %9llu [Bytes] (%.1f%%)", (unsigned long long)(stats.getOverheadSize()), stats.getOverheadAsPercent());
}

static void JitAllocatorTest_shuffle(void** a, void** b, size_t count, std::mt19937& prng) noexcept {
  for (size_t i = 0; i < count; ++i) {
    size_t si = size_t(prng() % count);

    void* ta = a[i];
    void* tb = b[i];

    a[i] = a[si];
    b[i] = b[si];

    a[si] = ta;
    b[si] = tb;
  }
}

UNIT(core_jitallocator) {
  JitAllocator allocator;
  std::mt19937 prng(100);

  int i;
  int kCount = 200000;

  INFO("Memory alloc/free test - %d allocations", kCount);

  void** a = (void**)MemUtils::alloc(sizeof(void*) * size_t(kCount));
  void** b = (void**)MemUtils::alloc(sizeof(void*) * size_t(kCount));
  EXPECT(a != nullptr && b != nullptr, "Couldn't allocate %u bytes on heap", kCount * 2);

  INFO("Allocating virtual memory...");
  for (i = 0; i < kCount; i++) {
    size_t r = (prng() % 1000) + 4;

    a[i] = allocator.alloc(r);
    EXPECT(a[i] != nullptr, "Couldn't allocate %d bytes of virtual memory", r);
    std::memset(a[i], 0, r);
  }
  JitAllocatorTest_stats(allocator);

  INFO("Freeing virtual memory...");
  for (i = 0; i < kCount; i++) {
    EXPECT(allocator.release(a[i]) == kErrorOk, "Failed to free %p", a[i]);
  }
  JitAllocatorTest_stats(allocator);

  INFO("Verified alloc/free test - %d allocations", kCount);
  for (i = 0; i < kCount; i++) {
    size_t r = (prng() % 1000) + 4;

    a[i] = allocator.alloc(r);
    EXPECT(a[i] != nullptr, "Couldn't allocate %d bytes of virtual memory", r);

    b[i] = MemUtils::alloc(r);
    EXPECT(b[i] != nullptr, "Couldn't allocate %d bytes on heap", r);

    JitAllocatorTest_fill(a[i], b[i], int(r), int(r % 256));
  }
  JitAllocatorTest_stats(allocator);

  INFO("Shuffling...");
  JitAllocatorTest_shuffle(a, b, unsigned(kCount), prng);

  INFO("Verify and free...");
  for (i = 0; i < kCount / 2; i++) {
    JitAllocatorTest_verify(a[i], b[i]);
    EXPECT(allocator.release(a[i]) == kErrorOk, "Failed to free %p", a[i]);
    MemUtils::release(b[i]);
  }
  JitAllocatorTest_stats(allocator);

  INFO("Alloc again");
  for (i = 0; i < kCount / 2; i++) {
    size_t r = (prng() % 1000) + 4;

    a[i] = allocator.alloc(r);
    EXPECT(a[i] != nullptr, "Couldn't allocate %d bytes of virtual memory", r);

    b[i] = MemUtils::alloc(r);
    EXPECT(b[i] != nullptr, "Couldn't allocate %d bytes on heap");

    JitAllocatorTest_fill(a[i], b[i], int(r), int(r % 256));
  }
  JitAllocatorTest_stats(allocator);

  INFO("Verify and free...");
  for (i = 0; i < kCount; i++) {
    JitAllocatorTest_verify(a[i], b[i]);
    EXPECT(allocator.release(a[i]) == kErrorOk, "Failed to free %p", a[i]);
    MemUtils::release(b[i]);
  }
  JitAllocatorTest_stats(allocator);

  MemUtils::release(a);
  MemUtils::release(b);
}
#endif

ASMJIT_END_NAMESPACE

#endif
