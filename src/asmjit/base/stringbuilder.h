// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_BASE_STRINGBUILDER_H
#define _ASMJIT_BASE_STRINGBUILDER_H

// [Dependencies]
#include "../base/zone.h"

// [Api-Begin]
#include "../asmjit_apibegin.h"

namespace asmjit {

//! \addtogroup asmjit_base
//! \{

// ============================================================================
// [asmjit::StringBuilder]
// ============================================================================

//! String builder.
//!
//! String builder was designed to be able to build a string using append like
//! operation to append numbers, other strings, or signle characters. It can
//! allocate it's own buffer or use a buffer created on the stack.
//!
//! String builder contains method specific to AsmJit functionality, used for
//! logging or HTML output.
class StringBuilder {
public:
  ASMJIT_NONCOPYABLE(StringBuilder)

  //! \internal
  //!
  //! String operation.
  enum OpType : uint32_t {
    kStringOpSet = 0,                    //!< Replace the current string by a given content.
    kStringOpAppend = 1                  //!< Append a given content to the current string.
  };

  //! \internal
  //!
  //! String format flags.
  enum StringFormatFlags : uint32_t {
    kStringFormatShowSign  = 0x00000001U,
    kStringFormatShowSpace = 0x00000002U,
    kStringFormatAlternate = 0x00000004U,
    kStringFormatSigned    = 0x80000000U
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  ASMJIT_API StringBuilder() noexcept;
  ASMJIT_API ~StringBuilder() noexcept;

  inline StringBuilder(Globals::Init_, char* data, size_t capacity, bool canFree) noexcept
    : _data(data),
      _length(0),
      _capacity(capacity),
      _canFree(canFree) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get whether the string is empty.
  inline bool isEmpty() const noexcept { return _length == 0; }
  //! Get length.
  inline size_t getLength() const noexcept { return _length; }
  //! Get capacity.
  inline size_t getCapacity() const noexcept { return _capacity; }

  //! Get null-terminated string data.
  inline char* getData() noexcept { return _data; }
  //! Get null-terminated string data (const).
  inline const char* getData() const noexcept { return _data; }

  // --------------------------------------------------------------------------
  // [Prepare / Reserve]
  // --------------------------------------------------------------------------

  //! Prepare to set/append.
  ASMJIT_API char* prepare(uint32_t op, size_t len) noexcept;

  //! Reserve `to` bytes in string builder.
  ASMJIT_API Error reserve(size_t to) noexcept;

  // --------------------------------------------------------------------------
  // [Clear]
  // --------------------------------------------------------------------------

  //! Clear the content in String builder.
  ASMJIT_API void clear() noexcept;

  // --------------------------------------------------------------------------
  // [Op]
  // --------------------------------------------------------------------------

  ASMJIT_API Error _opString(uint32_t op, const char* str, size_t len = Globals::kNullTerminated) noexcept;
  ASMJIT_API Error _opVFormat(uint32_t op, const char* fmt, va_list ap) noexcept;
  ASMJIT_API Error _opChar(uint32_t op, char c) noexcept;
  ASMJIT_API Error _opChars(uint32_t op, char c, size_t n) noexcept;
  ASMJIT_API Error _opNumber(uint32_t op, uint64_t i, uint32_t base = 0, size_t width = 0, uint32_t flags = 0) noexcept;
  ASMJIT_API Error _opHex(uint32_t op, const void* data, size_t len) noexcept;

  // --------------------------------------------------------------------------
  // [Set]
  // --------------------------------------------------------------------------

  //! Replace the current string with `str` having `len` characters (or possibly null terminated).
  inline Error setString(const char* str, size_t len = Globals::kNullTerminated) noexcept { return _opString(kStringOpSet, str, len); }
  //! Replace the current content by a formatted string `fmt`.
  ASMJIT_API Error setFormat(const char* fmt, ...) noexcept;
  //! Replace the current content by a formatted string `fmt` (va_list version).
  inline Error setFormatVA(const char* fmt, va_list ap) noexcept { return _opVFormat(kStringOpSet, fmt, ap); }

  //! Replace the current content by a single `c` character.
  inline Error setChar(char c) noexcept { return _opChar(kStringOpSet, c); }
  //! Replace the current content by `c` character `n` times.
  inline Error setChars(char c, size_t n) noexcept { return _opChars(kStringOpSet, c, n); }

  //! Replace the current content by a formatted integer `i` (signed).
  inline Error setInt(uint64_t i, uint32_t base = 0, size_t width = 0, uint32_t flags = 0) noexcept {
    return _opNumber(kStringOpSet, i, base, width, flags | kStringFormatSigned);
  }

  //! Replace the current content by a formatted integer `i` (unsigned).
  inline Error setUInt(uint64_t i, uint32_t base = 0, size_t width = 0, uint32_t flags = 0) noexcept {
    return _opNumber(kStringOpSet, i, base, width, flags);
  }

  //! Replace the current content by the given `data` converted to a HEX string.
  inline Error setHex(const void* data, size_t len) noexcept {
    return _opHex(kStringOpSet, data, len);
  }

  // --------------------------------------------------------------------------
  // [Append]
  // --------------------------------------------------------------------------

  //! Append string `str` having `len` characters (or possibly null terminated).
  inline Error appendString(const char* str, size_t len = Globals::kNullTerminated) noexcept { return _opString(kStringOpAppend, str, len); }
  //! Append a formatted string `fmt`.
  ASMJIT_API Error appendFormat(const char* fmt, ...) noexcept;
  //! Append a formatted string `fmt` (va_list version).
  inline Error appendFormatVA(const char* fmt, va_list ap) noexcept { return _opVFormat(kStringOpAppend, fmt, ap); }

  //! Append a single `c` character.
  inline Error appendChar(char c) noexcept { return _opChar(kStringOpAppend, c); }
  //! Append `c` character `n` times.
  inline Error appendChars(char c, size_t n) noexcept { return _opChars(kStringOpAppend, c, n); }

  ASMJIT_API Error padEnd(size_t n, char c = ' ') noexcept;

  //! Append `i`.
  inline Error appendInt(int64_t i, uint32_t base = 0, size_t width = 0, uint32_t flags = 0) noexcept {
    return _opNumber(kStringOpAppend, uint64_t(i), base, width, flags | kStringFormatSigned);
  }

  //! Append `i`.
  inline Error appendUInt(uint64_t i, uint32_t base = 0, size_t width = 0, uint32_t flags = 0) noexcept {
    return _opNumber(kStringOpAppend, i, base, width, flags);
  }

  //! Append the given `data` converted to a HEX string.
  inline Error appendHex(const void* data, size_t len) noexcept {
    return _opHex(kStringOpAppend, data, len);
  }

  // --------------------------------------------------------------------------
  // [Eq]
  // --------------------------------------------------------------------------

  //! Check for equality with other `str` of length `len`.
  ASMJIT_API bool eq(const char* str, size_t len = Globals::kNullTerminated) const noexcept;
  //! Check for equality with `other`.
  inline bool eq(const StringBuilder& other) const noexcept { return eq(other._data, other._length); }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  inline bool operator==(const StringBuilder& other) const noexcept { return  eq(other); }
  inline bool operator!=(const StringBuilder& other) const noexcept { return !eq(other); }

  inline bool operator==(const char* str) const noexcept { return  eq(str); }
  inline bool operator!=(const char* str) const noexcept { return !eq(str); }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  char* _data;                           //!< String data.
  size_t _length;                        //!< String length.
  size_t _capacity;                      //!< String capacity.
  size_t _canFree;                       //!< If the string data can be freed.
};

// ============================================================================
// [asmjit::StringBuilderTmp]
// ============================================================================

//! Temporary string builder, has statically allocated `N` bytes.
template<size_t N>
class StringBuilderTmp : public StringBuilder {
public:
  ASMJIT_NONCOPYABLE(StringBuilderTmp<N>)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline StringBuilderTmp() noexcept : StringBuilder(Globals::Init, _embeddedData, N, false) {}

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Embedded data.
  char _embeddedData[size_t(N + 1 + sizeof(intptr_t)) & ~size_t(sizeof(intptr_t) - 1)];
};

//! \}

} // asmjit namespace

// [Api-End]
#include "../asmjit_apiend.h"

// [Guard]
#endif // _ASMJIT_BASE_STRINGBUILDER_H
