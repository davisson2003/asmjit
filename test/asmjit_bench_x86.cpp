// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Dependencies]
#include <cstdio>
#include <cstring>

#include "./asmjit.h"
#include "./asmjit_test_misc.h"
#include "./asmjit_test_opcode.h"

using namespace asmjit;

// ============================================================================
// [Configuration]
// ============================================================================

static const uint32_t kNumRepeats = 20;
static const uint32_t kNumIterations = 1500;

// ============================================================================
// [Performance]
// ============================================================================

struct Performance {
  static inline uint32_t now() {
    return OSUtils::getTickCount();
  }

  inline void reset() {
    tick = 0U;
    best = 0xFFFFFFFFU;
  }

  inline uint32_t start() { return (tick = now()); }
  inline uint32_t diff() const { return now() - tick; }

  inline uint32_t end() {
    tick = diff();
    if (best > tick)
      best = tick;
    return tick;
  }

  uint32_t tick;
  uint32_t best;
};

static double mbps(uint32_t time, size_t outputSize) {
  if (!time) return 0.0;

  double bytesTotal = double(outputSize);
  return (bytesTotal * 1000) / (double(time) * 1024 * 1024);
}

// ============================================================================
// [Main]
// ============================================================================

#ifdef ASMJIT_BUILD_X86
static void benchX86(uint32_t archType) {
  CodeHolder code;
  Performance perf;

  X86Assembler a;
  X86Builder cb;
  X86Compiler cc;

  uint32_t r, i;
  const char* archName = archType == ArchInfo::kTypeX86 ? "X86" : "X64";

  size_t asmOutputSize = 0;
  size_t cbOutputSize  = 0;
  size_t ccOutputSize  = 0;

  // --------------------------------------------------------------------------
  // [Bench - Assembler]
  // --------------------------------------------------------------------------

  perf.reset();
  for (r = 0; r < kNumRepeats; r++) {
    asmOutputSize = 0;
    perf.start();
    for (i = 0; i < kNumIterations; i++) {
      code.init(CodeInfo(archType));
      code.attach(&a);

      asmtest::generateOpcodes(a.as<X86Emitter>());
      code.detach(&a);

      asmOutputSize += code.getCodeSize();
      code.reset(false);
    }
    perf.end();
  }

  std::printf("%-12s (%s) | Time: %-6u [ms] | Speed: %7.3f [MB/s]\n",
    "X86Assembler", archName, perf.best, mbps(perf.best, asmOutputSize));

  // --------------------------------------------------------------------------
  // [Bench - CodeBuilder (with finalize)]
  // --------------------------------------------------------------------------

  perf.reset();
  for (r = 0; r < kNumRepeats; r++) {
    cbOutputSize = 0;
    perf.start();
    for (i = 0; i < kNumIterations; i++) {
      code.init(CodeInfo(archType));
      code.attach(&cb);

      asmtest::generateOpcodes(cb.as<X86Emitter>());
      cb.finalize();

      cbOutputSize += code.getCodeSize();
      code.reset(false); // Detaches `cb`.
    }
    perf.end();
  }

  std::printf("%-12s (%s) | Time: %-6u [ms] | Speed: %7.3f [MB/s]\n",
    "X86Builder", archName, perf.best, mbps(perf.best, cbOutputSize));

  // --------------------------------------------------------------------------
  // [Bench - CodeBuilder (without finalize)]
  // --------------------------------------------------------------------------

  perf.reset();
  for (r = 0; r < kNumRepeats; r++) {
    perf.start();
    for (i = 0; i < kNumIterations; i++) {
      code.init(CodeInfo(archType));
      code.attach(&cb);

      asmtest::generateOpcodes(cb.as<X86Emitter>());
      code.reset(false); // Detaches `cb`.
    }
    perf.end();
  }

  std::printf("%-12s (%s) | Time: %-6u [ms] | Speed: N/A\n",
    "X86Builder*", archName, perf.best);

  // --------------------------------------------------------------------------
  // [Bench - CodeCompiler]
  // --------------------------------------------------------------------------

  perf.reset();
  for (r = 0; r < kNumRepeats; r++) {
    ccOutputSize = 0;
    perf.start();
    for (i = 0; i < kNumIterations; i++) {
      // NOTE: Since we don't have JitRuntime we don't know anything about
      // function calling conventions, which is required by generateAlphaBlend.
      // So we must setup this manually.
      CodeInfo ci(archType);
      ci.setCdeclCallConv(archType == ArchInfo::kTypeX86 ? CallConv::kIdX86CDecl : CallConv::kIdX86SysV64);

      code.init(ci);
      code.attach(&cc);

      asmtest::generateAlphaBlend(cc);
      cc.finalize();

      ccOutputSize += code.getCodeSize();
      code.reset(false); // Detaches `cc`.
    }
    perf.end();
  }

  std::printf("%-12s (%s) | Time: %-6u [ms] | Speed: %7.3f [MB/s]\n",
    "X86Compiler", archName, perf.best, mbps(perf.best, ccOutputSize));
}
#endif

int main(int argc, char* argv[]) {
#ifdef ASMJIT_BUILD_X86
  benchX86(ArchInfo::kTypeX86);
  benchX86(ArchInfo::kTypeX64);
#endif

  return 0;
}
