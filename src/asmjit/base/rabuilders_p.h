// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _ASMJIT_BASE_RABUILDERS_P_H
#define _ASMJIT_BASE_RABUILDERS_P_H

#include "../asmjit_build.h"
#if !defined(ASMJIT_DISABLE_COMPILER)

// [Dependencies]
#include "../base/rapass_p.h"

// [Api-Begin]
#include "../asmjit_apibegin.h"

namespace asmjit {

//! \addtogroup asmjit_ra
//! \{

// ============================================================================
// [asmjit::RACFGBuilder]
// ============================================================================

template<typename This>
class RACFGBuilder {
public:
  ASMJIT_INLINE RACFGBuilder(RAPass* pass) noexcept
    : _pass(pass),
      _cc(pass->cc()),
      _block(nullptr) {}

  ASMJIT_INLINE CodeCompiler* cc() const noexcept { return _cc; }

  Error run() noexcept {
    ASMJIT_RA_LOG_INIT(
      Logger* logger = _pass->getDebugLogger();
      StringBuilderTmp<512> sb;
      RABlock* lastPrintedBlock = nullptr;
      uint32_t loggerOptions = Logger::kOptionNodePosition;
    );
    ASMJIT_RA_LOG_FORMAT("[RAPass::BuildCFG]\n");

    CCFunc* func = _pass->getFunc();
    CBNode* node = func;

    bool hasCode = false;
    uint32_t exitLabelId = func->getExitNode()->getId();

    // Create the first (entry) block.
    _block = _pass->newBlock();
    if (ASMJIT_UNLIKELY(!_block))
      return DebugUtils::errored(kErrorNoHeapMemory);
    ASMJIT_PROPAGATE(_pass->addBlock(_block));

    RARegsStats blockRegStats;
    blockRegStats.reset();

    RAInstBuilder ib;

    ASMJIT_RA_LOG_COMPLEX({
      loggerOptions |= logger->getOptions();

      Logging::formatNode(sb, loggerOptions, cc(), node);
      logger->logf("  %s\n", sb.getData());

      lastPrintedBlock = _block;
      logger->logf("  {#%u}\n", lastPrintedBlock->getBlockId());
    });

    node = node->getNext();
    if (ASMJIT_UNLIKELY(!node))
      return DebugUtils::errored(kErrorInvalidState);

    _block->setFirst(node);
    _block->setLast(node);

    for (;;) {
      CBNode* next = node->getNext();
      ASMJIT_ASSERT(!node->hasPosition());

      // Unlikely: Assume there is more instructions than labels by default.
      if (ASMJIT_UNLIKELY(node->getType() == CBNode::kNodeLabel)) {
        if (!_block) {
          // If the current code is unreachable the label makes it reachable again.
          _block = node->getPassData<RABlock>();
          if (_block) {
            // If the label has a block assigned we can either continue with
            // it or skip it if the block has been constructed already.
            if (_block->isConstructed())
              break;
          }
          else {
            // No block assigned, to create a new one, and assign it.
            _block = _pass->newBlock(node);
            if (ASMJIT_UNLIKELY(!_block))
              return DebugUtils::errored(kErrorNoHeapMemory);

            node->setPassData<RABlock>(_block);
            hasCode = false;
            blockRegStats.reset();
          }

          ASMJIT_PROPAGATE(_pass->addBlock(_block));
        }
        else {
          if (node->hasPassData()) {
            RABlock* consecutive = node->getPassData<RABlock>();
            if (_block == consecutive) {
              // The label currently processed is part of the current block. This
              // is only possible for multiple labels that are right next to each
              // other, or are separated by non-code nodes (.align,  comments).
              if (ASMJIT_UNLIKELY(hasCode))
                return DebugUtils::errored(kErrorInvalidState);
            }
            else {
              // Label makes the current block constructed. There is a chance that the
              // Label is not used, but we don't know that at this point. In the worst
              // case there would be two blocks next to each other, it's just fine.
              ASMJIT_ASSERT(_block->getLast() != node);
              _block->setLast(node->getPrev());
              _block->addFlags(RABlock::kFlagHasConsecutive);
              _block->makeConstructed(blockRegStats);

              ASMJIT_PROPAGATE(_block->appendSuccessor(consecutive));
              ASMJIT_PROPAGATE(_pass->addBlock(consecutive));

              _block = consecutive;
              hasCode = false;
              blockRegStats.reset();
            }
          }
          else {
            // First time we see this label.
            if (hasCode) {
              // Cannot continue the current block if it already contains some
              // code. We need to create a new block and make it a successor.
              ASMJIT_ASSERT(_block->getLast() != node);
              _block->setLast(node->getPrev());
              _block->addFlags(RABlock::kFlagHasConsecutive);
              _block->makeConstructed(blockRegStats);

              RABlock* consecutive = _pass->newBlock(node);
              if (ASMJIT_UNLIKELY(!consecutive))
                return DebugUtils::errored(kErrorNoHeapMemory);

              ASMJIT_PROPAGATE(_block->appendSuccessor(consecutive));
              ASMJIT_PROPAGATE(_pass->addBlock(consecutive));

              _block = consecutive;
              hasCode = false;
              blockRegStats.reset();
            }

            node->setPassData<RABlock>(_block);
          }
        }

        ASMJIT_RA_LOG_COMPLEX({
          if (_block && _block != lastPrintedBlock) {
            lastPrintedBlock = _block;
            logger->logf("  {#%u}\n", lastPrintedBlock->getBlockId());
          }

          sb.clear();
          Logging::formatNode(sb, loggerOptions, cc(), node);
          logger->logf("  %s\n", sb.getData());
        });

        // Unlikely: Assume that the exit label is reached only once per function.
        if (ASMJIT_UNLIKELY(node->as<CBLabel>()->getId() == exitLabelId)) {
          _block->setLast(node);
          _block->addFlags(RABlock::kFlagIsFuncExit);
          _block->makeConstructed(blockRegStats);
          ASMJIT_PROPAGATE(_pass->_exits.append(_pass->getAllocator(), _block));

          _block = nullptr;
        }
      }
      else {
        if (node->actsAsInst()) {
          if (ASMJIT_UNLIKELY(!_block)) {
            // If this code is unreachable then it has to be removed.
            ASMJIT_RA_LOG_COMPLEX({
              sb.clear();
              Logging::formatNode(sb, loggerOptions, cc(), node);
              logger->logf("  <Removed> %s\n", sb.getData());
            });
            cc()->removeNode(node);
            node = next;
            continue;
          }
          else {
            // Handle `CBInst`, `CCFuncCall`, and `CCFuncRet`. All of
            // these share the `CBInst` interface and contain operands.
            hasCode = true;

            ASMJIT_RA_LOG_COMPLEX({
              sb.clear();
              Logging::formatNode(sb, loggerOptions, cc(), node);
              logger->logf("    %s\n", sb.getData());
            });

            CBInst* inst = node->as<CBInst>();
            uint32_t controlType = Inst::kControlRegular;

            ib.reset();
            ASMJIT_PROPAGATE(static_cast<This*>(this)->onInst(inst, controlType, ib));

            uint32_t nodeType = inst->getType();
            if (nodeType != CBNode::kNodeInst) {
              if (nodeType == CBNode::kNodeFuncCall) {
                ASMJIT_PROPAGATE(static_cast<This*>(this)->onCall(inst->as<CCFuncCall>(), ib));
              }
              else if (nodeType == CBNode::kNodeFuncRet) {
                ASMJIT_PROPAGATE(static_cast<This*>(this)->onRet(inst->as<CCFuncRet>(), ib));
                controlType = Inst::kControlReturn;
              }
              else {
                return DebugUtils::errored(kErrorInvalidInstruction);
              }
            }

            ASMJIT_PROPAGATE(_pass->assignRAInst(inst, _block, ib));
            blockRegStats.combineWith(ib._stats);

            if (controlType != Inst::kControlRegular) {
              // Support for conditional and unconditional jumps.
              if (controlType == Inst::kControlJump || controlType == Inst::kControlBranch) {
                // Jmp/Jcc/Call/Loop/etc...
                uint32_t opCount = inst->getOpCount();
                const Operand* opArray = inst->getOpArray();

                // The last operand must be label (this supports also instructions
                // like jecx in explicit form).
                if (ASMJIT_UNLIKELY(opCount == 0 || !opArray[opCount - 1].isLabel()))
                  return DebugUtils::errored(kErrorInvalidState);

                CBLabel* cbLabel;
                ASMJIT_PROPAGATE(cc()->getLabelNode(&cbLabel, opArray[opCount - 1].as<Label>()));

                RABlock* targetBlock = _pass->newBlockOrExistingAt(cbLabel);
                if (ASMJIT_UNLIKELY(!targetBlock))
                  return DebugUtils::errored(kErrorNoHeapMemory);

                _block->setLast(node);
                _block->addFlags(RABlock::kFlagHasTerminator);
                _block->makeConstructed(blockRegStats);
                ASMJIT_PROPAGATE(_block->appendSuccessor(targetBlock));

                if (controlType == Inst::kControlJump) {
                  // Unconditional jump makes the code after the jump unreachable,
                  // which will be removed instantly during the CFG construction;
                  // as we cannot allocate registers for instructions that are not
                  // part of any block. Of course we can leave these instructions
                  // as they are, however, that would only postpone the problem as
                  // assemblers can't encode instructions that use virtual registers.
                  _block = nullptr;
                }
                else {
                  node = next;
                  if (ASMJIT_UNLIKELY(!node))
                    return DebugUtils::errored(kErrorInvalidState);

                  RABlock* consecutiveBlock;
                  if (node->getType() == CBNode::kNodeLabel) {
                    if (node->hasPassData()) {
                      consecutiveBlock = node->getPassData<RABlock>();
                    }
                    else {
                      consecutiveBlock = _pass->newBlock(node);
                      if (ASMJIT_UNLIKELY(!consecutiveBlock))
                        return DebugUtils::errored(kErrorNoHeapMemory);
                      node->setPassData<RABlock>(consecutiveBlock);
                    }
                  }
                  else {
                    consecutiveBlock = _pass->newBlock(node);
                    if (ASMJIT_UNLIKELY(!consecutiveBlock))
                      return DebugUtils::errored(kErrorNoHeapMemory);
                  }

                  _block->addFlags(RABlock::kFlagHasConsecutive);
                  ASMJIT_PROPAGATE(_block->prependSuccessor(consecutiveBlock));

                  _block = consecutiveBlock;
                  hasCode = false;
                  blockRegStats.reset();

                  if (_block->isConstructed())
                    break;
                  ASMJIT_PROPAGATE(_pass->addBlock(consecutiveBlock));

                  ASMJIT_RA_LOG_COMPLEX({
                    lastPrintedBlock = _block;
                    logger->logf("  {#%u}\n", lastPrintedBlock->getBlockId());
                  });

                  continue;
                }
              }

              if (controlType == Inst::kControlReturn) {
                _block->setLast(node);
                _block->makeConstructed(blockRegStats);
                ASMJIT_PROPAGATE(_pass->_exits.append(_pass->getAllocator(), _block));

                _block = nullptr;
              }
            }
          }
        }
        else {
          ASMJIT_RA_LOG_COMPLEX({
            sb.clear();
            Logging::formatNode(sb, loggerOptions, cc(), node);
            logger->logf("    %s\n", sb.getData());
          });

          if (node->getType() == CBNode::kNodeSentinel) {
            if (node == func->getEnd()) {
              // Make sure we didn't flow here if this is the end of the function sentinel.
              if (ASMJIT_UNLIKELY(_block))
                return DebugUtils::errored(kErrorInvalidState);
              break;
            }
          }
          else if (node->getType() == CBNode::kNodeFunc) {
            // RAPass can only compile a single function at a time. If we
            // encountered a function it must be the current one, bail if not.
            if (ASMJIT_UNLIKELY(node != func))
              return DebugUtils::errored(kErrorInvalidState);
            // PASS if this is the first node.
          }
          else {
            // PASS if this is a non-interesting or unknown node.
          }
        }
      }

      // Advance to the next node.
      node = next;

      // NOTE: We cannot encounter a NULL node, because every function must be
      // terminated by a sentinel (`stop`) node. If we encountered a NULL node it
      // means that something went wrong and this node list is corrupted; bail in
      // such case.
      if (ASMJIT_UNLIKELY(!node))
        return DebugUtils::errored(kErrorInvalidState);
    }

    if (_pass->hasDanglingBlocks())
      return DebugUtils::errored(kErrorInvalidState);

    return kErrorOk;
  }

  RAPass* _pass;
  CodeCompiler* _cc;
  RABlock* _block;
};

//! \}

} // asmjit namespace

// [Api-End]
#include "../asmjit_apiend.h"

// [Guard]
#endif // !ASMJIT_DISABLE_COMPILER
#endif // _ASMJIT_BASE_RABUILDERS_P_H
