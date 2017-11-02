// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_CORE_ZONELIST_H
#define _ASMJIT_CORE_ZONELIST_H

// [Dependencies]
#include "../core/intutils.h"

ASMJIT_BEGIN_NAMESPACE

//! \addtogroup asmjit_core
//! \{

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
  // [Swap]
  // --------------------------------------------------------------------------

  inline void swapWith(ZoneList<T>& other) noexcept {
    std::swap(_first, other._first);
    std::swap(_last, other._last);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  Link* _first;
  Link* _last;
};

//! \}

ASMJIT_END_NAMESPACE

// [Guard]
#endif // _ASMJIT_CORE_ZONELIST_H
