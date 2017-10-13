// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_BASE_VIRTMEM_H
#define _ASMJIT_BASE_VIRTMEM_H

// [Dependencies]
#include "../base/globals.h"
#include "../base/osutils.h"

// [Api-Begin]
#include "../asmjit_apibegin.h"

namespace asmjit {

//! \addtogroup asmjit_base
//! \{

// ============================================================================
// [asmjit::VirtMem]
// ============================================================================

namespace VirtMem {
  //! Virtual memory flags.
  enum AccessFlags : uint32_t {
    kAccessNone      = 0x00000000U,      //!< No access flags.
    kAccessWrite     = 0x00000001U,      //!< Memory is writable.
    kAccessExecute   = 0x00000002U       //!< Memory is executable.
  };

  //! Information about OS virtual memory.
  struct Info {
    uint32_t pageSize;                   //!< Virtual memory page size.
    uint32_t pageGranularity;            //!< Virtual memory page granularity.
  };

  //! Get virtual memory information, see \ref VirtMem::Info for more details.
  ASMJIT_API Info getInfo() noexcept;

  //! Allocate virtual memory.
  ASMJIT_API void* alloc(size_t size, uint32_t accessFlags) noexcept;
  //! Release virtual memory previously allocated by `VirtMem::alloc()`.
  ASMJIT_API Error release(void* p, size_t size) noexcept;
}

// ============================================================================
// [asmjit::VirtMemManager]
// ============================================================================

//! Reference implementation of memory manager that uses `VirtMemUtils` to allocate
//! chunks of virtual memory and uses bit arrays to manage it.
class VirtMemManager {
public:
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create a `VirtMemManager` instance.
  ASMJIT_API VirtMemManager() noexcept;
  //! Destroy the `VirtMemManager` instance and free all blocks.
  ASMJIT_API ~VirtMemManager() noexcept;

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

} // asmjit namespace

// [Api-End]
#include "../asmjit_apiend.h"

// [Guard]
#endif // _ASMJIT_BASE_VIRTMEM_H
