// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Dependencies]
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <setjmp.h>

#include "./asmjit.h"
#include "./asmjit_test_misc.h"

using namespace asmjit;

// ============================================================================
// [CmdLine]
// ============================================================================

class CmdLine {
public:
  CmdLine(int argc, const char* const* argv)
    : _argc(argc),
      _argv(argv) {}

  bool hasArg(const char* arg) {
    for (int i = 1; i < _argc; i++)
      if (std::strcmp(_argv[i], arg) == 0)
        return true;
    return false;
  }

  int _argc;
  const char* const* _argv;
};

// ============================================================================
// [SimpleErrorHandler]
// ============================================================================

class SimpleErrorHandler : public ErrorHandler {
public:
  SimpleErrorHandler() : _err(kErrorOk) {}
  virtual void handleError(Error err, const char* message, CodeEmitter* origin) {
    ASMJIT_UNUSED(origin);
    _err = err;
    _message.setString(message);
  }

  Error _err;
  StringBuilder _message;
};

// ============================================================================
// [X86Test]
// ============================================================================

//! Base test interface for testing `X86Compiler`.
class X86Test {
public:
  X86Test(const char* name = NULL) { _name.setString(name); }
  virtual ~X86Test() {}

  inline const char* getName() const { return _name.getData(); }

  virtual void compile(X86Compiler& c) = 0;
  virtual bool run(void* func, StringBuilder& result, StringBuilder& expect) = 0;

  StringBuilder _name;
};

// ============================================================================
// [X86TestApp]
// ============================================================================

class X86TestApp {
public:
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  X86TestApp()
    : _zone(8096 - Zone::kZoneOverhead),
      _allocator(&_zone),
      _returnCode(0),
      _binSize(0),
      _verbose(false),
      _dumpAsm(false) {}

  ~X86TestApp() {
    uint32_t i;
    uint32_t count = _tests.getLength();

    for (i = 0; i < count; i++) {
      X86Test* test = _tests[i];
      delete test;
    }
  }

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  Error add(X86Test* test) { return _tests.append(&_allocator, test); }
  template<class T> inline void addT() { T::add(*this); }

  int handleArgs(int argc, const char* const* argv);
  void showInfo();
  int run();

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  Zone _zone;
  ZoneAllocator _allocator;
  ZoneVector<X86Test*> _tests;

  int _returnCode;
  int _binSize;

  bool _verbose;
  bool _dumpAsm;
};

int X86TestApp::handleArgs(int argc, const char* const* argv) {
  CmdLine cmd(argc, argv);

  if (cmd.hasArg("--verbose")) _verbose = true;
  if (cmd.hasArg("--dump-asm")) _dumpAsm = true;

  return 0;
}

void X86TestApp::showInfo() {
  printf("AsmJit::X86Compiler Test:\n");
  printf("  [%s] Verbose (use --verbose to turn verbose output ON)\n", _verbose ? "x" : " ");
  printf("  [%s] DumpAsm (use --dump-asm to turn assembler dumps ON)\n", _dumpAsm ? "x" : " ");
}

int X86TestApp::run() {
  uint32_t i;
  uint32_t count = _tests.getLength();
  std::FILE* file = stdout;

#if !defined(ASMJIT_DISABLE_LOGGING)
  uint32_t logOptions = Logger::kOptionBinaryForm    |
                        Logger::kOptionExplainConsts |
                        Logger::kOptionRegCasts      |
                        Logger::kOptionAnnotate      |
                        Logger::kOptionDebugPasses   |
                        Logger::kOptionDebugRA       ;

  FileLogger fileLogger(file);
  fileLogger.addOptions(logOptions);

  StringLogger stringLogger;
  stringLogger.addOptions(logOptions);
#endif

  for (i = 0; i < count; i++) {
    JitRuntime runtime;
    CodeHolder code;
    SimpleErrorHandler errorHandler;

    code.init(runtime.getCodeInfo());
    code.setErrorHandler(&errorHandler);

#if !defined(ASMJIT_DISABLE_LOGGING)
    if (_verbose) {
      code.setLogger(&fileLogger);
    }
    else {
      stringLogger.clearString();
      code.setLogger(&stringLogger);
    }
#endif

    X86Test* test = _tests[i];
    fprintf(file, "[Test] %s", test->getName());

#if !defined(ASMJIT_DISABLE_LOGGING)
    if (_verbose) fprintf(file, "\n");
#endif

    X86Compiler cc(&code);
    test->compile(cc);

    Error err = errorHandler._err;
    if (!err)
      err = cc.finalize();
    void* func;

#if !defined(ASMJIT_DISABLE_LOGGING)
    if (_dumpAsm) {
      if (!_verbose) fprintf(file, "\n");

      StringBuilder sb;
      cc.dump(sb, logOptions);
      fprintf(file, "%s", sb.getData());
    }
#endif

    if (err == kErrorOk)
      err = runtime.add(&func, &code);

    if (_verbose)
      fflush(file);

    if (err == kErrorOk) {
      StringBuilderTmp<128> result;
      StringBuilderTmp<128> expect;

      if (test->run(func, result, expect)) {
        if (!_verbose) fprintf(file, " [OK]\n");
      }
      else {
        if (!_verbose) fprintf(file, " [FAILED]\n");

#if !defined(ASMJIT_DISABLE_LOGGING)
        if (!_verbose) fprintf(file, "%s", stringLogger.getString());
#endif

        fprintf(file, "[Status]\n");
        fprintf(file, "  Returned: %s\n", result.getData());
        fprintf(file, "  Expected: %s\n", expect.getData());

        _returnCode = 1;
      }

      runtime.release(func);
    }
    else {
      if (!_verbose) fprintf(file, " [FAILED]\n");

#if !defined(ASMJIT_DISABLE_LOGGING)
      if (!_verbose) fprintf(file, "%s", stringLogger.getString());
#endif

      fprintf(file, "[Status]\n");
      fprintf(file, "  ERROR 0x%08X: %s\n", unsigned(err), errorHandler._message.getData());

      _returnCode = 1;
    }

    fflush(file);
  }

  fprintf(file, "\n");
  fflush(file);

  return _returnCode;
}

// ============================================================================
// [X86Test_AlignBase]
// ============================================================================

class X86Test_AlignBase : public X86Test {
public:
  X86Test_AlignBase(uint32_t argCount, uint32_t alignment, bool preserveFP)
    : _argCount(argCount),
      _alignment(alignment),
      _preserveFP(preserveFP) {
    _name.setFormat("AlignBase {NumArgs=%u Alignment=%u PreserveFP=%c}", argCount, alignment, preserveFP ? 'Y' : 'N');
  }

  static void add(X86TestApp& app) {
    for (uint32_t i = 0; i <= 16; i++) {
      for (uint32_t a = 16; a <= 32; a += 16) {
        app.add(new X86Test_AlignBase(i, a, true));
        app.add(new X86Test_AlignBase(i, a, false));
      }
    }
  }

  virtual void compile(X86Compiler& cc) {
    uint32_t i;
    uint32_t argCount = _argCount;

    FuncSignatureX signature(CallConv::kIdHost);
    signature.setRetT<int>();
    for (i = 0; i < argCount; i++)
      signature.addArgT<int>();

    cc.addFunc(signature);
    if (_preserveFP)
      cc.getFunc()->getFrame().setPreservedFP();

    X86Gp gpVar = cc.newIntPtr("gpVar");
    X86Gp gpSum;
    X86Mem stack = cc.newStack(_alignment, _alignment);

    // Do a sum of arguments to verify a possible relocation when misaligned.
    if (argCount) {
      for (i = 0; i < argCount; i++) {
        X86Gp gpArg = cc.newInt32("gpArg%u", i);
        cc.setArg(i, gpArg);

        if (i == 0)
          gpSum = gpArg;
        else
          cc.add(gpSum, gpArg);
      }
    }

    // Check alignment of xmmVar (has to be 16).
    cc.lea(gpVar, stack);
    cc.and_(gpVar, _alignment - 1);

    // Add a sum of all arguments to check if they are correct.
    if (argCount)
      cc.or_(gpVar.r32(), gpSum);

    cc.ret(gpVar);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func0)();
    typedef int (*Func1)(int);
    typedef int (*Func2)(int, int);
    typedef int (*Func3)(int, int, int);
    typedef int (*Func4)(int, int, int, int);
    typedef int (*Func5)(int, int, int, int, int);
    typedef int (*Func6)(int, int, int, int, int, int);
    typedef int (*Func7)(int, int, int, int, int, int, int);
    typedef int (*Func8)(int, int, int, int, int, int, int, int);
    typedef int (*Func9)(int, int, int, int, int, int, int, int, int);
    typedef int (*Func10)(int, int, int, int, int, int, int, int, int, int);
    typedef int (*Func11)(int, int, int, int, int, int, int, int, int, int, int);
    typedef int (*Func12)(int, int, int, int, int, int, int, int, int, int, int, int);
    typedef int (*Func13)(int, int, int, int, int, int, int, int, int, int, int, int, int);
    typedef int (*Func14)(int, int, int, int, int, int, int, int, int, int, int, int, int, int);
    typedef int (*Func15)(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int);
    typedef int (*Func16)(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int);

    unsigned int resultRet = 0;
    unsigned int expectRet = 0;

    switch (_argCount) {
      case 0:
        resultRet = ptr_as_func<Func0>(_func)();
        expectRet = 0;
        break;
      case 1:
        resultRet = ptr_as_func<Func1>(_func)(1);
        expectRet = 1;
        break;
      case 2:
        resultRet = ptr_as_func<Func2>(_func)(1, 2);
        expectRet = 1 + 2;
        break;
      case 3:
        resultRet = ptr_as_func<Func3>(_func)(1, 2, 3);
        expectRet = 1 + 2 + 3;
        break;
      case 4:
        resultRet = ptr_as_func<Func4>(_func)(1, 2, 3, 4);
        expectRet = 1 + 2 + 3 + 4;
        break;
      case 5:
        resultRet = ptr_as_func<Func5>(_func)(1, 2, 3, 4, 5);
        expectRet = 1 + 2 + 3 + 4 + 5;
        break;
      case 6:
        resultRet = ptr_as_func<Func6>(_func)(1, 2, 3, 4, 5, 6);
        expectRet = 1 + 2 + 3 + 4 + 5 + 6;
        break;
      case 7:
        resultRet = ptr_as_func<Func7>(_func)(1, 2, 3, 4, 5, 6, 7);
        expectRet = 1 + 2 + 3 + 4 + 5 + 6 + 7;
        break;
      case 8:
        resultRet = ptr_as_func<Func8>(_func)(1, 2, 3, 4, 5, 6, 7, 8);
        expectRet = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8;
        break;
      case 9:
        resultRet = ptr_as_func<Func9>(_func)(1, 2, 3, 4, 5, 6, 7, 8, 9);
        expectRet = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9;
        break;
      case 10:
        resultRet = ptr_as_func<Func10>(_func)(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
        expectRet = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10;
        break;
      case 11:
        resultRet = ptr_as_func<Func11>(_func)(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);
        expectRet = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11;
        break;
      case 12:
        resultRet = ptr_as_func<Func12>(_func)(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
        expectRet = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12;
        break;
      case 13:
        resultRet = ptr_as_func<Func13>(_func)(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13);
        expectRet = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12 + 13;
        break;
      case 14:
        resultRet = ptr_as_func<Func14>(_func)(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14);
        expectRet = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12 + 13 + 14;
        break;
      case 15:
        resultRet = ptr_as_func<Func15>(_func)(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
        expectRet = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12 + 13 + 14 + 15;
        break;
      case 16:
        resultRet = ptr_as_func<Func16>(_func)(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        expectRet = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12 + 13 + 14 + 15 + 16;
        break;
    }

    result.setFormat("ret={%u, %u}", resultRet >> 28, resultRet & 0x0FFFFFFFU);
    expect.setFormat("ret={%u, %u}", expectRet >> 28, expectRet & 0x0FFFFFFFU);

    return resultRet == expectRet;
  }

  uint32_t _argCount;
  uint32_t _alignment;
  bool _preserveFP;
};

// ============================================================================
// [X86Test_NoCode]
// ============================================================================

class X86Test_NoCode : public X86Test {
public:
  X86Test_NoCode() : X86Test("NoCode") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_NoCode());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void>(CallConv::kIdHost));
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void(*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    func();
    return true;
  }
};

// ============================================================================
// [X86Test_AlignNone]
// ============================================================================

class X86Test_NoAlign : public X86Test {
public:
  X86Test_NoAlign() : X86Test("NoAlign") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_NoAlign());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void>(CallConv::kIdHost));
    cc.align(kAlignCode, 0);
    cc.align(kAlignCode, 1);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    func();
    return true;
  }
};

// ============================================================================
// [X86Test_JumpMerge]
// ============================================================================

class X86Test_JumpMerge : public X86Test {
public:
  X86Test_JumpMerge() : X86Test("JumpMerge") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_JumpMerge());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void, int*, int>(CallConv::kIdHost));

    Label L0 = cc.newLabel();
    Label L1 = cc.newLabel();
    Label L2 = cc.newLabel();
    Label LEnd = cc.newLabel();

    X86Gp dst = cc.newIntPtr("dst");
    X86Gp val = cc.newIntPtr("val");

    cc.setArg(0, dst);
    cc.setArg(1, val);

    cc.cmp(val, 0);
    cc.je(L0);

    cc.cmp(val, 1);
    cc.je(L1);

    cc.cmp(val, 2);
    cc.je(L2);

    cc.mov(x86::dword_ptr(dst), val);
    cc.jmp(LEnd);

    // On purpose. This tests whether the CFG constructs a single basic-block
    // from multiple labels next to each other.
    cc.bind(L0);
    cc.bind(L1);
    cc.bind(L2);
    cc.mov(x86::dword_ptr(dst), 0);

    cc.bind(LEnd);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void(*Func)(int*, int);
    Func func = ptr_as_func<Func>(_func);

    int arr[5] = { -1, -1, -1, -1, -1 };
    int exp[5] = {  0,  0,  0,  3,  4 };

    for (int i = 0; i < 5; i++)
      func(&arr[i], i);

    result.setFormat("ret={%d, %d, %d, %d, %d}", arr[0], arr[1], arr[2], arr[3], arr[4]);
    expect.setFormat("ret={%d, %d, %d, %d, %d}", exp[0], exp[1], exp[2], exp[3], exp[4]);

    return true;
  }
};

// ============================================================================
// [X86Test_JumpCross]
// ============================================================================

class X86Test_JumpCross : public X86Test {
public:
  X86Test_JumpCross() : X86Test("JumpCross") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_JumpCross());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void>(CallConv::kIdHost));

    Label L1 = cc.newLabel();
    Label L2 = cc.newLabel();
    Label L3 = cc.newLabel();

    cc.jmp(L2);

    cc.bind(L1);
    cc.jmp(L3);

    cc.bind(L2);
    cc.jmp(L1);

    cc.bind(L3);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    func();
    return true;
  }
};

// ============================================================================
// [X86Test_JumpMany]
// ============================================================================

class X86Test_JumpMany : public X86Test {
public:
  X86Test_JumpMany() : X86Test("JumpMany") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_JumpMany());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int>(CallConv::kIdHost));
    for (uint32_t i = 0; i < 1000; i++) {
      Label L = cc.newLabel();
      cc.jmp(L);
      cc.bind(L);
    }

    X86Gp ret = cc.newInt32("ret");
    cc.xor_(ret, ret);
    cc.ret(ret);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(void);

    Func func = ptr_as_func<Func>(_func);

    int resultRet = func();
    int expectRet = 0;

    result.setFormat("ret={%d}", resultRet);
    expect.setFormat("ret={%d}", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_JumpUnreachable1]
// ============================================================================

class X86Test_JumpUnreachable1 : public X86Test {
public:
  X86Test_JumpUnreachable1() : X86Test("JumpUnreachable1") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_JumpUnreachable1());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void>(CallConv::kIdHost));

    Label L_1 = cc.newLabel();
    Label L_2 = cc.newLabel();
    Label L_3 = cc.newLabel();
    Label L_4 = cc.newLabel();
    Label L_5 = cc.newLabel();
    Label L_6 = cc.newLabel();
    Label L_7 = cc.newLabel();

    X86Gp v0 = cc.newUInt32("v0");
    X86Gp v1 = cc.newUInt32("v1");

    cc.bind(L_2);
    cc.bind(L_3);

    cc.jmp(L_1);

    cc.bind(L_5);
    cc.mov(v0, 0);

    cc.bind(L_6);
    cc.jmp(L_3);
    cc.mov(v1, 1);
    cc.jmp(L_1);

    cc.bind(L_4);
    cc.jmp(L_2);
    cc.bind(L_7);
    cc.add(v0, v1);

    cc.align(kAlignCode, 16);
    cc.bind(L_1);
    cc.ret();
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    func();

    result.appendString("ret={}");
    expect.appendString("ret={}");

    return true;
  }
};

// ============================================================================
// [X86Test_JumpUnreachable2]
// ============================================================================

class X86Test_JumpUnreachable2 : public X86Test {
public:
  X86Test_JumpUnreachable2() : X86Test("JumpUnreachable2") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_JumpUnreachable2());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void>(CallConv::kIdHost));

    Label L_1 = cc.newLabel();
    Label L_2 = cc.newLabel();

    X86Gp v0 = cc.newUInt32("v0");
    X86Gp v1 = cc.newUInt32("v1");

    cc.jmp(L_1);
    cc.bind(L_2);
    cc.mov(v0, 1);
    cc.mov(v1, 2);
    cc.cmp(v0, v1);
    cc.jz(L_2);
    cc.jmp(L_1);

    cc.bind(L_1);
    cc.ret();
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    func();

    result.appendString("ret={}");
    expect.appendString("ret={}");

    return true;
  }
};

// ============================================================================
// [X86Test_AllocBase]
// ============================================================================

class X86Test_AllocBase : public X86Test {
public:
  X86Test_AllocBase() : X86Test("AllocBase") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocBase());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int>(CallConv::kIdHost));

    X86Gp v0 = cc.newInt32("v0");
    X86Gp v1 = cc.newInt32("v1");
    X86Gp v2 = cc.newInt32("v2");
    X86Gp v3 = cc.newInt32("v3");
    X86Gp v4 = cc.newInt32("v4");

    cc.xor_(v0, v0);

    cc.mov(v1, 1);
    cc.mov(v2, 2);
    cc.mov(v3, 3);
    cc.mov(v4, 4);

    cc.add(v0, v1);
    cc.add(v0, v2);
    cc.add(v0, v3);
    cc.add(v0, v4);

    cc.ret(v0);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func();
    int expectRet = 1 + 2 + 3 + 4;

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_AllocMany1]
// ============================================================================

class X86Test_AllocMany1 : public X86Test {
public:
  X86Test_AllocMany1() : X86Test("AllocMany1") {}

  enum { kCount = 8 };

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocMany1());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void, int*, int*>(CallConv::kIdHost));

    X86Gp a0 = cc.newIntPtr("a0");
    X86Gp a1 = cc.newIntPtr("a1");

    cc.setArg(0, a0);
    cc.setArg(1, a1);

    // Create some variables.
    X86Gp t = cc.newInt32("t");
    X86Gp x[kCount];

    uint32_t i;

    // Setup variables (use mov with reg/imm to se if register allocator works).
    for (i = 0; i < kCount; i++) x[i] = cc.newInt32("x%u", i);
    for (i = 0; i < kCount; i++) cc.mov(x[i], int(i + 1));

    // Make sum (addition).
    cc.xor_(t, t);
    for (i = 0; i < kCount; i++) cc.add(t, x[i]);

    // Store result to a given pointer in first argument.
    cc.mov(x86::dword_ptr(a0), t);

    // Clear t.
    cc.xor_(t, t);

    // Make sum (subtraction).
    for (i = 0; i < kCount; i++) cc.sub(t, x[i]);

    // Store result to a given pointer in second argument.
    cc.mov(x86::dword_ptr(a1), t);

    // End of function.
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(int*, int*);
    Func func = ptr_as_func<Func>(_func);

    int resultX;
    int resultY;

    int expectX =  36;
    int expectY = -36;

    func(&resultX, &resultY);

    result.setFormat("ret={x=%d, y=%d}", resultX, resultY);
    expect.setFormat("ret={x=%d, y=%d}", expectX, expectY);

    return resultX == expectX && resultY == expectY;
  }
};

// ============================================================================
// [X86Test_AllocMany2]
// ============================================================================

class X86Test_AllocMany2 : public X86Test {
public:
  X86Test_AllocMany2() : X86Test("AllocMany2") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocMany2());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void, int*>(CallConv::kIdHost));

    X86Gp a = cc.newIntPtr("a");
    X86Gp v[32];

    uint32_t i;
    cc.setArg(0, a);

    for (i = 0; i < ASMJIT_ARRAY_SIZE(v); i++) v[i] = cc.newInt32("v%d", i);
    for (i = 0; i < ASMJIT_ARRAY_SIZE(v); i++) cc.xor_(v[i], v[i]);

    X86Gp x = cc.newInt32("x");
    Label L = cc.newLabel();

    cc.mov(x, 32);
    cc.bind(L);
    for (i = 0; i < ASMJIT_ARRAY_SIZE(v); i++) cc.add(v[i], i);

    cc.dec(x);
    cc.jnz(L);
    for (i = 0; i < ASMJIT_ARRAY_SIZE(v); i++) cc.mov(x86::dword_ptr(a, i * 4), v[i]);

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(int*);
    Func func = ptr_as_func<Func>(_func);

    int i;
    int resultBuf[32];
    int expectBuf[32];

    for (i = 0; i < ASMJIT_ARRAY_SIZE(resultBuf); i++)
      expectBuf[i] = i * 32;
    func(resultBuf);

    for (i = 0; i < ASMJIT_ARRAY_SIZE(resultBuf); i++) {
      if (i != 0) {
        result.appendChar(',');
        expect.appendChar(',');
      }

      result.appendFormat("%d", resultBuf[i]);
      expect.appendFormat("%d", expectBuf[i]);
    }

    return result == expect;
  }
};

// ============================================================================
// [X86Test_AllocImul1]
// ============================================================================

class X86Test_AllocImul1 : public X86Test {
public:
  X86Test_AllocImul1() : X86Test("AllocImul1") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocImul1());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void, int*, int*, int, int>(CallConv::kIdHost));

    X86Gp dstHi = cc.newIntPtr("dstHi");
    X86Gp dstLo = cc.newIntPtr("dstLo");

    X86Gp vHi = cc.newInt32("vHi");
    X86Gp vLo = cc.newInt32("vLo");
    X86Gp src = cc.newInt32("src");

    cc.setArg(0, dstHi);
    cc.setArg(1, dstLo);
    cc.setArg(2, vLo);
    cc.setArg(3, src);

    cc.imul(vHi, vLo, src);

    cc.mov(x86::dword_ptr(dstHi), vHi);
    cc.mov(x86::dword_ptr(dstLo), vLo);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(int*, int*, int, int);
    Func func = ptr_as_func<Func>(_func);

    int v0 = 4;
    int v1 = 4;

    int resultHi;
    int resultLo;

    int expectHi = 0;
    int expectLo = v0 * v1;

    func(&resultHi, &resultLo, v0, v1);

    result.setFormat("hi=%d, lo=%d", resultHi, resultLo);
    expect.setFormat("hi=%d, lo=%d", expectHi, expectLo);

    return resultHi == expectHi && resultLo == expectLo;
  }
};

// ============================================================================
// [X86Test_AllocImul2]
// ============================================================================

class X86Test_AllocImul2 : public X86Test {
public:
  X86Test_AllocImul2() : X86Test("AllocImul2") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocImul2());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void, int*, const int*>(CallConv::kIdHost));

    X86Gp dst = cc.newIntPtr("dst");
    X86Gp src = cc.newIntPtr("src");

    cc.setArg(0, dst);
    cc.setArg(1, src);

    for (unsigned int i = 0; i < 4; i++) {
      X86Gp x  = cc.newInt32("x");
      X86Gp y  = cc.newInt32("y");
      X86Gp hi = cc.newInt32("hi");

      cc.mov(x, x86::dword_ptr(src, 0));
      cc.mov(y, x86::dword_ptr(src, 4));

      cc.imul(hi, x, y);
      cc.add(x86::dword_ptr(dst, 0), hi);
      cc.add(x86::dword_ptr(dst, 4), x);
    }

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(int*, const int*);
    Func func = ptr_as_func<Func>(_func);

    int src[2] = { 4, 9 };
    int resultRet[2] = { 0, 0 };
    int expectRet[2] = { 0, (4 * 9) * 4 };

    func(resultRet, src);

    result.setFormat("ret={%d, %d}", resultRet[0], resultRet[1]);
    expect.setFormat("ret={%d, %d}", expectRet[0], expectRet[1]);

    return resultRet[0] == expectRet[0] && resultRet[1] == expectRet[1];
  }
};

// ============================================================================
// [X86Test_AllocIdiv1]
// ============================================================================

class X86Test_AllocIdiv1 : public X86Test {
public:
  X86Test_AllocIdiv1() : X86Test("AllocIdiv1") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocIdiv1());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int, int, int>(CallConv::kIdHost));

    X86Gp a = cc.newInt32("a");
    X86Gp b = cc.newInt32("b");
    X86Gp dummy = cc.newInt32("dummy");

    cc.setArg(0, a);
    cc.setArg(1, b);

    cc.xor_(dummy, dummy);
    cc.idiv(dummy, a, b);

    cc.ret(a);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int, int);
    Func func = ptr_as_func<Func>(_func);

    int v0 = 2999;
    int v1 = 245;

    int resultRet = func(v0, v1);
    int expectRet = 2999 / 245;

    result.setFormat("result=%d", resultRet);
    expect.setFormat("result=%d", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_AllocSetz]
// ============================================================================

class X86Test_AllocSetz : public X86Test {
public:
  X86Test_AllocSetz() : X86Test("AllocSetz") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocSetz());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void, int, int, char*>(CallConv::kIdHost));

    X86Gp src0 = cc.newInt32("src0");
    X86Gp src1 = cc.newInt32("src1");
    X86Gp dst0 = cc.newIntPtr("dst0");

    cc.setArg(0, src0);
    cc.setArg(1, src1);
    cc.setArg(2, dst0);

    cc.cmp(src0, src1);
    cc.setz(x86::byte_ptr(dst0));

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(int, int, char*);
    Func func = ptr_as_func<Func>(_func);

    char resultBuf[4];
    char expectBuf[4] = { 1, 0, 0, 1 };

    func(0, 0, &resultBuf[0]); // We are expecting 1 (0 == 0).
    func(0, 1, &resultBuf[1]); // We are expecting 0 (0 != 1).
    func(1, 0, &resultBuf[2]); // We are expecting 0 (1 != 0).
    func(1, 1, &resultBuf[3]); // We are expecting 1 (1 == 1).

    result.setFormat("out={%d, %d, %d, %d}", resultBuf[0], resultBuf[1], resultBuf[2], resultBuf[3]);
    expect.setFormat("out={%d, %d, %d, %d}", expectBuf[0], expectBuf[1], expectBuf[2], expectBuf[3]);

    return resultBuf[0] == expectBuf[0] &&
           resultBuf[1] == expectBuf[1] &&
           resultBuf[2] == expectBuf[2] &&
           resultBuf[3] == expectBuf[3] ;
  }
};

// ============================================================================
// [X86Test_AllocShlRor]
// ============================================================================

class X86Test_AllocShlRor : public X86Test {
public:
  X86Test_AllocShlRor() : X86Test("AllocShlRor") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocShlRor());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void, int*, int, int, int>(CallConv::kIdHost));

    X86Gp dst = cc.newIntPtr("dst");
    X86Gp var = cc.newInt32("var");
    X86Gp vShlParam = cc.newInt32("vShlParam");
    X86Gp vRorParam = cc.newInt32("vRorParam");

    cc.setArg(0, dst);
    cc.setArg(1, var);
    cc.setArg(2, vShlParam);
    cc.setArg(3, vRorParam);

    cc.shl(var, vShlParam);
    cc.ror(var, vRorParam);

    cc.mov(x86::dword_ptr(dst), var);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(int*, int, int, int);
    Func func = ptr_as_func<Func>(_func);

    int v0 = 0x000000FF;

    int resultRet;
    int expectRet = 0x0000FF00;

    func(&resultRet, v0, 16, 8);

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_AllocGpbLo]
// ============================================================================

class X86Test_AllocGpbLo : public X86Test {
public:
  X86Test_AllocGpbLo() : X86Test("AllocGpbLo") {}

  enum { kCount = 32 };

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocGpbLo());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<uint32_t, uint32_t*>(CallConv::kIdHost));

    X86Gp rPtr = cc.newUIntPtr("rPtr");
    X86Gp rSum = cc.newUInt32("rSum");

    cc.setArg(0, rPtr);

    X86Gp x[kCount];
    uint32_t i;

    for (i = 0; i < kCount; i++) {
      x[i] = cc.newUInt32("x%u", i);
    }

    // Init pseudo-regs with values from our array.
    for (i = 0; i < kCount; i++) {
      cc.mov(x[i], x86::dword_ptr(rPtr, i * 4));
    }

    for (i = 2; i < kCount; i++) {
      // Add and truncate to 8 bit; no purpose, just mess with jit.
      cc.add  (x[i  ], x[i-1]);
      cc.movzx(x[i  ], x[i  ].r8());
      cc.movzx(x[i-2], x[i-1].r8());
      cc.movzx(x[i-1], x[i-2].r8());
    }

    // Sum up all computed values.
    cc.mov(rSum, 0);
    for (i = 0; i < kCount; i++) {
      cc.add(rSum, x[i]);
    }

    // Return the sum.
    cc.ret(rSum);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(uint32_t*);
    Func func = ptr_as_func<Func>(_func);

    unsigned int i;

    uint32_t buf[kCount];
    uint32_t resultRet;
    uint32_t expectRet;

    expectRet = 0;
    for (i = 0; i < kCount; i++) {
      buf[i] = 1;
    }

    for (i = 2; i < kCount; i++) {
      buf[i  ]+= buf[i-1];
      buf[i  ] = buf[i  ] & 0xFF;
      buf[i-2] = buf[i-1] & 0xFF;
      buf[i-1] = buf[i-2] & 0xFF;
    }

    for (i = 0; i < kCount; i++) {
      expectRet += buf[i];
    }

    for (i = 0; i < kCount; i++) {
      buf[i] = 1;
    }
    resultRet = func(buf);

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_AllocRepMovsb]
// ============================================================================

class X86Test_AllocRepMovsb : public X86Test {
public:
  X86Test_AllocRepMovsb() : X86Test("AllocRepMovsb") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocRepMovsb());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void, void*, void*, size_t>(CallConv::kIdHost));

    X86Gp dst = cc.newIntPtr("dst");
    X86Gp src = cc.newIntPtr("src");
    X86Gp cnt = cc.newIntPtr("cnt");

    cc.setArg(0, dst);
    cc.setArg(1, src);
    cc.setArg(2, cnt);

    cc.rep(cnt).movs(x86::byte_ptr(dst), x86::byte_ptr(src));
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(void*, void*, size_t);
    Func func = ptr_as_func<Func>(_func);

    char dst[20] = { 0 };
    char src[20] = "Hello AsmJit!";
    func(dst, src, strlen(src) + 1);

    result.setFormat("ret=\"%s\"", dst);
    expect.setFormat("ret=\"%s\"", src);

    return result == expect;
  }
};

// ============================================================================
// [X86Test_AllocIfElse1]
// ============================================================================

class X86Test_AllocIfElse1 : public X86Test {
public:
  X86Test_AllocIfElse1() : X86Test("AllocIfElse1") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocIfElse1());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int, int, int>(CallConv::kIdHost));

    X86Gp v1 = cc.newInt32("v1");
    X86Gp v2 = cc.newInt32("v2");

    Label L_1 = cc.newLabel();
    Label L_2 = cc.newLabel();

    cc.setArg(0, v1);
    cc.setArg(1, v2);

    cc.cmp(v1, v2);
    cc.jg(L_1);

    cc.mov(v1, 1);
    cc.jmp(L_2);

    cc.bind(L_1);
    cc.mov(v1, 2);

    cc.bind(L_2);
    cc.ret(v1);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int, int);
    Func func = ptr_as_func<Func>(_func);

    int a = func(0, 1);
    int b = func(1, 0);

    result.appendFormat("ret={%d, %d}", a, b);
    result.appendFormat("ret={%d, %d}", 1, 2);

    return a == 1 && b == 2;
  }
};

// ============================================================================
// [X86Test_AllocIfElse2]
// ============================================================================

class X86Test_AllocIfElse2 : public X86Test {
public:
  X86Test_AllocIfElse2() : X86Test("AllocIfElse2") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocIfElse2());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int, int, int>(CallConv::kIdHost));

    X86Gp v1 = cc.newInt32("v1");
    X86Gp v2 = cc.newInt32("v2");

    Label L_1 = cc.newLabel();
    Label L_2 = cc.newLabel();
    Label L_3 = cc.newLabel();
    Label L_4 = cc.newLabel();

    cc.setArg(0, v1);
    cc.setArg(1, v2);

    cc.jmp(L_1);
    cc.bind(L_2);
    cc.jmp(L_4);
    cc.bind(L_1);

    cc.cmp(v1, v2);
    cc.jg(L_3);

    cc.mov(v1, 1);
    cc.jmp(L_2);

    cc.bind(L_3);
    cc.mov(v1, 2);
    cc.jmp(L_2);

    cc.bind(L_4);

    cc.ret(v1);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int, int);
    Func func = ptr_as_func<Func>(_func);

    int a = func(0, 1);
    int b = func(1, 0);

    result.appendFormat("ret={%d, %d}", a, b);
    result.appendFormat("ret={%d, %d}", 1, 2);

    return a == 1 && b == 2;
  }
};

// ============================================================================
// [X86Test_AllocIfElse3]
// ============================================================================

class X86Test_AllocIfElse3 : public X86Test {
public:
  X86Test_AllocIfElse3() : X86Test("AllocIfElse3") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocIfElse3());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int, int, int>(CallConv::kIdHost));

    X86Gp v1 = cc.newInt32("v1");
    X86Gp v2 = cc.newInt32("v2");
    X86Gp counter = cc.newInt32("counter");

    Label L_1 = cc.newLabel();
    Label L_Loop = cc.newLabel();
    Label L_Exit = cc.newLabel();

    cc.setArg(0, v1);
    cc.setArg(1, v2);

    cc.cmp(v1, v2);
    cc.jg(L_1);

    cc.mov(counter, 0);

    cc.bind(L_Loop);
    cc.mov(v1, counter);

    cc.inc(counter);
    cc.cmp(counter, 1);
    cc.jle(L_Loop);
    cc.jmp(L_Exit);

    cc.bind(L_1);
    cc.mov(v1, 2);

    cc.bind(L_Exit);
    cc.ret(v1);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int, int);
    Func func = ptr_as_func<Func>(_func);

    int a = func(0, 1);
    int b = func(1, 0);

    result.appendFormat("ret={%d, %d}", a, b);
    result.appendFormat("ret={%d, %d}", 1, 2);

    return a == 1 && b == 2;
  }
};

// ============================================================================
// [X86Test_AllocIfElse4]
// ============================================================================

class X86Test_AllocIfElse4 : public X86Test {
public:
  X86Test_AllocIfElse4() : X86Test("AllocIfElse4") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocIfElse4());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int, int, int>(CallConv::kIdHost));

    X86Gp v1 = cc.newInt32("v1");
    X86Gp v2 = cc.newInt32("v2");
    X86Gp counter = cc.newInt32("counter");

    Label L_1 = cc.newLabel();
    Label L_Loop1 = cc.newLabel();
    Label L_Loop2 = cc.newLabel();
    Label L_Exit = cc.newLabel();

    cc.mov(counter, 0);

    cc.setArg(0, v1);
    cc.setArg(1, v2);

    cc.cmp(v1, v2);
    cc.jg(L_1);

    cc.bind(L_Loop1);
    cc.mov(v1, counter);

    cc.inc(counter);
    cc.cmp(counter, 1);
    cc.jle(L_Loop1);
    cc.jmp(L_Exit);

    cc.bind(L_1);
    cc.bind(L_Loop2);
    cc.mov(v1, counter);
    cc.inc(counter);
    cc.cmp(counter, 2);
    cc.jle(L_Loop2);

    cc.bind(L_Exit);
    cc.ret(v1);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int, int);
    Func func = ptr_as_func<Func>(_func);

    int a = func(0, 1);
    int b = func(1, 0);

    result.appendFormat("ret={%d, %d}", a, b);
    result.appendFormat("ret={%d, %d}", 1, 2);

    return a == 1 && b == 2;
  }
};

// ============================================================================
// [X86Test_AllocInt8]
// ============================================================================

class X86Test_AllocInt8 : public X86Test {
public:
  X86Test_AllocInt8() : X86Test("AllocInt8") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocInt8());
  }

  virtual void compile(X86Compiler& cc) {
    X86Gp x = cc.newInt8("x");
    X86Gp y = cc.newInt32("y");

    cc.addFunc(FuncSignatureT<int, char>(CallConv::kIdHost));
    cc.setArg(0, x);

    cc.movsx(y, x);

    cc.ret(y);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(char);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func(-13);
    int expectRet = -13;

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_AllocUnhandledArg]
// ============================================================================

class X86Test_AllocUnhandledArg : public X86Test {
public:
  X86Test_AllocUnhandledArg() : X86Test("AllocUnhandledArg") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocUnhandledArg());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int, int, int, int>(CallConv::kIdHost));

    X86Gp x = cc.newInt32("x");
    cc.setArg(2, x);
    cc.ret(x);

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int, int, int);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func(42, 155, 199);
    int expectRet = 199;

    result.setFormat("ret={%d}", resultRet);
    expect.setFormat("ret={%d}", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_AllocArgsIntPtr]
// ============================================================================

class X86Test_AllocArgsIntPtr : public X86Test {
public:
  X86Test_AllocArgsIntPtr() : X86Test("AllocArgsIntPtr") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocArgsIntPtr());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void, void*, void*, void*, void*, void*, void*, void*, void*>(CallConv::kIdHost));

    uint32_t i;
    X86Gp var[8];

    for (i = 0; i < 8; i++) {
      var[i] = cc.newIntPtr("var%u", i);
      cc.setArg(i, var[i]);
    }

    for (i = 0; i < 8; i++) {
      cc.add(var[i], int(i + 1));
    }

    // Move some data into buffer provided by arguments so we can verify if it
    // really works without looking into assembler output.
    for (i = 0; i < 8; i++) {
      cc.add(x86::byte_ptr(var[i]), int(i + 1));
    }

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(void*, void*, void*, void*, void*, void*, void*, void*);
    Func func = ptr_as_func<Func>(_func);

    uint8_t resultBuf[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t expectBuf[9] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };

    func(resultBuf, resultBuf, resultBuf, resultBuf,
         resultBuf, resultBuf, resultBuf, resultBuf);

    result.setFormat("buf={%d, %d, %d, %d, %d, %d, %d, %d, %d}",
      resultBuf[0], resultBuf[1], resultBuf[2], resultBuf[3],
      resultBuf[4], resultBuf[5], resultBuf[6], resultBuf[7],
      resultBuf[8]);
    expect.setFormat("buf={%d, %d, %d, %d, %d, %d, %d, %d, %d}",
      expectBuf[0], expectBuf[1], expectBuf[2], expectBuf[3],
      expectBuf[4], expectBuf[5], expectBuf[6], expectBuf[7],
      expectBuf[8]);

    return result == expect;
  }
};

// ============================================================================
// [X86Test_AllocArgsFloat]
// ============================================================================

class X86Test_AllocArgsFloat : public X86Test {
public:
  X86Test_AllocArgsFloat() : X86Test("AllocArgsFloat") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocArgsFloat());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void, float, float, float, float, float, float, float, void*>(CallConv::kIdHost));

    uint32_t i;

    X86Gp p = cc.newIntPtr("p");
    X86Xmm xv[7];

    for (i = 0; i < 7; i++) {
      xv[i] = cc.newXmmSs("xv%u", i);
      cc.setArg(i, xv[i]);
    }

    cc.setArg(7, p);

    cc.addss(xv[0], xv[1]);
    cc.addss(xv[0], xv[2]);
    cc.addss(xv[0], xv[3]);
    cc.addss(xv[0], xv[4]);
    cc.addss(xv[0], xv[5]);
    cc.addss(xv[0], xv[6]);

    cc.movss(x86::ptr(p), xv[0]);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(float, float, float, float, float, float, float, float*);
    Func func = ptr_as_func<Func>(_func);

    float resultRet;
    float expectRet = 1.0f + 2.0f + 3.0f + 4.0f + 5.0f + 6.0f + 7.0f;

    func(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, &resultRet);

    result.setFormat("ret={%g}", resultRet);
    expect.setFormat("ret={%g}", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_AllocArgsDouble]
// ============================================================================

class X86Test_AllocArgsDouble : public X86Test {
public:
  X86Test_AllocArgsDouble() : X86Test("AllocArgsDouble") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocArgsDouble());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<void, double, double, double, double, double, double, double, void*>(CallConv::kIdHost));

    uint32_t i;

    X86Gp p = cc.newIntPtr("p");
    X86Xmm xv[7];

    for (i = 0; i < 7; i++) {
      xv[i] = cc.newXmmSd("xv%u", i);
      cc.setArg(i, xv[i]);
    }

    cc.setArg(7, p);

    cc.addsd(xv[0], xv[1]);
    cc.addsd(xv[0], xv[2]);
    cc.addsd(xv[0], xv[3]);
    cc.addsd(xv[0], xv[4]);
    cc.addsd(xv[0], xv[5]);
    cc.addsd(xv[0], xv[6]);

    cc.movsd(x86::ptr(p), xv[0]);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(double, double, double, double, double, double, double, double*);
    Func func = ptr_as_func<Func>(_func);

    double resultRet;
    double expectRet = 1.0 + 2.0 + 3.0 + 4.0 + 5.0 + 6.0 + 7.0;

    func(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, &resultRet);

    result.setFormat("ret={%g}", resultRet);
    expect.setFormat("ret={%g}", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_AllocRetFloat1]
// ============================================================================

class X86Test_AllocRetFloat1 : public X86Test {
public:
  X86Test_AllocRetFloat1() : X86Test("AllocRetFloat1") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocRetFloat1());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<float, float>(CallConv::kIdHost));

    X86Xmm x = cc.newXmmSs("x");
    cc.setArg(0, x);
    cc.ret(x);

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef float (*Func)(float);
    Func func = ptr_as_func<Func>(_func);

    float resultRet = func(42.0f);
    float expectRet = 42.0f;

    result.setFormat("ret={%g}", resultRet);
    expect.setFormat("ret={%g}", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_AllocRetFloat2]
// ============================================================================

class X86Test_AllocRetFloat2 : public X86Test {
public:
  X86Test_AllocRetFloat2() : X86Test("AllocRetFloat2") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocRetFloat2());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<float, float, float>(CallConv::kIdHost));

    X86Xmm x = cc.newXmmSs("x");
    X86Xmm y = cc.newXmmSs("y");

    cc.setArg(0, x);
    cc.setArg(1, y);

    cc.addss(x, y);
    cc.ret(x);

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef float (*Func)(float, float);
    Func func = ptr_as_func<Func>(_func);

    float resultRet = func(1.0f, 2.0f);
    float expectRet = 1.0f + 2.0f;

    result.setFormat("ret={%g}", resultRet);
    expect.setFormat("ret={%g}", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_AllocRetDouble1]
// ============================================================================

class X86Test_AllocRetDouble1 : public X86Test {
public:
  X86Test_AllocRetDouble1() : X86Test("AllocRetDouble1") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocRetDouble1());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<double, double>(CallConv::kIdHost));

    X86Xmm x = cc.newXmmSd("x");
    cc.setArg(0, x);
    cc.ret(x);

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef double (*Func)(double);
    Func func = ptr_as_func<Func>(_func);

    double resultRet = func(42.0);
    double expectRet = 42.0;

    result.setFormat("ret={%g}", resultRet);
    expect.setFormat("ret={%g}", expectRet);

    return resultRet == expectRet;
  }
};
// ============================================================================
// [X86Test_AllocRetDouble2]
// ============================================================================

class X86Test_AllocRetDouble2 : public X86Test {
public:
  X86Test_AllocRetDouble2() : X86Test("AllocRetDouble2") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocRetDouble2());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<double, double, double>(CallConv::kIdHost));

    X86Xmm x = cc.newXmmSd("x");
    X86Xmm y = cc.newXmmSd("y");

    cc.setArg(0, x);
    cc.setArg(1, y);

    cc.addsd(x, y);
    cc.ret(x);

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef double (*Func)(double, double);
    Func func = ptr_as_func<Func>(_func);

    double resultRet = func(1.0, 2.0);
    double expectRet = 1.0 + 2.0;

    result.setFormat("ret={%g}", resultRet);
    expect.setFormat("ret={%g}", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_AllocStack]
// ============================================================================

class X86Test_AllocStack : public X86Test {
public:
  X86Test_AllocStack() : X86Test("AllocStack") {}

  enum { kSize = 256 };

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocStack());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int>(CallConv::kIdHost));

    X86Mem stack = cc.newStack(kSize, 1);
    stack.setSize(1);

    X86Gp i = cc.newIntPtr("i");
    X86Gp a = cc.newInt32("a");
    X86Gp b = cc.newInt32("b");

    Label L_1 = cc.newLabel();
    Label L_2 = cc.newLabel();

    // Fill stack by sequence [0, 1, 2, 3 ... 255].
    cc.xor_(i, i);

    X86Mem stackWithIndex = stack.clone();
    stackWithIndex.setIndex(i, 0);

    cc.bind(L_1);
    cc.mov(stackWithIndex, i.r8());
    cc.inc(i);
    cc.cmp(i, 255);
    cc.jle(L_1);

    // Sum sequence in stack.
    cc.xor_(i, i);
    cc.xor_(a, a);

    cc.bind(L_2);
    cc.movzx(b, stackWithIndex);
    cc.add(a, b);
    cc.inc(i);
    cc.cmp(i, 255);
    cc.jle(L_2);

    cc.ret(a);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func();
    int expectRet = 32640;

    result.setInt(resultRet);
    expect.setInt(expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_AllocMemcpy]
// ============================================================================

class X86Test_AllocMemcpy : public X86Test {
public:
  X86Test_AllocMemcpy() : X86Test("AllocMemcpy") {}

  enum { kCount = 32 };

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocMemcpy());
  }

  virtual void compile(X86Compiler& cc) {
    X86Gp dst = cc.newIntPtr("dst");
    X86Gp src = cc.newIntPtr("src");
    X86Gp cnt = cc.newUIntPtr("cnt");

    Label L_Loop = cc.newLabel();                   // Create base labels we use
    Label L_Exit = cc.newLabel();                   // in our function.

    cc.addFunc(FuncSignatureT<void, uint32_t*, const uint32_t*, size_t>(CallConv::kIdHost));
    cc.setArg(0, dst);
    cc.setArg(1, src);
    cc.setArg(2, cnt);

    cc.test(cnt, cnt);                              // Exit if length is zero.
    cc.jz(L_Exit);

    cc.bind(L_Loop);                                // Bind the loop label here.

    X86Gp tmp = cc.newInt32("tmp");                 // Copy a single dword (4 bytes).
    cc.mov(tmp, x86::dword_ptr(src));
    cc.mov(x86::dword_ptr(dst), tmp);

    cc.add(src, 4);                                 // Increment dst/src pointers.
    cc.add(dst, 4);

    cc.dec(cnt);                                    // Loop until cnt isn't zero.
    cc.jnz(L_Loop);

    cc.bind(L_Exit);                                // Bind the exit label here.
    cc.endFunc();                                   // End of function.
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(uint32_t*, const uint32_t*, size_t);
    Func func = ptr_as_func<Func>(_func);

    uint32_t i;

    uint32_t dstBuffer[kCount];
    uint32_t srcBuffer[kCount];

    for (i = 0; i < kCount; i++) {
      dstBuffer[i] = 0;
      srcBuffer[i] = i;
    }

    func(dstBuffer, srcBuffer, kCount);

    result.setString("buf={");
    expect.setString("buf={");

    for (i = 0; i < kCount; i++) {
      if (i != 0) {
        result.appendString(", ");
        expect.appendString(", ");
      }

      result.appendFormat("%u", unsigned(dstBuffer[i]));
      expect.appendFormat("%u", unsigned(srcBuffer[i]));
    }

    result.appendString("}");
    expect.appendString("}");

    return result == expect;
  }
};

// ============================================================================
// [X86Test_AllocExtraBlock]
// ============================================================================

class X86Test_AllocExtraBlock : public X86Test {
public:
  X86Test_AllocExtraBlock() : X86Test("AllocExtraBlock") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocExtraBlock());
  }

  virtual void compile(X86Compiler& cc) {
    X86Gp cond = cc.newInt32("cond");
    X86Gp ret = cc.newInt32("ret");
    X86Gp a = cc.newInt32("a");
    X86Gp b = cc.newInt32("b");

    cc.addFunc(FuncSignatureT<int, int, int, int>(CallConv::kIdHost));
    cc.setArg(0, cond);
    cc.setArg(1, a);
    cc.setArg(2, b);

    Label L_Ret = cc.newLabel();
    Label L_Extra = cc.newLabel();

    cc.test(cond, cond);
    cc.jnz(L_Extra);

    cc.mov(ret, a);
    cc.add(ret, b);

    cc.bind(L_Ret);
    cc.ret(ret);

    // Emit code sequence at the end of the function.
    CBNode* prevCursor = cc.setCursor(cc.getFunc()->getEnd()->getPrev());
    cc.bind(L_Extra);
    cc.mov(ret, a);
    cc.sub(ret, b);
    cc.jmp(L_Ret);
    cc.setCursor(prevCursor);

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int, int, int);
    Func func = ptr_as_func<Func>(_func);

    int ret1 = func(0, 4, 5);
    int ret2 = func(1, 4, 5);

    int exp1 = 4 + 5;
    int exp2 = 4 - 5;

    result.setFormat("ret={%d, %d}", ret1, ret2);
    expect.setFormat("ret={%d, %d}", exp1, exp2);

    return result == expect;
  }
};

// ============================================================================
// [X86Test_AllocAlphaBlend]
// ============================================================================

class X86Test_AllocAlphaBlend : public X86Test {
public:
  X86Test_AllocAlphaBlend() : X86Test("AllocAlphaBlend") {}

  enum { kCount = 17 };

  static void add(X86TestApp& app) {
    app.add(new X86Test_AllocAlphaBlend());
  }

  static uint32_t blendSrcOver(uint32_t d, uint32_t s) {
    uint32_t saInv = ~s >> 24;

    uint32_t d_20 = (d     ) & 0x00FF00FF;
    uint32_t d_31 = (d >> 8) & 0x00FF00FF;

    d_20 *= saInv;
    d_31 *= saInv;

    d_20 = ((d_20 + ((d_20 >> 8) & 0x00FF00FFU) + 0x00800080U) & 0xFF00FF00U) >> 8;
    d_31 = ((d_31 + ((d_31 >> 8) & 0x00FF00FFU) + 0x00800080U) & 0xFF00FF00U);

    return d_20 + d_31 + s;
  }

  virtual void compile(X86Compiler& cc) {
    asmtest::generateAlphaBlend(cc);
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(void*, const void*, size_t);
    Func func = ptr_as_func<Func>(_func);

    static const uint32_t dstConstData[] = { 0x00000000, 0x10101010, 0x20100804, 0x30200003, 0x40204040, 0x5000004D, 0x60302E2C, 0x706F6E6D, 0x807F4F2F, 0x90349001, 0xA0010203, 0xB03204AB, 0xC023AFBD, 0xD0D0D0C0, 0xE0AABBCC, 0xFFFFFFFF, 0xF8F4F2F1 };
    static const uint32_t srcConstData[] = { 0xE0E0E0E0, 0xA0008080, 0x341F1E1A, 0xFEFEFEFE, 0x80302010, 0x49490A0B, 0x998F7798, 0x00000000, 0x01010101, 0xA0264733, 0xBAB0B1B9, 0xFF000000, 0xDAB0A0C1, 0xE0BACFDA, 0x99887766, 0xFFFFFF80, 0xEE0A5FEC };

    uint32_t _dstBuffer[kCount + 3];
    uint32_t _srcBuffer[kCount + 3];

    // Has to be aligned.
    uint32_t* dstBuffer = (uint32_t*)IntUtils::alignUp<intptr_t>((intptr_t)_dstBuffer, 16);
    uint32_t* srcBuffer = (uint32_t*)IntUtils::alignUp<intptr_t>((intptr_t)_srcBuffer, 16);

    std::memcpy(dstBuffer, dstConstData, sizeof(dstConstData));
    std::memcpy(srcBuffer, srcConstData, sizeof(srcConstData));

    uint32_t i;
    uint32_t expBuffer[kCount];

    for (i = 0; i < kCount; i++) {
      expBuffer[i] = blendSrcOver(dstBuffer[i], srcBuffer[i]);
    }

    func(dstBuffer, srcBuffer, kCount);

    result.setString("buf={");
    expect.setString("buf={");

    for (i = 0; i < kCount; i++) {
      if (i != 0) {
        result.appendString(", ");
        expect.appendString(", ");
      }

      result.appendFormat("%08X", unsigned(dstBuffer[i]));
      expect.appendFormat("%08X", unsigned(expBuffer[i]));
    }

    result.appendString("}");
    expect.appendString("}");

    return result == expect;
  }
};

// ============================================================================
// [X86Test_FuncCallBase1]
// ============================================================================

class X86Test_FuncCallBase1 : public X86Test {
public:
  X86Test_FuncCallBase1() : X86Test("FuncCallBase1") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallBase1());
  }

  virtual void compile(X86Compiler& cc) {
    X86Gp v0 = cc.newInt32("v0");
    X86Gp v1 = cc.newInt32("v1");
    X86Gp v2 = cc.newInt32("v2");

    cc.addFunc(FuncSignatureT<int, int, int, int>(CallConv::kIdHost));
    cc.setArg(0, v0);
    cc.setArg(1, v1);
    cc.setArg(2, v2);

    // Just do something.
    cc.shl(v0, 1);
    cc.shl(v1, 1);
    cc.shl(v2, 1);

    // Call a function.
    X86Gp fn = cc.newIntPtr("fn");
    cc.mov(fn, imm_ptr(calledFunc));

    CCFuncCall* call = cc.call(fn, FuncSignatureT<int, int, int, int>(CallConv::kIdHost));
    call->setArg(0, v2);
    call->setArg(1, v1);
    call->setArg(2, v0);
    call->setRet(0, v0);

    cc.ret(v0);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int, int, int);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func(3, 2, 1);
    int expectRet = 36;

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }

  static int calledFunc(int a, int b, int c) { return (a + b) * c; }
};

// ============================================================================
// [X86Test_FuncCallBase2]
// ============================================================================

class X86Test_FuncCallBase2 : public X86Test {
public:
  X86Test_FuncCallBase2() : X86Test("FuncCallBase2") {}

  enum { kSize = 256 };

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallBase2());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int>(CallConv::kIdHost));

    const int kTokenSize = 32;

    X86Mem s1 = cc.newStack(kTokenSize, 32);
    X86Mem s2 = cc.newStack(kTokenSize, 32);

    X86Gp p1 = cc.newIntPtr("p1");
    X86Gp p2 = cc.newIntPtr("p2");

    X86Gp ret = cc.newInt32("ret");
    Label L_Exit = cc.newLabel();

    static const char token[kTokenSize] = "-+:|abcdefghijklmnopqrstuvwxyz|";
    CCFuncCall* call;

    cc.lea(p1, s1);
    cc.lea(p2, s2);

    // Try to corrupt the stack if wrongly allocated.
    call = cc.call(imm_ptr((void*)std::memcpy), FuncSignatureT<void*, void*, void*, size_t>(CallConv::kIdHostCDecl));
    call->setArg(0, p1);
    call->setArg(1, imm_ptr(token));
    call->setArg(2, imm(kTokenSize));
    call->setRet(0, p1);

    call = cc.call(imm_ptr((void*)std::memcpy), FuncSignatureT<void*, void*, void*, size_t>(CallConv::kIdHostCDecl));
    call->setArg(0, p2);
    call->setArg(1, imm_ptr(token));
    call->setArg(2, imm(kTokenSize));
    call->setRet(0, p2);

    call = cc.call(imm_ptr((void*)std::memcmp), FuncSignatureT<int, void*, void*, size_t>(CallConv::kIdHostCDecl));
    call->setArg(0, p1);
    call->setArg(1, p2);
    call->setArg(2, imm(kTokenSize));
    call->setRet(0, ret);

    // This should be 0 on success, however, if both `p1` and `p2` were
    // allocated in the same address this check will still pass.
    cc.cmp(ret, 0);
    cc.jnz(L_Exit);

    // Checks whether `p1` and `p2` are different (must be).
    cc.xor_(ret, ret);
    cc.cmp(p1, p2);
    cc.setz(ret.r8());

    cc.bind(L_Exit);
    cc.ret(ret);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func();
    int expectRet = 0; // Must be zero, stack addresses must be different.

    result.setInt(resultRet);
    expect.setInt(expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_FuncCallFast]
// ============================================================================

class X86Test_FuncCallFast : public X86Test {
public:
  X86Test_FuncCallFast() : X86Test("FuncCallFast") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallFast());
  }

  virtual void compile(X86Compiler& cc) {
    X86Gp var = cc.newInt32("var");
    X86Gp fn = cc.newIntPtr("fn");

    cc.addFunc(FuncSignatureT<int, int>(CallConv::kIdHost));
    cc.setArg(0, var);

    cc.mov(fn, imm_ptr(calledFunc));
    CCFuncCall* call;

    call = cc.call(fn, FuncSignatureT<int, int>(CallConv::kIdHostFastCall));
    call->setArg(0, var);
    call->setRet(0, var);

    call = cc.call(fn, FuncSignatureT<int, int>(CallConv::kIdHostFastCall));
    call->setArg(0, var);
    call->setRet(0, var);

    cc.ret(var);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func(9);
    int expectRet = (9 * 9) * (9 * 9);

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }

  // Function that is called inside the generated one. Because this test is
  // mainly about register arguments, we need to use the fastcall calling
  // convention when running 32-bit.
  static int ASMJIT_FASTCALL calledFunc(int a) { return a * a; }
};

// ============================================================================
// [X86Test_FuncCallLight]
// ============================================================================

class X86Test_FuncCallLight : public X86Test {
public:
  X86Test_FuncCallLight() : X86Test("FuncCallLight") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallLight());
  }

  virtual void compile(X86Compiler& cc) {
    FuncSignatureT<void, const void*, const void*, const void*, const void*, void*> funcSig(CallConv::kIdHostCDecl);
    FuncSignatureT<X86Xmm, X86Xmm, X86Xmm> fastSig(CallConv::kIdHostLightCall2);

    CCFunc* func = cc.newFunc(funcSig);
    CCFunc* fast = cc.newFunc(fastSig);

    {
      X86Gp aPtr = cc.newIntPtr("aPtr");
      X86Gp bPtr = cc.newIntPtr("bPtr");
      X86Gp cPtr = cc.newIntPtr("cPtr");
      X86Gp dPtr = cc.newIntPtr("dPtr");
      X86Gp pOut = cc.newIntPtr("pOut");

      X86Xmm aXmm = cc.newXmm("aXmm");
      X86Xmm bXmm = cc.newXmm("bXmm");
      X86Xmm cXmm = cc.newXmm("cXmm");
      X86Xmm dXmm = cc.newXmm("dXmm");

      cc.addFunc(func);

      cc.setArg(0, aPtr);
      cc.setArg(1, bPtr);
      cc.setArg(2, cPtr);
      cc.setArg(3, dPtr);
      cc.setArg(4, pOut);

      cc.movups(aXmm, x86::ptr(aPtr));
      cc.movups(bXmm, x86::ptr(bPtr));
      cc.movups(cXmm, x86::ptr(cPtr));
      cc.movups(dXmm, x86::ptr(dPtr));

      X86Xmm xXmm = cc.newXmm("xXmm");
      X86Xmm yXmm = cc.newXmm("yXmm");

      CCFuncCall* call1 = cc.call(fast->getLabel(), fastSig);
      call1->setArg(0, aXmm);
      call1->setArg(1, bXmm);
      call1->setRet(0, xXmm);

      CCFuncCall* call2 = cc.call(fast->getLabel(), fastSig);
      call2->setArg(0, cXmm);
      call2->setArg(1, dXmm);
      call2->setRet(0, yXmm);

      cc.pmullw(xXmm, yXmm);
      cc.movups(x86::ptr(pOut), xXmm);

      cc.endFunc();
    }

    {
      X86Xmm aXmm = cc.newXmm("aXmm");
      X86Xmm bXmm = cc.newXmm("bXmm");

      cc.addFunc(fast);
      cc.setArg(0, aXmm);
      cc.setArg(1, bXmm);
      cc.paddw(aXmm, bXmm);
      cc.ret(aXmm);
      cc.endFunc();
    }
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef void (*Func)(const void*, const void*, const void*, const void*, void*);

    Func func = ptr_as_func<Func>(_func);

    int16_t a[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    int16_t b[8] = { 7, 6, 5, 4, 3, 2, 1, 0 };
    int16_t c[8] = { 1, 3, 9, 7, 5, 4, 2, 1 };
    int16_t d[8] = { 2, 0,-6,-4,-2,-1, 1, 2 };

    int16_t o[8];
    int oExp = 7 * 3;

    func(a, b, c, d, o);

    result.setFormat("ret={%02X %02X %02X %02X %02X %02X %02X %02X}", o[0], o[1], o[2], o[3], o[4], o[5], o[6], o[7]);
    expect.setFormat("ret={%02X %02X %02X %02X %02X %02X %02X %02X}", oExp, oExp, oExp, oExp, oExp, oExp, oExp, oExp);

    return result == expect;
  }
};

// ============================================================================
// [X86Test_FuncCallManyArgs]
// ============================================================================

class X86Test_FuncCallManyArgs : public X86Test {
public:
  X86Test_FuncCallManyArgs() : X86Test("FuncCallManyArgs") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallManyArgs());
  }

  static int calledFunc(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
    return (a * b * c * d * e) + (f * g * h * i * j);
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int>(CallConv::kIdHost));

    // Prepare.
    X86Gp fn = cc.newIntPtr("fn");
    X86Gp va = cc.newInt32("va");
    X86Gp vb = cc.newInt32("vb");
    X86Gp vc = cc.newInt32("vc");
    X86Gp vd = cc.newInt32("vd");
    X86Gp ve = cc.newInt32("ve");
    X86Gp vf = cc.newInt32("vf");
    X86Gp vg = cc.newInt32("vg");
    X86Gp vh = cc.newInt32("vh");
    X86Gp vi = cc.newInt32("vi");
    X86Gp vj = cc.newInt32("vj");

    cc.mov(fn, imm_ptr(calledFunc));
    cc.mov(va, 0x03);
    cc.mov(vb, 0x12);
    cc.mov(vc, 0xA0);
    cc.mov(vd, 0x0B);
    cc.mov(ve, 0x2F);
    cc.mov(vf, 0x02);
    cc.mov(vg, 0x0C);
    cc.mov(vh, 0x12);
    cc.mov(vi, 0x18);
    cc.mov(vj, 0x1E);

    // Call function.
    CCFuncCall* call = cc.call(fn, FuncSignatureT<int, int, int, int, int, int, int, int, int, int, int>(CallConv::kIdHost));
    call->setArg(0, va);
    call->setArg(1, vb);
    call->setArg(2, vc);
    call->setArg(3, vd);
    call->setArg(4, ve);
    call->setArg(5, vf);
    call->setArg(6, vg);
    call->setArg(7, vh);
    call->setArg(8, vi);
    call->setArg(9, vj);
    call->setRet(0, va);

    cc.ret(va);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func();
    int expectRet = calledFunc(0x03, 0x12, 0xA0, 0x0B, 0x2F, 0x02, 0x0C, 0x12, 0x18, 0x1E);

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_FuncCallDuplicateArgs]
// ============================================================================

class X86Test_FuncCallDuplicateArgs : public X86Test {
public:
  X86Test_FuncCallDuplicateArgs() : X86Test("FuncCallDuplicateArgs") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallDuplicateArgs());
  }

  static int calledFunc(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
    return (a * b * c * d * e) + (f * g * h * i * j);
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int>(CallConv::kIdHost));

    // Prepare.
    X86Gp fn = cc.newIntPtr("fn");
    X86Gp a = cc.newInt32("a");

    cc.mov(fn, imm_ptr(calledFunc));
    cc.mov(a, 3);

    // Call function.
    CCFuncCall* call = cc.call(fn, FuncSignatureT<int, int, int, int, int, int, int, int, int, int, int>(CallConv::kIdHost));
    call->setArg(0, a);
    call->setArg(1, a);
    call->setArg(2, a);
    call->setArg(3, a);
    call->setArg(4, a);
    call->setArg(5, a);
    call->setArg(6, a);
    call->setArg(7, a);
    call->setArg(8, a);
    call->setArg(9, a);
    call->setRet(0, a);

    cc.ret(a);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func();
    int expectRet = calledFunc(3, 3, 3, 3, 3, 3, 3, 3, 3, 3);

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_FuncCallImmArgs]
// ============================================================================

class X86Test_FuncCallImmArgs : public X86Test {
public:
  X86Test_FuncCallImmArgs() : X86Test("FuncCallImmArgs") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallImmArgs());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int>(CallConv::kIdHost));

    // Prepare.
    X86Gp fn = cc.newIntPtr("fn");
    X86Gp rv = cc.newInt32("rv");

    cc.mov(fn, imm_ptr(X86Test_FuncCallManyArgs::calledFunc));

    // Call function.
    CCFuncCall* call = cc.call(fn, FuncSignatureT<int, int, int, int, int, int, int, int, int, int, int>(CallConv::kIdHost));
    call->setArg(0, imm(0x03));
    call->setArg(1, imm(0x12));
    call->setArg(2, imm(0xA0));
    call->setArg(3, imm(0x0B));
    call->setArg(4, imm(0x2F));
    call->setArg(5, imm(0x02));
    call->setArg(6, imm(0x0C));
    call->setArg(7, imm(0x12));
    call->setArg(8, imm(0x18));
    call->setArg(9, imm(0x1E));
    call->setRet(0, rv);

    cc.ret(rv);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func();
    int expectRet = X86Test_FuncCallManyArgs::calledFunc(0x03, 0x12, 0xA0, 0x0B, 0x2F, 0x02, 0x0C, 0x12, 0x18, 0x1E);

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_FuncCallPtrArgs]
// ============================================================================

class X86Test_FuncCallPtrArgs : public X86Test {
public:
  X86Test_FuncCallPtrArgs() : X86Test("FuncCallPtrArgs") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallPtrArgs());
  }

  static int calledFunc(void* a, void* b, void* c, void* d, void* e, void* f, void* g, void* h, void* i, void* j) {
    return int((intptr_t)a) +
           int((intptr_t)b) +
           int((intptr_t)c) +
           int((intptr_t)d) +
           int((intptr_t)e) +
           int((intptr_t)f) +
           int((intptr_t)g) +
           int((intptr_t)h) +
           int((intptr_t)i) +
           int((intptr_t)j) ;
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int>(CallConv::kIdHost));

    // Prepare.
    X86Gp fn = cc.newIntPtr("fn");
    X86Gp rv = cc.newInt32("rv");

    cc.mov(fn, imm_ptr(calledFunc));

    // Call function.
    CCFuncCall* call = cc.call(fn, FuncSignatureT<int, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*>(CallConv::kIdHost));
    call->setArg(0, imm(0x01));
    call->setArg(1, imm(0x02));
    call->setArg(2, imm(0x03));
    call->setArg(3, imm(0x04));
    call->setArg(4, imm(0x05));
    call->setArg(5, imm(0x06));
    call->setArg(6, imm(0x07));
    call->setArg(7, imm(0x08));
    call->setArg(8, imm(0x09));
    call->setArg(9, imm(0x0A));
    call->setRet(0, rv);

    cc.ret(rv);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func();
    int expectRet = 55;

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_FuncCallFloatAsXmmRet]
// ============================================================================

class X86Test_FuncCallFloatAsXmmRet : public X86Test {
public:
  X86Test_FuncCallFloatAsXmmRet() : X86Test("FuncCallFloatAsXmmRet") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallFloatAsXmmRet());
  }

  static float calledFunc(float a, float b) {
    return a * b;
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<float, float, float>(CallConv::kIdHost));

    X86Xmm a = cc.newXmmSs("a");
    X86Xmm b = cc.newXmmSs("b");
    X86Xmm ret = cc.newXmmSs("ret");

    cc.setArg(0, a);
    cc.setArg(1, b);

    // Prepare.
    X86Gp fn = cc.newIntPtr("fn");
    cc.mov(fn, imm_ptr(calledFunc));

    // Call function.
    CCFuncCall* call = cc.call(fn, FuncSignatureT<float, float, float>(CallConv::kIdHost));

    call->setArg(0, a);
    call->setArg(1, b);
    call->setRet(0, ret);

    cc.ret(ret);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef float (*Func)(float, float);
    Func func = ptr_as_func<Func>(_func);

    float resultRet = func(15.5f, 2.0f);
    float expectRet = calledFunc(15.5f, 2.0f);

    result.setFormat("ret=%g", resultRet);
    expect.setFormat("ret=%g", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_FuncCallDoubleAsXmmRet]
// ============================================================================

class X86Test_FuncCallDoubleAsXmmRet : public X86Test {
public:
  X86Test_FuncCallDoubleAsXmmRet() : X86Test("FuncCallDoubleAsXmmRet") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallDoubleAsXmmRet());
  }

  static double calledFunc(double a, double b) {
    return a * b;
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<double, double, double>(CallConv::kIdHost));

    X86Xmm a = cc.newXmmSd("a");
    X86Xmm b = cc.newXmmSd("b");
    X86Xmm ret = cc.newXmmSd("ret");

    cc.setArg(0, a);
    cc.setArg(1, b);

    X86Gp fn = cc.newIntPtr("fn");
    cc.mov(fn, imm_ptr(calledFunc));

    CCFuncCall* call = cc.call(fn, FuncSignatureT<double, double, double>(CallConv::kIdHost));

    call->setArg(0, a);
    call->setArg(1, b);
    call->setRet(0, ret);

    cc.ret(ret);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef double (*Func)(double, double);
    Func func = ptr_as_func<Func>(_func);

    double resultRet = func(15.5, 2.0);
    double expectRet = calledFunc(15.5, 2.0);

    result.setFormat("ret=%g", resultRet);
    expect.setFormat("ret=%g", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_FuncCallConditional]
// ============================================================================

class X86Test_FuncCallConditional : public X86Test {
public:
  X86Test_FuncCallConditional() : X86Test("FuncCallConditional") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallConditional());
  }

  virtual void compile(X86Compiler& cc) {
    X86Gp x = cc.newInt32("x");
    X86Gp y = cc.newInt32("y");
    X86Gp op = cc.newInt32("op");

    CCFuncCall* call;
    X86Gp result;

    cc.addFunc(FuncSignatureT<int, int, int, int>(CallConv::kIdHost));
    cc.setArg(0, x);
    cc.setArg(1, y);
    cc.setArg(2, op);

    Label opAdd = cc.newLabel();
    Label opMul = cc.newLabel();

    cc.cmp(op, 0);
    cc.jz(opAdd);
    cc.cmp(op, 1);
    cc.jz(opMul);

    result = cc.newInt32("result_0");
    cc.mov(result, 0);
    cc.ret(result);

    cc.bind(opAdd);
    result = cc.newInt32("result_1");

    call = cc.call((uint64_t)calledFuncAdd, FuncSignatureT<int, int, int>(CallConv::kIdHost));
    call->setArg(0, x);
    call->setArg(1, y);
    call->setRet(0, result);
    cc.ret(result);

    cc.bind(opMul);
    result = cc.newInt32("result_2");

    call = cc.call((uint64_t)calledFuncMul, FuncSignatureT<int, int, int>(CallConv::kIdHost));
    call->setArg(0, x);
    call->setArg(1, y);
    call->setRet(0, result);

    cc.ret(result);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int, int, int);
    Func func = ptr_as_func<Func>(_func);

    int arg1 = 4;
    int arg2 = 8;

    int resultAdd = func(arg1, arg2, 0);
    int expectAdd = calledFuncAdd(arg1, arg2);

    int resultMul = func(arg1, arg2, 1);
    int expectMul = calledFuncMul(arg1, arg2);

    result.setFormat("ret={add=%d, mul=%d}", resultAdd, resultMul);
    expect.setFormat("ret={add=%d, mul=%d}", expectAdd, expectMul);

    return (resultAdd == expectAdd) && (resultMul == expectMul);
  }

  static int calledFuncAdd(int x, int y) { return x + y; }
  static int calledFuncMul(int x, int y) { return x * y; }
};

// ============================================================================
// [X86Test_FuncCallMultiple]
// ============================================================================

class X86Test_FuncCallMultiple : public X86Test {
public:
  X86Test_FuncCallMultiple() : X86Test("FuncCallMultiple") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallMultiple());
  }

  static int ASMJIT_FASTCALL calledFunc(int* pInt, int index) {
    return pInt[index];
  }

  virtual void compile(X86Compiler& cc) {
    unsigned int i;

    X86Gp buf = cc.newIntPtr("buf");
    X86Gp acc0 = cc.newInt32("acc0");
    X86Gp acc1 = cc.newInt32("acc1");

    cc.addFunc(FuncSignatureT<int, int*>(CallConv::kIdHost));
    cc.setArg(0, buf);

    cc.mov(acc0, 0);
    cc.mov(acc1, 0);

    for (i = 0; i < 4; i++) {
      X86Gp ret = cc.newInt32("ret");
      X86Gp ptr = cc.newIntPtr("ptr");
      X86Gp idx = cc.newInt32("idx");
      CCFuncCall* call;

      cc.mov(ptr, buf);
      cc.mov(idx, int(i));

      call = cc.call((uint64_t)calledFunc, FuncSignatureT<int, int*, int>(CallConv::kIdHostFastCall));
      call->setArg(0, ptr);
      call->setArg(1, idx);
      call->setRet(0, ret);

      cc.add(acc0, ret);

      cc.mov(ptr, buf);
      cc.mov(idx, int(i));

      call = cc.call((uint64_t)calledFunc, FuncSignatureT<int, int*, int>(CallConv::kIdHostFastCall));
      call->setArg(0, ptr);
      call->setArg(1, idx);
      call->setRet(0, ret);

      cc.sub(acc1, ret);
    }

    cc.add(acc0, acc1);
    cc.ret(acc0);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int*);
    Func func = ptr_as_func<Func>(_func);

    int buffer[4] = { 127, 87, 23, 17 };

    int resultRet = func(buffer);
    int expectRet = 0;

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_FuncCallRecursive]
// ============================================================================

class X86Test_FuncCallRecursive : public X86Test {
public:
  X86Test_FuncCallRecursive() : X86Test("FuncCallRecursive") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallRecursive());
  }

  virtual void compile(X86Compiler& cc) {
    X86Gp val = cc.newInt32("val");
    Label skip = cc.newLabel();

    CCFunc* func = cc.addFunc(FuncSignatureT<int, int>(CallConv::kIdHost));
    cc.setArg(0, val);

    cc.cmp(val, 1);
    cc.jle(skip);

    X86Gp tmp = cc.newInt32("tmp");
    cc.mov(tmp, val);
    cc.dec(tmp);

    CCFuncCall* call = cc.call(func->getLabel(), FuncSignatureT<int, int>(CallConv::kIdHost));
    call->setArg(0, tmp);
    call->setRet(0, tmp);
    cc.mul(cc.newInt32(), val, tmp);

    cc.bind(skip);
    cc.ret(val);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func(5);
    int expectRet = 1 * 2 * 3 * 4 * 5;

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_FuncCallMisc1]
// ============================================================================

class X86Test_FuncCallMisc1 : public X86Test {
public:
  X86Test_FuncCallMisc1() : X86Test("FuncCallMisc1") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallMisc1());
  }

  static void dummy(int a, int b) {}

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int, int, int>(CallConv::kIdHost));

    X86Gp a = cc.newInt32("a");
    X86Gp b = cc.newInt32("b");
    X86Gp r = cc.newInt32("r");

    cc.setArg(0, a);
    cc.setArg(1, b);

    CCFuncCall* call = cc.call(imm_ptr(dummy), FuncSignatureT<void, int, int>(CallConv::kIdHost));
    call->setArg(0, a);
    call->setArg(1, b);

    cc.lea(r, x86::ptr(a, b));
    cc.ret(r);

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int, int);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func(44, 199);
    int expectRet = 243;

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_FuncCallMisc2]
// ============================================================================

class X86Test_FuncCallMisc2 : public X86Test {
public:
  X86Test_FuncCallMisc2() : X86Test("FuncCallMisc2") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallMisc2());
  }

  virtual void compile(X86Compiler& cc) {
    CCFunc* func = cc.addFunc(FuncSignatureT<double, const double*>(CallConv::kIdHost));

    X86Gp p = cc.newIntPtr("p");
    X86Gp fn = cc.newIntPtr("fn");

    X86Xmm arg = cc.newXmmSd("arg");
    X86Xmm ret = cc.newXmmSd("ret");

    cc.setArg(0, p);
    cc.movsd(arg, x86::ptr(p));
    cc.mov(fn, imm_ptr(op));

    CCFuncCall* call = cc.call(fn, FuncSignatureT<double, double>(CallConv::kIdHost));
    call->setArg(0, arg);
    call->setRet(0, ret);

    cc.ret(ret);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef double (*Func)(const double*);
    Func func = ptr_as_func<Func>(_func);

    double arg = 2;

    double resultRet = func(&arg);
    double expectRet = op(arg);

    result.setFormat("ret=%g", resultRet);
    expect.setFormat("ret=%g", expectRet);

    return resultRet == expectRet;
  }

  static double op(double a) { return a * a; }
};

// ============================================================================
// [X86Test_FuncCallMisc3]
// ============================================================================

class X86Test_FuncCallMisc3 : public X86Test {
public:
  X86Test_FuncCallMisc3() : X86Test("FuncCallMisc3") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallMisc3());
  }

  virtual void compile(X86Compiler& cc) {
    CCFunc* func = cc.addFunc(FuncSignatureT<double, const double*>(CallConv::kIdHost));

    X86Gp p = cc.newIntPtr("p");
    X86Gp fn = cc.newIntPtr("fn");

    X86Xmm arg = cc.newXmmSd("arg");
    X86Xmm ret = cc.newXmmSd("ret");

    cc.setArg(0, p);
    cc.movsd(arg, x86::ptr(p));
    cc.mov(fn, imm_ptr(op));

    CCFuncCall* call = cc.call(fn, FuncSignatureT<double, double>(CallConv::kIdHost));
    call->setArg(0, arg);
    call->setRet(0, ret);

    cc.xorps(arg, arg);
    cc.subsd(arg, ret);

    cc.ret(arg);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef double (*Func)(const double*);
    Func func = ptr_as_func<Func>(_func);

    double arg = 2;

    double resultRet = func(&arg);
    double expectRet = -op(arg);

    result.setFormat("ret=%g", resultRet);
    expect.setFormat("ret=%g", expectRet);

    return resultRet == expectRet;
  }

  static double op(double a) { return a * a; }
};

// ============================================================================
// [X86Test_FuncCallMisc4]
// ============================================================================

class X86Test_FuncCallMisc4 : public X86Test {
public:
  X86Test_FuncCallMisc4() : X86Test("FuncCallMisc4") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallMisc4());
  }

  virtual void compile(X86Compiler& cc) {
    FuncSignatureX funcPrototype;
    funcPrototype.setCallConv(CallConv::kIdHost);
    funcPrototype.setRet(Type::kIdF64);
    cc.addFunc(funcPrototype);

    FuncSignatureX callPrototype;
    callPrototype.setCallConv(CallConv::kIdHost);
    callPrototype.setRet(Type::kIdF64);
    CCFuncCall* call = cc.call(imm_ptr(calledFunc), callPrototype);

    X86Xmm ret = cc.newXmmSd("ret");
    call->setRet(0, ret);
    cc.ret(ret);

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef double (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    double resultRet = func();
    double expectRet = 3.14;

    result.setFormat("ret=%g", resultRet);
    expect.setFormat("ret=%g", expectRet);

    return resultRet == expectRet;
  }

  static double calledFunc() { return 3.14; }
};

// ============================================================================
// [X86Test_FuncCallMisc5]
// ============================================================================

// The register allocator should clobber the register used by the `call` itself.
class X86Test_FuncCallMisc5 : public X86Test {
public:
  X86Test_FuncCallMisc5() : X86Test("FuncCallMisc5") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_FuncCallMisc5());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int>(CallConv::kIdHost));

    X86Gp pFn = cc.newIntPtr("pFn");
    X86Gp vars[16];

    uint32_t i, regCount = cc.getGpCount();
    ASMJIT_ASSERT(regCount <= ASMJIT_ARRAY_SIZE(vars));

    cc.mov(pFn, imm_ptr(calledFunc));

    for (i = 0; i < regCount; i++) {
      if (i == X86Gp::kIdBp || i == X86Gp::kIdSp)
        continue;

      vars[i] = cc.newInt32("%%%u", unsigned(i));
      cc.mov(vars[i], 1);
    }

    cc.call(pFn, FuncSignatureT<void>(CallConv::kIdHost));
    for (i = 1; i < regCount; i++)
      if (vars[i].isValid())
        cc.add(vars[0], vars[i]);
    cc.ret(vars[0]);

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func();
    int expectRet = sizeof(void*) == 4 ? 6 : 14;

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }

  static void calledFunc() {}
};

// ============================================================================
// [X86Test_MiscConstPool]
// ============================================================================

class X86Test_MiscConstPool : public X86Test {
public:
  X86Test_MiscConstPool() : X86Test("MiscConstPool1") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_MiscConstPool());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int>(CallConv::kIdHost));

    X86Gp v0 = cc.newInt32("v0");
    X86Gp v1 = cc.newInt32("v1");

    X86Mem c0 = cc.newInt32Const(kConstScopeLocal, 200);
    X86Mem c1 = cc.newInt32Const(kConstScopeLocal, 33);

    cc.mov(v0, c0);
    cc.mov(v1, c1);
    cc.add(v0, v1);

    cc.ret(v0);
    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(void);
    Func func = ptr_as_func<Func>(_func);

    int resultRet = func();
    int expectRet = 233;

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return resultRet == expectRet;
  }
};

// ============================================================================
// [X86Test_MiscMultiRet]
// ============================================================================

struct X86Test_MiscMultiRet : public X86Test {
  X86Test_MiscMultiRet() : X86Test("MiscMultiRet") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_MiscMultiRet());
  }

  virtual void compile(X86Compiler& cc) {
    cc.addFunc(FuncSignatureT<int, int, int, int>(CallConv::kIdHost));

    X86Gp op = cc.newInt32("op");
    X86Gp a = cc.newInt32("a");
    X86Gp b = cc.newInt32("b");

    Label L_Zero = cc.newLabel();
    Label L_Add = cc.newLabel();
    Label L_Sub = cc.newLabel();
    Label L_Mul = cc.newLabel();
    Label L_Div = cc.newLabel();

    cc.setArg(0, op);
    cc.setArg(1, a);
    cc.setArg(2, b);

    cc.cmp(op, 0);
    cc.jz(L_Add);

    cc.cmp(op, 1);
    cc.jz(L_Sub);

    cc.cmp(op, 2);
    cc.jz(L_Mul);

    cc.cmp(op, 3);
    cc.jz(L_Div);

    cc.bind(L_Zero);
    cc.xor_(a, a);
    cc.ret(a);

    cc.bind(L_Add);
    cc.add(a, b);
    cc.ret(a);

    cc.bind(L_Sub);
    cc.sub(a, b);
    cc.ret(a);

    cc.bind(L_Mul);
    cc.imul(a, b);
    cc.ret(a);

    cc.bind(L_Div);
    cc.cmp(b, 0);
    cc.jz(L_Zero);

    X86Gp zero = cc.newInt32("zero");
    cc.xor_(zero, zero);
    cc.idiv(zero, a, b);
    cc.ret(a);

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int, int, int);

    Func func = ptr_as_func<Func>(_func);

    int a = 44;
    int b = 3;

    int r0 = func(0, a, b);
    int r1 = func(1, a, b);
    int r2 = func(2, a, b);
    int r3 = func(3, a, b);
    int e0 = a + b;
    int e1 = a - b;
    int e2 = a * b;
    int e3 = a / b;

    result.setFormat("ret={%d %d %d %d}", r0, r1, r2, r3);
    expect.setFormat("ret={%d %d %d %d}", e0, e1, e2, e3);

    return result.eq(expect);
  }
};

// ============================================================================
// [X86Test_MiscMultiFunc]
// ============================================================================

class X86Test_MiscMultiFunc : public X86Test {
public:
  X86Test_MiscMultiFunc() : X86Test("MiscMultiFunc") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_MiscMultiFunc());
  }

  virtual void compile(X86Compiler& cc) {
    CCFunc* f1 = cc.newFunc(FuncSignatureT<int, int, int>(CallConv::kIdHost));
    CCFunc* f2 = cc.newFunc(FuncSignatureT<int, int, int>(CallConv::kIdHost));

    {
      X86Gp a = cc.newInt32("a");
      X86Gp b = cc.newInt32("b");

      cc.addFunc(f1);
      cc.setArg(0, a);
      cc.setArg(1, b);

      CCFuncCall* call = cc.call(f2->getLabel(), FuncSignatureT<int, int, int>(CallConv::kIdHost));
      call->setArg(0, a);
      call->setArg(1, b);
      call->setRet(0, a);

      cc.ret(a);
      cc.endFunc();
    }

    {
      X86Gp a = cc.newInt32("a");
      X86Gp b = cc.newInt32("b");

      cc.addFunc(f2);
      cc.setArg(0, a);
      cc.setArg(1, b);

      cc.add(a, b);
      cc.ret(a);
      cc.endFunc();
    }
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (*Func)(int, int);

    Func func = ptr_as_func<Func>(_func);

    int resultRet = func(56, 22);
    int expectRet = 56 + 22;

    result.setFormat("ret=%d", resultRet);
    expect.setFormat("ret=%d", expectRet);

    return result.eq(expect);
  }
};

// ============================================================================
// [X86Test_MiscUnfollow]
// ============================================================================

// Global (I didn't find a better way to test this).
static jmp_buf globalJmpBuf;

class X86Test_MiscUnfollow : public X86Test {
public:
  X86Test_MiscUnfollow() : X86Test("MiscUnfollow") {}

  static void add(X86TestApp& app) {
    app.add(new X86Test_MiscUnfollow());
  }

  virtual void compile(X86Compiler& cc) {
    // NOTE: Fastcall calling convention is the most appropriate here, as all
    // arguments will be passed by registers and there won't be any stack
    // misalignment when we call the `handler()`. This was failing on OSX
    // when targeting 32-bit.
    cc.addFunc(FuncSignatureT<void, int, void*>(CallConv::kIdHostFastCall));

    X86Gp a = cc.newInt32("a");
    X86Gp b = cc.newIntPtr("b");
    Label tramp = cc.newLabel();

    cc.setArg(0, a);
    cc.setArg(1, b);

    cc.cmp(a, 0);
    cc.jz(tramp);

    cc.ret(a);

    cc.bind(tramp);
    cc.unfollow().jmp(b);

    cc.endFunc();
  }

  virtual bool run(void* _func, StringBuilder& result, StringBuilder& expect) {
    typedef int (ASMJIT_FASTCALL *Func)(int, void*);

    Func func = ptr_as_func<Func>(_func);

    int resultRet = 0;
    int expectRet = 1;

    if (!setjmp(globalJmpBuf))
      resultRet = func(0, (void*)handler);
    else
      resultRet = 1;

    result.setFormat("ret={%d}", resultRet);
    expect.setFormat("ret={%d}", expectRet);

    return resultRet == expectRet;
  }

  static void ASMJIT_FASTCALL handler() { longjmp(globalJmpBuf, 1); }
};

// ============================================================================
// [Main]
// ============================================================================

int main(int argc, char* argv[]) {
  X86TestApp app;

  app.handleArgs(argc, argv);
  app.showInfo();

  // Base tests.
  app.addT<X86Test_NoCode>();
  app.addT<X86Test_NoAlign>();
  app.addT<X86Test_AlignBase>();

  // Jump tests.
  app.addT<X86Test_JumpMerge>();
  app.addT<X86Test_JumpCross>();
  app.addT<X86Test_JumpMany>();
  app.addT<X86Test_JumpUnreachable1>();
  app.addT<X86Test_JumpUnreachable2>();

  // Alloc tests.
  app.addT<X86Test_AllocBase>();
  app.addT<X86Test_AllocMany1>();
  app.addT<X86Test_AllocMany2>();
  app.addT<X86Test_AllocImul1>();
  app.addT<X86Test_AllocImul2>();
  app.addT<X86Test_AllocIdiv1>();
  app.addT<X86Test_AllocSetz>();
  app.addT<X86Test_AllocShlRor>();
  app.addT<X86Test_AllocGpbLo>();
  app.addT<X86Test_AllocRepMovsb>();
  app.addT<X86Test_AllocIfElse1>();
  app.addT<X86Test_AllocIfElse2>();
  app.addT<X86Test_AllocIfElse3>();
  app.addT<X86Test_AllocIfElse4>();
  app.addT<X86Test_AllocInt8>();
  app.addT<X86Test_AllocUnhandledArg>();
  app.addT<X86Test_AllocArgsIntPtr>();
  app.addT<X86Test_AllocArgsFloat>();
  app.addT<X86Test_AllocArgsDouble>();
  app.addT<X86Test_AllocRetFloat1>();
  app.addT<X86Test_AllocRetFloat2>();
  app.addT<X86Test_AllocRetDouble1>();
  app.addT<X86Test_AllocRetDouble2>();
  app.addT<X86Test_AllocStack>();
  app.addT<X86Test_AllocMemcpy>();

  app.addT<X86Test_AllocExtraBlock>();
  app.addT<X86Test_AllocAlphaBlend>();

  // Function call tests.
  app.addT<X86Test_FuncCallBase1>();
  app.addT<X86Test_FuncCallBase2>();
  app.addT<X86Test_FuncCallFast>();
  app.addT<X86Test_FuncCallLight>();
  app.addT<X86Test_FuncCallManyArgs>();
  app.addT<X86Test_FuncCallDuplicateArgs>();
  app.addT<X86Test_FuncCallImmArgs>();
  app.addT<X86Test_FuncCallPtrArgs>();
  app.addT<X86Test_FuncCallFloatAsXmmRet>();
  app.addT<X86Test_FuncCallDoubleAsXmmRet>();
  app.addT<X86Test_FuncCallConditional>();
  app.addT<X86Test_FuncCallMultiple>();
  app.addT<X86Test_FuncCallRecursive>();
  app.addT<X86Test_FuncCallMisc1>();
  app.addT<X86Test_FuncCallMisc2>();
  app.addT<X86Test_FuncCallMisc3>();
  app.addT<X86Test_FuncCallMisc4>();
  app.addT<X86Test_FuncCallMisc5>();

  // Miscellaneous tests.
  app.addT<X86Test_MiscConstPool>();
  app.addT<X86Test_MiscMultiRet>();
  app.addT<X86Test_MiscMultiFunc>();
  app.addT<X86Test_MiscUnfollow>();
 
  return app.run();
}
