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
// a block is described by `MemNode::density`. Count of blocks is stored in
// `MemNode::blocks`. For example if density is 64 and count of blocks is 20,
// memory node contains 64*20 bytes of memory and the smallest possible allocation
// (and also alignment) is 64 bytes. So density is also related to memory
// alignment. Binary trees (RB) are used to enable fast lookup into all addresses
// allocated by memory manager instance. This is used mainly by `VMemPrivate::release()`.
//
// Bit array looks like this (empty = unused, X = used) - Size of block 64:
//
//   -------------------------------------------------------------------------
//   | |X|X| | | | | |X|X|X|X|X|X| | | | | | | | | | | | |X| | | | |X|X|X| | |
//   -------------------------------------------------------------------------
//                               (Maximum continuous block)
//
// These bits show that there are 12 allocated blocks (X) of 64 bytes, so total
// size allocated is 768 bytes. Maximum count of continuous memory is 12 * 64.

// [Export]
#define ASMJIT_EXPORTS

// [Dependencies]
#include "../base/intutils.h"
#include "../base/osutils.h"
#include "../base/virtmem.h"

#if ASMJIT_OS_POSIX
# include <sys/types.h>
# include <sys/mman.h>
# include <unistd.h>
#endif

// [Api-Begin]
#include "../asmjit_apibegin.h"

namespace asmjit {

// ============================================================================
// [asmjit::VirtMem]
// ============================================================================

// Windows specific implementation using `VirtualAlloc` and `VirtualFree`.
#if ASMJIT_OS_WINDOWS
VirtMem::Info VirtMem::getInfo() noexcept {
  VirtMem::Info vmInfo;
  SYSTEM_INFO systemInfo;

  ::GetSystemInfo(&systemInfo);
  vmInfo.pageSize = IntUtils::alignUpPowerOf2<uint32_t>(systemInfo.dwPageSize);
  vmInfo.pageGranularity = systemInfo.dwAllocationGranularity;

  return vmInfo;
}

void* VirtMem::alloc(size_t size, uint32_t flags) noexcept {
  if (size == 0)
    return nullptr;

  // Windows XP-SP2, Vista and newer support data-execution-prevention (DEP).
  DWORD protectFlags = 0;
  if (flags & kAccessExecute)
    protectFlags |= (flags & kAccessWrite) ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
  else
    protectFlags |= (flags & kAccessWrite) ? PAGE_READWRITE : PAGE_READONLY;

  return ::VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protectFlags);
}

Error VirtMem::release(void* p, size_t size) noexcept {
  if (ASMJIT_UNLIKELY(!::VirtualFree(p, 0, MEM_RELEASE)))
    return DebugUtils::errored(kErrorInvalidState);

  return kErrorOk;
}
#endif

// Posix specific implementation using `mmap()` and `munmap()`.
#if ASMJIT_OS_POSIX

// Mac uses MAP_ANON instead of MAP_ANONYMOUS.
#if !defined(MAP_ANONYMOUS)
# define MAP_ANONYMOUS MAP_ANON
#endif

VirtMem::Info VirtMem::getInfo() noexcept {
  VirtMem::Info vmInfo;

  uint32_t pageSize = uint32_t(::getpagesize());
  vmInfo.pageSize = pageSize;
  vmInfo.pageGranularity = std::max<uint32_t>(pageSize, 65536);

  return vmInfo;
}

void* VirtMem::alloc(size_t size, uint32_t flags) noexcept {
  int protection = PROT_READ;
  if (flags & kAccessWrite  ) protection |= PROT_WRITE;
  if (flags & kAccessExecute) protection |= PROT_EXEC;

  void* mbase = ::mmap(nullptr, size, protection, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ASMJIT_UNLIKELY(mbase == MAP_FAILED)) return nullptr;

  return mbase;
}

Error VirtMem::release(void* p, size_t size) noexcept {
  if (ASMJIT_UNLIKELY(::munmap(p, size) != 0))
    return DebugUtils::errored(kErrorInvalidState);

  return kErrorOk;
}
#endif

// ============================================================================
// [asmjit::VirtMemManager::TypeDefs]
// ============================================================================

using Globals::BitWord;
using Globals::kBitWordSize;

typedef VirtMemManager::RBNode RBNode;
typedef VirtMemManager::MemNode MemNode;

// ============================================================================
// [asmjit::VirtMemManager - BitOps]
// ============================================================================

#define M_DIV(x, y) ((x) / (y))
#define M_MOD(x, y) ((x) % (y))

//! \internal
//!
//! Set `len` bits in `buf` starting at `index` bit index.
static void VirtMemManager_setBits(BitWord* buf, size_t index, size_t len) noexcept {
  if (len == 0)
    return;

  size_t i = index / kBitWordSize;                      // BitWord[]
  size_t j = index % kBitWordSize;                      // BitWord[][] bit index
  size_t n = std::min<size_t>(kBitWordSize - j, len);   // How many bytes process in the first group.

  buf += i;
  *buf++ |= (~BitWord(0) >> (kBitWordSize - n)) << j;
  len -= n;

  while (len >= kBitWordSize) {
    *buf++ = ~BitWord(0);
    len -= kBitWordSize;
  }

  if (len)
    *buf |= ~BitWord(0) >> (kBitWordSize - len);
}

// ============================================================================
// [asmjit::VirtMemManager::RBNode]
// ============================================================================

//! \internal
//!
//! Base red-black tree node.
struct VirtMemManager::RBNode {
  // Implementation is based on article by Julienne Walker (Public Domain),
  // including C code and original comments. Thanks for the excellent article.
  //! Get whether the node is red (nullptr or node with red flag).
  static ASMJIT_INLINE bool isRed(RBNode* node) noexcept { return node && node->red; }

  RBNode* node[2];                       //!< Left[0] and right[1] nodes.
  uint8_t* mem;                          //!< Virtual memory address.
  uint32_t red;                          //!< Node color (red vs. black).
};

//! \internal
//!
//! Check whether the RB tree is valid.
static int rbAssert(RBNode* root) noexcept {
  if (!root) return 1;

  RBNode* ln = root->node[0];
  RBNode* rn = root->node[1];

  // Red violation.
  ASMJIT_ASSERT(!(RBNode::isRed(root) && (RBNode::isRed(ln) || RBNode::isRed(rn))));

  int lh = rbAssert(ln);
  int rh = rbAssert(rn);

  // Invalid btree.
  ASMJIT_ASSERT(ln == nullptr || ln->mem < root->mem);
  ASMJIT_ASSERT(rn == nullptr || rn->mem > root->mem);

  // Black violation.
  ASMJIT_ASSERT(!(lh != 0 && rh != 0 && lh != rh));

  // Only count black links.
  if (lh != 0 && rh != 0)
    return RBNode::isRed(root) ? lh : lh + 1;
  else
    return 0;
}

//! \internal
//!
//! Single rotation.
static ASMJIT_INLINE RBNode* rbRotateSingle(RBNode* root, int dir) noexcept {
  RBNode* save = root->node[!dir];

  root->node[!dir] = save->node[dir];
  save->node[dir] = root;

  root->red = 1;
  save->red = 0;

  return save;
}

//! \internal
//!
//! Double rotation.
static ASMJIT_INLINE RBNode* rbRotateDouble(RBNode* root, int dir) noexcept {
  root->node[!dir] = rbRotateSingle(root->node[!dir], !dir);
  return rbRotateSingle(root, dir);
}

// ============================================================================
// [asmjit::VirtMemManager::MemNode]
// ============================================================================

struct VirtMemManager::MemNode : public RBNode {
  ASMJIT_INLINE void init(MemNode* other) noexcept {
    mem = other->mem;

    size = other->size;
    used = other->used;
    blocks = other->blocks;
    density = other->density;
    largestBlock = other->largestBlock;

    baUsed = other->baUsed;
    baCont = other->baCont;
  }

  // Get available space.
  ASMJIT_INLINE size_t getAvailable() const noexcept { return size - used; }

  MemNode* prev;         // Prev node in list.
  MemNode* next;         // Next node in list.

  size_t size;           // How many bytes contain this node.
  size_t used;           // How many bytes are used in this node.
  size_t blocks;         // How many blocks are here.
  size_t density;        // Minimum count of allocated bytes in this node (also alignment).
  size_t largestBlock;   // Contains largest block that can be allocated.

  BitWord* baUsed;       // Contains bits about used blocks       (0 = unused, 1 = used).
  BitWord* baCont;       // Contains bits about continuous blocks (0 = stop  , 1 = continue).
};

// ============================================================================
// [asmjit::VirtMemManager - Private]
// ============================================================================

//! \internal
//!
//! Check whether the Red-Black tree is valid.
static bool VirtMemManager_checkTree(VirtMemManager* self) noexcept {
  return rbAssert(self->_root) > 0;
}

//! \internal
//!
//! Alloc virtual memory including a heap memory needed for `MemNode` data.
//!
//! Returns set-up `MemNode*` or nullptr if allocation failed.
static MemNode* VirtMemManager_newNode(VirtMemManager* self, size_t size, size_t density) noexcept {
  ASMJIT_UNUSED(self);

  uint8_t* vmem = static_cast<uint8_t*>(VirtMem::alloc(size, VirtMem::kAccessWrite | VirtMem::kAccessExecute));
  if (ASMJIT_UNLIKELY(!vmem))
    return nullptr;

  size_t blocks = size / density;
  size_t bsize = (((blocks + 7) >> 3) + sizeof(size_t) - 1) & ~size_t(sizeof(size_t) - 1);

  MemNode* node = static_cast<MemNode*>(AsmJitInternal::allocMemory(sizeof(MemNode)));
  uint8_t* data = static_cast<uint8_t*>(AsmJitInternal::allocMemory(bsize * 2));

  // Out of memory.
  if (!node || !data) {
    VirtMem::release(vmem, size);
    if (node) AsmJitInternal::releaseMemory(node);
    if (data) AsmJitInternal::releaseMemory(data);
    return nullptr;
  }

  // Initialize RBNode data.
  node->node[0] = nullptr;
  node->node[1] = nullptr;
  node->mem = vmem;
  node->red = 1;

  // Initialize MemNode data.
  node->prev = nullptr;
  node->next = nullptr;

  node->size = size;
  node->used = 0;
  node->blocks = blocks;
  node->density = density;
  node->largestBlock = size;

  std::memset(data, 0, bsize * 2);
  node->baUsed = reinterpret_cast<BitWord*>(data);
  node->baCont = reinterpret_cast<BitWord*>(data + bsize);

  return node;
}

static void VirtMemManager_insertNode(VirtMemManager* self, MemNode* node) noexcept {
  if (!self->_root) {
    // Empty tree case.
    self->_root = node;
  }
  else {
    // False tree root.
    RBNode head = { { nullptr, nullptr }, 0, 0 };

    // Grandparent & parent.
    RBNode* g = nullptr;
    RBNode* t = &head;

    // Iterator & parent.
    RBNode* p = nullptr;
    RBNode* q = t->node[1] = self->_root;

    int dir = 0;
    int last = 0; // Not needed to initialize, but makes some tools happy.

    // Search down the tree.
    for (;;) {
      if (!q) {
        // Insert new node at the bottom.
        q = node;
        p->node[dir] = node;
      }
      else if (RBNode::isRed(q->node[0]) && RBNode::isRed(q->node[1])) {
        // Color flip.
        q->red = 1;
        q->node[0]->red = 0;
        q->node[1]->red = 0;
      }

      // Fix red violation.
      if (RBNode::isRed(q) && RBNode::isRed(p)) {
        int dir2 = t->node[1] == g;
        t->node[dir2] = q == p->node[last] ? rbRotateSingle(g, !last) : rbRotateDouble(g, !last);
      }

      // Stop if found.
      if (q == node)
        break;

      last = dir;
      dir = q->mem < node->mem;

      // Update helpers.
      if (g) t = g;

      g = p;
      p = q;
      q = q->node[dir];
    }

    // Update root.
    self->_root = static_cast<MemNode*>(head.node[1]);
  }

  // Make root black.
  self->_root->red = 0;

  // Link with others.
  node->prev = self->_last;

  if (!self->_first) {
    self->_first = node;
    self->_last = node;
    self->_optimal = node;
  }
  else {
    node->prev = self->_last;
    self->_last->next = node;
    self->_last = node;
  }
}

//! \internal
//!
//! Remove node from Red-Black tree.
//!
//! Returns node that should be freed, but it doesn't have to be necessarily
//! the `node` passed.
static MemNode* VirtMemManager_removeNode(VirtMemManager* self, MemNode* node) noexcept {
  // False tree root.
  RBNode head = { { nullptr, nullptr }, 0, 0 };

  // Helpers.
  RBNode* q = &head;
  RBNode* p = nullptr;
  RBNode* g = nullptr;

  // Found item.
  RBNode* f = nullptr;
  int dir = 1;

  // Set up.
  q->node[1] = self->_root;

  // Search and push a red down.
  while (q->node[dir]) {
    int last = dir;

    // Update helpers.
    g = p;
    p = q;
    q = q->node[dir];
    dir = q->mem < node->mem;

    // Save found node.
    if (q == node)
      f = q;

    // Push the red node down.
    if (!RBNode::isRed(q) && !RBNode::isRed(q->node[dir])) {
      if (RBNode::isRed(q->node[!dir])) {
        p = p->node[last] = rbRotateSingle(q, dir);
      }
      else if (!RBNode::isRed(q->node[!dir])) {
        RBNode* s = p->node[!last];

        if (s) {
          if (!RBNode::isRed(s->node[!last]) && !RBNode::isRed(s->node[last])) {
            // Color flip.
            p->red = 0;
            s->red = 1;
            q->red = 1;
          }
          else {
            int dir2 = g->node[1] == p;

            if (RBNode::isRed(s->node[last]))
              g->node[dir2] = rbRotateDouble(p, last);
            else if (RBNode::isRed(s->node[!last]))
              g->node[dir2] = rbRotateSingle(p, last);

            // Ensure correct coloring.
            q->red = g->node[dir2]->red = 1;
            g->node[dir2]->node[0]->red = 0;
            g->node[dir2]->node[1]->red = 0;
          }
        }
      }
    }
  }

  // Replace and remove.
  ASMJIT_ASSERT(f != nullptr);
  ASMJIT_ASSERT(f != &head);
  ASMJIT_ASSERT(q != &head);

  if (f != q) {
    ASMJIT_ASSERT(f != &head);
    static_cast<MemNode*>(f)->init(static_cast<MemNode*>(q));
  }

  p->node[p->node[1] == q] = q->node[q->node[0] == nullptr];

  // Update root and make it black.
  self->_root = static_cast<MemNode*>(head.node[1]);
  if (self->_root) self->_root->red = 0;

  // Unlink.
  MemNode* next = static_cast<MemNode*>(q)->next;
  MemNode* prev = static_cast<MemNode*>(q)->prev;

  if (prev)
    prev->next = next;
  else
    self->_first = next;

  if (next)
    next->prev = prev;
  else
    self->_last  = prev;

  if (self->_optimal == q)
    self->_optimal = prev ? prev : next;

  return static_cast<MemNode*>(q);
}

static MemNode* VirtMemManager_getNodeByPtr(VirtMemManager* self, uint8_t* mem) noexcept {
  MemNode* node = self->_root;
  while (node) {
    uint8_t* nodeMem = node->mem;

    // Go left.
    if (mem < nodeMem) {
      node = static_cast<MemNode*>(node->node[0]);
      continue;
    }

    // Go right.
    uint8_t* nodeEnd = nodeMem + node->size;
    if (mem >= nodeEnd) {
      node = static_cast<MemNode*>(node->node[1]);
      continue;
    }

    // Match.
    break;
  }
  return node;
}

// ============================================================================
// [asmjit::VirtMemManager - Construction / Destruction]
// ============================================================================

VirtMemManager::VirtMemManager() noexcept {
  VirtMem::Info vmInfo = VirtMem::getInfo();

  _pageSize = vmInfo.pageSize;
  _blockSize = vmInfo.pageGranularity;
  _blockDensity = 64;

  _usedBytes = 0;
  _allocatedBytes = 0;

  _root = nullptr;
  _first = nullptr;
  _last = nullptr;
  _optimal = nullptr;
}

VirtMemManager::~VirtMemManager() noexcept {
  reset();
}

// ============================================================================
// [asmjit::VirtMemManager - Reset]
// ============================================================================

void VirtMemManager::reset() noexcept {
  MemNode* node = _first;

  while (node) {
    MemNode* next = node->next;

    VirtMem::release(node->mem, node->size);
    AsmJitInternal::releaseMemory(node->baUsed);
    AsmJitInternal::releaseMemory(node);

    node = next;
  }

  _allocatedBytes = 0;
  _usedBytes = 0;

  _root = nullptr;
  _first = nullptr;
  _last = nullptr;
  _optimal = nullptr;
}

// ============================================================================
// [asmjit::VirtMemManager - Alloc / Release]
// ============================================================================

void* VirtMemManager::alloc(size_t size) noexcept {
  // Current index.
  size_t i;

  // How many we need to be freed.
  size_t need;
  size_t minVSize;

  // Align to 32 bytes by default.
  size = IntUtils::alignUp<size_t>(size, 32);
  if (size == 0)
    return nullptr;

  AutoLock locked(_lock);
  MemNode* node = _optimal;
  minVSize = _blockSize;

  // Try to find memory block in existing nodes.
  while (node) {
    // Skip this node?
    if ((node->getAvailable() < size) || (node->largestBlock < size && node->largestBlock != 0)) {
      MemNode* next = node->next;

      if (node->getAvailable() < minVSize && node == _optimal && next)
        _optimal = next;

      node = next;
      continue;
    }

    BitWord* up = node->baUsed;    // Current ubits address.
    BitWord ubits;                 // Current ubits[0] value.
    BitWord bit;                   // Current bit mask.

    size_t blocks = node->blocks;  // Count of blocks in node.
    size_t cont = 0;               // How many bits are currently freed in find loop.
    size_t maxCont = 0;            // Largest continuous block (bits count).
    size_t j;

    need = M_DIV((size + node->density - 1), node->density);
    i = 0;

    // Try to find node that is large enough.
    while (i < blocks) {
      ubits = *up++;

      // Fast skip used blocks.
      if (ubits == ~BitWord(0)) {
        if (cont > maxCont)
          maxCont = cont;
        cont = 0;

        i += kBitWordSize;
        continue;
      }

      size_t max = kBitWordSize;
      if (i + max > blocks)
        max = blocks - i;

      for (j = 0, bit = 1; j < max; bit <<= 1) {
        j++;
        if ((ubits & bit) == 0) {
          if (++cont == need) {
            i += j;
            i -= cont;
            goto L_Found;
          }

          continue;
        }

        if (cont > maxCont) maxCont = cont;
        cont = 0;
      }

      i += kBitWordSize;
    }

    // Because we traversed the entire node, we can set largest node size that
    // will be used to cache next traversing.
    node->largestBlock = maxCont * node->density;

    node = node->next;
  }

  // If we are here, we failed to find existing memory block and we must
  // allocate a new one.
  {
    size_t newBlockSize = _blockSize;
    if (newBlockSize < size) newBlockSize = size;

    node = VirtMemManager_newNode(this, newBlockSize, this->_blockDensity);
    if (!node) return nullptr;

    // Update binary tree.
    VirtMemManager_insertNode(this, node);
    ASMJIT_ASSERT(VirtMemManager_checkTree(this));

    // Alloc first node at start.
    i = 0;
    need = (size + node->density - 1) / node->density;

    // Update statistics.
    _allocatedBytes += node->size;
  }

L_Found:
  // Update bits.
  VirtMemManager_setBits(node->baUsed, i, need);
  VirtMemManager_setBits(node->baCont, i, need - 1);

  // Update statistics.
  {
    size_t u = need * node->density;
    node->used += u;
    node->largestBlock = 0;
    _usedBytes += u;
  }

  // And return pointer to allocated memory.
  uint8_t* result = node->mem + i * node->density;
  ASMJIT_ASSERT(result >= node->mem && result <= node->mem + node->size - size);
  return result;
}

Error VirtMemManager::release(void* p) noexcept {
  if (!p)
    return kErrorOk;

  AutoLock locked(_lock);
  MemNode* node = VirtMemManager_getNodeByPtr(this, static_cast<uint8_t*>(p));
  if (!node) return DebugUtils::errored(kErrorInvalidArgument);

  size_t offset = (size_t)((uint8_t*)p - (uint8_t*)node->mem);
  size_t bitpos = M_DIV(offset, node->density);
  size_t i = (bitpos / kBitWordSize);

  BitWord* up = node->baUsed + i;  // Current ubits address.
  BitWord* cp = node->baCont + i;  // Current cbits address.
  BitWord ubits = *up;             // Current ubits[0] value.
  BitWord cbits = *cp;             // Current cbits[0] value.
  BitWord bit = BitWord(1) << (bitpos % kBitWordSize);

  size_t cont = 0;
  bool stop;

  for (;;) {
    stop = (cbits & bit) == 0;
    ubits &= ~bit;
    cbits &= ~bit;

    bit <<= 1;
    cont++;

    if (stop || bit == 0) {
      *up = ubits;
      *cp = cbits;
      if (stop)
        break;

      ubits = *++up;
      cbits = *++cp;
      bit = 1;
    }
  }

  // If the freed block is fully allocated node then it's needed to
  // update 'optimal' pointer in memory manager.
  if (node->used == node->size) {
    MemNode* cur = _optimal;

    do {
      cur = cur->prev;
      if (cur == node) {
        _optimal = node;
        break;
      }
    } while (cur);
  }

  // Statistics.
  cont *= node->density;
  if (node->largestBlock < cont)
    node->largestBlock = cont;

  node->used -= cont;
  _usedBytes -= cont;

  // If page is empty, we can free it.
  if (node->used == 0) {
    // Free memory associated with node (this memory is not accessed
    // anymore so it's safe).
    VirtMem::release(node->mem, node->size);
    AsmJitInternal::releaseMemory(node->baUsed);

    node->baUsed = nullptr;
    node->baCont = nullptr;

    // Statistics.
    _allocatedBytes -= node->size;

    // Remove node. This function can return different node than
    // passed into, but data is copied into previous node if needed.
    AsmJitInternal::releaseMemory(VirtMemManager_removeNode(this, node));
    ASMJIT_ASSERT(VirtMemManager_checkTree(this));
  }

  return kErrorOk;
}

Error VirtMemManager::shrink(void* p, size_t used) noexcept {
  if (!p) return kErrorOk;
  if (used == 0)
    return release(p);

  AutoLock locked(_lock);
  MemNode* node = VirtMemManager_getNodeByPtr(this, (uint8_t*)p);
  if (!node) return DebugUtils::errored(kErrorInvalidArgument);

  size_t offset = (size_t)((uint8_t*)p - (uint8_t*)node->mem);
  size_t bitpos = M_DIV(offset, node->density);
  size_t i = (bitpos / kBitWordSize);

  BitWord* up = node->baUsed + i;  // Current ubits address.
  BitWord* cp = node->baCont + i;  // Current cbits address.
  BitWord ubits = *up;             // Current ubits[0] value.
  BitWord cbits = *cp;             // Current cbits[0] value.
  BitWord bit = BitWord(1) << (bitpos % kBitWordSize);

  size_t cont = 0;
  size_t usedBlocks = (used + node->density - 1) / node->density;

  bool stop;

  // Find the first block we can mark as free.
  for (;;) {
    stop = (cbits & bit) == 0;
    if (stop)
      return kErrorOk;

    if (++cont == usedBlocks)
      break;

    bit <<= 1;
    if (bit == 0) {
      ubits = *++up;
      cbits = *++cp;
      bit = 1;
    }
  }

  // Free the tail blocks.
  cont = ~BitWord(0);
  stop = (cbits & bit) == 0;
  ubits &= ~bit;

  for (;;) {
    cbits &= ~bit;
    bit <<= 1;
    cont++;

    if (stop || bit == 0) {
      *up = ubits;
      *cp = cbits;
      if (stop)
        break;

      ubits = *++up;
      cbits = *++cp;
      bit = 1;
    }

    stop = (cbits & bit) == 0;
    ubits &= ~bit;
  }

  // Statistics.
  cont *= node->density;
  if (node->largestBlock < cont)
    node->largestBlock = cont;

  node->used -= cont;
  _usedBytes -= cont;

  return kErrorOk;
}

// ============================================================================
// [asmjit::VMem - Test]
// ============================================================================

#if defined(ASMJIT_TEST)
static void VirtMemTest_fill(void* a, void* b, int i) noexcept {
  int pattern = rand() % 256;
  *(int *)a = i;
  *(int *)b = i;
  std::memset((char*)a + sizeof(int), pattern, unsigned(i) - sizeof(int));
  std::memset((char*)b + sizeof(int), pattern, unsigned(i) - sizeof(int));
}

static void VirtMemTest_verify(void* a, void* b) noexcept {
  int ai = *(int*)a;
  int bi = *(int*)b;

  EXPECT(ai == bi, "The length of 'a' (%d) and 'b' (%d) should be same", ai, bi);
  EXPECT(std::memcmp(a, b, size_t(ai)) == 0, "Pattern (%p) doesn't match", a);
}

static void VirtMemTest_stats(VirtMemManager& memmgr) noexcept {
  INFO("Used     : %llu", uint64_t(memmgr.getUsedBytes()));
  INFO("Allocated: %llu", uint64_t(memmgr.getAllocatedBytes()));
}

static void VirtMemTest_shuffle(void** a, void** b, size_t count) noexcept {
  for (size_t i = 0; i < count; ++i) {
    size_t si = size_t(unsigned(rand()) % count);

    void* ta = a[i];
    void* tb = b[i];

    a[i] = a[si];
    b[i] = b[si];

    a[si] = ta;
    b[si] = tb;
  }
}

UNIT(base_virtmem) {
  VirtMemManager memmgr;

  // Should be predictible.
  srand(100);

  int i;
  int kCount = 200000;

  INFO("Memory alloc/free test - %d allocations", kCount);

  void** a = (void**)AsmJitInternal::allocMemory(sizeof(void*) * size_t(kCount));
  void** b = (void**)AsmJitInternal::allocMemory(sizeof(void*) * size_t(kCount));
  EXPECT(a != nullptr && b != nullptr, "Couldn't allocate %u bytes on heap", kCount * 2);

  INFO("Allocating virtual memory...");
  for (i = 0; i < kCount; i++) {
    size_t r = (unsigned(rand()) % 1000) + 4;

    a[i] = memmgr.alloc(r);
    EXPECT(a[i] != nullptr, "Couldn't allocate %d bytes of virtual memory", r);
    std::memset(a[i], 0, r);
  }
  VirtMemTest_stats(memmgr);

  INFO("Freeing virtual memory...");
  for (i = 0; i < kCount; i++) {
    EXPECT(memmgr.release(a[i]) == kErrorOk, "Failed to free %p", b[i]);
  }
  VirtMemTest_stats(memmgr);

  INFO("Verified alloc/free test - %d allocations", kCount);
  for (i = 0; i < kCount; i++) {
    size_t r = (unsigned(rand()) % 1000) + 4;

    a[i] = memmgr.alloc(r);
    EXPECT(a[i] != nullptr, "Couldn't allocate %d bytes of virtual memory", r);

    b[i] = AsmJitInternal::allocMemory(r);
    EXPECT(b[i] != nullptr, "Couldn't allocate %d bytes on heap", r);

    VirtMemTest_fill(a[i], b[i], int(r));
  }
  VirtMemTest_stats(memmgr);

  INFO("Shuffling...");
  VirtMemTest_shuffle(a, b, unsigned(kCount));

  INFO("Verify and free...");
  for (i = 0; i < kCount / 2; i++) {
    VirtMemTest_verify(a[i], b[i]);
    EXPECT(memmgr.release(a[i]) == kErrorOk, "Failed to free %p", a[i]);
    AsmJitInternal::releaseMemory(b[i]);
  }
  VirtMemTest_stats(memmgr);

  INFO("Alloc again");
  for (i = 0; i < kCount / 2; i++) {
    size_t r = (unsigned(rand()) % 1000) + 4;

    a[i] = memmgr.alloc(r);
    EXPECT(a[i] != nullptr, "Couldn't allocate %d bytes of virtual memory", r);

    b[i] = AsmJitInternal::allocMemory(r);
    EXPECT(b[i] != nullptr, "Couldn't allocate %d bytes on heap");

    VirtMemTest_fill(a[i], b[i], int(r));
  }
  VirtMemTest_stats(memmgr);

  INFO("Verify and free...");
  for (i = 0; i < kCount; i++) {
    VirtMemTest_verify(a[i], b[i]);
    EXPECT(memmgr.release(a[i]) == kErrorOk, "Failed to free %p", a[i]);
    AsmJitInternal::releaseMemory(b[i]);
  }
  VirtMemTest_stats(memmgr);

  AsmJitInternal::releaseMemory(a);
  AsmJitInternal::releaseMemory(b);
}
#endif

} // asmjit namespace
