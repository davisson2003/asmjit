// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_CORE_ZONERBTREE_H
#define _ASMJIT_CORE_ZONERBTREE_H

// [Dependencies]
#include "../core/globals.h"

ASMJIT_BEGIN_NAMESPACE

//! \addtogroup asmjit_core
//! \{

// ============================================================================
// [asmjit::ZoneRBNode]
// ============================================================================

class ZoneRBNode {
public:
  ASMJIT_NONCOPYABLE(ZoneRBNode)

  inline ZoneRBNode() noexcept
    : _rbLink { nullptr, nullptr },
      _rbRed(false) {}

  inline ZoneRBNode* getLeft() const noexcept { return _rbLink[Globals::kLinkLeft]; }
  inline ZoneRBNode* getRight() const noexcept { return _rbLink[Globals::kLinkRight]; }

  inline bool isRed() const noexcept { return _rbRed; }
  inline void setRed(bool value) noexcept { _rbRed = value; }

  ZoneRBNode* _rbLink[Globals::kLinkCount];
  bool _rbRed;
};

// ============================================================================
// [asmjit::ZoneRBTree]
// ============================================================================

template<typename NODE>
class ZoneRBTree {
public:
  ASMJIT_NONCOPYABLE(ZoneRBTree)

  typedef NODE Node;

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline ZoneRBTree() noexcept
    : _root(nullptr) {}

  inline ZoneRBTree(ZoneRBTree&& other) noexcept
    : _root(other._root) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline bool isEmpty() const noexcept { return _root == nullptr; }
  inline NODE* getRoot() const noexcept { return static_cast<NODE*>(_root); }

  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  inline void reset() noexcept { _root = nullptr; }

  // --------------------------------------------------------------------------
  // [Operations]
  // --------------------------------------------------------------------------

  void insert(NODE* node) noexcept {
    // Node to insert must not contain garbage.
    ASMJIT_ASSERT(node->_rbLink[0] == nullptr);
    ASMJIT_ASSERT(node->_rbLink[1] == nullptr);
    ASMJIT_ASSERT(node->isRed() == false);

    if (!_root) {
      _root = node;
      return;
    }

    ZoneRBNode head;
    head._rbLink[1] = _root;   // False root node.

    ZoneRBNode* g = nullptr;   // Grandparent.
    ZoneRBNode* p = nullptr;   // Parent.
    ZoneRBNode* t = &head;     // Iterator.
    ZoneRBNode* q = _root;     // Query.

    size_t dir = 0;            // Direction (for accessing _rbLink[]).
    size_t last = 0;           // Not needed to initialize, but makes some tools happy.

    node->setRed(true);        // New nodes are always red and violations fixed appropriately.

    // Search down the tree.
    for (;;) {
      if (!q) {
        // Insert new node at the bottom.
        q = node;
        p->_rbLink[dir] = node;
      }
      else if (isNodeRed(q->_rbLink[0]) && isNodeRed(q->_rbLink[1])) {
        // Color flip.
        q->setRed(true);
        q->_rbLink[0]->setRed(false);
        q->_rbLink[1]->setRed(false);
      }

      // Fix red violation.
      if (isNodeRed(q) && isNodeRed(p)) {
        size_t dir2 = t->_rbLink[1] == g;
        t->_rbLink[dir2] = q == p->_rbLink[last] ? _singleRotate(g, !last) : _doubleRotate(g, !last);
      }

      // Stop if found.
      if (q == node)
        break;

      last = dir;
      dir = static_cast<NODE*>(q)->lt(static_cast<NODE*>(node));

      // Update helpers.
      if (g) t = g;

      g = p;
      p = q;
      q = q->_rbLink[dir];
    }

    // Update root and make it black.
    _root = static_cast<NODE*>(head._rbLink[1]);
    _root->setRed(false);
  }

  //! Remove node from RBTree.
  void remove(ZoneRBNode* node) noexcept {
    ZoneRBNode head;
    head._rbLink[1] = _root;   // False root node.

    ZoneRBNode* g = nullptr;   // Grandparent.
    ZoneRBNode* p = nullptr;   // Parent.
    ZoneRBNode* q = &head;     // Query.

    ZoneRBNode* f  = nullptr;  // Found item.
    ZoneRBNode* gf = nullptr;  // Found grandparent.
    size_t dir = 1;            // Direction (0 or 1).

    // Search and push a red down.
    while (q->_rbLink[dir]) {
      size_t last = dir;

      // Update helpers.
      g = p;
      p = q;
      q = q->_rbLink[dir];
      dir = static_cast<NODE*>(q)->lt(static_cast<NODE*>(node));

      // Save found node.
      if (q == node) {
        f = q;
        gf = g;
      }

      // Push the red node down.
      if (!isNodeRed(q) && !isNodeRed(q->_rbLink[dir])) {
        if (isNodeRed(q->_rbLink[!dir])) {
          p = p->_rbLink[last] = _singleRotate(q, dir);
        }
        else if (!isNodeRed(q->_rbLink[!dir]) && p->_rbLink[!last]) {
          ZoneRBNode* s = p->_rbLink[!last];
          if (!isNodeRed(s->_rbLink[!last]) && !isNodeRed(s->_rbLink[last])) {
            // Color flip.
            p->setRed(false);
            s->setRed(true);
            q->setRed(true);
          }
          else {
            size_t dir2 = g->_rbLink[1] == p;

            if (isNodeRed(s->_rbLink[last]))
              g->_rbLink[dir2] = _doubleRotate(p, last);
            else if (isNodeRed(s->_rbLink[!last]))
              g->_rbLink[dir2] = _singleRotate(p, last);

            // Ensure correct coloring.
            g->_rbLink[dir2]->setRed(true);
            q->setRed(true);
            g->_rbLink[dir2]->_rbLink[0]->setRed(false);
            g->_rbLink[dir2]->_rbLink[1]->setRed(false);
          }
        }
      }
    }

    // Replace and remove.
    ASMJIT_ASSERT(f != nullptr);
    ASMJIT_ASSERT(f != &head);
    ASMJIT_ASSERT(q != &head);
    p->_rbLink[p->_rbLink[1] == q] = q->_rbLink[q->_rbLink[0] == nullptr];

    // NOTE: The original algorithm used a trick to just copy 'key/value' to
    // `f` and mark `q` for deletion. But this is unacceptable here as we
    // really want to destroy the passed `node`. So, we really have to make
    // sure that we really removed `f` and not `q` from the tree.
    if (f != q) {
      ASMJIT_ASSERT(f != &head);
      ASMJIT_ASSERT(f != gf);

      ZoneRBNode* n = gf ? gf : &head;
      dir = (n == &head) ? 1 : static_cast<NODE*>(n)->lt(static_cast<NODE*>(node));

      for (;;) {
        if (n->_rbLink[dir] == f) {
          n->_rbLink[dir] = q;
          q->_rbLink[0] = f->_rbLink[0];
          q->_rbLink[1] = f->_rbLink[1];
          q->setRed(f->isRed());
          break;
        }

        n = n->_rbLink[dir];
        ASMJIT_ASSERT(n != nullptr);
        dir = static_cast<NODE*>(n)->lt(static_cast<NODE*>(node));
      }
    }

    // Update root and make it black.
    _root = static_cast<NODE*>(head._rbLink[1]);
    if (_root) _root->setRed(false);
  }

  template<typename KEY>
  ASMJIT_FORCEINLINE NODE* get(const KEY& key) const noexcept {
    ZoneRBNode* node = _root;
    while (node) {
      auto result = static_cast<const NODE*>(node)->cmpKey(key);
      if (result == 0)
        break;
      // Go left or right depending on `result`.
      node = node->_rbLink[result > 0];
    }
    return static_cast<NODE*>(node);
  }

  // --------------------------------------------------------------------------
  // [Swap]
  // --------------------------------------------------------------------------

  inline void swapWith(ZoneRBTree& other) noexcept {
    std::swap(_root, other._root);
  }

  // --------------------------------------------------------------------------
  // [Internal]
  // --------------------------------------------------------------------------

  //! Get whether the node is red (NULL or node with red flag).
  static inline bool isNodeRed(ZoneRBNode* node) noexcept { return node && node->isRed(); }

  //! Single rotation.
  static ASMJIT_FORCEINLINE ZoneRBNode* _singleRotate(ZoneRBNode* root, size_t dir) noexcept {
    ZoneRBNode* save = root->_rbLink[!dir];
    root->_rbLink[!dir] = save->_rbLink[dir];
    save->_rbLink[dir] = root;
    root->setRed(true);
    save->setRed(false);
    return save;
  }

  //! Double rotation.
  static ASMJIT_FORCEINLINE ZoneRBNode* _doubleRotate(ZoneRBNode* root, size_t dir) noexcept {
    root->_rbLink[!dir] = _singleRotate(root->_rbLink[!dir], !dir);
    return _singleRotate(root, dir);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  NODE* _root;
};

//! \}

ASMJIT_END_NAMESPACE

// [Guard]
#endif // _ASMJIT_CORE_ZONERBTREE_H
