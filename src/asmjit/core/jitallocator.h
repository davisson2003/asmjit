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

  //! Free all allocated memory.
  ASMJIT_API void reset() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get a page size (smallest possible allocable chunk of virtual memory).
  inline uint32_t getPageSize() const noexcept { return _pageSize; }
  inline uint32_t getBlockSize() const noexcept { return _blockSize; }
  inline uint32_t getBlockDensity() const noexcept { return _blockDensity; }

  //! Get how many bytes are currently used.
  inline size_t getUsedBytes() const noexcept { return _usedBytes; }
  //! Get how many bytes are currently allocated.
  inline size_t getAllocatedBytes() const noexcept { return _allocatedBytes; }

  // --------------------------------------------------------------------------
  // [Alloc / Release]
  // --------------------------------------------------------------------------

  //! Allocate a `size` bytes of virtual memory.
  //!
  //! Note that if you are implementing your own virtual memory manager then you
  //! can quitly ignore type of allocation. This is mainly for AsmJit to memory
  //! manager that allocated memory will be never freed.
  ASMJIT_API void* alloc(size_t size) noexcept;
  //! Free previously allocated memory at a given `address`.
  ASMJIT_API Error release(void* p) noexcept;
  //! Free extra memory allocated with `p`.
  ASMJIT_API Error shrink(void* p, size_t used) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  // Internal types.
  struct RBNode;
  struct MemNode;

  Lock _lock;                            //!< Lock for thread safety.
  uint32_t _pageSize;                    //!< Page size.
  uint32_t _blockSize;                   //!< Default block size.
  uint32_t _blockDensity;                //!< Default block density.
  size_t _usedBytes;                     //!< How many bytes are currently used.
  size_t _allocatedBytes;                //!< How many bytes are currently allocated.

  MemNode* _root;                        //!< RBTree root node.
  MemNode* _first;                       //!< First node in node-list.
  MemNode* _last;                        //!< Last node in node-list.
  MemNode* _optimal;                     //!< Where to start look first.
};

//! \}

ASMJIT_END_NAMESPACE

#endif
#endif
