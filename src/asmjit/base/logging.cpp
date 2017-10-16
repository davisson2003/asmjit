// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define ASMJIT_EXPORTS

// [Guard]
#include "../asmjit_build.h"
#if !defined(ASMJIT_DISABLE_LOGGING)

// [Dependencies]
#include "../base/codebuilder.h"
#include "../base/codecompiler.h"
#include "../base/codeholder.h"
#include "../base/codeemitter.h"
#include "../base/logging.h"
#include "../base/stringbuilder.h"
#include "../base/stringutils.h"

#if defined(ASMJIT_BUILD_X86)
# include "../x86/x86logging_p.h"
#endif

#if defined(ASMJIT_BUILD_ARM)
# include "../arm/armlogging_p.h"
#endif

#if defined(ASMJIT_DISABLE_COMPILER)
namespace asmjit { class VirtReg; }
#endif

// [Api-Begin]
#include "../asmjit_apibegin.h"

namespace asmjit {

// ============================================================================
// [asmjit::Logger - Construction / Destruction]
// ============================================================================

Logger::Logger() noexcept {
  _options = 0;
  std::memset(_indentation, 0, ASMJIT_ARRAY_SIZE(_indentation));
}
Logger::~Logger() noexcept {}

// ============================================================================
// [asmjit::Logger - Logging]
// ============================================================================

Error Logger::logf(const char* fmt, ...) noexcept {
  Error err;
  std::va_list ap;

  va_start(ap, fmt);
  err = logv(fmt, ap);
  va_end(ap);

  return err;
}

Error Logger::logv(const char* fmt, std::va_list ap) noexcept {
  StringBuilderTmp<2048> sb;
  ASMJIT_PROPAGATE(sb.appendFormatVA(fmt, ap));
  return log(sb);
}

Error Logger::logBinary(const void* data, size_t size) noexcept {
  static const char prefix[] = ".db ";

  StringBuilderTmp<256> sb;
  sb.appendString(prefix, ASMJIT_ARRAY_SIZE(prefix) - 1);

  size_t i = size;
  const uint8_t* s = static_cast<const uint8_t*>(data);

  while (i) {
    uint32_t n = uint32_t(std::min<size_t>(i, 16));
    sb.truncate(ASMJIT_ARRAY_SIZE(prefix) - 1);
    sb.appendHex(s, n);
    sb.appendChar('\n');
    ASMJIT_PROPAGATE(log(sb));
    s += n;
    i -= n;
  }

  return kErrorOk;
}

// ============================================================================
// [asmjit::Logger - Indentation]
// ============================================================================

void Logger::setIndentation(const char* indentation) noexcept {
  std::memset(_indentation, 0, ASMJIT_ARRAY_SIZE(_indentation));
  if (!indentation)
    return;

  size_t length = StringUtils::strLen(indentation, ASMJIT_ARRAY_SIZE(_indentation) - 1);
  std::memcpy(_indentation, indentation, length);
}

// ============================================================================
// [asmjit::FileLogger - Construction / Destruction]
// ============================================================================

FileLogger::FileLogger(FILE* stream) noexcept : _stream(nullptr) { setStream(stream); }
FileLogger::~FileLogger() noexcept {}

// ============================================================================
// [asmjit::FileLogger - Logging]
// ============================================================================

Error FileLogger::_log(const char* buf, size_t len) noexcept {
  if (!_stream)
    return kErrorOk;

  if (len == Globals::kNullTerminated)
    len = strlen(buf);

  fwrite(buf, 1, len, _stream);
  return kErrorOk;
}

// ============================================================================
// [asmjit::StringLogger - Construction / Destruction]
// ============================================================================

StringLogger::StringLogger() noexcept {}
StringLogger::~StringLogger() noexcept {}

// ============================================================================
// [asmjit::StringLogger - Logging]
// ============================================================================

Error StringLogger::_log(const char* buf, size_t len) noexcept {
  return _stringBuilder.appendString(buf, len);
}

// ============================================================================
// [asmjit::Logging]
// ============================================================================

Error Logging::formatLabel(
  StringBuilder& sb,
  uint32_t logOptions,
  const CodeEmitter* emitter,
  uint32_t labelId) noexcept {

  ASMJIT_UNUSED(logOptions);

  const LabelEntry* le = emitter->getCode()->getLabelEntry(labelId);
  if (ASMJIT_UNLIKELY(!le))
    return sb.appendFormat("InvalidLabel[Id=%u]", labelId);

  if (le->hasName()) {
    if (le->hasParent()) {
      uint32_t parentId = le->getParentId();
      const LabelEntry* pe = emitter->getCode()->getLabelEntry(parentId);

      if (ASMJIT_UNLIKELY(!pe))
        ASMJIT_PROPAGATE(sb.appendFormat("InvalidLabel[Id=%u]", labelId));
      else if (ASMJIT_UNLIKELY(!pe->hasName()))
        ASMJIT_PROPAGATE(sb.appendFormat("L%u", Operand::unpackId(parentId)));
      else
        ASMJIT_PROPAGATE(sb.appendString(pe->getName()));

      ASMJIT_PROPAGATE(sb.appendChar('.'));
    }
    return sb.appendString(le->getName());
  }
  else {
    return sb.appendFormat("L%u", Operand::unpackId(labelId));
  }
}

Error Logging::formatRegister(
  StringBuilder& sb,
  uint32_t logOptions,
  const CodeEmitter* emitter,
  uint32_t archType,
  uint32_t regType,
  uint32_t regId) noexcept {

#if defined(ASMJIT_BUILD_X86)
  if (ArchInfo::isX86Family(archType))
    return X86Logging::formatRegister(sb, logOptions, emitter, archType, regType, regId);
#endif

#if defined(ASMJIT_BUILD_ARM)
  if (ArchInfo::isArmFamily(archType))
    return ArmLogging::formatRegister(sb, logOptions, emitter, archType, regType, regId);
#endif

  return kErrorInvalidArch;
}

Error Logging::formatOperand(
  StringBuilder& sb,
  uint32_t logOptions,
  const CodeEmitter* emitter,
  uint32_t archType,
  const Operand_& op) noexcept {

#if defined(ASMJIT_BUILD_X86)
  if (ArchInfo::isX86Family(archType))
    return X86Logging::formatOperand(sb, logOptions, emitter, archType, op);
#endif

#if defined(ASMJIT_BUILD_ARM)
  if (ArchInfo::isArmFamily(archType))
    return ArmLogging::formatOperand(sb, logOptions, emitter, archType, op);
#endif

  return kErrorInvalidArch;
}

Error Logging::formatInstruction(
  StringBuilder& sb,
  uint32_t logOptions,
  const CodeEmitter* emitter,
  uint32_t archType,
  const Inst::Detail& detail, const Operand_* operands, uint32_t count) noexcept {

#if defined(ASMJIT_BUILD_X86)
  if (ArchInfo::isX86Family(archType))
    return X86Logging::formatInstruction(sb, logOptions, emitter, archType, detail, operands, count);
#endif

#if defined(ASMJIT_BUILD_ARM)
  if (ArchInfo::isArmFamily(archType))
    return ArmLogging::formatInstruction(sb, logOptions, emitter, archType, detail, operands, count);
#endif

  return kErrorInvalidArch;
}

Error Logging::formatTypeId(StringBuilder& sb, uint32_t typeId) noexcept {
  if (typeId == TypeId::kVoid)
    return sb.appendString("void");

  if (!TypeId::isValid(typeId))
    return sb.appendString("unknown");

  const char* typeName = "unknown";
  uint32_t typeSize = TypeId::sizeOf(typeId);

  uint32_t elementId = TypeId::elementOf(typeId);
  switch (elementId) {
    case TypeId::kIntPtr : typeName = "intptr" ; break;
    case TypeId::kUIntPtr: typeName = "uintptr"; break;
    case TypeId::kI8     : typeName = "i8"     ; break;
    case TypeId::kU8     : typeName = "u8"     ; break;
    case TypeId::kI16    : typeName = "i16"    ; break;
    case TypeId::kU16    : typeName = "u16"    ; break;
    case TypeId::kI32    : typeName = "i32"    ; break;
    case TypeId::kU32    : typeName = "u32"    ; break;
    case TypeId::kI64    : typeName = "i64"    ; break;
    case TypeId::kU64    : typeName = "u64"    ; break;
    case TypeId::kF32    : typeName = "f32"    ; break;
    case TypeId::kF64    : typeName = "f64"    ; break;
    case TypeId::kF80    : typeName = "f80"    ; break;
    case TypeId::kMask8  : typeName = "mask8"  ; break;
    case TypeId::kMask16 : typeName = "mask16" ; break;
    case TypeId::kMask32 : typeName = "mask32" ; break;
    case TypeId::kMask64 : typeName = "mask64" ; break;
    case TypeId::kMmx32  : typeName = "mmx32"  ; break;
    case TypeId::kMmx64  : typeName = "mmx64"  ; break;
  }

  uint32_t elementSize = TypeId::sizeOf(elementId);
  if (typeSize > elementSize) {
    uint32_t numElements = typeSize / elementSize;
    return sb.appendFormat("%sx%u", typeName, unsigned(numElements));
  }
  else {
    return sb.appendString(typeName);
  }

}

#if !defined(ASMJIT_DISABLE_BUILDER)
static Error formatFuncValue(StringBuilder& sb, uint32_t logOptions, const CodeEmitter* emitter, FuncValue value) noexcept {
  uint32_t typeId = value.getTypeId();
  ASMJIT_PROPAGATE(Logging::formatTypeId(sb, typeId));

  if (value.isReg()) {
    ASMJIT_PROPAGATE(sb.appendChar('@'));
    ASMJIT_PROPAGATE(Logging::formatRegister(sb, logOptions, emitter, emitter->getArchType(), value.getRegType(), value.getRegId()));
  }

  if (value.isStack()) {
    ASMJIT_PROPAGATE(sb.appendFormat("@[%d]", int(value.getStackOffset())));
  }

  return kErrorOk;
}

static Error formatFuncRets(
  StringBuilder& sb,
  uint32_t logOptions,
  const CodeEmitter* emitter,
  const FuncDetail& fd,
  VirtReg* const* vRegs) noexcept {

  if (!fd.hasRet())
    return sb.appendString("void");

  for (uint32_t i = 0; i < fd.getRetCount(); i++) {
    if (i) ASMJIT_PROPAGATE(sb.appendString(", "));
    ASMJIT_PROPAGATE(formatFuncValue(sb, logOptions, emitter, fd.getRet(i)));

#if !defined(ASMJIT_DISABLE_COMPILER)
    if (vRegs) {
      static const char nullRet[] = "<none>";
      ASMJIT_PROPAGATE(sb.appendFormat(" %s", vRegs[i] ? vRegs[i]->getName() : nullRet));
    }
#endif
  }

  return kErrorOk;
}

static Error formatFuncArgs(
  StringBuilder& sb,
  uint32_t logOptions,
  const CodeEmitter* emitter,
  const FuncDetail& fd,
  VirtReg* const* vRegs) noexcept {

  uint32_t count = fd.getArgCount();
  if (!count)
    return sb.appendString("void");

  for (uint32_t i = 0; i < count; i++) {
    if (i) ASMJIT_PROPAGATE(sb.appendString(", "));
    ASMJIT_PROPAGATE(formatFuncValue(sb, logOptions, emitter, fd.getArg(i)));

#if !defined(ASMJIT_DISABLE_COMPILER)
    if (vRegs) {
      static const char nullArg[] = "<none>";
      ASMJIT_PROPAGATE(sb.appendFormat(" %s", vRegs[i] ? vRegs[i]->getName() : nullArg));
    }
#endif
  }

  return kErrorOk;
}

Error Logging::formatNode(
  StringBuilder& sb,
  uint32_t logOptions,
  const CodeBuilder* cb,
  const CBNode* node_) noexcept {

  if (node_->hasPosition() && (logOptions & Logger::kOptionNodePosition) != 0)
    ASMJIT_PROPAGATE(sb.appendFormat("<%05u> ", node_->getPosition()));

  switch (node_->getType()) {
    case CBNode::kNodeInst: {
      const CBInst* node = node_->as<CBInst>();
      ASMJIT_PROPAGATE(
        Logging::formatInstruction(sb, logOptions, cb,
          cb->getArchType(),
          node->getInstDetail(), node->getOpArray(), node->getOpCount()));
      break;
    }

    case CBNode::kNodeLabel: {
      const CBLabel* node = node_->as<CBLabel>();
      ASMJIT_PROPAGATE(sb.appendFormat("L%u:", Operand::unpackId(node->getId())));
      break;
    }

    case CBNode::kNodeData: {
      const CBData* node = node_->as<CBData>();
      ASMJIT_PROPAGATE(sb.appendFormat(".embed (%u bytes)", node->getSize()));
      break;
    }

    case CBNode::kNodeAlign: {
      const CBAlign* node = node_->as<CBAlign>();
      ASMJIT_PROPAGATE(
        sb.appendFormat(".align %u (%s)",
          node->getAlignment(),
          node->getMode() == kAlignCode ? "code" : "data"));
      break;
    }

    case CBNode::kNodeComment: {
      const CBComment* node = node_->as<CBComment>();
      ASMJIT_PROPAGATE(sb.appendFormat("; %s", node->getInlineComment()));
      break;
    }

    case CBNode::kNodeSentinel: {
      const CBSentinel* node = node_->as<CBSentinel>();
      const char* sentinelName = nullptr;

      switch (node->getSentinelType()) {
        case CBSentinel::kSentinelFuncEnd:
          sentinelName = "[FuncEnd]";
          break;

        default:
          sentinelName = "[Sentinel]";
          break;
      }

      ASMJIT_PROPAGATE(sb.appendString(sentinelName));
      break;
    }

#if !defined(ASMJIT_DISABLE_COMPILER)
    case CBNode::kNodeFunc: {
      const CCFunc* node = node_->as<CCFunc>();

      ASMJIT_PROPAGATE(formatLabel(sb, logOptions, cb, node->getId()));
      ASMJIT_PROPAGATE(sb.appendString(": "));

      ASMJIT_PROPAGATE(formatFuncRets(sb, logOptions, cb, node->getDetail(), nullptr));
      ASMJIT_PROPAGATE(sb.appendString(" Func("));
      ASMJIT_PROPAGATE(formatFuncArgs(sb, logOptions, cb, node->getDetail(), node->getArgs()));
      ASMJIT_PROPAGATE(sb.appendString(")"));
      break;
    }

    case CBNode::kNodeFuncRet: {
      const CCFuncRet* node = node_->as<CCFuncRet>();
      ASMJIT_PROPAGATE(sb.appendString("[FuncRet]"));

      for (uint32_t i = 0; i < 2; i++) {
        const Operand_& op = node->_opArray[i];
        if (!op.isNone()) {
          ASMJIT_PROPAGATE(sb.appendString(i == 0 ? " " : ", "));
          ASMJIT_PROPAGATE(formatOperand(sb, logOptions, cb, cb->getArchType(), op));
        }
      }
      break;
    }

    case CBNode::kNodeFuncCall: {
      const CCFuncCall* node = node_->as<CCFuncCall>();
      ASMJIT_PROPAGATE(
        Logging::formatInstruction(sb, logOptions, cb,
          cb->getArchType(),
          node->getInstDetail(), node->getOpArray(), node->getOpCount()));
      break;
    }
#endif

    default: {
      ASMJIT_PROPAGATE(sb.appendFormat("[User:%u]", node_->getType()));
      break;
    }
  }

  return kErrorOk;
}
#endif

Error Logging::formatLine(StringBuilder& sb, const uint8_t* binData, size_t binLen, size_t dispLen, size_t imLen, const char* comment) noexcept {
  size_t currentLen = sb.getLength();
  size_t commentLen = comment ? StringUtils::strLen(comment, kMaxCommentLength) : 0;

  ASMJIT_ASSERT(binLen >= dispLen);
  const size_t kNoBinLen = std::numeric_limits<size_t>::max();

  if ((binLen != 0 && binLen != kNoBinLen) || commentLen) {
    size_t align = kMaxInstLength;
    char sep = ';';

    for (size_t i = (binLen == kNoBinLen); i < 2; i++) {
      size_t begin = sb.getLength();
      ASMJIT_PROPAGATE(sb.padEnd(align));

      if (sep) {
        ASMJIT_PROPAGATE(sb.appendChar(sep));
        ASMJIT_PROPAGATE(sb.appendChar(' '));
      }

      // Append binary data or comment.
      if (i == 0) {
        ASMJIT_PROPAGATE(sb.appendHex(binData, binLen - dispLen - imLen));
        ASMJIT_PROPAGATE(sb.appendChars('.', dispLen * 2));
        ASMJIT_PROPAGATE(sb.appendHex(binData + binLen - imLen, imLen));
        if (commentLen == 0) break;
      }
      else {
        ASMJIT_PROPAGATE(sb.appendString(comment, commentLen));
      }

      currentLen += sb.getLength() - begin;
      align += kMaxBinaryLength;
      sep = '|';
    }
  }

  return sb.appendChar('\n');
}

} // asmjit namespace

// [Api-End]
#include "../asmjit_apiend.h"

// [Guard]
#endif // !ASMJIT_DISABLE_LOGGING
