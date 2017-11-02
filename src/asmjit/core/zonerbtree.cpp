// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define ASMJIT_EXPORTS

// [Dependencies]
#include "../core/intutils.h"
#include "../core/zone.h"
#include "../core/zonerbtree.h"

ASMJIT_BEGIN_NAMESPACE

// ============================================================================
// [asmjit::ZoneRBTree - Unit]
// ============================================================================

#if defined(ASMJIT_BUILD_TEST)
template<typename NODE>
struct ZoneRBUnit {
  typedef ZoneRBTree<NODE> Tree;

  static void verifyTree(Tree& tree) noexcept {
    EXPECT(checkHeight(static_cast<NODE*>(tree._root)) > 0);
  }

  // Check whether the Red-Black tree is valid.
  static int checkHeight(NODE* node) noexcept {
    if (!node) return 1;

    NODE* ln = static_cast<NODE*>(node->_rbLink[0]);
    NODE* rn = static_cast<NODE*>(node->_rbLink[1]);

    // Invalid tree.
    EXPECT(ln == nullptr ||  ln->lt(node));
    EXPECT(rn == nullptr || !rn->lt(node));

    // Red violation.
    EXPECT(!node->isRed() || (!Tree::isNodeRed(ln) && !Tree::isNodeRed(rn)));

    // Black violation.
    int lh = checkHeight(ln);
    int rh = checkHeight(rn);
    EXPECT(!lh || !rh || lh == rh);

    // Only count black links.
    return (lh && rh) ? lh + !node->isRed() : 0;
  }
};

class MyRBNode : public ZoneRBNode {
public:
  ASMJIT_NONCOPYABLE(MyRBNode)

  explicit inline MyRBNode(uint32_t key) noexcept
    : ZoneRBNode(),
      _key(key) {}

  inline bool lt(const MyRBNode* other) const noexcept { return _key < other->_key; }
  inline int64_t cmpKey(uint32_t key) const noexcept { return int64_t(key) - int64_t(_key); }

  uint32_t _key;
};

UNIT(core_zone_rbtree) {
  constexpr uint32_t kCount = 10000;

  Zone zone(4096);
  ZoneRBTree<MyRBNode> rbTree;

  uint32_t key;
  INFO("Inserting %u elements and validating each operation", unsigned(kCount));
  for (key = 0; key < kCount; key++) {
    rbTree.insert(zone.newT<MyRBNode>(key));
    ZoneRBUnit<MyRBNode>::verifyTree(rbTree);
  }

  uint32_t count = kCount;
  INFO("Removing %u elements and validating each operation", unsigned(kCount));
  do {
    MyRBNode* node;

    for (key = 0; key < count; key++) {
      node = rbTree.get(key);
      EXPECT(node != nullptr);
      EXPECT(node->_key == key);
    }

    node = rbTree.get(--count);
    rbTree.remove(node);
    ZoneRBUnit<MyRBNode>::verifyTree(rbTree);
  } while (count);

  EXPECT(rbTree.isEmpty());
}
#endif

ASMJIT_END_NAMESPACE
