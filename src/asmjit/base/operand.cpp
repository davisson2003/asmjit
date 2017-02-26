// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define ASMJIT_EXPORTS

// [Dependencies]
#include "../base/operand.h"

// [Api-Begin]
#include "../asmjit_apibegin.h"

namespace asmjit {

// ============================================================================
// [asmjit::TypeId]
// ============================================================================

template<int ID>
struct TypeIdSizeOf_T {
  enum {
    kValue = (ID == TypeId::kI8    ) ?  1 :
             (ID == TypeId::kU8    ) ?  1 :
             (ID == TypeId::kI16   ) ?  2 :
             (ID == TypeId::kU16   ) ?  2 :
             (ID == TypeId::kI32   ) ?  4 :
             (ID == TypeId::kU32   ) ?  4 :
             (ID == TypeId::kI64   ) ?  8 :
             (ID == TypeId::kU64   ) ?  8 :
             (ID == TypeId::kF32   ) ?  4 :
             (ID == TypeId::kF64   ) ?  8 :
             (ID == TypeId::kF80   ) ? 10 :
             (ID == TypeId::kMask8 ) ?  1 :
             (ID == TypeId::kMask16) ?  2 :
             (ID == TypeId::kMask32) ?  4 :
             (ID == TypeId::kMask64) ?  8 :
             (ID == TypeId::kMmx32 ) ?  4 :
             (ID == TypeId::kMmx64 ) ?  8 :
             (ID >= TypeId::_kVec32Start  && ID <= TypeId::_kVec32End ) ?  4 :
             (ID >= TypeId::_kVec64Start  && ID <= TypeId::_kVec64End ) ?  8 :
             (ID >= TypeId::_kVec128Start && ID <= TypeId::_kVec128End) ? 16 :
             (ID >= TypeId::_kVec256Start && ID <= TypeId::_kVec256End) ? 32 :
             (ID >= TypeId::_kVec512Start && ID <= TypeId::_kVec512End) ? 64 : 0
  };
};

template<int ID>
struct TypeIdElementOf_T {
  enum {
    kValue = (ID == TypeId::kMask8 ) ? TypeId::kU8  :
             (ID == TypeId::kMask16) ? TypeId::kU16 :
             (ID == TypeId::kMask32) ? TypeId::kU32 :
             (ID == TypeId::kMask64) ? TypeId::kU64 :
             (ID == TypeId::kMmx32 ) ? TypeId::kI32 :
             (ID == TypeId::kMmx64 ) ? TypeId::kI64 :
             (ID >= TypeId::kI8           && ID <= TypeId::kF80       ) ? ID   :
             (ID >= TypeId::_kVec32Start  && ID <= TypeId::_kVec32End ) ? ID - TypeId::_kVec32Start  + TypeId::kI8 :
             (ID >= TypeId::_kVec64Start  && ID <= TypeId::_kVec64End ) ? ID - TypeId::_kVec64Start  + TypeId::kI8 :
             (ID >= TypeId::_kVec128Start && ID <= TypeId::_kVec128End) ? ID - TypeId::_kVec128Start + TypeId::kI8 :
             (ID >= TypeId::_kVec256Start && ID <= TypeId::_kVec256End) ? ID - TypeId::_kVec256Start + TypeId::kI8 :
             (ID >= TypeId::_kVec512Start && ID <= TypeId::_kVec512End) ? ID - TypeId::_kVec512Start + TypeId::kI8 : 0
  };
};

#define R(TMPL, I) TMPL<I +  0>::kValue, TMPL<I +  1>::kValue, \
                   TMPL<I +  2>::kValue, TMPL<I +  3>::kValue, \
                   TMPL<I +  4>::kValue, TMPL<I +  5>::kValue, \
                   TMPL<I +  6>::kValue, TMPL<I +  7>::kValue, \
                   TMPL<I +  8>::kValue, TMPL<I +  9>::kValue, \
                   TMPL<I + 10>::kValue, TMPL<I + 11>::kValue, \
                   TMPL<I + 12>::kValue, TMPL<I + 13>::kValue, \
                   TMPL<I + 14>::kValue, TMPL<I + 15>::kValue
ASMJIT_API const TypeId::Info TypeId::_info = {
  // SizeOf[128]
  {
    R(TypeIdSizeOf_T,  0), R(TypeIdSizeOf_T,  16),
    R(TypeIdSizeOf_T, 32), R(TypeIdSizeOf_T,  48),
    R(TypeIdSizeOf_T, 64), R(TypeIdSizeOf_T,  80),
    R(TypeIdSizeOf_T, 96), R(TypeIdSizeOf_T, 112)
  },

  // ElementOf[128]
  {
    R(TypeIdElementOf_T,  0), R(TypeIdElementOf_T,  16),
    R(TypeIdElementOf_T, 32), R(TypeIdElementOf_T,  48),
    R(TypeIdElementOf_T, 64), R(TypeIdElementOf_T,  80),
    R(TypeIdElementOf_T, 96), R(TypeIdElementOf_T, 112)
  }
};
#undef R

// ============================================================================
// [asmjit::Operand - Test]
// ============================================================================

#if defined(ASMJIT_TEST)
UNIT(base_operand) {
  INFO("Checking operand sizes");
  EXPECT(sizeof(Operand) == 16);
  EXPECT(sizeof(Reg)     == 16);
  EXPECT(sizeof(Mem)     == 16);
  EXPECT(sizeof(Imm)     == 16);
  EXPECT(sizeof(Label)   == 16);

  INFO("Checking basic functionality of Operand");
  Operand a, b;
  Operand dummy;

  EXPECT(a.isNone() == true);
  EXPECT(a.isReg() == false);
  EXPECT(a.isMem() == false);
  EXPECT(a.isImm() == false);
  EXPECT(a.isLabel() == false);
  EXPECT(a == b);

  EXPECT(a._p32[2] == 0);
  EXPECT(a._p32[3] == 0);

  INFO("Checking basic functionality of Label");
  Label label;
  EXPECT(label.isValid() == false);
  EXPECT(label.getId() == 0);

  INFO("Checking basic functionality of Reg");
  EXPECT(Reg().isReg() == false);
  EXPECT(Reg().isValid() == false);

  EXPECT(Reg()._p32[2] == 0);
  EXPECT(Reg()._p32[3] == 0);
  EXPECT(dummy.as<Reg>().isValid() == false);

  // Create some register (not specific to any architecture).
  uint32_t rSig = Operand::kOpReg | (1 << Operand::kSignatureRegTypeShift ) |
                                    (2 << Operand::kSignatureRegGroupShift) |
                                    (8 << Operand::kSignatureSizeShift    ) ;
  Reg r1(rSig, 5);

  EXPECT(r1.isValid()      == true);
  EXPECT(r1.isReg()        == true);
  EXPECT(r1.isReg(1)       == true);
  EXPECT(r1.isPhysReg()    == true);
  EXPECT(r1.isVirtReg()    == false);
  EXPECT(r1.getSignature() == rSig);
  EXPECT(r1.getType()      == 1);
  EXPECT(r1.getGroup()     == 2);
  EXPECT(r1.getSize()      == 8);
  EXPECT(r1.getId()        == 5);
  EXPECT(r1.isReg(1, 5)    == true); // RegType and Id.
  EXPECT(r1._p32[2] == 0);
  EXPECT(r1._p32[3] == 0);

  // The same type of register having different id.
  Reg r2(r1, 6);
  EXPECT(r2.isValid()      == true);
  EXPECT(r2.isReg()        == true);
  EXPECT(r2.isReg(1)       == true);
  EXPECT(r2.isPhysReg()    == true);
  EXPECT(r2.isVirtReg()    == false);
  EXPECT(r2.getSignature() == rSig);
  EXPECT(r2.getType()      == r1.getType());
  EXPECT(r2.getGroup()     == r1.getGroup());
  EXPECT(r2.getSize()      == r1.getSize());
  EXPECT(r2.getId()        == 6);
  EXPECT(r2.isReg(1, 6)    == true);

  r1.reset();
  EXPECT(!r1.isValid());
  EXPECT(!r1.isReg());

  INFO("Checking basic functionality of Mem");
  Mem m;
  EXPECT(m.isMem());
  EXPECT(m == Mem());
  EXPECT(m.hasBase() == false);
  EXPECT(m.hasIndex() == false);
  EXPECT(m.hasOffset() == false);
  EXPECT(m.isOffset64Bit() == true);
  EXPECT(m.getOffset() == 0);

  m.setOffset(-1);
  EXPECT(m.getOffsetLo32() == -1);
  EXPECT(m.getOffset() == -1);

  int64_t x = int64_t(ASMJIT_UINT64_C(0xFF00FF0000000001));
  int32_t xHi = int32_t(0xFF00FF00);
  m.setOffset(x);
  EXPECT(m.getOffset() == x);
  EXPECT(m.getOffsetLo32() == 1);
  EXPECT(m.getOffsetHi32() == xHi);

  INFO("Checking basic functionality of Imm");
  EXPECT(Imm(-1).getInt64() == int64_t(-1));
}
#endif

} // asmjit namespace

// [Api-End]
#include "../asmjit_apiend.h"
