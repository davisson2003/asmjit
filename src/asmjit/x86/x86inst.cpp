// [AsmJit]
// Complete x86/x64 JIT and Remote Assembler for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// ----------------------------------------------------------------------------
// IMPORTANT: AsmJit now uses an external instruction database to populate
// static tables within this file. Perform the following steps to regenerate
// all tables enclosed by ${...}:
//
//   1. Install node.js environment <https://nodejs.org>
//   2. Go to asmjit/tools directory
//   3. Install either asmdb package by executing `npm install asmdb` or get
//      the latest asmdb from <https://github.com/asmjit/asmdb> and copy/link
//      the `asmdb` directory to `asmjit/tools/asmdb`.
//   4. Execute `node generate-x86.js`
//
// Instruction encoding and opcodes were added to the `x86inst.cpp` database
// manually in the past and they are not updated by the script as they seem
// consistent. However, everything else is updated including instruction
// operands and tables required to validate them, instruction read/write
// information (including registers and flags), and all indexes to all tables.
// ----------------------------------------------------------------------------

// [Export]
#define ASMJIT_EXPORTS

// [Guard]
#include "../core/build.h"
#if defined(ASMJIT_BUILD_X86)

// [Dependencies]
#include "../core/cpuinfo.h"
#include "../core/stringutils.h"
#include "../x86/x86inst.h"
#include "../x86/x86operand.h"

ASMJIT_BEGIN_NAMESPACE

// ============================================================================
// [Enums (Internal)]
// ============================================================================

//! \internal
enum ODATA_ : uint32_t {
  // PREFIX.
  ODATA_000000  = X86Inst::kOpCode_PP_00 | X86Inst::kOpCode_MM_00,
  ODATA_000F00  = X86Inst::kOpCode_PP_00 | X86Inst::kOpCode_MM_0F,
  ODATA_000F01  = X86Inst::kOpCode_PP_00 | X86Inst::kOpCode_MM_0F01,
  ODATA_000F38  = X86Inst::kOpCode_PP_00 | X86Inst::kOpCode_MM_0F38,
  ODATA_000F3A  = X86Inst::kOpCode_PP_00 | X86Inst::kOpCode_MM_0F3A,
  ODATA_660000  = X86Inst::kOpCode_PP_66 | X86Inst::kOpCode_MM_00,
  ODATA_660F00  = X86Inst::kOpCode_PP_66 | X86Inst::kOpCode_MM_0F,
  ODATA_660F38  = X86Inst::kOpCode_PP_66 | X86Inst::kOpCode_MM_0F38,
  ODATA_660F3A  = X86Inst::kOpCode_PP_66 | X86Inst::kOpCode_MM_0F3A,
  ODATA_F20000  = X86Inst::kOpCode_PP_F2 | X86Inst::kOpCode_MM_00,
  ODATA_F20F00  = X86Inst::kOpCode_PP_F2 | X86Inst::kOpCode_MM_0F,
  ODATA_F20F38  = X86Inst::kOpCode_PP_F2 | X86Inst::kOpCode_MM_0F38,
  ODATA_F20F3A  = X86Inst::kOpCode_PP_F2 | X86Inst::kOpCode_MM_0F3A,
  ODATA_F30000  = X86Inst::kOpCode_PP_F3 | X86Inst::kOpCode_MM_00,
  ODATA_F30F00  = X86Inst::kOpCode_PP_F3 | X86Inst::kOpCode_MM_0F,
  ODATA_F30F38  = X86Inst::kOpCode_PP_F3 | X86Inst::kOpCode_MM_0F38,
  ODATA_F30F3A  = X86Inst::kOpCode_PP_F3 | X86Inst::kOpCode_MM_0F3A,
  ODATA_000F0F  = X86Inst::kOpCode_PP_00 | X86Inst::kOpCode_MM_0F, // 3DNOW, special case.

  ODATA_FPU_00  = X86Inst::kOpCode_PP_00,
  ODATA_FPU_9B  = X86Inst::kOpCode_PP_9B,

  ODATA_XOP_M8  = X86Inst::kOpCode_MM_XOP08,
  ODATA_XOP_M9  = X86Inst::kOpCode_MM_XOP09,

  ODATA_O__     = 0,
  ODATA_O_0     = 0 << X86Inst::kOpCode_O_Shift,
  ODATA_O_1     = 1 << X86Inst::kOpCode_O_Shift,
  ODATA_O_2     = 2 << X86Inst::kOpCode_O_Shift,
  ODATA_O_3     = 3 << X86Inst::kOpCode_O_Shift,
  ODATA_O_4     = 4 << X86Inst::kOpCode_O_Shift,
  ODATA_O_5     = 5 << X86Inst::kOpCode_O_Shift,
  ODATA_O_6     = 6 << X86Inst::kOpCode_O_Shift,
  ODATA_O_7     = 7 << X86Inst::kOpCode_O_Shift,

  // REX/VEX.
  ODATA_LL__    = 0,                                  // L is unspecified.
  ODATA_LL_x    = 0,                                  // L is based on operand(s).
  ODATA_LL_I    = 0,                                  // L is ignored (LIG).
  ODATA_LL_0    = 0,                                  // L has to be zero (L.128).
  ODATA_LL_1    = X86Inst::kOpCode_LL_256,            // L has to be one (L.256).
  ODATA_LL_2    = X86Inst::kOpCode_LL_512,            // L has to be two (L.512).

  ODATA_W__     = 0,                                  // W is unspecified.
  ODATA_W_x     = 0,                                  // W is based on operand(s).
  ODATA_W_I     = 0,                                  // W is ignored (WIG).
  ODATA_W_0     = 0,                                  // W has to be zero (W0).
  ODATA_W_1     = X86Inst::kOpCode_W,                 // W has to be one (W1).

  // EVEX.
  ODATA_EvexW__ = 0,                                  // Not EVEX instruction.
  ODATA_EvexW_x = 0,                                  // EVEX.W is based on operand(s).
  ODATA_EvexW_I = 0,                                  // EVEX.W is ignored     (EVEX.WIG).
  ODATA_EvexW_0 = 0,                                  // EVEX.W has to be zero (EVEX.W0).
  ODATA_EvexW_1 = X86Inst::kOpCode_EW,                // EVEX.W has to be one  (EVEX.W1).

  ODATA_N__      = 0,                                 // Base element size not used.
  ODATA_N_0      = 0 << X86Inst::kOpCode_CDSHL_Shift, // N << 0 (BYTE).
  ODATA_N_1      = 1 << X86Inst::kOpCode_CDSHL_Shift, // N << 1 (WORD).
  ODATA_N_2      = 2 << X86Inst::kOpCode_CDSHL_Shift, // N << 2 (DWORD).
  ODATA_N_3      = 3 << X86Inst::kOpCode_CDSHL_Shift, // N << 3 (QWORD).
  ODATA_N_4      = 4 << X86Inst::kOpCode_CDSHL_Shift, // N << 4 (OWORD).
  ODATA_N_5      = 5 << X86Inst::kOpCode_CDSHL_Shift, // N << 5 (YWORD).

  ODATA_TT__     = 0,
  ODATA_TT_FV    = X86Inst::kOpCode_CDTT_FV,
  ODATA_TT_HV    = X86Inst::kOpCode_CDTT_HV,
  ODATA_TT_FVM   = X86Inst::kOpCode_CDTT_FVM,
  ODATA_TT_T1S   = X86Inst::kOpCode_CDTT_T1S,
  ODATA_TT_T1F   = X86Inst::kOpCode_CDTT_T1F,
  ODATA_TT_T1W   = X86Inst::kOpCode_CDTT_T1W,
  ODATA_TT_T2    = X86Inst::kOpCode_CDTT_T2,
  ODATA_TT_T4    = X86Inst::kOpCode_CDTT_T4,
  ODATA_TT_T8    = X86Inst::kOpCode_CDTT_T8,
  ODATA_TT_HVM   = X86Inst::kOpCode_CDTT_HVM,
  ODATA_TT_OVM   = X86Inst::kOpCode_CDTT_OVM,
  ODATA_TT_QVM   = X86Inst::kOpCode_CDTT_QVM,
  ODATA_TT_128   = X86Inst::kOpCode_CDTT_128,
  ODATA_TT_DUP   = X86Inst::kOpCode_CDTT_DUP,
  ODATA_TT_T4X   = X86Inst::kOpCode_CDTT_T1_4X
};

// ============================================================================
// [asmjit::X86Inst]
// ============================================================================

// Instruction opcode definitions:
//   - `O` encodes X86|MMX|SSE instructions.
//   - `V` encodes VEX|XOP|EVEX instructions.
#define O_ENCODE(VEX, PREFIX, OPCODE, O, L, W, EvexW, N, TT) \
  ((PREFIX) | (OPCODE) | (O) | (L) | (W) | (EvexW) | (N) | (TT) | \
   (VEX && ((PREFIX) & X86Inst::kOpCode_MM_Mask) != X86Inst::kOpCode_MM_0F ? int(X86Inst::kOpCode_MM_ForceVex3) : 0))

#define O(PREFIX, OPCODE, O, LL, W, EvexW, N, TT) (O_ENCODE(0, ODATA_##PREFIX, 0x##OPCODE, ODATA_O_##O, ODATA_LL_##LL, ODATA_W_##W, ODATA_EvexW_##EvexW, ODATA_N_##N, ODATA_TT_##TT))
#define V(PREFIX, OPCODE, O, LL, W, EvexW, N, TT) (O_ENCODE(1, ODATA_##PREFIX, 0x##OPCODE, ODATA_O_##O, ODATA_LL_##LL, ODATA_W_##W, ODATA_EvexW_##EvexW, ODATA_N_##N, ODATA_TT_##TT))

#define O_FPU(PREFIX, OPCODE, O) (ODATA_FPU_##PREFIX | (0x##OPCODE & 0xFFU) | ((0x##OPCODE >> 8) << X86Inst::kOpCode_FPU_2B_Shift) | ODATA_O_##O)

// Don't store `_nameDataIndex` if instruction names are disabled. Since some
// APIs can use `_nameDataIndex` it's much safer if it's zero if it's not used.
#if defined(ASMJIT_DISABLE_TEXT)
# define NAME_DATA_INDEX(X) 0
#else
# define NAME_DATA_INDEX(X) X
#endif

// Defines an X86/X64 instruction.
#define INST(id, encoding, opcode0, opcode1, writeIndex, writeSize, nameDataIndex, commonDataIndex, operationDataIndex, seeToAvxDataIndex) { \
  uint32_t(X86Inst::kEncoding##encoding),   \
  uint32_t(NAME_DATA_INDEX(nameDataIndex)), \
  uint32_t(commonDataIndex),                \
  uint32_t(operationDataIndex),             \
  uint32_t(seeToAvxDataIndex),              \
  0,                                        \
  opcode0                                   \
}
const X86Inst X86InstDB::instData[] = {
  // <-----------------+--------------------+------------------+--------+------------------+--------+-------+-----+----+----+---+
  //                   |                    |    Main OpCode   |#0 EVEX |Alternative OpCode|#1 EVEX | Write |     |    |    |Sse|
  //    Instruction    |   Inst. Encoding   |                  +--------+                  +--------+---+---+NameX|ComX|OpnX|<->|
  //                   |                    |#0:PP-MMM OP/O L|W|W|N|TT. |#1:PP-MMM OP/O L|W|W|N|TT. |Idx|Cnt|     |    |    |Avx|
  // <-----------------+--------------------+------------------+--------+------------------+--------+---+---+-----+----+----+---+
  //                                                                                                (..automatically generated..)
  // ${instData:Begin}
  INST(None            , None               , 0                         , 0                         , 0 , 0 , 0   , 0  , 0  , 0 ), // #0
  INST(Aaa             , X86Op_xAX          , O(000000,37,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1   , 1  , 1  , 0 ), // #1
  INST(Aad             , X86I_xAX           , O(000000,D5,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5   , 2  , 1  , 0 ), // #2
  INST(Aam             , X86I_xAX           , O(000000,D4,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9   , 2  , 1  , 0 ), // #3
  INST(Aas             , X86Op_xAX          , O(000000,3F,_,_,_,_,_,_  ), 0                         , 0 , 0 , 13  , 1  , 1  , 0 ), // #4
  INST(Adc             , X86Arith           , O(000000,10,2,_,x,_,_,_  ), 0                         , 0 , 0 , 17  , 3  , 2  , 0 ), // #5
  INST(Adcx            , X86Rm              , O(660F38,F6,_,_,x,_,_,_  ), 0                         , 0 , 0 , 21  , 4  , 3  , 0 ), // #6
  INST(Add             , X86Arith           , O(000000,00,0,_,x,_,_,_  ), 0                         , 0 , 0 , 732 , 3  , 1  , 0 ), // #7
  INST(Addpd           , ExtRm              , O(660F00,58,_,_,_,_,_,_  ), 0                         , 0 , 0 , 4635, 5  , 4  , 1 ), // #8
  INST(Addps           , ExtRm              , O(000F00,58,_,_,_,_,_,_  ), 0                         , 0 , 0 , 4647, 5  , 5  , 1 ), // #9
  INST(Addsd           , ExtRm              , O(F20F00,58,_,_,_,_,_,_  ), 0                         , 0 , 0 , 4869, 6  , 4  , 1 ), // #10
  INST(Addss           , ExtRm              , O(F30F00,58,_,_,_,_,_,_  ), 0                         , 0 , 0 , 4879, 7  , 5  , 1 ), // #11
  INST(Addsubpd        , ExtRm              , O(660F00,D0,_,_,_,_,_,_  ), 0                         , 0 , 0 , 4374, 5  , 6  , 1 ), // #12
  INST(Addsubps        , ExtRm              , O(F20F00,D0,_,_,_,_,_,_  ), 0                         , 0 , 0 , 4386, 5  , 6  , 1 ), // #13
  INST(Adox            , X86Rm              , O(F30F38,F6,_,_,x,_,_,_  ), 0                         , 0 , 0 , 26  , 4  , 7  , 0 ), // #14
  INST(Aesdec          , ExtRm              , O(660F38,DE,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2870, 5  , 8  , 2 ), // #15
  INST(Aesdeclast      , ExtRm              , O(660F38,DF,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2878, 5  , 8  , 2 ), // #16
  INST(Aesenc          , ExtRm              , O(660F38,DC,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2890, 5  , 8  , 2 ), // #17
  INST(Aesenclast      , ExtRm              , O(660F38,DD,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2898, 5  , 8  , 2 ), // #18
  INST(Aesimc          , ExtRm              , O(660F38,DB,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2910, 8  , 8  , 3 ), // #19
  INST(Aeskeygenassist , ExtRmi             , O(660F3A,DF,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2918, 9  , 8  , 3 ), // #20
  INST(And             , X86Arith           , O(000000,20,4,_,x,_,_,_  ), 0                         , 0 , 0 , 2333, 10 , 1  , 0 ), // #21
  INST(Andn            , VexRvm_Wx          , V(000F38,F2,_,0,x,_,_,_  ), 0                         , 0 , 0 , 6166, 11 , 9  , 0 ), // #22
  INST(Andnpd          , ExtRm              , O(660F00,55,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2951, 5  , 4  , 2 ), // #23
  INST(Andnps          , ExtRm              , O(000F00,55,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2959, 5  , 5  , 2 ), // #24
  INST(Andpd           , ExtRm              , O(660F00,54,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3888, 12 , 4  , 2 ), // #25
  INST(Andps           , ExtRm              , O(000F00,54,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3898, 12 , 5  , 2 ), // #26
  INST(Arpl            , X86Mr_NoSize       , O(000000,63,_,_,_,_,_,_  ), 0                         , 0 , 0 , 31  , 13 , 10 , 0 ), // #27
  INST(Bextr           , VexRmv_Wx          , V(000F38,F7,_,0,x,_,_,_  ), 0                         , 0 , 0 , 36  , 14 , 9  , 0 ), // #28
  INST(Blcfill         , VexVm_Wx           , V(XOP_M9,01,1,0,x,_,_,_  ), 0                         , 0 , 0 , 42  , 15 , 11 , 0 ), // #29
  INST(Blci            , VexVm_Wx           , V(XOP_M9,02,6,0,x,_,_,_  ), 0                         , 0 , 0 , 50  , 15 , 11 , 0 ), // #30
  INST(Blcic           , VexVm_Wx           , V(XOP_M9,01,5,0,x,_,_,_  ), 0                         , 0 , 0 , 55  , 15 , 11 , 0 ), // #31
  INST(Blcmsk          , VexVm_Wx           , V(XOP_M9,02,1,0,x,_,_,_  ), 0                         , 0 , 0 , 61  , 15 , 11 , 0 ), // #32
  INST(Blcs            , VexVm_Wx           , V(XOP_M9,01,3,0,x,_,_,_  ), 0                         , 0 , 0 , 68  , 15 , 11 , 0 ), // #33
  INST(Blendpd         , ExtRmi             , O(660F3A,0D,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3037, 16 , 12 , 4 ), // #34
  INST(Blendps         , ExtRmi             , O(660F3A,0C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3046, 16 , 12 , 4 ), // #35
  INST(Blendvpd        , ExtRm_XMM0         , O(660F38,15,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3055, 17 , 12 , 5 ), // #36
  INST(Blendvps        , ExtRm_XMM0         , O(660F38,14,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3065, 17 , 12 , 5 ), // #37
  INST(Blsfill         , VexVm_Wx           , V(XOP_M9,01,2,0,x,_,_,_  ), 0                         , 0 , 0 , 73  , 15 , 11 , 0 ), // #38
  INST(Blsi            , VexVm_Wx           , V(000F38,F3,3,0,x,_,_,_  ), 0                         , 0 , 0 , 81  , 15 , 9  , 0 ), // #39
  INST(Blsic           , VexVm_Wx           , V(XOP_M9,01,6,0,x,_,_,_  ), 0                         , 0 , 0 , 86  , 15 , 11 , 0 ), // #40
  INST(Blsmsk          , VexVm_Wx           , V(000F38,F3,2,0,x,_,_,_  ), 0                         , 0 , 0 , 92  , 15 , 9  , 0 ), // #41
  INST(Blsr            , VexVm_Wx           , V(000F38,F3,1,0,x,_,_,_  ), 0                         , 0 , 0 , 99  , 15 , 9  , 0 ), // #42
  INST(Bndcl           , X86Rm              , O(F30F00,1A,_,_,_,_,_,_  ), 0                         , 0 , 0 , 104 , 18 , 13 , 0 ), // #43
  INST(Bndcn           , X86Rm              , O(F20F00,1B,_,_,_,_,_,_  ), 0                         , 0 , 0 , 110 , 18 , 13 , 0 ), // #44
  INST(Bndcu           , X86Rm              , O(F20F00,1A,_,_,_,_,_,_  ), 0                         , 0 , 0 , 116 , 18 , 13 , 0 ), // #45
  INST(Bndldx          , X86Rm              , O(000F00,1A,_,_,_,_,_,_  ), 0                         , 0 , 0 , 122 , 19 , 13 , 0 ), // #46
  INST(Bndmk           , X86Rm              , O(F30F00,1B,_,_,_,_,_,_  ), 0                         , 0 , 0 , 129 , 20 , 13 , 0 ), // #47
  INST(Bndmov          , X86Bndmov          , O(660F00,1A,_,_,_,_,_,_  ), O(660F00,1B,_,_,_,_,_,_  ), 0 , 0 , 135 , 21 , 13 , 0 ), // #48
  INST(Bndstx          , X86Mr              , O(000F00,1B,_,_,_,_,_,_  ), 0                         , 0 , 0 , 142 , 22 , 13 , 0 ), // #49
  INST(Bound           , X86Rm              , O(000000,62,_,_,_,_,_,_  ), 0                         , 0 , 0 , 149 , 23 , 0  , 0 ), // #50
  INST(Bsf             , X86Rm              , O(000F00,BC,_,_,x,_,_,_  ), 0                         , 0 , 0 , 155 , 24 , 1  , 0 ), // #51
  INST(Bsr             , X86Rm              , O(000F00,BD,_,_,x,_,_,_  ), 0                         , 0 , 0 , 159 , 24 , 1  , 0 ), // #52
  INST(Bswap           , X86Bswap           , O(000F00,C8,_,_,x,_,_,_  ), 0                         , 0 , 0 , 163 , 25 , 0  , 0 ), // #53
  INST(Bt              , X86Bt              , O(000F00,A3,_,_,x,_,_,_  ), O(000F00,BA,4,_,x,_,_,_  ), 0 , 0 , 169 , 26 , 14 , 0 ), // #54
  INST(Btc             , X86Bt              , O(000F00,BB,_,_,x,_,_,_  ), O(000F00,BA,7,_,x,_,_,_  ), 0 , 0 , 172 , 27 , 15 , 0 ), // #55
  INST(Btr             , X86Bt              , O(000F00,B3,_,_,x,_,_,_  ), O(000F00,BA,6,_,x,_,_,_  ), 0 , 0 , 176 , 28 , 15 , 0 ), // #56
  INST(Bts             , X86Bt              , O(000F00,AB,_,_,x,_,_,_  ), O(000F00,BA,5,_,x,_,_,_  ), 0 , 0 , 180 , 29 , 15 , 0 ), // #57
  INST(Bzhi            , VexRmv_Wx          , V(000F38,F5,_,0,x,_,_,_  ), 0                         , 0 , 0 , 184 , 14 , 16 , 0 ), // #58
  INST(Call            , X86Call            , O(000000,FF,2,_,_,_,_,_  ), 0                         , 0 , 0 , 2729, 30 , 1  , 0 ), // #59
  INST(Cbw             , X86Op_xAX          , O(660000,98,_,_,_,_,_,_  ), 0                         , 0 , 0 , 189 , 31 , 0  , 0 ), // #60
  INST(Cdq             , X86Op_xDX_xAX      , O(000000,99,_,_,_,_,_,_  ), 0                         , 0 , 0 , 193 , 32 , 0  , 0 ), // #61
  INST(Cdqe            , X86Op_xAX          , O(000000,98,_,_,1,_,_,_  ), 0                         , 0 , 0 , 197 , 33 , 0  , 0 ), // #62
  INST(Clac            , X86Op              , O(000F01,CA,_,_,_,_,_,_  ), 0                         , 0 , 0 , 202 , 34 , 17 , 0 ), // #63
  INST(Clc             , X86Op              , O(000000,F8,_,_,_,_,_,_  ), 0                         , 0 , 0 , 207 , 34 , 18 , 0 ), // #64
  INST(Cld             , X86Op              , O(000000,FC,_,_,_,_,_,_  ), 0                         , 0 , 0 , 211 , 34 , 19 , 0 ), // #65
  INST(Clflush         , X86M_Only          , O(000F00,AE,7,_,_,_,_,_  ), 0                         , 0 , 0 , 215 , 35 , 20 , 0 ), // #66
  INST(Clflushopt      , X86M_Only          , O(660F00,AE,7,_,_,_,_,_  ), 0                         , 0 , 0 , 223 , 35 , 21 , 0 ), // #67
  INST(Cli             , X86Op              , O(000000,FA,_,_,_,_,_,_  ), 0                         , 0 , 0 , 234 , 34 , 22 , 0 ), // #68
  INST(Clts            , X86Op              , O(000F00,06,_,_,_,_,_,_  ), 0                         , 0 , 0 , 238 , 34 , 23 , 0 ), // #69
  INST(Clwb            , X86M_Only          , O(660F00,AE,6,_,_,_,_,_  ), 0                         , 0 , 0 , 243 , 35 , 24 , 0 ), // #70
  INST(Clzero          , X86Op_ZAX          , O(000F01,FC,_,_,_,_,_,_  ), 0                         , 0 , 0 , 248 , 36 , 25 , 0 ), // #71
  INST(Cmc             , X86Op              , O(000000,F5,_,_,_,_,_,_  ), 0                         , 0 , 0 , 255 , 34 , 26 , 0 ), // #72
  INST(Cmova           , X86Rm              , O(000F00,47,_,_,x,_,_,_  ), 0                         , 0 , 0 , 259 , 37 , 27 , 0 ), // #73
  INST(Cmovae          , X86Rm              , O(000F00,43,_,_,x,_,_,_  ), 0                         , 0 , 0 , 265 , 37 , 28 , 0 ), // #74
  INST(Cmovb           , X86Rm              , O(000F00,42,_,_,x,_,_,_  ), 0                         , 0 , 0 , 589 , 37 , 28 , 0 ), // #75
  INST(Cmovbe          , X86Rm              , O(000F00,46,_,_,x,_,_,_  ), 0                         , 0 , 0 , 596 , 37 , 27 , 0 ), // #76
  INST(Cmovc           , X86Rm              , O(000F00,42,_,_,x,_,_,_  ), 0                         , 0 , 0 , 272 , 37 , 28 , 0 ), // #77
  INST(Cmove           , X86Rm              , O(000F00,44,_,_,x,_,_,_  ), 0                         , 0 , 0 , 604 , 37 , 29 , 0 ), // #78
  INST(Cmovg           , X86Rm              , O(000F00,4F,_,_,x,_,_,_  ), 0                         , 0 , 0 , 278 , 37 , 30 , 0 ), // #79
  INST(Cmovge          , X86Rm              , O(000F00,4D,_,_,x,_,_,_  ), 0                         , 0 , 0 , 284 , 37 , 31 , 0 ), // #80
  INST(Cmovl           , X86Rm              , O(000F00,4C,_,_,x,_,_,_  ), 0                         , 0 , 0 , 291 , 37 , 31 , 0 ), // #81
  INST(Cmovle          , X86Rm              , O(000F00,4E,_,_,x,_,_,_  ), 0                         , 0 , 0 , 297 , 37 , 30 , 0 ), // #82
  INST(Cmovna          , X86Rm              , O(000F00,46,_,_,x,_,_,_  ), 0                         , 0 , 0 , 304 , 37 , 27 , 0 ), // #83
  INST(Cmovnae         , X86Rm              , O(000F00,42,_,_,x,_,_,_  ), 0                         , 0 , 0 , 311 , 37 , 28 , 0 ), // #84
  INST(Cmovnb          , X86Rm              , O(000F00,43,_,_,x,_,_,_  ), 0                         , 0 , 0 , 611 , 37 , 28 , 0 ), // #85
  INST(Cmovnbe         , X86Rm              , O(000F00,47,_,_,x,_,_,_  ), 0                         , 0 , 0 , 619 , 37 , 27 , 0 ), // #86
  INST(Cmovnc          , X86Rm              , O(000F00,43,_,_,x,_,_,_  ), 0                         , 0 , 0 , 319 , 37 , 28 , 0 ), // #87
  INST(Cmovne          , X86Rm              , O(000F00,45,_,_,x,_,_,_  ), 0                         , 0 , 0 , 628 , 37 , 29 , 0 ), // #88
  INST(Cmovng          , X86Rm              , O(000F00,4E,_,_,x,_,_,_  ), 0                         , 0 , 0 , 326 , 37 , 30 , 0 ), // #89
  INST(Cmovnge         , X86Rm              , O(000F00,4C,_,_,x,_,_,_  ), 0                         , 0 , 0 , 333 , 37 , 31 , 0 ), // #90
  INST(Cmovnl          , X86Rm              , O(000F00,4D,_,_,x,_,_,_  ), 0                         , 0 , 0 , 341 , 37 , 31 , 0 ), // #91
  INST(Cmovnle         , X86Rm              , O(000F00,4F,_,_,x,_,_,_  ), 0                         , 0 , 0 , 348 , 37 , 30 , 0 ), // #92
  INST(Cmovno          , X86Rm              , O(000F00,41,_,_,x,_,_,_  ), 0                         , 0 , 0 , 356 , 37 , 32 , 0 ), // #93
  INST(Cmovnp          , X86Rm              , O(000F00,4B,_,_,x,_,_,_  ), 0                         , 0 , 0 , 363 , 37 , 33 , 0 ), // #94
  INST(Cmovns          , X86Rm              , O(000F00,49,_,_,x,_,_,_  ), 0                         , 0 , 0 , 370 , 37 , 34 , 0 ), // #95
  INST(Cmovnz          , X86Rm              , O(000F00,45,_,_,x,_,_,_  ), 0                         , 0 , 0 , 377 , 37 , 29 , 0 ), // #96
  INST(Cmovo           , X86Rm              , O(000F00,40,_,_,x,_,_,_  ), 0                         , 0 , 0 , 384 , 37 , 32 , 0 ), // #97
  INST(Cmovp           , X86Rm              , O(000F00,4A,_,_,x,_,_,_  ), 0                         , 0 , 0 , 390 , 37 , 33 , 0 ), // #98
  INST(Cmovpe          , X86Rm              , O(000F00,4A,_,_,x,_,_,_  ), 0                         , 0 , 0 , 396 , 37 , 33 , 0 ), // #99
  INST(Cmovpo          , X86Rm              , O(000F00,4B,_,_,x,_,_,_  ), 0                         , 0 , 0 , 403 , 37 , 33 , 0 ), // #100
  INST(Cmovs           , X86Rm              , O(000F00,48,_,_,x,_,_,_  ), 0                         , 0 , 0 , 410 , 37 , 34 , 0 ), // #101
  INST(Cmovz           , X86Rm              , O(000F00,44,_,_,x,_,_,_  ), 0                         , 0 , 0 , 416 , 37 , 29 , 0 ), // #102
  INST(Cmp             , X86Arith           , O(000000,38,7,_,x,_,_,_  ), 0                         , 0 , 0 , 422 , 38 , 1  , 0 ), // #103
  INST(Cmppd           , ExtRmi             , O(660F00,C2,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3291, 16 , 4  , 6 ), // #104
  INST(Cmpps           , ExtRmi             , O(000F00,C2,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3298, 16 , 5  , 6 ), // #105
  INST(Cmps            , X86StrMm           , O(000000,A6,_,_,_,_,_,_  ), 0                         , 0 , 0 , 426 , 39 , 35 , 0 ), // #106
  INST(Cmpsd           , ExtRmi             , O(F20F00,C2,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3305, 40 , 4  , 7 ), // #107
  INST(Cmpss           , ExtRmi             , O(F30F00,C2,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3312, 41 , 5  , 7 ), // #108
  INST(Cmpxchg         , X86Cmpxchg         , O(000F00,B0,_,_,x,_,_,_  ), 0                         , 0 , 0 , 431 , 42 , 36 , 0 ), // #109
  INST(Cmpxchg16b      , X86Cmpxchg8b_16b   , O(000F00,C7,1,_,1,_,_,_  ), 0                         , 0 , 0 , 439 , 43 , 37 , 0 ), // #110
  INST(Cmpxchg8b       , X86Cmpxchg8b_16b   , O(000F00,C7,1,_,_,_,_,_  ), 0                         , 0 , 0 , 450 , 44 , 38 , 0 ), // #111
  INST(Comisd          , ExtRm              , O(660F00,2F,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9387, 45 , 39 , 8 ), // #112
  INST(Comiss          , ExtRm              , O(000F00,2F,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9396, 46 , 40 , 8 ), // #113
  INST(Cpuid           , X86Op              , O(000F00,A2,_,_,_,_,_,_  ), 0                         , 0 , 0 , 460 , 47 , 41 , 0 ), // #114
  INST(Cqo             , X86Op_xDX_xAX      , O(000000,99,_,_,1,_,_,_  ), 0                         , 0 , 0 , 466 , 48 , 0  , 0 ), // #115
  INST(Crc32           , X86Crc             , O(F20F38,F0,_,_,x,_,_,_  ), 0                         , 0 , 0 , 470 , 49 , 42 , 0 ), // #116
  INST(Cvtdq2pd        , ExtRm              , O(F30F00,E6,_,_,_,_,_,_  ), 0                         , 0 , 16, 3359, 50 , 4  , 9 ), // #117
  INST(Cvtdq2ps        , ExtRm              , O(000F00,5B,_,_,_,_,_,_  ), 0                         , 0 , 16, 3369, 51 , 4  , 9 ), // #118
  INST(Cvtpd2dq        , ExtRm              , O(F20F00,E6,_,_,_,_,_,_  ), 0                         , 0 , 16, 3379, 51 , 4  , 9 ), // #119
  INST(Cvtpd2pi        , ExtRm              , O(660F00,2D,_,_,_,_,_,_  ), 0                         , 0 , 8 , 476 , 52 , 4  , 0 ), // #120
  INST(Cvtpd2ps        , ExtRm              , O(660F00,5A,_,_,_,_,_,_  ), 0                         , 0 , 16, 3389, 51 , 4  , 10), // #121
  INST(Cvtpi2pd        , ExtRm              , O(660F00,2A,_,_,_,_,_,_  ), 0                         , 0 , 16, 485 , 53 , 4  , 0 ), // #122
  INST(Cvtpi2ps        , ExtRm              , O(000F00,2A,_,_,_,_,_,_  ), 0                         , 0 , 8 , 494 , 54 , 5  , 0 ), // #123
  INST(Cvtps2dq        , ExtRm              , O(660F00,5B,_,_,_,_,_,_  ), 0                         , 0 , 16, 3441, 51 , 4  , 8 ), // #124
  INST(Cvtps2pd        , ExtRm              , O(000F00,5A,_,_,_,_,_,_  ), 0                         , 0 , 16, 3451, 50 , 4  , 8 ), // #125
  INST(Cvtps2pi        , ExtRm              , O(000F00,2D,_,_,_,_,_,_  ), 0                         , 0 , 8 , 503 , 55 , 5  , 0 ), // #126
  INST(Cvtsd2si        , ExtRm_Wx           , O(F20F00,2D,_,_,x,_,_,_  ), 0                         , 0 , 8 , 3523, 56 , 4  , 11), // #127
  INST(Cvtsd2ss        , ExtRm              , O(F20F00,5A,_,_,_,_,_,_  ), 0                         , 0 , 4 , 3533, 57 , 4  , 12), // #128
  INST(Cvtsi2sd        , ExtRm_Wx           , O(F20F00,2A,_,_,x,_,_,_  ), 0                         , 0 , 8 , 3554, 58 , 4  , 13), // #129
  INST(Cvtsi2ss        , ExtRm_Wx           , O(F30F00,2A,_,_,x,_,_,_  ), 0                         , 0 , 4 , 3564, 59 , 5  , 13), // #130
  INST(Cvtss2sd        , ExtRm              , O(F30F00,5A,_,_,_,_,_,_  ), 0                         , 0 , 8 , 3574, 60 , 4  , 13), // #131
  INST(Cvtss2si        , ExtRm_Wx           , O(F30F00,2D,_,_,x,_,_,_  ), 0                         , 0 , 8 , 3584, 61 , 5  , 14), // #132
  INST(Cvttpd2dq       , ExtRm              , O(660F00,E6,_,_,_,_,_,_  ), 0                         , 0 , 16, 3605, 51 , 4  , 15), // #133
  INST(Cvttpd2pi       , ExtRm              , O(660F00,2C,_,_,_,_,_,_  ), 0                         , 0 , 8 , 512 , 52 , 4  , 0 ), // #134
  INST(Cvttps2dq       , ExtRm              , O(F30F00,5B,_,_,_,_,_,_  ), 0                         , 0 , 16, 3651, 51 , 4  , 16), // #135
  INST(Cvttps2pi       , ExtRm              , O(000F00,2C,_,_,_,_,_,_  ), 0                         , 0 , 8 , 522 , 55 , 5  , 0 ), // #136
  INST(Cvttsd2si       , ExtRm_Wx           , O(F20F00,2C,_,_,x,_,_,_  ), 0                         , 0 , 8 , 3697, 56 , 4  , 17), // #137
  INST(Cvttss2si       , ExtRm_Wx           , O(F30F00,2C,_,_,x,_,_,_  ), 0                         , 0 , 8 , 3720, 61 , 5  , 18), // #138
  INST(Cwd             , X86Op_xDX_xAX      , O(660000,99,_,_,_,_,_,_  ), 0                         , 0 , 0 , 532 , 62 , 0  , 0 ), // #139
  INST(Cwde            , X86Op_xAX          , O(000000,98,_,_,_,_,_,_  ), 0                         , 0 , 0 , 536 , 63 , 0  , 0 ), // #140
  INST(Daa             , X86Op              , O(000000,27,_,_,_,_,_,_  ), 0                         , 0 , 0 , 541 , 1  , 1  , 0 ), // #141
  INST(Das             , X86Op              , O(000000,2F,_,_,_,_,_,_  ), 0                         , 0 , 0 , 545 , 1  , 1  , 0 ), // #142
  INST(Dec             , X86IncDec          , O(000000,FE,1,_,x,_,_,_  ), O(000000,48,_,_,x,_,_,_  ), 0 , 0 , 2873, 64 , 43 , 0 ), // #143
  INST(Div             , X86M_GPB_MulDiv    , O(000000,F6,6,_,x,_,_,_  ), 0                         , 0 , 0 , 751 , 65 , 1  , 0 ), // #144
  INST(Divpd           , ExtRm              , O(660F00,5E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3819, 5  , 4  , 19), // #145
  INST(Divps           , ExtRm              , O(000F00,5E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3826, 5  , 5  , 19), // #146
  INST(Divsd           , ExtRm              , O(F20F00,5E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3833, 6  , 4  , 19), // #147
  INST(Divss           , ExtRm              , O(F30F00,5E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3840, 7  , 5  , 19), // #148
  INST(Dppd            , ExtRmi             , O(660F3A,41,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3847, 16 , 12 , 19), // #149
  INST(Dpps            , ExtRmi             , O(660F3A,40,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3853, 16 , 12 , 19), // #150
  INST(Emms            , X86Op              , O(000F00,77,_,_,_,_,_,_  ), 0                         , 0 , 0 , 719 , 66 , 44 , 0 ), // #151
  INST(Enter           , X86Enter           , O(000000,C8,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2737, 67 , 45 , 0 ), // #152
  INST(Extractps       , ExtExtract         , O(660F3A,17,_,_,_,_,_,_  ), 0                         , 0 , 8 , 4043, 68 , 12 , 20), // #153
  INST(Extrq           , ExtExtrq           , O(660F00,79,_,_,_,_,_,_  ), O(660F00,78,0,_,_,_,_,_  ), 0 , 0 , 6880, 69 , 46 , 0 ), // #154
  INST(F2xm1           , FpuOp              , O_FPU(00,D9F0,_)          , 0                         , 0 , 0 , 549 , 34 , 47 , 0 ), // #155
  INST(Fabs            , FpuOp              , O_FPU(00,D9E1,_)          , 0                         , 0 , 0 , 555 , 34 , 47 , 0 ), // #156
  INST(Fadd            , FpuArith           , O_FPU(00,C0C0,0)          , 0                         , 0 , 0 , 1973, 70 , 47 , 0 ), // #157
  INST(Faddp           , FpuRDef            , O_FPU(00,DEC0,_)          , 0                         , 0 , 0 , 560 , 71 , 47 , 0 ), // #158
  INST(Fbld            , X86M_Only          , O_FPU(00,00DF,4)          , 0                         , 0 , 0 , 566 , 72 , 47 , 0 ), // #159
  INST(Fbstp           , X86M_Only          , O_FPU(00,00DF,6)          , 0                         , 0 , 0 , 571 , 73 , 47 , 0 ), // #160
  INST(Fchs            , FpuOp              , O_FPU(00,D9E0,_)          , 0                         , 0 , 0 , 577 , 34 , 47 , 0 ), // #161
  INST(Fclex           , FpuOp              , O_FPU(9B,DBE2,_)          , 0                         , 0 , 0 , 582 , 34 , 47 , 0 ), // #162
  INST(Fcmovb          , FpuR               , O_FPU(00,DAC0,_)          , 0                         , 0 , 0 , 588 , 74 , 48 , 0 ), // #163
  INST(Fcmovbe         , FpuR               , O_FPU(00,DAD0,_)          , 0                         , 0 , 0 , 595 , 74 , 48 , 0 ), // #164
  INST(Fcmove          , FpuR               , O_FPU(00,DAC8,_)          , 0                         , 0 , 0 , 603 , 74 , 48 , 0 ), // #165
  INST(Fcmovnb         , FpuR               , O_FPU(00,DBC0,_)          , 0                         , 0 , 0 , 610 , 74 , 48 , 0 ), // #166
  INST(Fcmovnbe        , FpuR               , O_FPU(00,DBD0,_)          , 0                         , 0 , 0 , 618 , 74 , 48 , 0 ), // #167
  INST(Fcmovne         , FpuR               , O_FPU(00,DBC8,_)          , 0                         , 0 , 0 , 627 , 74 , 48 , 0 ), // #168
  INST(Fcmovnu         , FpuR               , O_FPU(00,DBD8,_)          , 0                         , 0 , 0 , 635 , 74 , 48 , 0 ), // #169
  INST(Fcmovu          , FpuR               , O_FPU(00,DAD8,_)          , 0                         , 0 , 0 , 643 , 74 , 48 , 0 ), // #170
  INST(Fcom            , FpuCom             , O_FPU(00,D0D0,2)          , 0                         , 0 , 0 , 650 , 75 , 47 , 0 ), // #171
  INST(Fcomi           , FpuR               , O_FPU(00,DBF0,_)          , 0                         , 0 , 0 , 655 , 76 , 49 , 0 ), // #172
  INST(Fcomip          , FpuR               , O_FPU(00,DFF0,_)          , 0                         , 0 , 0 , 661 , 76 , 49 , 0 ), // #173
  INST(Fcomp           , FpuCom             , O_FPU(00,D8D8,3)          , 0                         , 0 , 0 , 668 , 75 , 47 , 0 ), // #174
  INST(Fcompp          , FpuOp              , O_FPU(00,DED9,_)          , 0                         , 0 , 0 , 674 , 34 , 47 , 0 ), // #175
  INST(Fcos            , FpuOp              , O_FPU(00,D9FF,_)          , 0                         , 0 , 0 , 681 , 34 , 47 , 0 ), // #176
  INST(Fdecstp         , FpuOp              , O_FPU(00,D9F6,_)          , 0                         , 0 , 0 , 686 , 34 , 47 , 0 ), // #177
  INST(Fdiv            , FpuArith           , O_FPU(00,F0F8,6)          , 0                         , 0 , 0 , 694 , 70 , 47 , 0 ), // #178
  INST(Fdivp           , FpuRDef            , O_FPU(00,DEF8,_)          , 0                         , 0 , 0 , 699 , 71 , 47 , 0 ), // #179
  INST(Fdivr           , FpuArith           , O_FPU(00,F8F0,7)          , 0                         , 0 , 0 , 705 , 70 , 47 , 0 ), // #180
  INST(Fdivrp          , FpuRDef            , O_FPU(00,DEF0,_)          , 0                         , 0 , 0 , 711 , 71 , 47 , 0 ), // #181
  INST(Femms           , X86Op              , O(000F00,0E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 718 , 34 , 50 , 0 ), // #182
  INST(Ffree           , FpuR               , O_FPU(00,DDC0,_)          , 0                         , 0 , 0 , 724 , 74 , 47 , 0 ), // #183
  INST(Fiadd           , FpuM               , O_FPU(00,00DA,0)          , 0                         , 0 , 0 , 730 , 77 , 47 , 0 ), // #184
  INST(Ficom           , FpuM               , O_FPU(00,00DA,2)          , 0                         , 0 , 0 , 736 , 77 , 47 , 0 ), // #185
  INST(Ficomp          , FpuM               , O_FPU(00,00DA,3)          , 0                         , 0 , 0 , 742 , 77 , 47 , 0 ), // #186
  INST(Fidiv           , FpuM               , O_FPU(00,00DA,6)          , 0                         , 0 , 0 , 749 , 77 , 47 , 0 ), // #187
  INST(Fidivr          , FpuM               , O_FPU(00,00DA,7)          , 0                         , 0 , 0 , 755 , 77 , 47 , 0 ), // #188
  INST(Fild            , FpuM               , O_FPU(00,00DB,0)          , O_FPU(00,00DF,5)          , 0 , 0 , 762 , 78 , 47 , 0 ), // #189
  INST(Fimul           , FpuM               , O_FPU(00,00DA,1)          , 0                         , 0 , 0 , 767 , 77 , 47 , 0 ), // #190
  INST(Fincstp         , FpuOp              , O_FPU(00,D9F7,_)          , 0                         , 0 , 0 , 773 , 34 , 47 , 0 ), // #191
  INST(Finit           , FpuOp              , O_FPU(9B,DBE3,_)          , 0                         , 0 , 0 , 781 , 34 , 47 , 0 ), // #192
  INST(Fist            , FpuM               , O_FPU(00,00DB,2)          , 0                         , 0 , 0 , 787 , 79 , 47 , 0 ), // #193
  INST(Fistp           , FpuM               , O_FPU(00,00DB,3)          , O_FPU(00,00DF,7)          , 0 , 0 , 792 , 80 , 47 , 0 ), // #194
  INST(Fisttp          , FpuM               , O_FPU(00,00DB,1)          , O_FPU(00,00DD,1)          , 0 , 0 , 798 , 81 , 51 , 0 ), // #195
  INST(Fisub           , FpuM               , O_FPU(00,00DA,4)          , 0                         , 0 , 0 , 805 , 77 , 47 , 0 ), // #196
  INST(Fisubr          , FpuM               , O_FPU(00,00DA,5)          , 0                         , 0 , 0 , 811 , 77 , 47 , 0 ), // #197
  INST(Fld             , FpuFldFst          , O_FPU(00,00D9,0)          , O_FPU(00,00DB,5)          , 0 , 0 , 818 , 82 , 47 , 0 ), // #198
  INST(Fld1            , FpuOp              , O_FPU(00,D9E8,_)          , 0                         , 0 , 0 , 822 , 34 , 47 , 0 ), // #199
  INST(Fldcw           , X86M_Only          , O_FPU(00,00D9,5)          , 0                         , 0 , 0 , 827 , 83 , 47 , 0 ), // #200
  INST(Fldenv          , X86M_Only          , O_FPU(00,00D9,4)          , 0                         , 0 , 0 , 833 , 35 , 47 , 0 ), // #201
  INST(Fldl2e          , FpuOp              , O_FPU(00,D9EA,_)          , 0                         , 0 , 0 , 840 , 34 , 47 , 0 ), // #202
  INST(Fldl2t          , FpuOp              , O_FPU(00,D9E9,_)          , 0                         , 0 , 0 , 847 , 34 , 47 , 0 ), // #203
  INST(Fldlg2          , FpuOp              , O_FPU(00,D9EC,_)          , 0                         , 0 , 0 , 854 , 34 , 47 , 0 ), // #204
  INST(Fldln2          , FpuOp              , O_FPU(00,D9ED,_)          , 0                         , 0 , 0 , 861 , 34 , 47 , 0 ), // #205
  INST(Fldpi           , FpuOp              , O_FPU(00,D9EB,_)          , 0                         , 0 , 0 , 868 , 34 , 47 , 0 ), // #206
  INST(Fldz            , FpuOp              , O_FPU(00,D9EE,_)          , 0                         , 0 , 0 , 874 , 34 , 47 , 0 ), // #207
  INST(Fmul            , FpuArith           , O_FPU(00,C8C8,1)          , 0                         , 0 , 0 , 2015, 84 , 47 , 0 ), // #208
  INST(Fmulp           , FpuRDef            , O_FPU(00,DEC8,_)          , 0                         , 0 , 0 , 879 , 71 , 47 , 0 ), // #209
  INST(Fnclex          , FpuOp              , O_FPU(00,DBE2,_)          , 0                         , 0 , 0 , 885 , 34 , 47 , 0 ), // #210
  INST(Fninit          , FpuOp              , O_FPU(00,DBE3,_)          , 0                         , 0 , 0 , 892 , 34 , 47 , 0 ), // #211
  INST(Fnop            , FpuOp              , O_FPU(00,D9D0,_)          , 0                         , 0 , 0 , 899 , 34 , 47 , 0 ), // #212
  INST(Fnsave          , X86M_Only          , O_FPU(00,00DD,6)          , 0                         , 0 , 0 , 904 , 85 , 47 , 0 ), // #213
  INST(Fnstcw          , X86M_Only          , O_FPU(00,00D9,7)          , 0                         , 0 , 0 , 911 , 86 , 47 , 0 ), // #214
  INST(Fnstenv         , X86M_Only          , O_FPU(00,00D9,6)          , 0                         , 0 , 0 , 918 , 85 , 47 , 0 ), // #215
  INST(Fnstsw          , FpuStsw            , O_FPU(00,00DD,7)          , O_FPU(00,DFE0,_)          , 0 , 0 , 926 , 87 , 47 , 0 ), // #216
  INST(Fpatan          , FpuOp              , O_FPU(00,D9F3,_)          , 0                         , 0 , 0 , 933 , 34 , 47 , 0 ), // #217
  INST(Fprem           , FpuOp              , O_FPU(00,D9F8,_)          , 0                         , 0 , 0 , 940 , 34 , 47 , 0 ), // #218
  INST(Fprem1          , FpuOp              , O_FPU(00,D9F5,_)          , 0                         , 0 , 0 , 946 , 34 , 47 , 0 ), // #219
  INST(Fptan           , FpuOp              , O_FPU(00,D9F2,_)          , 0                         , 0 , 0 , 953 , 34 , 47 , 0 ), // #220
  INST(Frndint         , FpuOp              , O_FPU(00,D9FC,_)          , 0                         , 0 , 0 , 959 , 34 , 47 , 0 ), // #221
  INST(Frstor          , X86M_Only          , O_FPU(00,00DD,4)          , 0                         , 0 , 0 , 967 , 35 , 47 , 0 ), // #222
  INST(Fsave           , X86M_Only          , O_FPU(9B,00DD,6)          , 0                         , 0 , 0 , 974 , 85 , 47 , 0 ), // #223
  INST(Fscale          , FpuOp              , O_FPU(00,D9FD,_)          , 0                         , 0 , 0 , 980 , 34 , 47 , 0 ), // #224
  INST(Fsin            , FpuOp              , O_FPU(00,D9FE,_)          , 0                         , 0 , 0 , 987 , 34 , 47 , 0 ), // #225
  INST(Fsincos         , FpuOp              , O_FPU(00,D9FB,_)          , 0                         , 0 , 0 , 992 , 34 , 47 , 0 ), // #226
  INST(Fsqrt           , FpuOp              , O_FPU(00,D9FA,_)          , 0                         , 0 , 0 , 1000, 34 , 47 , 0 ), // #227
  INST(Fst             , FpuFldFst          , O_FPU(00,00D9,2)          , 0                         , 0 , 0 , 1006, 88 , 47 , 0 ), // #228
  INST(Fstcw           , X86M_Only          , O_FPU(9B,00D9,7)          , 0                         , 0 , 0 , 1010, 86 , 47 , 0 ), // #229
  INST(Fstenv          , X86M_Only          , O_FPU(9B,00D9,6)          , 0                         , 0 , 0 , 1016, 85 , 47 , 0 ), // #230
  INST(Fstp            , FpuFldFst          , O_FPU(00,00D9,3)          , O(000000,DB,7,_,_,_,_,_  ), 0 , 0 , 1023, 89 , 47 , 0 ), // #231
  INST(Fstsw           , FpuStsw            , O_FPU(9B,00DD,7)          , O_FPU(9B,DFE0,_)          , 0 , 0 , 1028, 90 , 47 , 0 ), // #232
  INST(Fsub            , FpuArith           , O_FPU(00,E0E8,4)          , 0                         , 0 , 0 , 2093, 70 , 47 , 0 ), // #233
  INST(Fsubp           , FpuRDef            , O_FPU(00,DEE8,_)          , 0                         , 0 , 0 , 1034, 71 , 47 , 0 ), // #234
  INST(Fsubr           , FpuArith           , O_FPU(00,E8E0,5)          , 0                         , 0 , 0 , 2099, 70 , 47 , 0 ), // #235
  INST(Fsubrp          , FpuRDef            , O_FPU(00,DEE0,_)          , 0                         , 0 , 0 , 1040, 71 , 47 , 0 ), // #236
  INST(Ftst            , FpuOp              , O_FPU(00,D9E4,_)          , 0                         , 0 , 0 , 1047, 34 , 47 , 0 ), // #237
  INST(Fucom           , FpuRDef            , O_FPU(00,DDE0,_)          , 0                         , 0 , 0 , 1052, 91 , 47 , 0 ), // #238
  INST(Fucomi          , FpuR               , O_FPU(00,DBE8,_)          , 0                         , 0 , 0 , 1058, 76 , 49 , 0 ), // #239
  INST(Fucomip         , FpuR               , O_FPU(00,DFE8,_)          , 0                         , 0 , 0 , 1065, 76 , 49 , 0 ), // #240
  INST(Fucomp          , FpuRDef            , O_FPU(00,DDE8,_)          , 0                         , 0 , 0 , 1073, 91 , 47 , 0 ), // #241
  INST(Fucompp         , FpuOp              , O_FPU(00,DAE9,_)          , 0                         , 0 , 0 , 1080, 34 , 47 , 0 ), // #242
  INST(Fwait           , X86Op              , O_FPU(00,00DB,_)          , 0                         , 0 , 0 , 1088, 34 , 47 , 0 ), // #243
  INST(Fxam            , FpuOp              , O_FPU(00,D9E5,_)          , 0                         , 0 , 0 , 1094, 34 , 47 , 0 ), // #244
  INST(Fxch            , FpuR               , O_FPU(00,D9C8,_)          , 0                         , 0 , 0 , 1099, 71 , 47 , 0 ), // #245
  INST(Fxrstor         , X86M_Only          , O(000F00,AE,1,_,_,_,_,_  ), 0                         , 0 , 0 , 1104, 35 , 52 , 0 ), // #246
  INST(Fxrstor64       , X86M_Only          , O(000F00,AE,1,_,1,_,_,_  ), 0                         , 0 , 0 , 1112, 92 , 52 , 0 ), // #247
  INST(Fxsave          , X86M_Only          , O(000F00,AE,0,_,_,_,_,_  ), 0                         , 0 , 0 , 1122, 85 , 53 , 0 ), // #248
  INST(Fxsave64        , X86M_Only          , O(000F00,AE,0,_,1,_,_,_  ), 0                         , 0 , 0 , 1129, 93 , 53 , 0 ), // #249
  INST(Fxtract         , FpuOp              , O_FPU(00,D9F4,_)          , 0                         , 0 , 0 , 1138, 34 , 47 , 0 ), // #250
  INST(Fyl2x           , FpuOp              , O_FPU(00,D9F1,_)          , 0                         , 0 , 0 , 1146, 34 , 47 , 0 ), // #251
  INST(Fyl2xp1         , FpuOp              , O_FPU(00,D9F9,_)          , 0                         , 0 , 0 , 1152, 34 , 47 , 0 ), // #252
  INST(Haddpd          , ExtRm              , O(660F00,7C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5398, 5  , 6  , 21), // #253
  INST(Haddps          , ExtRm              , O(F20F00,7C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5406, 5  , 6  , 21), // #254
  INST(Hlt             , X86Op              , O(000000,F4,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1160, 34 , 23 , 0 ), // #255
  INST(Hsubpd          , ExtRm              , O(660F00,7D,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5414, 5  , 6  , 22), // #256
  INST(Hsubps          , ExtRm              , O(F20F00,7D,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5422, 5  , 6  , 22), // #257
  INST(Idiv            , X86M_GPB_MulDiv    , O(000000,F6,7,_,x,_,_,_  ), 0                         , 0 , 0 , 750 , 65 , 1  , 0 ), // #258
  INST(Imul            , X86Imul            , O(000000,F6,5,_,x,_,_,_  ), 0                         , 0 , 0 , 768 , 94 , 1  , 0 ), // #259
  INST(In              , X86In              , O(000000,EC,_,_,_,_,_,_  ), O(000000,E4,_,_,_,_,_,_  ), 0 , 0 , 9524, 95 , 45 , 0 ), // #260
  INST(Inc             , X86IncDec          , O(000000,FE,0,_,x,_,_,_  ), O(000000,40,_,_,x,_,_,_  ), 0 , 0 , 1164, 96 , 43 , 0 ), // #261
  INST(Ins             , X86Ins             , O(000000,6C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1168, 97 , 45 , 0 ), // #262
  INST(Insertps        , ExtRmi             , O(660F3A,21,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5558, 41 , 12 , 23), // #263
  INST(Insertq         , ExtInsertq         , O(F20F00,79,_,_,_,_,_,_  ), O(F20F00,78,_,_,_,_,_,_  ), 0 , 0 , 1172, 98 , 46 , 0 ), // #264
  INST(Int             , X86Int             , O(000000,CD,_,_,_,_,_,_  ), 0                         , 0 , 0 , 963 , 99 , 45 , 0 ), // #265
  INST(Int3            , X86Op              , O(000000,CC,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1180, 34 , 45 , 0 ), // #266
  INST(Into            , X86Op              , O(000000,CE,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1185, 100, 54 , 0 ), // #267
  INST(Invd            , X86Op              , O(000F00,08,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9479, 34 , 55 , 0 ), // #268
  INST(Invlpg          , X86M_Only          , O(000F00,01,7,_,_,_,_,_  ), 0                         , 0 , 0 , 1190, 35 , 55 , 0 ), // #269
  INST(Invpcid         , X86Rm_NoRexW       , O(660F38,82,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1197, 101, 55 , 0 ), // #270
  INST(Iret            , X86Op              , O(000000,CF,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1205, 102, 1  , 0 ), // #271
  INST(Iretd           , X86Op              , O(000000,CF,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1210, 102, 1  , 0 ), // #272
  INST(Iretq           , X86Op              , O(000000,CF,_,_,1,_,_,_  ), 0                         , 0 , 0 , 1216, 103, 1  , 0 ), // #273
  INST(Iretw           , X86Op              , O(660000,CF,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1222, 102, 1  , 0 ), // #274
  INST(Ja              , X86Jcc             , O(000F00,87,_,_,_,_,_,_  ), O(000000,77,_,_,_,_,_,_  ), 0 , 0 , 1228, 104, 56 , 0 ), // #275
  INST(Jae             , X86Jcc             , O(000F00,83,_,_,_,_,_,_  ), O(000000,73,_,_,_,_,_,_  ), 0 , 0 , 1231, 105, 57 , 0 ), // #276
  INST(Jb              , X86Jcc             , O(000F00,82,_,_,_,_,_,_  ), O(000000,72,_,_,_,_,_,_  ), 0 , 0 , 1235, 106, 57 , 0 ), // #277
  INST(Jbe             , X86Jcc             , O(000F00,86,_,_,_,_,_,_  ), O(000000,76,_,_,_,_,_,_  ), 0 , 0 , 1238, 107, 56 , 0 ), // #278
  INST(Jc              , X86Jcc             , O(000F00,82,_,_,_,_,_,_  ), O(000000,72,_,_,_,_,_,_  ), 0 , 0 , 1242, 106, 57 , 0 ), // #279
  INST(Je              , X86Jcc             , O(000F00,84,_,_,_,_,_,_  ), O(000000,74,_,_,_,_,_,_  ), 0 , 0 , 1245, 108, 58 , 0 ), // #280
  INST(Jecxz           , X86JecxzLoop       , 0                         , O(000000,E3,_,_,_,_,_,_  ), 0 , 0 , 1248, 109, 0  , 0 ), // #281
  INST(Jg              , X86Jcc             , O(000F00,8F,_,_,_,_,_,_  ), O(000000,7F,_,_,_,_,_,_  ), 0 , 0 , 1254, 110, 59 , 0 ), // #282
  INST(Jge             , X86Jcc             , O(000F00,8D,_,_,_,_,_,_  ), O(000000,7D,_,_,_,_,_,_  ), 0 , 0 , 1257, 111, 60 , 0 ), // #283
  INST(Jl              , X86Jcc             , O(000F00,8C,_,_,_,_,_,_  ), O(000000,7C,_,_,_,_,_,_  ), 0 , 0 , 1261, 112, 60 , 0 ), // #284
  INST(Jle             , X86Jcc             , O(000F00,8E,_,_,_,_,_,_  ), O(000000,7E,_,_,_,_,_,_  ), 0 , 0 , 1264, 113, 59 , 0 ), // #285
  INST(Jmp             , X86Jmp             , O(000000,FF,4,_,_,_,_,_  ), O(000000,EB,_,_,_,_,_,_  ), 0 , 0 , 1268, 114, 0  , 0 ), // #286
  INST(Jna             , X86Jcc             , O(000F00,86,_,_,_,_,_,_  ), O(000000,76,_,_,_,_,_,_  ), 0 , 0 , 1272, 107, 56 , 0 ), // #287
  INST(Jnae            , X86Jcc             , O(000F00,82,_,_,_,_,_,_  ), O(000000,72,_,_,_,_,_,_  ), 0 , 0 , 1276, 106, 57 , 0 ), // #288
  INST(Jnb             , X86Jcc             , O(000F00,83,_,_,_,_,_,_  ), O(000000,73,_,_,_,_,_,_  ), 0 , 0 , 1281, 105, 57 , 0 ), // #289
  INST(Jnbe            , X86Jcc             , O(000F00,87,_,_,_,_,_,_  ), O(000000,77,_,_,_,_,_,_  ), 0 , 0 , 1285, 104, 56 , 0 ), // #290
  INST(Jnc             , X86Jcc             , O(000F00,83,_,_,_,_,_,_  ), O(000000,73,_,_,_,_,_,_  ), 0 , 0 , 1290, 105, 57 , 0 ), // #291
  INST(Jne             , X86Jcc             , O(000F00,85,_,_,_,_,_,_  ), O(000000,75,_,_,_,_,_,_  ), 0 , 0 , 1294, 115, 58 , 0 ), // #292
  INST(Jng             , X86Jcc             , O(000F00,8E,_,_,_,_,_,_  ), O(000000,7E,_,_,_,_,_,_  ), 0 , 0 , 1298, 113, 59 , 0 ), // #293
  INST(Jnge            , X86Jcc             , O(000F00,8C,_,_,_,_,_,_  ), O(000000,7C,_,_,_,_,_,_  ), 0 , 0 , 1302, 112, 60 , 0 ), // #294
  INST(Jnl             , X86Jcc             , O(000F00,8D,_,_,_,_,_,_  ), O(000000,7D,_,_,_,_,_,_  ), 0 , 0 , 1307, 111, 60 , 0 ), // #295
  INST(Jnle            , X86Jcc             , O(000F00,8F,_,_,_,_,_,_  ), O(000000,7F,_,_,_,_,_,_  ), 0 , 0 , 1311, 110, 59 , 0 ), // #296
  INST(Jno             , X86Jcc             , O(000F00,81,_,_,_,_,_,_  ), O(000000,71,_,_,_,_,_,_  ), 0 , 0 , 1316, 116, 61 , 0 ), // #297
  INST(Jnp             , X86Jcc             , O(000F00,8B,_,_,_,_,_,_  ), O(000000,7B,_,_,_,_,_,_  ), 0 , 0 , 1320, 117, 62 , 0 ), // #298
  INST(Jns             , X86Jcc             , O(000F00,89,_,_,_,_,_,_  ), O(000000,79,_,_,_,_,_,_  ), 0 , 0 , 1324, 118, 63 , 0 ), // #299
  INST(Jnz             , X86Jcc             , O(000F00,85,_,_,_,_,_,_  ), O(000000,75,_,_,_,_,_,_  ), 0 , 0 , 1328, 115, 58 , 0 ), // #300
  INST(Jo              , X86Jcc             , O(000F00,80,_,_,_,_,_,_  ), O(000000,70,_,_,_,_,_,_  ), 0 , 0 , 1332, 119, 61 , 0 ), // #301
  INST(Jp              , X86Jcc             , O(000F00,8A,_,_,_,_,_,_  ), O(000000,7A,_,_,_,_,_,_  ), 0 , 0 , 1335, 120, 62 , 0 ), // #302
  INST(Jpe             , X86Jcc             , O(000F00,8A,_,_,_,_,_,_  ), O(000000,7A,_,_,_,_,_,_  ), 0 , 0 , 1338, 120, 62 , 0 ), // #303
  INST(Jpo             , X86Jcc             , O(000F00,8B,_,_,_,_,_,_  ), O(000000,7B,_,_,_,_,_,_  ), 0 , 0 , 1342, 117, 62 , 0 ), // #304
  INST(Js              , X86Jcc             , O(000F00,88,_,_,_,_,_,_  ), O(000000,78,_,_,_,_,_,_  ), 0 , 0 , 1346, 121, 63 , 0 ), // #305
  INST(Jz              , X86Jcc             , O(000F00,84,_,_,_,_,_,_  ), O(000000,74,_,_,_,_,_,_  ), 0 , 0 , 1349, 108, 58 , 0 ), // #306
  INST(Kaddb           , VexRvm             , V(660F00,4A,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1352, 122, 64 , 0 ), // #307
  INST(Kaddd           , VexRvm             , V(660F00,4A,_,1,1,_,_,_  ), 0                         , 0 , 0 , 1358, 122, 65 , 0 ), // #308
  INST(Kaddq           , VexRvm             , V(000F00,4A,_,1,1,_,_,_  ), 0                         , 0 , 0 , 1364, 122, 65 , 0 ), // #309
  INST(Kaddw           , VexRvm             , V(000F00,4A,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1370, 122, 64 , 0 ), // #310
  INST(Kandb           , VexRvm             , V(660F00,41,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1376, 122, 64 , 0 ), // #311
  INST(Kandd           , VexRvm             , V(660F00,41,_,1,1,_,_,_  ), 0                         , 0 , 0 , 1382, 122, 65 , 0 ), // #312
  INST(Kandnb          , VexRvm             , V(660F00,42,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1388, 122, 64 , 0 ), // #313
  INST(Kandnd          , VexRvm             , V(660F00,42,_,1,1,_,_,_  ), 0                         , 0 , 0 , 1395, 122, 65 , 0 ), // #314
  INST(Kandnq          , VexRvm             , V(000F00,42,_,1,1,_,_,_  ), 0                         , 0 , 0 , 1402, 122, 65 , 0 ), // #315
  INST(Kandnw          , VexRvm             , V(000F00,42,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1409, 122, 66 , 0 ), // #316
  INST(Kandq           , VexRvm             , V(000F00,41,_,1,1,_,_,_  ), 0                         , 0 , 0 , 1416, 122, 65 , 0 ), // #317
  INST(Kandw           , VexRvm             , V(000F00,41,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1422, 122, 66 , 0 ), // #318
  INST(Kmovb           , VexKmov            , V(660F00,90,_,0,0,_,_,_  ), V(660F00,92,_,0,0,_,_,_  ), 0 , 0 , 1428, 123, 64 , 0 ), // #319
  INST(Kmovd           , VexKmov            , V(660F00,90,_,0,1,_,_,_  ), V(F20F00,92,_,0,0,_,_,_  ), 0 , 0 , 7360, 124, 65 , 0 ), // #320
  INST(Kmovq           , VexKmov            , V(000F00,90,_,0,1,_,_,_  ), V(F20F00,92,_,0,1,_,_,_  ), 0 , 0 , 7371, 125, 65 , 0 ), // #321
  INST(Kmovw           , VexKmov            , V(000F00,90,_,0,0,_,_,_  ), V(000F00,92,_,0,0,_,_,_  ), 0 , 0 , 1434, 126, 66 , 0 ), // #322
  INST(Knotb           , VexRm              , V(660F00,44,_,0,0,_,_,_  ), 0                         , 0 , 0 , 1440, 127, 64 , 0 ), // #323
  INST(Knotd           , VexRm              , V(660F00,44,_,0,1,_,_,_  ), 0                         , 0 , 0 , 1446, 127, 65 , 0 ), // #324
  INST(Knotq           , VexRm              , V(000F00,44,_,0,1,_,_,_  ), 0                         , 0 , 0 , 1452, 127, 65 , 0 ), // #325
  INST(Knotw           , VexRm              , V(000F00,44,_,0,0,_,_,_  ), 0                         , 0 , 0 , 1458, 127, 66 , 0 ), // #326
  INST(Korb            , VexRvm             , V(660F00,45,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1464, 122, 64 , 0 ), // #327
  INST(Kord            , VexRvm             , V(660F00,45,_,1,1,_,_,_  ), 0                         , 0 , 0 , 1469, 122, 65 , 0 ), // #328
  INST(Korq            , VexRvm             , V(000F00,45,_,1,1,_,_,_  ), 0                         , 0 , 0 , 1474, 122, 65 , 0 ), // #329
  INST(Kortestb        , VexRm              , V(660F00,98,_,0,0,_,_,_  ), 0                         , 0 , 0 , 1479, 128, 67 , 0 ), // #330
  INST(Kortestd        , VexRm              , V(660F00,98,_,0,1,_,_,_  ), 0                         , 0 , 0 , 1488, 128, 68 , 0 ), // #331
  INST(Kortestq        , VexRm              , V(000F00,98,_,0,1,_,_,_  ), 0                         , 0 , 0 , 1497, 128, 68 , 0 ), // #332
  INST(Kortestw        , VexRm              , V(000F00,98,_,0,0,_,_,_  ), 0                         , 0 , 0 , 1506, 128, 69 , 0 ), // #333
  INST(Korw            , VexRvm             , V(000F00,45,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1515, 122, 66 , 0 ), // #334
  INST(Kshiftlb        , VexRmi             , V(660F3A,32,_,0,0,_,_,_  ), 0                         , 0 , 0 , 1520, 129, 64 , 0 ), // #335
  INST(Kshiftld        , VexRmi             , V(660F3A,33,_,0,0,_,_,_  ), 0                         , 0 , 0 , 1529, 129, 65 , 0 ), // #336
  INST(Kshiftlq        , VexRmi             , V(660F3A,33,_,0,1,_,_,_  ), 0                         , 0 , 0 , 1538, 129, 65 , 0 ), // #337
  INST(Kshiftlw        , VexRmi             , V(660F3A,32,_,0,1,_,_,_  ), 0                         , 0 , 0 , 1547, 129, 66 , 0 ), // #338
  INST(Kshiftrb        , VexRmi             , V(660F3A,30,_,0,0,_,_,_  ), 0                         , 0 , 0 , 1556, 129, 64 , 0 ), // #339
  INST(Kshiftrd        , VexRmi             , V(660F3A,31,_,0,0,_,_,_  ), 0                         , 0 , 0 , 1565, 129, 65 , 0 ), // #340
  INST(Kshiftrq        , VexRmi             , V(660F3A,31,_,0,1,_,_,_  ), 0                         , 0 , 0 , 1574, 129, 65 , 0 ), // #341
  INST(Kshiftrw        , VexRmi             , V(660F3A,30,_,0,1,_,_,_  ), 0                         , 0 , 0 , 1583, 129, 66 , 0 ), // #342
  INST(Ktestb          , VexRm              , V(660F00,99,_,0,0,_,_,_  ), 0                         , 0 , 0 , 1592, 128, 67 , 0 ), // #343
  INST(Ktestd          , VexRm              , V(660F00,99,_,0,1,_,_,_  ), 0                         , 0 , 0 , 1599, 128, 68 , 0 ), // #344
  INST(Ktestq          , VexRm              , V(000F00,99,_,0,1,_,_,_  ), 0                         , 0 , 0 , 1606, 128, 68 , 0 ), // #345
  INST(Ktestw          , VexRm              , V(000F00,99,_,0,0,_,_,_  ), 0                         , 0 , 0 , 1613, 128, 67 , 0 ), // #346
  INST(Kunpckbw        , VexRvm             , V(660F00,4B,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1620, 122, 66 , 0 ), // #347
  INST(Kunpckdq        , VexRvm             , V(000F00,4B,_,1,1,_,_,_  ), 0                         , 0 , 0 , 1629, 122, 65 , 0 ), // #348
  INST(Kunpckwd        , VexRvm             , V(000F00,4B,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1638, 122, 65 , 0 ), // #349
  INST(Kxnorb          , VexRvm             , V(660F00,46,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1647, 122, 64 , 0 ), // #350
  INST(Kxnord          , VexRvm             , V(660F00,46,_,1,1,_,_,_  ), 0                         , 0 , 0 , 1654, 122, 65 , 0 ), // #351
  INST(Kxnorq          , VexRvm             , V(000F00,46,_,1,1,_,_,_  ), 0                         , 0 , 0 , 1661, 122, 65 , 0 ), // #352
  INST(Kxnorw          , VexRvm             , V(000F00,46,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1668, 122, 66 , 0 ), // #353
  INST(Kxorb           , VexRvm             , V(660F00,47,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1675, 122, 64 , 0 ), // #354
  INST(Kxord           , VexRvm             , V(660F00,47,_,1,1,_,_,_  ), 0                         , 0 , 0 , 1681, 122, 65 , 0 ), // #355
  INST(Kxorq           , VexRvm             , V(000F00,47,_,1,1,_,_,_  ), 0                         , 0 , 0 , 1687, 122, 65 , 0 ), // #356
  INST(Kxorw           , VexRvm             , V(000F00,47,_,1,0,_,_,_  ), 0                         , 0 , 0 , 1693, 122, 66 , 0 ), // #357
  INST(Lahf            , X86Op              , O(000000,9F,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1699, 130, 70 , 0 ), // #358
  INST(Lar             , X86Rm              , O(000F00,02,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1704, 131, 71 , 0 ), // #359
  INST(Lddqu           , ExtRm              , O(F20F00,F0,_,_,_,_,_,_  ), 0                         , 0 , 16, 5568, 132, 6  , 24), // #360
  INST(Ldmxcsr         , X86M_Only          , O(000F00,AE,2,_,_,_,_,_  ), 0                         , 0 , 0 , 5575, 133, 5  , 0 ), // #361
  INST(Lds             , X86Rm              , O(000000,C5,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1708, 134, 45 , 0 ), // #362
  INST(Lea             , X86Lea             , O(000000,8D,_,_,x,_,_,_  ), 0                         , 0 , 0 , 1712, 135, 0  , 0 ), // #363
  INST(Leave           , X86Op              , O(000000,C9,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1716, 34 , 45 , 0 ), // #364
  INST(Les             , X86Rm              , O(000000,C4,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1722, 134, 45 , 0 ), // #365
  INST(Lfence          , X86Fence           , O(000F00,AE,5,_,_,_,_,_  ), 0                         , 0 , 0 , 1726, 34 , 72 , 0 ), // #366
  INST(Lfs             , X86Rm              , O(000F00,B4,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1733, 136, 45 , 0 ), // #367
  INST(Lgdt            , X86M_Only          , O(000F00,01,2,_,_,_,_,_  ), 0                         , 0 , 0 , 1737, 35 , 23 , 0 ), // #368
  INST(Lgs             , X86Rm              , O(000F00,B5,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1742, 136, 45 , 0 ), // #369
  INST(Lidt            , X86M_Only          , O(000F00,01,3,_,_,_,_,_  ), 0                         , 0 , 0 , 1746, 35 , 23 , 0 ), // #370
  INST(Lldt            , X86M               , O(000F00,00,2,_,_,_,_,_  ), 0                         , 0 , 0 , 1751, 137, 23 , 0 ), // #371
  INST(Lmsw            , X86M               , O(000F00,01,6,_,_,_,_,_  ), 0                         , 0 , 0 , 1756, 137, 23 , 0 ), // #372
  INST(Lods            , X86StrRm           , O(000000,AC,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1761, 138, 73 , 0 ), // #373
  INST(Loop            , X86JecxzLoop       , 0                         , O(000000,E2,_,_,_,_,_,_  ), 0 , 0 , 1766, 139, 0  , 0 ), // #374
  INST(Loope           , X86JecxzLoop       , 0                         , O(000000,E1,_,_,_,_,_,_  ), 0 , 0 , 1771, 140, 58 , 0 ), // #375
  INST(Loopne          , X86JecxzLoop       , 0                         , O(000000,E0,_,_,_,_,_,_  ), 0 , 0 , 1777, 141, 58 , 0 ), // #376
  INST(Lsl             , X86Rm              , O(000F00,03,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1784, 142, 71 , 0 ), // #377
  INST(Lss             , X86Rm              , O(000F00,B2,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5975, 136, 45 , 0 ), // #378
  INST(Ltr             , X86M               , O(000F00,00,3,_,_,_,_,_  ), 0                         , 0 , 0 , 1788, 137, 23 , 0 ), // #379
  INST(Lzcnt           , X86Rm_Raw66H       , O(F30F00,BD,_,_,x,_,_,_  ), 0                         , 0 , 0 , 1792, 24 , 74 , 0 ), // #380
  INST(Maskmovdqu      , ExtRm_ZDI          , O(660F00,57,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5584, 143, 4  , 25), // #381
  INST(Maskmovq        , ExtRm_ZDI          , O(000F00,F7,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7368, 144, 75 , 0 ), // #382
  INST(Maxpd           , ExtRm              , O(660F00,5F,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5618, 5  , 4  , 26), // #383
  INST(Maxps           , ExtRm              , O(000F00,5F,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5625, 5  , 5  , 26), // #384
  INST(Maxsd           , ExtRm              , O(F20F00,5F,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7387, 6  , 4  , 26), // #385
  INST(Maxss           , ExtRm              , O(F30F00,5F,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5639, 7  , 5  , 26), // #386
  INST(Mfence          , X86Fence           , O(000F00,AE,6,_,_,_,_,_  ), 0                         , 0 , 0 , 1798, 34 , 72 , 0 ), // #387
  INST(Minpd           , ExtRm              , O(660F00,5D,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5646, 5  , 4  , 27), // #388
  INST(Minps           , ExtRm              , O(000F00,5D,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5653, 5  , 5  , 27), // #389
  INST(Minsd           , ExtRm              , O(F20F00,5D,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7451, 6  , 4  , 27), // #390
  INST(Minss           , ExtRm              , O(F30F00,5D,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5667, 7  , 5  , 27), // #391
  INST(Monitor         , X86Op              , O(000F01,C8,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1805, 145, 76 , 0 ), // #392
  INST(Monitorx        , X86Op              , O(000F01,FA,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1813, 145, 77 , 0 ), // #393
  INST(Mov             , X86Mov             , 0                         , 0                         , 0 , 0 , 138 , 146, 78 , 0 ), // #394
  INST(Movapd          , ExtMov             , O(660F00,28,_,_,_,_,_,_  ), O(660F00,29,_,_,_,_,_,_  ), 0 , 16, 5674, 147, 4  , 28), // #395
  INST(Movaps          , ExtMov             , O(000F00,28,_,_,_,_,_,_  ), O(000F00,29,_,_,_,_,_,_  ), 0 , 16, 5682, 148, 5  , 28), // #396
  INST(Movbe           , ExtMovbe           , O(000F38,F0,_,_,x,_,_,_  ), O(000F38,F1,_,_,x,_,_,_  ), 0 , 0 , 597 , 149, 79 , 0 ), // #397
  INST(Movd            , ExtMovd            , O(000F00,6E,_,_,_,_,_,_  ), O(000F00,7E,_,_,_,_,_,_  ), 0 , 16, 7361, 150, 80 , 29), // #398
  INST(Movddup         , ExtMov             , O(F20F00,12,_,_,_,_,_,_  ), 0                         , 0 , 16, 5696, 50 , 6  , 29), // #399
  INST(Movdq2q         , ExtMov             , O(F20F00,D6,_,_,_,_,_,_  ), 0                         , 0 , 8 , 1822, 151, 4  , 0 ), // #400
  INST(Movdqa          , ExtMov             , O(660F00,6F,_,_,_,_,_,_  ), O(660F00,7F,_,_,_,_,_,_  ), 0 , 16, 5705, 152, 4  , 30), // #401
  INST(Movdqu          , ExtMov             , O(F30F00,6F,_,_,_,_,_,_  ), O(F30F00,7F,_,_,_,_,_,_  ), 0 , 16, 5588, 153, 4  , 28), // #402
  INST(Movhlps         , ExtMov             , O(000F00,12,_,_,_,_,_,_  ), 0                         , 0 , 8 , 5780, 154, 5  , 26), // #403
  INST(Movhpd          , ExtMov             , O(660F00,16,_,_,_,_,_,_  ), O(660F00,17,_,_,_,_,_,_  ), 8 , 8 , 5789, 155, 4  , 31), // #404
  INST(Movhps          , ExtMov             , O(000F00,16,_,_,_,_,_,_  ), O(000F00,17,_,_,_,_,_,_  ), 8 , 8 , 5797, 156, 5  , 31), // #405
  INST(Movlhps         , ExtMov             , O(000F00,16,_,_,_,_,_,_  ), 0                         , 8 , 8 , 5805, 157, 5  , 26), // #406
  INST(Movlpd          , ExtMov             , O(660F00,12,_,_,_,_,_,_  ), O(660F00,13,_,_,_,_,_,_  ), 0 , 8 , 5814, 158, 4  , 31), // #407
  INST(Movlps          , ExtMov             , O(000F00,12,_,_,_,_,_,_  ), O(000F00,13,_,_,_,_,_,_  ), 0 , 8 , 5822, 159, 5  , 31), // #408
  INST(Movmskpd        , ExtMov             , O(660F00,50,_,_,_,_,_,_  ), 0                         , 0 , 8 , 5830, 160, 4  , 32), // #409
  INST(Movmskps        , ExtMov             , O(000F00,50,_,_,_,_,_,_  ), 0                         , 0 , 8 , 5840, 160, 5  , 32), // #410
  INST(Movntdq         , ExtMov             , 0                         , O(660F00,E7,_,_,_,_,_,_  ), 0 , 16, 5850, 161, 4  , 32), // #411
  INST(Movntdqa        , ExtMov             , O(660F38,2A,_,_,_,_,_,_  ), 0                         , 0 , 16, 5859, 132, 12 , 32), // #412
  INST(Movnti          , ExtMovnti          , O(000F00,C3,_,_,x,_,_,_  ), 0                         , 0 , 8 , 1830, 162, 4  , 0 ), // #413
  INST(Movntpd         , ExtMov             , 0                         , O(660F00,2B,_,_,_,_,_,_  ), 0 , 16, 5869, 163, 4  , 25), // #414
  INST(Movntps         , ExtMov             , 0                         , O(000F00,2B,_,_,_,_,_,_  ), 0 , 16, 5878, 164, 5  , 25), // #415
  INST(Movntq          , ExtMov             , 0                         , O(000F00,E7,_,_,_,_,_,_  ), 0 , 8 , 1837, 165, 75 , 0 ), // #416
  INST(Movntsd         , ExtMov             , 0                         , O(F20F00,2B,_,_,_,_,_,_  ), 0 , 8 , 1844, 166, 46 , 0 ), // #417
  INST(Movntss         , ExtMov             , 0                         , O(F30F00,2B,_,_,_,_,_,_  ), 0 , 4 , 1852, 167, 46 , 0 ), // #418
  INST(Movq            , ExtMovq            , O(000F00,6E,_,_,x,_,_,_  ), O(000F00,7E,_,_,x,_,_,_  ), 0 , 16, 7372, 168, 80 , 28), // #419
  INST(Movq2dq         , ExtRm              , O(F30F00,D6,_,_,_,_,_,_  ), 0                         , 0 , 16, 1860, 169, 4  , 0 ), // #420
  INST(Movs            , X86StrMm           , O(000000,A4,_,_,_,_,_,_  ), 0                         , 0 , 0 , 411 , 170, 73 , 0 ), // #421
  INST(Movsd           , ExtMov             , O(F20F00,10,_,_,_,_,_,_  ), O(F20F00,11,_,_,_,_,_,_  ), 0 , 8 , 5893, 171, 81 , 33), // #422
  INST(Movshdup        , ExtRm              , O(F30F00,16,_,_,_,_,_,_  ), 0                         , 0 , 16, 5900, 51 , 6  , 30), // #423
  INST(Movsldup        , ExtRm              , O(F30F00,12,_,_,_,_,_,_  ), 0                         , 0 , 16, 5910, 51 , 6  , 30), // #424
  INST(Movss           , ExtMov             , O(F30F00,10,_,_,_,_,_,_  ), O(F30F00,11,_,_,_,_,_,_  ), 0 , 4 , 5920, 172, 82 , 33), // #425
  INST(Movsx           , X86MovsxMovzx      , O(000F00,BE,_,_,x,_,_,_  ), 0                         , 0 , 0 , 1868, 173, 0  , 0 ), // #426
  INST(Movsxd          , X86Rm              , O(000000,63,_,_,1,_,_,_  ), 0                         , 0 , 0 , 1874, 174, 0  , 0 ), // #427
  INST(Movupd          , ExtMov             , O(660F00,10,_,_,_,_,_,_  ), O(660F00,11,_,_,_,_,_,_  ), 0 , 16, 5927, 175, 4  , 34), // #428
  INST(Movups          , ExtMov             , O(000F00,10,_,_,_,_,_,_  ), O(000F00,11,_,_,_,_,_,_  ), 0 , 16, 5935, 176, 5  , 34), // #429
  INST(Movzx           , X86MovsxMovzx      , O(000F00,B6,_,_,x,_,_,_  ), 0                         , 0 , 0 , 1881, 173, 0  , 0 ), // #430
  INST(Mpsadbw         , ExtRmi             , O(660F3A,42,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5943, 16 , 12 , 35), // #431
  INST(Mul             , X86M_GPB_MulDiv    , O(000000,F6,4,_,x,_,_,_  ), 0                         , 0 , 0 , 769 , 177, 1  , 0 ), // #432
  INST(Mulpd           , ExtRm              , O(660F00,59,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5952, 5  , 4  , 36), // #433
  INST(Mulps           , ExtRm              , O(000F00,59,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5959, 5  , 5  , 36), // #434
  INST(Mulsd           , ExtRm              , O(F20F00,59,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5966, 6  , 4  , 36), // #435
  INST(Mulss           , ExtRm              , O(F30F00,59,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5973, 7  , 5  , 36), // #436
  INST(Mulx            , VexRvm_ZDX_Wx      , V(F20F38,F6,_,0,x,_,_,_  ), 0                         , 0 , 0 , 1887, 178, 83 , 0 ), // #437
  INST(Mwait           , X86Op              , O(000F01,C9,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1892, 179, 76 , 0 ), // #438
  INST(Mwaitx          , X86Op              , O(000F01,FB,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1898, 180, 77 , 0 ), // #439
  INST(Neg             , X86M_GPB           , O(000000,F6,3,_,x,_,_,_  ), 0                         , 0 , 0 , 1905, 181, 84 , 0 ), // #440
  INST(Nop             , X86Op              , O(000000,90,_,_,_,_,_,_  ), 0                         , 0 , 0 , 900 , 182, 0  , 0 ), // #441
  INST(Not             , X86M_GPB           , O(000000,F6,2,_,x,_,_,_  ), 0                         , 0 , 0 , 1909, 181, 0  , 0 ), // #442
  INST(Or              , X86Arith           , O(000000,08,1,_,x,_,_,_  ), 0                         , 0 , 0 , 1109, 183, 1  , 0 ), // #443
  INST(Orpd            , ExtRm              , O(660F00,56,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9445, 12 , 4  , 37), // #444
  INST(Orps            , ExtRm              , O(000F00,56,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9452, 12 , 5  , 37), // #445
  INST(Out             , X86Out             , O(000000,EE,_,_,_,_,_,_  ), O(000000,E6,_,_,_,_,_,_  ), 0 , 0 , 1913, 184, 45 , 0 ), // #446
  INST(Outs            , X86Outs            , O(000000,6E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1917, 185, 45 , 0 ), // #447
  INST(Pabsb           , ExtRm_P            , O(000F38,1C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6013, 186, 85 , 38), // #448
  INST(Pabsd           , ExtRm_P            , O(000F38,1E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6020, 186, 85 , 38), // #449
  INST(Pabsw           , ExtRm_P            , O(000F38,1D,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6034, 186, 85 , 39), // #450
  INST(Packssdw        , ExtRm_P            , O(000F00,6B,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6041, 187, 80 , 40), // #451
  INST(Packsswb        , ExtRm_P            , O(000F00,63,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6051, 187, 80 , 40), // #452
  INST(Packusdw        , ExtRm              , O(660F38,2B,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6061, 5  , 12 , 40), // #453
  INST(Packuswb        , ExtRm_P            , O(000F00,67,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6071, 187, 80 , 40), // #454
  INST(Paddb           , ExtRm_P            , O(000F00,FC,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6081, 187, 80 , 40), // #455
  INST(Paddd           , ExtRm_P            , O(000F00,FE,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6088, 187, 80 , 40), // #456
  INST(Paddq           , ExtRm_P            , O(000F00,D4,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6095, 187, 4  , 40), // #457
  INST(Paddsb          , ExtRm_P            , O(000F00,EC,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6102, 187, 80 , 40), // #458
  INST(Paddsw          , ExtRm_P            , O(000F00,ED,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6110, 187, 80 , 40), // #459
  INST(Paddusb         , ExtRm_P            , O(000F00,DC,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6118, 187, 80 , 40), // #460
  INST(Paddusw         , ExtRm_P            , O(000F00,DD,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6127, 187, 80 , 40), // #461
  INST(Paddw           , ExtRm_P            , O(000F00,FD,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6136, 187, 80 , 40), // #462
  INST(Palignr         , ExtRmi_P           , O(000F3A,0F,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6143, 188, 6  , 40), // #463
  INST(Pand            , ExtRm_P            , O(000F00,DB,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6152, 189, 80 , 40), // #464
  INST(Pandn           , ExtRm_P            , O(000F00,DF,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6165, 190, 80 , 41), // #465
  INST(Pause           , X86Op              , O(F30000,90,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1922, 34 , 45 , 0 ), // #466
  INST(Pavgb           , ExtRm_P            , O(000F00,E0,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6195, 187, 86 , 42), // #467
  INST(Pavgusb         , Ext3dNow           , O(000F0F,BF,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1928, 191, 87 , 0 ), // #468
  INST(Pavgw           , ExtRm_P            , O(000F00,E3,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6202, 187, 86 , 43), // #469
  INST(Pblendvb        , ExtRm_XMM0         , O(660F38,10,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6218, 17 , 12 , 44), // #470
  INST(Pblendw         , ExtRmi             , O(660F3A,0E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6228, 16 , 12 , 42), // #471
  INST(Pclmulqdq       , ExtRmi             , O(660F3A,44,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6321, 16 , 88 , 45), // #472
  INST(Pcmpeqb         , ExtRm_P            , O(000F00,74,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6353, 190, 80 , 46), // #473
  INST(Pcmpeqd         , ExtRm_P            , O(000F00,76,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6362, 190, 80 , 46), // #474
  INST(Pcmpeqq         , ExtRm              , O(660F38,29,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6371, 192, 12 , 46), // #475
  INST(Pcmpeqw         , ExtRm_P            , O(000F00,75,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6380, 190, 80 , 46), // #476
  INST(Pcmpestri       , ExtRmi             , O(660F3A,61,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6389, 193, 89 , 28), // #477
  INST(Pcmpestrm       , ExtRmi             , O(660F3A,60,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6400, 194, 89 , 28), // #478
  INST(Pcmpgtb         , ExtRm_P            , O(000F00,64,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6411, 190, 80 , 46), // #479
  INST(Pcmpgtd         , ExtRm_P            , O(000F00,66,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6420, 190, 80 , 46), // #480
  INST(Pcmpgtq         , ExtRm              , O(660F38,37,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6429, 192, 42 , 46), // #481
  INST(Pcmpgtw         , ExtRm_P            , O(000F00,65,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6438, 190, 80 , 46), // #482
  INST(Pcmpistri       , ExtRmi             , O(660F3A,63,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6447, 195, 89 , 28), // #483
  INST(Pcmpistrm       , ExtRmi             , O(660F3A,62,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6458, 196, 89 , 28), // #484
  INST(Pcommit         , X86Op_O            , O(660F00,AE,7,_,_,_,_,_  ), 0                         , 0 , 0 , 1936, 34 , 90 , 0 ), // #485
  INST(Pdep            , VexRvm_Wx          , V(F20F38,F5,_,0,x,_,_,_  ), 0                         , 0 , 0 , 1944, 11 , 83 , 0 ), // #486
  INST(Pext            , VexRvm_Wx          , V(F30F38,F5,_,0,x,_,_,_  ), 0                         , 0 , 0 , 1949, 11 , 83 , 0 ), // #487
  INST(Pextrb          , ExtExtract         , O(000F3A,14,_,_,_,_,_,_  ), 0                         , 0 , 8 , 6863, 197, 12 , 47), // #488
  INST(Pextrd          , ExtExtract         , O(000F3A,16,_,_,_,_,_,_  ), 0                         , 0 , 8 , 6871, 68 , 12 , 47), // #489
  INST(Pextrq          , ExtExtract         , O(000F3A,16,_,_,1,_,_,_  ), 0                         , 0 , 8 , 6879, 198, 12 , 47), // #490
  INST(Pextrw          , ExtPextrw          , O(000F00,C5,_,_,_,_,_,_  ), O(000F3A,15,_,_,_,_,_,_  ), 0 , 8 , 6887, 199, 91 , 47), // #491
  INST(Pf2id           , Ext3dNow           , O(000F0F,1D,_,_,_,_,_,_  ), 0                         , 0 , 8 , 1954, 200, 87 , 0 ), // #492
  INST(Pf2iw           , Ext3dNow           , O(000F0F,1C,_,_,_,_,_,_  ), 0                         , 0 , 8 , 1960, 200, 92 , 0 ), // #493
  INST(Pfacc           , Ext3dNow           , O(000F0F,AE,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1966, 191, 87 , 0 ), // #494
  INST(Pfadd           , Ext3dNow           , O(000F0F,9E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1972, 191, 87 , 0 ), // #495
  INST(Pfcmpeq         , Ext3dNow           , O(000F0F,B0,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1978, 191, 87 , 0 ), // #496
  INST(Pfcmpge         , Ext3dNow           , O(000F0F,90,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1986, 191, 87 , 0 ), // #497
  INST(Pfcmpgt         , Ext3dNow           , O(000F0F,A0,_,_,_,_,_,_  ), 0                         , 0 , 0 , 1994, 191, 87 , 0 ), // #498
  INST(Pfmax           , Ext3dNow           , O(000F0F,A4,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2002, 191, 87 , 0 ), // #499
  INST(Pfmin           , Ext3dNow           , O(000F0F,94,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2008, 191, 87 , 0 ), // #500
  INST(Pfmul           , Ext3dNow           , O(000F0F,B4,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2014, 191, 87 , 0 ), // #501
  INST(Pfnacc          , Ext3dNow           , O(000F0F,8A,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2020, 191, 92 , 0 ), // #502
  INST(Pfpnacc         , Ext3dNow           , O(000F0F,8E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2027, 191, 92 , 0 ), // #503
  INST(Pfrcp           , Ext3dNow           , O(000F0F,96,_,_,_,_,_,_  ), 0                         , 0 , 8 , 2035, 200, 87 , 0 ), // #504
  INST(Pfrcpit1        , Ext3dNow           , O(000F0F,A6,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2041, 191, 87 , 0 ), // #505
  INST(Pfrcpit2        , Ext3dNow           , O(000F0F,B6,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2050, 191, 87 , 0 ), // #506
  INST(Pfrcpv          , Ext3dNow           , O(000F0F,86,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2059, 191, 93 , 0 ), // #507
  INST(Pfrsqit1        , Ext3dNow           , O(000F0F,A7,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2066, 201, 87 , 0 ), // #508
  INST(Pfrsqrt         , Ext3dNow           , O(000F0F,97,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2075, 201, 87 , 0 ), // #509
  INST(Pfrsqrtv        , Ext3dNow           , O(000F0F,87,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2083, 191, 93 , 0 ), // #510
  INST(Pfsub           , Ext3dNow           , O(000F0F,9A,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2092, 191, 87 , 0 ), // #511
  INST(Pfsubr          , Ext3dNow           , O(000F0F,AA,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2098, 191, 87 , 0 ), // #512
  INST(Phaddd          , ExtRm_P            , O(000F38,02,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6966, 187, 85 , 48), // #513
  INST(Phaddsw         , ExtRm_P            , O(000F38,03,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6983, 187, 85 , 49), // #514
  INST(Phaddw          , ExtRm_P            , O(000F38,01,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7052, 187, 85 , 50), // #515
  INST(Phminposuw      , ExtRm              , O(660F38,41,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7078, 8  , 12 , 51), // #516
  INST(Phsubd          , ExtRm_P            , O(000F38,06,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7099, 187, 85 , 52), // #517
  INST(Phsubsw         , ExtRm_P            , O(000F38,07,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7116, 187, 85 , 53), // #518
  INST(Phsubw          , ExtRm_P            , O(000F38,05,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7125, 187, 85 , 53), // #519
  INST(Pi2fd           , Ext3dNow           , O(000F0F,0D,_,_,_,_,_,_  ), 0                         , 0 , 8 , 2105, 200, 87 , 0 ), // #520
  INST(Pi2fw           , Ext3dNow           , O(000F0F,0C,_,_,_,_,_,_  ), 0                         , 0 , 8 , 2111, 200, 92 , 0 ), // #521
  INST(Pinsrb          , ExtRmi             , O(660F3A,20,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7142, 202, 12 , 52), // #522
  INST(Pinsrd          , ExtRmi             , O(660F3A,22,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7150, 203, 12 , 52), // #523
  INST(Pinsrq          , ExtRmi             , O(660F3A,22,_,_,1,_,_,_  ), 0                         , 0 , 0 , 7158, 204, 12 , 52), // #524
  INST(Pinsrw          , ExtRmi_P           , O(000F00,C4,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7166, 205, 86 , 52), // #525
  INST(Pmaddubsw       , ExtRm_P            , O(000F38,04,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7336, 187, 85 , 54), // #526
  INST(Pmaddwd         , ExtRm_P            , O(000F00,F5,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7347, 187, 80 , 54), // #527
  INST(Pmaxsb          , ExtRm              , O(660F38,3C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7378, 12 , 12 , 55), // #528
  INST(Pmaxsd          , ExtRm              , O(660F38,3D,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7386, 12 , 12 , 55), // #529
  INST(Pmaxsw          , ExtRm_P            , O(000F00,EE,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7402, 189, 86 , 56), // #530
  INST(Pmaxub          , ExtRm_P            , O(000F00,DE,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7410, 189, 86 , 56), // #531
  INST(Pmaxud          , ExtRm              , O(660F38,3F,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7418, 12 , 12 , 56), // #532
  INST(Pmaxuw          , ExtRm              , O(660F38,3E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7434, 12 , 12 , 57), // #533
  INST(Pminsb          , ExtRm              , O(660F38,38,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7442, 12 , 12 , 57), // #534
  INST(Pminsd          , ExtRm              , O(660F38,39,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7450, 12 , 12 , 57), // #535
  INST(Pminsw          , ExtRm_P            , O(000F00,EA,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7466, 189, 86 , 58), // #536
  INST(Pminub          , ExtRm_P            , O(000F00,DA,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7474, 189, 86 , 58), // #537
  INST(Pminud          , ExtRm              , O(660F38,3B,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7482, 12 , 12 , 58), // #538
  INST(Pminuw          , ExtRm              , O(660F38,3A,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7498, 12 , 12 , 59), // #539
  INST(Pmovmskb        , ExtRm_P            , O(000F00,D7,_,_,_,_,_,_  ), 0                         , 0 , 8 , 7576, 206, 86 , 60), // #540
  INST(Pmovsxbd        , ExtRm              , O(660F38,21,_,_,_,_,_,_  ), 0                         , 0 , 16, 7673, 207, 12 , 61), // #541
  INST(Pmovsxbq        , ExtRm              , O(660F38,22,_,_,_,_,_,_  ), 0                         , 0 , 16, 7683, 208, 12 , 61), // #542
  INST(Pmovsxbw        , ExtRm              , O(660F38,20,_,_,_,_,_,_  ), 0                         , 0 , 16, 7693, 50 , 12 , 61), // #543
  INST(Pmovsxdq        , ExtRm              , O(660F38,25,_,_,_,_,_,_  ), 0                         , 0 , 16, 7703, 50 , 12 , 61), // #544
  INST(Pmovsxwd        , ExtRm              , O(660F38,23,_,_,_,_,_,_  ), 0                         , 0 , 16, 7713, 50 , 12 , 61), // #545
  INST(Pmovsxwq        , ExtRm              , O(660F38,24,_,_,_,_,_,_  ), 0                         , 0 , 16, 7723, 207, 12 , 61), // #546
  INST(Pmovzxbd        , ExtRm              , O(660F38,31,_,_,_,_,_,_  ), 0                         , 0 , 16, 7810, 207, 12 , 18), // #547
  INST(Pmovzxbq        , ExtRm              , O(660F38,32,_,_,_,_,_,_  ), 0                         , 0 , 16, 7820, 208, 12 , 18), // #548
  INST(Pmovzxbw        , ExtRm              , O(660F38,30,_,_,_,_,_,_  ), 0                         , 0 , 16, 7830, 50 , 12 , 18), // #549
  INST(Pmovzxdq        , ExtRm              , O(660F38,35,_,_,_,_,_,_  ), 0                         , 0 , 16, 7840, 50 , 12 , 18), // #550
  INST(Pmovzxwd        , ExtRm              , O(660F38,33,_,_,_,_,_,_  ), 0                         , 0 , 16, 7850, 50 , 12 , 18), // #551
  INST(Pmovzxwq        , ExtRm              , O(660F38,34,_,_,_,_,_,_  ), 0                         , 0 , 16, 7860, 207, 12 , 18), // #552
  INST(Pmuldq          , ExtRm              , O(660F38,28,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7870, 5  , 12 , 62), // #553
  INST(Pmulhrsw        , ExtRm_P            , O(000F38,0B,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7878, 187, 85 , 62), // #554
  INST(Pmulhrw         , Ext3dNow           , O(000F0F,B7,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2117, 191, 87 , 0 ), // #555
  INST(Pmulhuw         , ExtRm_P            , O(000F00,E4,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7888, 187, 86 , 63), // #556
  INST(Pmulhw          , ExtRm_P            , O(000F00,E5,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7897, 187, 80 , 63), // #557
  INST(Pmulld          , ExtRm              , O(660F38,40,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7905, 5  , 12 , 63), // #558
  INST(Pmullw          , ExtRm_P            , O(000F00,D5,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7921, 187, 80 , 62), // #559
  INST(Pmuludq         , ExtRm_P            , O(000F00,F4,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7944, 187, 4  , 64), // #560
  INST(Pop             , X86Pop             , O(000000,8F,0,_,_,_,_,_  ), O(000000,58,_,_,_,_,_,_  ), 0 , 0 , 2125, 209, 0  , 0 ), // #561
  INST(Popa            , X86Op              , O(660000,61,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2129, 100, 0  , 0 ), // #562
  INST(Popad           , X86Op              , O(000000,61,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2134, 100, 0  , 0 ), // #563
  INST(Popcnt          , X86Rm_Raw66H       , O(F30F00,B8,_,_,x,_,_,_  ), 0                         , 0 , 0 , 2140, 24 , 94 , 0 ), // #564
  INST(Popf            , X86Op              , O(660000,9D,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2147, 34 , 1  , 0 ), // #565
  INST(Popfd           , X86Op              , O(000000,9D,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2152, 100, 1  , 0 ), // #566
  INST(Popfq           , X86Op              , O(000000,9D,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2158, 210, 1  , 0 ), // #567
  INST(Por             , ExtRm_P            , O(000F00,EB,_,_,_,_,_,_  ), 0                         , 0 , 0 , 7971, 189, 80 , 65), // #568
  INST(Prefetch        , X86M_Only          , O(000F00,0D,0,_,_,_,_,_  ), 0                         , 0 , 0 , 2164, 35 , 95 , 0 ), // #569
  INST(Prefetchnta     , X86M_Only          , O(000F00,18,0,_,_,_,_,_  ), 0                         , 0 , 0 , 2173, 35 , 96 , 0 ), // #570
  INST(Prefetcht0      , X86M_Only          , O(000F00,18,1,_,_,_,_,_  ), 0                         , 0 , 0 , 2185, 35 , 96 , 0 ), // #571
  INST(Prefetcht1      , X86M_Only          , O(000F00,18,2,_,_,_,_,_  ), 0                         , 0 , 0 , 2196, 35 , 96 , 0 ), // #572
  INST(Prefetcht2      , X86M_Only          , O(000F00,18,3,_,_,_,_,_  ), 0                         , 0 , 0 , 2207, 35 , 96 , 0 ), // #573
  INST(Prefetchw       , X86M_Only          , O(000F00,0D,1,_,_,_,_,_  ), 0                         , 0 , 0 , 2218, 35 , 97 , 0 ), // #574
  INST(Prefetchwt1     , X86M_Only          , O(000F00,0D,2,_,_,_,_,_  ), 0                         , 0 , 0 , 2228, 35 , 98 , 0 ), // #575
  INST(Psadbw          , ExtRm_P            , O(000F00,F6,_,_,_,_,_,_  ), 0                         , 0 , 0 , 3811, 187, 86 , 66), // #576
  INST(Pshufb          , ExtRm_P            , O(000F38,00,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8195, 187, 85 , 67), // #577
  INST(Pshufd          , ExtRmi             , O(660F00,70,_,_,_,_,_,_  ), 0                         , 0 , 16, 8203, 211, 4  , 20), // #578
  INST(Pshufhw         , ExtRmi             , O(F30F00,70,_,_,_,_,_,_  ), 0                         , 0 , 16, 8211, 211, 4  , 20), // #579
  INST(Pshuflw         , ExtRmi             , O(F20F00,70,_,_,_,_,_,_  ), 0                         , 0 , 16, 8220, 211, 4  , 20), // #580
  INST(Pshufw          , ExtRmi_P           , O(000F00,70,_,_,_,_,_,_  ), 0                         , 0 , 8 , 2240, 212, 75 , 0 ), // #581
  INST(Psignb          , ExtRm_P            , O(000F38,08,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8229, 187, 85 , 68), // #582
  INST(Psignd          , ExtRm_P            , O(000F38,0A,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8237, 187, 85 , 68), // #583
  INST(Psignw          , ExtRm_P            , O(000F38,09,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8245, 187, 85 , 68), // #584
  INST(Pslld           , ExtRmRi_P          , O(000F00,F2,_,_,_,_,_,_  ), O(000F00,72,6,_,_,_,_,_  ), 0 , 0 , 8253, 213, 80 , 68), // #585
  INST(Pslldq          , ExtRmRi            , 0                         , O(660F00,73,7,_,_,_,_,_  ), 0 , 0 , 8260, 214, 4  , 68), // #586
  INST(Psllq           , ExtRmRi_P          , O(000F00,F3,_,_,_,_,_,_  ), O(000F00,73,6,_,_,_,_,_  ), 0 , 0 , 8268, 215, 80 , 68), // #587
  INST(Psllw           , ExtRmRi_P          , O(000F00,F1,_,_,_,_,_,_  ), O(000F00,71,6,_,_,_,_,_  ), 0 , 0 , 8299, 216, 80 , 69), // #588
  INST(Psrad           , ExtRmRi_P          , O(000F00,E2,_,_,_,_,_,_  ), O(000F00,72,4,_,_,_,_,_  ), 0 , 0 , 8306, 217, 80 , 69), // #589
  INST(Psraw           , ExtRmRi_P          , O(000F00,E1,_,_,_,_,_,_  ), O(000F00,71,4,_,_,_,_,_  ), 0 , 0 , 8344, 218, 80 , 70), // #590
  INST(Psrld           , ExtRmRi_P          , O(000F00,D2,_,_,_,_,_,_  ), O(000F00,72,2,_,_,_,_,_  ), 0 , 0 , 8351, 219, 80 , 70), // #591
  INST(Psrldq          , ExtRmRi            , 0                         , O(660F00,73,3,_,_,_,_,_  ), 0 , 0 , 8358, 220, 4  , 70), // #592
  INST(Psrlq           , ExtRmRi_P          , O(000F00,D3,_,_,_,_,_,_  ), O(000F00,73,2,_,_,_,_,_  ), 0 , 0 , 8366, 221, 80 , 70), // #593
  INST(Psrlw           , ExtRmRi_P          , O(000F00,D1,_,_,_,_,_,_  ), O(000F00,71,2,_,_,_,_,_  ), 0 , 0 , 8397, 222, 80 , 71), // #594
  INST(Psubb           , ExtRm_P            , O(000F00,F8,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8404, 190, 80 , 71), // #595
  INST(Psubd           , ExtRm_P            , O(000F00,FA,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8411, 190, 80 , 71), // #596
  INST(Psubq           , ExtRm_P            , O(000F00,FB,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8418, 190, 4  , 71), // #597
  INST(Psubsb          , ExtRm_P            , O(000F00,E8,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8425, 190, 80 , 71), // #598
  INST(Psubsw          , ExtRm_P            , O(000F00,E9,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8433, 190, 80 , 71), // #599
  INST(Psubusb         , ExtRm_P            , O(000F00,D8,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8441, 190, 80 , 71), // #600
  INST(Psubusw         , ExtRm_P            , O(000F00,D9,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8450, 190, 80 , 71), // #601
  INST(Psubw           , ExtRm_P            , O(000F00,F9,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8459, 190, 80 , 71), // #602
  INST(Pswapd          , Ext3dNow           , O(000F0F,BB,_,_,_,_,_,_  ), 0                         , 0 , 8 , 2247, 200, 92 , 0 ), // #603
  INST(Ptest           , ExtRm              , O(660F38,17,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8488, 223, 99 , 72), // #604
  INST(Punpckhbw       , ExtRm_P            , O(000F00,68,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8571, 187, 80 , 73), // #605
  INST(Punpckhdq       , ExtRm_P            , O(000F00,6A,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8582, 187, 80 , 73), // #606
  INST(Punpckhqdq      , ExtRm              , O(660F00,6D,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8593, 5  , 4  , 73), // #607
  INST(Punpckhwd       , ExtRm_P            , O(000F00,69,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8605, 187, 80 , 73), // #608
  INST(Punpcklbw       , ExtRm_P            , O(000F00,60,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8616, 187, 80 , 73), // #609
  INST(Punpckldq       , ExtRm_P            , O(000F00,62,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8627, 187, 80 , 73), // #610
  INST(Punpcklqdq      , ExtRm              , O(660F00,6C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8638, 5  , 4  , 73), // #611
  INST(Punpcklwd       , ExtRm_P            , O(000F00,61,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8650, 187, 80 , 73), // #612
  INST(Push            , X86Push            , O(000000,FF,6,_,_,_,_,_  ), O(000000,50,_,_,_,_,_,_  ), 0 , 0 , 2254, 224, 0  , 0 ), // #613
  INST(Pusha           , X86Op              , O(660000,60,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2259, 100, 0  , 0 ), // #614
  INST(Pushad          , X86Op              , O(000000,60,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2265, 100, 0  , 0 ), // #615
  INST(Pushf           , X86Op              , O(660000,9C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2272, 34 , 0  , 0 ), // #616
  INST(Pushfd          , X86Op              , O(000000,9C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2278, 100, 0  , 0 ), // #617
  INST(Pushfq          , X86Op              , O(000000,9C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2285, 210, 0  , 0 ), // #618
  INST(Pxor            , ExtRm_P            , O(000F00,EF,_,_,_,_,_,_  ), 0                         , 0 , 0 , 8661, 190, 80 , 22), // #619
  INST(Rcl             , X86Rot             , O(000000,D0,2,_,x,_,_,_  ), 0                         , 0 , 0 , 2292, 225, 100, 0 ), // #620
  INST(Rcpps           , ExtRm              , O(000F00,53,_,_,_,_,_,_  ), 0                         , 0 , 16, 8789, 51 , 5  , 74), // #621
  INST(Rcpss           , ExtRm              , O(F30F00,53,_,_,_,_,_,_  ), 0                         , 0 , 4 , 8796, 226, 5  , 75), // #622
  INST(Rcr             , X86Rot             , O(000000,D0,3,_,x,_,_,_  ), 0                         , 0 , 0 , 2296, 225, 100, 0 ), // #623
  INST(Rdfsbase        , X86M               , O(F30F00,AE,0,_,x,_,_,_  ), 0                         , 0 , 8 , 2300, 227, 101, 0 ), // #624
  INST(Rdgsbase        , X86M               , O(F30F00,AE,1,_,x,_,_,_  ), 0                         , 0 , 8 , 2309, 227, 101, 0 ), // #625
  INST(Rdmsr           , X86Op              , O(000F00,32,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2318, 228, 102, 0 ), // #626
  INST(Rdpmc           , X86Op              , O(000F00,33,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2324, 228, 23 , 0 ), // #627
  INST(Rdrand          , X86M               , O(000F00,C7,6,_,x,_,_,_  ), 0                         , 0 , 8 , 2330, 229, 103, 0 ), // #628
  INST(Rdseed          , X86M               , O(000F00,C7,7,_,x,_,_,_  ), 0                         , 0 , 8 , 2337, 229, 104, 0 ), // #629
  INST(Rdtsc           , X86Op              , O(000F00,31,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2344, 230, 105, 0 ), // #630
  INST(Rdtscp          , X86Op              , O(000F01,F9,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2350, 231, 106, 0 ), // #631
  INST(Ret             , X86Ret             , O(000000,C2,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2764, 232, 45 , 0 ), // #632
  INST(Rol             , X86Rot             , O(000000,D0,0,_,x,_,_,_  ), 0                         , 0 , 0 , 2357, 225, 107, 0 ), // #633
  INST(Ror             , X86Rot             , O(000000,D0,1,_,x,_,_,_  ), 0                         , 0 , 0 , 2361, 225, 107, 0 ), // #634
  INST(Rorx            , VexRmi_Wx          , V(F20F3A,F0,_,0,x,_,_,_  ), 0                         , 0 , 0 , 2365, 233, 83 , 0 ), // #635
  INST(Roundpd         , ExtRmi             , O(660F3A,09,_,_,_,_,_,_  ), 0                         , 0 , 16, 8891, 211, 12 , 76), // #636
  INST(Roundps         , ExtRmi             , O(660F3A,08,_,_,_,_,_,_  ), 0                         , 0 , 16, 8900, 211, 12 , 76), // #637
  INST(Roundsd         , ExtRmi             , O(660F3A,0B,_,_,_,_,_,_  ), 0                         , 0 , 8 , 8909, 234, 12 , 77), // #638
  INST(Roundss         , ExtRmi             , O(660F3A,0A,_,_,_,_,_,_  ), 0                         , 0 , 4 , 8918, 235, 12 , 77), // #639
  INST(Rsm             , X86Op              , O(000F00,AA,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2370, 100, 108, 0 ), // #640
  INST(Rsqrtps         , ExtRm              , O(000F00,52,_,_,_,_,_,_  ), 0                         , 0 , 16, 9015, 51 , 5  , 78), // #641
  INST(Rsqrtss         , ExtRm              , O(F30F00,52,_,_,_,_,_,_  ), 0                         , 0 , 4 , 9024, 226, 5  , 79), // #642
  INST(Sahf            , X86Op              , O(000000,9E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2374, 236, 109, 0 ), // #643
  INST(Sal             , X86Rot             , O(000000,D0,4,_,x,_,_,_  ), 0                         , 0 , 0 , 2379, 225, 1  , 0 ), // #644
  INST(Sar             , X86Rot             , O(000000,D0,7,_,x,_,_,_  ), 0                         , 0 , 0 , 2383, 225, 1  , 0 ), // #645
  INST(Sarx            , VexRmv_Wx          , V(F30F38,F7,_,0,x,_,_,_  ), 0                         , 0 , 0 , 2387, 14 , 83 , 0 ), // #646
  INST(Sbb             , X86Arith           , O(000000,18,3,_,x,_,_,_  ), 0                         , 0 , 0 , 2392, 3  , 2  , 0 ), // #647
  INST(Scas            , X86StrRm           , O(000000,AE,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2396, 237, 35 , 0 ), // #648
  INST(Seta            , X86Set             , O(000F00,97,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2401, 238, 56 , 0 ), // #649
  INST(Setae           , X86Set             , O(000F00,93,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2406, 238, 57 , 0 ), // #650
  INST(Setb            , X86Set             , O(000F00,92,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2412, 238, 57 , 0 ), // #651
  INST(Setbe           , X86Set             , O(000F00,96,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2417, 238, 56 , 0 ), // #652
  INST(Setc            , X86Set             , O(000F00,92,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2423, 238, 57 , 0 ), // #653
  INST(Sete            , X86Set             , O(000F00,94,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2428, 238, 58 , 0 ), // #654
  INST(Setg            , X86Set             , O(000F00,9F,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2433, 238, 59 , 0 ), // #655
  INST(Setge           , X86Set             , O(000F00,9D,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2438, 238, 60 , 0 ), // #656
  INST(Setl            , X86Set             , O(000F00,9C,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2444, 238, 60 , 0 ), // #657
  INST(Setle           , X86Set             , O(000F00,9E,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2449, 238, 59 , 0 ), // #658
  INST(Setna           , X86Set             , O(000F00,96,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2455, 238, 56 , 0 ), // #659
  INST(Setnae          , X86Set             , O(000F00,92,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2461, 238, 57 , 0 ), // #660
  INST(Setnb           , X86Set             , O(000F00,93,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2468, 238, 57 , 0 ), // #661
  INST(Setnbe          , X86Set             , O(000F00,97,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2474, 238, 56 , 0 ), // #662
  INST(Setnc           , X86Set             , O(000F00,93,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2481, 238, 57 , 0 ), // #663
  INST(Setne           , X86Set             , O(000F00,95,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2487, 238, 58 , 0 ), // #664
  INST(Setng           , X86Set             , O(000F00,9E,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2493, 238, 59 , 0 ), // #665
  INST(Setnge          , X86Set             , O(000F00,9C,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2499, 238, 60 , 0 ), // #666
  INST(Setnl           , X86Set             , O(000F00,9D,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2506, 238, 60 , 0 ), // #667
  INST(Setnle          , X86Set             , O(000F00,9F,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2512, 238, 59 , 0 ), // #668
  INST(Setno           , X86Set             , O(000F00,91,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2519, 238, 61 , 0 ), // #669
  INST(Setnp           , X86Set             , O(000F00,9B,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2525, 238, 62 , 0 ), // #670
  INST(Setns           , X86Set             , O(000F00,99,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2531, 238, 63 , 0 ), // #671
  INST(Setnz           , X86Set             , O(000F00,95,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2537, 238, 58 , 0 ), // #672
  INST(Seto            , X86Set             , O(000F00,90,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2543, 238, 61 , 0 ), // #673
  INST(Setp            , X86Set             , O(000F00,9A,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2548, 238, 62 , 0 ), // #674
  INST(Setpe           , X86Set             , O(000F00,9A,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2553, 238, 62 , 0 ), // #675
  INST(Setpo           , X86Set             , O(000F00,9B,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2559, 238, 62 , 0 ), // #676
  INST(Sets            , X86Set             , O(000F00,98,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2565, 238, 63 , 0 ), // #677
  INST(Setz            , X86Set             , O(000F00,94,_,_,_,_,_,_  ), 0                         , 0 , 1 , 2570, 238, 58 , 0 ), // #678
  INST(Sfence          , X86Fence           , O(000F00,AE,7,_,_,_,_,_  ), 0                         , 0 , 0 , 2575, 34 , 110, 0 ), // #679
  INST(Sgdt            , X86M_Only          , O(000F00,01,0,_,_,_,_,_  ), 0                         , 0 , 0 , 2582, 85 , 45 , 0 ), // #680
  INST(Sha1msg1        , ExtRm              , O(000F38,C9,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2587, 5  , 111, 0 ), // #681
  INST(Sha1msg2        , ExtRm              , O(000F38,CA,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2596, 5  , 111, 0 ), // #682
  INST(Sha1nexte       , ExtRm              , O(000F38,C8,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2605, 5  , 111, 0 ), // #683
  INST(Sha1rnds4       , ExtRmi             , O(000F3A,CC,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2615, 16 , 111, 0 ), // #684
  INST(Sha256msg1      , ExtRm              , O(000F38,CC,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2625, 5  , 111, 0 ), // #685
  INST(Sha256msg2      , ExtRm              , O(000F38,CD,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2636, 5  , 111, 0 ), // #686
  INST(Sha256rnds2     , ExtRm_XMM0         , O(000F38,CB,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2647, 17 , 111, 0 ), // #687
  INST(Shl             , X86Rot             , O(000000,D0,4,_,x,_,_,_  ), 0                         , 0 , 0 , 2659, 225, 1  , 0 ), // #688
  INST(Shld            , X86ShldShrd        , O(000F00,A4,_,_,x,_,_,_  ), 0                         , 0 , 0 , 8175, 239, 1  , 0 ), // #689
  INST(Shlx            , VexRmv_Wx          , V(660F38,F7,_,0,x,_,_,_  ), 0                         , 0 , 0 , 2663, 14 , 83 , 0 ), // #690
  INST(Shr             , X86Rot             , O(000000,D0,5,_,x,_,_,_  ), 0                         , 0 , 0 , 2668, 225, 1  , 0 ), // #691
  INST(Shrd            , X86ShldShrd        , O(000F00,AC,_,_,x,_,_,_  ), 0                         , 0 , 0 , 2672, 239, 1  , 0 ), // #692
  INST(Shrx            , VexRmv_Wx          , V(F20F38,F7,_,0,x,_,_,_  ), 0                         , 0 , 0 , 2677, 14 , 83 , 0 ), // #693
  INST(Shufpd          , ExtRmi             , O(660F00,C6,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9285, 16 , 4  , 80), // #694
  INST(Shufps          , ExtRmi             , O(000F00,C6,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9293, 16 , 5  , 80), // #695
  INST(Sidt            , X86M_Only          , O(000F00,01,1,_,_,_,_,_  ), 0                         , 0 , 0 , 2682, 85 , 45 , 0 ), // #696
  INST(Sldt            , X86M               , O(000F00,00,0,_,_,_,_,_  ), 0                         , 0 , 0 , 2687, 240, 45 , 0 ), // #697
  INST(Smsw            , X86M               , O(000F00,01,4,_,_,_,_,_  ), 0                         , 0 , 0 , 2692, 240, 45 , 0 ), // #698
  INST(Sqrtpd          , ExtRm              , O(660F00,51,_,_,_,_,_,_  ), 0                         , 0 , 16, 9301, 51 , 4  , 81), // #699
  INST(Sqrtps          , ExtRm              , O(000F00,51,_,_,_,_,_,_  ), 0                         , 0 , 16, 9016, 51 , 5  , 81), // #700
  INST(Sqrtsd          , ExtRm              , O(F20F00,51,_,_,_,_,_,_  ), 0                         , 0 , 8 , 9317, 241, 4  , 82), // #701
  INST(Sqrtss          , ExtRm              , O(F30F00,51,_,_,_,_,_,_  ), 0                         , 0 , 4 , 9025, 226, 5  , 82), // #702
  INST(Stac            , X86Op              , O(000F01,CB,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2697, 34 , 17 , 0 ), // #703
  INST(Stc             , X86Op              , O(000000,F9,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2702, 34 , 18 , 0 ), // #704
  INST(Std             , X86Op              , O(000000,FD,_,_,_,_,_,_  ), 0                         , 0 , 0 , 6258, 34 , 19 , 0 ), // #705
  INST(Sti             , X86Op              , O(000000,FB,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2706, 34 , 22 , 0 ), // #706
  INST(Stmxcsr         , X86M_Only          , O(000F00,AE,3,_,_,_,_,_  ), 0                         , 0 , 0 , 9333, 242, 5  , 0 ), // #707
  INST(Stos            , X86StrMr           , O(000000,AA,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2710, 243, 73 , 0 ), // #708
  INST(Str             , X86M               , O(000F00,00,1,_,_,_,_,_  ), 0                         , 0 , 0 , 2715, 240, 45 , 0 ), // #709
  INST(Sub             , X86Arith           , O(000000,28,5,_,x,_,_,_  ), 0                         , 0 , 0 , 807 , 244, 1  , 0 ), // #710
  INST(Subpd           , ExtRm              , O(660F00,5C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 4377, 5  , 4  , 19), // #711
  INST(Subps           , ExtRm              , O(000F00,5C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 4389, 5  , 5  , 19), // #712
  INST(Subsd           , ExtRm              , O(F20F00,5C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5065, 6  , 4  , 19), // #713
  INST(Subss           , ExtRm              , O(F30F00,5C,_,_,_,_,_,_  ), 0                         , 0 , 0 , 5075, 7  , 5  , 19), // #714
  INST(Swapgs          , X86Op              , O(000F01,F8,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2719, 210, 23 , 0 ), // #715
  INST(Syscall         , X86Op              , O(000F00,05,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2726, 210, 45 , 0 ), // #716
  INST(Sysenter        , X86Op              , O(000F00,34,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2734, 34 , 45 , 0 ), // #717
  INST(Sysexit         , X86Op              , O(000F00,35,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2743, 34 , 23 , 0 ), // #718
  INST(Sysexit64       , X86Op              , O(000F00,35,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2751, 34 , 23 , 0 ), // #719
  INST(Sysret          , X86Op              , O(000F00,07,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2761, 210, 23 , 0 ), // #720
  INST(Sysret64        , X86Op              , O(000F00,07,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2768, 210, 23 , 0 ), // #721
  INST(T1mskc          , VexVm_Wx           , V(XOP_M9,01,7,0,x,_,_,_  ), 0                         , 0 , 0 , 2777, 15 , 11 , 0 ), // #722
  INST(Test            , X86Test            , O(000000,84,_,_,x,_,_,_  ), O(000000,F6,_,_,x,_,_,_  ), 0 , 0 , 8489, 245, 1  , 0 ), // #723
  INST(Tzcnt           , X86Rm_Raw66H       , O(F30F00,BC,_,_,x,_,_,_  ), 0                         , 0 , 0 , 2784, 24 , 9  , 0 ), // #724
  INST(Tzmsk           , VexVm_Wx           , V(XOP_M9,01,4,0,x,_,_,_  ), 0                         , 0 , 0 , 2790, 15 , 11 , 0 ), // #725
  INST(Ucomisd         , ExtRm              , O(660F00,2E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9386, 45 , 39 , 11), // #726
  INST(Ucomiss         , ExtRm              , O(000F00,2E,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9395, 46 , 40 , 11), // #727
  INST(Ud2             , X86Op              , O(000F00,0B,_,_,_,_,_,_  ), 0                         , 0 , 0 , 2796, 34 , 0  , 0 ), // #728
  INST(Unpckhpd        , ExtRm              , O(660F00,15,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9404, 5  , 4  , 6 ), // #729
  INST(Unpckhps        , ExtRm              , O(000F00,15,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9414, 5  , 5  , 6 ), // #730
  INST(Unpcklpd        , ExtRm              , O(660F00,14,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9424, 5  , 4  , 6 ), // #731
  INST(Unpcklps        , ExtRm              , O(000F00,14,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9434, 5  , 5  , 6 ), // #732
  INST(V4fmaddps       , VexRm_T1_4X        , V(F20F38,9A,_,2,_,0,2,T4X), 0                         , 0 , 0 , 2800, 246, 112, 0 ), // #733
  INST(V4fnmaddps      , VexRm_T1_4X        , V(F20F38,AA,_,2,_,0,2,T4X), 0                         , 0 , 0 , 2810, 246, 112, 0 ), // #734
  INST(Vaddpd          , VexRvm_Lx          , V(660F00,58,_,x,I,1,4,FV ), 0                         , 0 , 0 , 2821, 247, 113, 1 ), // #735
  INST(Vaddps          , VexRvm_Lx          , V(000F00,58,_,x,I,0,4,FV ), 0                         , 0 , 0 , 2828, 248, 113, 1 ), // #736
  INST(Vaddsd          , VexRvm             , V(F20F00,58,_,I,I,1,3,T1S), 0                         , 0 , 0 , 2835, 249, 114, 1 ), // #737
  INST(Vaddss          , VexRvm             , V(F30F00,58,_,I,I,0,2,T1S), 0                         , 0 , 0 , 2842, 250, 114, 1 ), // #738
  INST(Vaddsubpd       , VexRvm_Lx          , V(660F00,D0,_,x,I,_,_,_  ), 0                         , 0 , 0 , 2849, 251, 115, 1 ), // #739
  INST(Vaddsubps       , VexRvm_Lx          , V(F20F00,D0,_,x,I,_,_,_  ), 0                         , 0 , 0 , 2859, 251, 115, 1 ), // #740
  INST(Vaesdec         , VexRvm             , V(660F38,DE,_,0,I,_,_,_  ), 0                         , 0 , 0 , 2869, 252, 116, 2 ), // #741
  INST(Vaesdeclast     , VexRvm             , V(660F38,DF,_,0,I,_,_,_  ), 0                         , 0 , 0 , 2877, 252, 116, 2 ), // #742
  INST(Vaesenc         , VexRvm             , V(660F38,DC,_,0,I,_,_,_  ), 0                         , 0 , 0 , 2889, 252, 116, 2 ), // #743
  INST(Vaesenclast     , VexRvm             , V(660F38,DD,_,0,I,_,_,_  ), 0                         , 0 , 0 , 2897, 252, 116, 2 ), // #744
  INST(Vaesimc         , VexRm              , V(660F38,DB,_,0,I,_,_,_  ), 0                         , 0 , 0 , 2909, 253, 116, 3 ), // #745
  INST(Vaeskeygenassist, VexRmi             , V(660F3A,DF,_,0,I,_,_,_  ), 0                         , 0 , 0 , 2917, 254, 116, 3 ), // #746
  INST(Valignd         , VexRvmi_Lx         , V(660F3A,03,_,x,_,0,4,FV ), 0                         , 0 , 0 , 2934, 255, 117, 0 ), // #747
  INST(Valignq         , VexRvmi_Lx         , V(660F3A,03,_,x,_,1,4,FV ), 0                         , 0 , 0 , 2942, 256, 117, 0 ), // #748
  INST(Vandnpd         , VexRvm_Lx          , V(660F00,55,_,x,I,1,4,FV ), 0                         , 0 , 0 , 2950, 257, 118, 2 ), // #749
  INST(Vandnps         , VexRvm_Lx          , V(000F00,55,_,x,I,0,4,FV ), 0                         , 0 , 0 , 2958, 258, 118, 2 ), // #750
  INST(Vandpd          , VexRvm_Lx          , V(660F00,54,_,x,I,1,4,FV ), 0                         , 0 , 0 , 2966, 259, 118, 2 ), // #751
  INST(Vandps          , VexRvm_Lx          , V(000F00,54,_,x,I,0,4,FV ), 0                         , 0 , 0 , 2973, 260, 118, 2 ), // #752
  INST(Vblendmb        , VexRvm_Lx          , V(660F38,66,_,x,_,0,4,FVM), 0                         , 0 , 0 , 2980, 261, 119, 0 ), // #753
  INST(Vblendmd        , VexRvm_Lx          , V(660F38,64,_,x,_,0,4,FV ), 0                         , 0 , 0 , 2989, 262, 117, 0 ), // #754
  INST(Vblendmpd       , VexRvm_Lx          , V(660F38,65,_,x,_,1,4,FV ), 0                         , 0 , 0 , 2998, 263, 117, 0 ), // #755
  INST(Vblendmps       , VexRvm_Lx          , V(660F38,65,_,x,_,0,4,FV ), 0                         , 0 , 0 , 3008, 262, 117, 0 ), // #756
  INST(Vblendmq        , VexRvm_Lx          , V(660F38,64,_,x,_,1,4,FV ), 0                         , 0 , 0 , 3018, 263, 117, 0 ), // #757
  INST(Vblendmw        , VexRvm_Lx          , V(660F38,66,_,x,_,1,4,FVM), 0                         , 0 , 0 , 3027, 261, 119, 0 ), // #758
  INST(Vblendpd        , VexRvmi_Lx         , V(660F3A,0D,_,x,I,_,_,_  ), 0                         , 0 , 0 , 3036, 264, 115, 4 ), // #759
  INST(Vblendps        , VexRvmi_Lx         , V(660F3A,0C,_,x,I,_,_,_  ), 0                         , 0 , 0 , 3045, 264, 115, 4 ), // #760
  INST(Vblendvpd       , VexRvmr_Lx         , V(660F3A,4B,_,x,0,_,_,_  ), 0                         , 0 , 0 , 3054, 265, 115, 5 ), // #761
  INST(Vblendvps       , VexRvmr_Lx         , V(660F3A,4A,_,x,0,_,_,_  ), 0                         , 0 , 0 , 3064, 265, 115, 5 ), // #762
  INST(Vbroadcastf128  , VexRm              , V(660F38,1A,_,1,0,_,_,_  ), 0                         , 0 , 0 , 3074, 266, 115, 0 ), // #763
  INST(Vbroadcastf32x2 , VexRm_Lx           , V(660F38,19,_,x,_,0,3,T2 ), 0                         , 0 , 0 , 3089, 267, 120, 0 ), // #764
  INST(Vbroadcastf32x4 , VexRm_Lx           , V(660F38,1A,_,x,_,0,4,T4 ), 0                         , 0 , 0 , 3105, 268, 66 , 0 ), // #765
  INST(Vbroadcastf32x8 , VexRm              , V(660F38,1B,_,2,_,0,5,T8 ), 0                         , 0 , 0 , 3121, 269, 64 , 0 ), // #766
  INST(Vbroadcastf64x2 , VexRm_Lx           , V(660F38,1A,_,x,_,1,4,T2 ), 0                         , 0 , 0 , 3137, 268, 120, 0 ), // #767
  INST(Vbroadcastf64x4 , VexRm              , V(660F38,1B,_,2,_,1,5,T4 ), 0                         , 0 , 0 , 3153, 269, 66 , 0 ), // #768
  INST(Vbroadcasti128  , VexRm              , V(660F38,5A,_,1,0,_,_,_  ), 0                         , 0 , 0 , 3169, 266, 121, 0 ), // #769
  INST(Vbroadcasti32x2 , VexRm_Lx           , V(660F38,59,_,x,_,0,3,T2 ), 0                         , 0 , 0 , 3184, 270, 120, 0 ), // #770
  INST(Vbroadcasti32x4 , VexRm_Lx           , V(660F38,5A,_,x,_,0,4,T4 ), 0                         , 0 , 0 , 3200, 268, 117, 0 ), // #771
  INST(Vbroadcasti32x8 , VexRm              , V(660F38,5B,_,2,_,0,5,T8 ), 0                         , 0 , 0 , 3216, 269, 64 , 0 ), // #772
  INST(Vbroadcasti64x2 , VexRm_Lx           , V(660F38,5A,_,x,_,1,4,T2 ), 0                         , 0 , 0 , 3232, 268, 120, 0 ), // #773
  INST(Vbroadcasti64x4 , VexRm              , V(660F38,5B,_,2,_,1,5,T4 ), 0                         , 0 , 0 , 3248, 269, 66 , 0 ), // #774
  INST(Vbroadcastsd    , VexRm_Lx           , V(660F38,19,_,x,0,1,3,T1S), 0                         , 0 , 0 , 3264, 271, 122, 0 ), // #775
  INST(Vbroadcastss    , VexRm_Lx           , V(660F38,18,_,x,0,0,2,T1S), 0                         , 0 , 0 , 3277, 272, 122, 0 ), // #776
  INST(Vcmppd          , VexRvmi_Lx         , V(660F00,C2,_,x,I,1,4,FV ), 0                         , 0 , 0 , 3290, 273, 113, 6 ), // #777
  INST(Vcmpps          , VexRvmi_Lx         , V(000F00,C2,_,x,I,0,4,FV ), 0                         , 0 , 0 , 3297, 274, 113, 6 ), // #778
  INST(Vcmpsd          , VexRvmi            , V(F20F00,C2,_,I,I,1,3,T1S), 0                         , 0 , 0 , 3304, 275, 114, 7 ), // #779
  INST(Vcmpss          , VexRvmi            , V(F30F00,C2,_,I,I,0,2,T1S), 0                         , 0 , 0 , 3311, 276, 114, 7 ), // #780
  INST(Vcomisd         , VexRm              , V(660F00,2F,_,I,I,1,3,T1S), 0                         , 0 , 0 , 3318, 277, 123, 8 ), // #781
  INST(Vcomiss         , VexRm              , V(000F00,2F,_,I,I,0,2,T1S), 0                         , 0 , 0 , 3326, 278, 123, 8 ), // #782
  INST(Vcompresspd     , VexMr_Lx           , V(660F38,8A,_,x,_,1,3,T1S), 0                         , 0 , 0 , 3334, 279, 117, 0 ), // #783
  INST(Vcompressps     , VexMr_Lx           , V(660F38,8A,_,x,_,0,2,T1S), 0                         , 0 , 0 , 3346, 279, 117, 0 ), // #784
  INST(Vcvtdq2pd       , VexRm_Lx           , V(F30F00,E6,_,x,I,0,3,HV ), 0                         , 0 , 0 , 3358, 280, 113, 9 ), // #785
  INST(Vcvtdq2ps       , VexRm_Lx           , V(000F00,5B,_,x,I,0,4,FV ), 0                         , 0 , 0 , 3368, 281, 113, 9 ), // #786
  INST(Vcvtpd2dq       , VexRm_Lx           , V(F20F00,E6,_,x,I,1,4,FV ), 0                         , 0 , 0 , 3378, 282, 113, 9 ), // #787
  INST(Vcvtpd2ps       , VexRm_Lx           , V(660F00,5A,_,x,I,1,4,FV ), 0                         , 0 , 0 , 3388, 282, 113, 10), // #788
  INST(Vcvtpd2qq       , VexRm_Lx           , V(660F00,7B,_,x,_,1,4,FV ), 0                         , 0 , 0 , 3398, 283, 120, 0 ), // #789
  INST(Vcvtpd2udq      , VexRm_Lx           , V(000F00,79,_,x,_,1,4,FV ), 0                         , 0 , 0 , 3408, 284, 117, 0 ), // #790
  INST(Vcvtpd2uqq      , VexRm_Lx           , V(660F00,79,_,x,_,1,4,FV ), 0                         , 0 , 0 , 3419, 283, 120, 0 ), // #791
  INST(Vcvtph2ps       , VexRm_Lx           , V(660F38,13,_,x,0,0,3,HVM), 0                         , 0 , 0 , 3430, 285, 124, 0 ), // #792
  INST(Vcvtps2dq       , VexRm_Lx           , V(660F00,5B,_,x,I,0,4,FV ), 0                         , 0 , 0 , 3440, 281, 113, 8 ), // #793
  INST(Vcvtps2pd       , VexRm_Lx           , V(000F00,5A,_,x,I,0,4,HV ), 0                         , 0 , 0 , 3450, 286, 113, 8 ), // #794
  INST(Vcvtps2ph       , VexMri_Lx          , V(660F3A,1D,_,x,0,0,3,HVM), 0                         , 0 , 0 , 3460, 287, 124, 0 ), // #795
  INST(Vcvtps2qq       , VexRm_Lx           , V(660F00,7B,_,x,_,0,3,HV ), 0                         , 0 , 0 , 3470, 288, 120, 0 ), // #796
  INST(Vcvtps2udq      , VexRm_Lx           , V(000F00,79,_,x,_,0,4,FV ), 0                         , 0 , 0 , 3480, 289, 117, 0 ), // #797
  INST(Vcvtps2uqq      , VexRm_Lx           , V(660F00,79,_,x,_,0,3,HV ), 0                         , 0 , 0 , 3491, 288, 120, 0 ), // #798
  INST(Vcvtqq2pd       , VexRm_Lx           , V(F30F00,E6,_,x,_,1,4,FV ), 0                         , 0 , 0 , 3502, 283, 120, 0 ), // #799
  INST(Vcvtqq2ps       , VexRm_Lx           , V(000F00,5B,_,x,_,1,4,FV ), 0                         , 0 , 0 , 3512, 284, 120, 0 ), // #800
  INST(Vcvtsd2si       , VexRm              , V(F20F00,2D,_,I,x,x,3,T1F), 0                         , 0 , 0 , 3522, 290, 114, 11), // #801
  INST(Vcvtsd2ss       , VexRvm             , V(F20F00,5A,_,I,I,1,3,T1S), 0                         , 0 , 0 , 3532, 249, 114, 12), // #802
  INST(Vcvtsd2usi      , VexRm              , V(F20F00,79,_,I,_,x,3,T1F), 0                         , 0 , 0 , 3542, 291, 66 , 0 ), // #803
  INST(Vcvtsi2sd       , VexRvm             , V(F20F00,2A,_,I,x,x,2,T1W), 0                         , 0 , 0 , 3553, 292, 114, 13), // #804
  INST(Vcvtsi2ss       , VexRvm             , V(F30F00,2A,_,I,x,x,2,T1W), 0                         , 0 , 0 , 3563, 292, 114, 13), // #805
  INST(Vcvtss2sd       , VexRvm             , V(F30F00,5A,_,I,I,0,2,T1S), 0                         , 0 , 0 , 3573, 293, 114, 13), // #806
  INST(Vcvtss2si       , VexRm              , V(F20F00,2D,_,I,x,x,2,T1F), 0                         , 0 , 0 , 3583, 294, 114, 14), // #807
  INST(Vcvtss2usi      , VexRm              , V(F30F00,79,_,I,_,x,2,T1F), 0                         , 0 , 0 , 3593, 295, 66 , 0 ), // #808
  INST(Vcvttpd2dq      , VexRm_Lx           , V(660F00,E6,_,x,I,1,4,FV ), 0                         , 0 , 0 , 3604, 296, 113, 15), // #809
  INST(Vcvttpd2qq      , VexRm_Lx           , V(660F00,7A,_,x,_,1,4,FV ), 0                         , 0 , 0 , 3615, 297, 117, 0 ), // #810
  INST(Vcvttpd2udq     , VexRm_Lx           , V(000F00,78,_,x,_,1,4,FV ), 0                         , 0 , 0 , 3626, 298, 117, 0 ), // #811
  INST(Vcvttpd2uqq     , VexRm_Lx           , V(660F00,78,_,x,_,1,4,FV ), 0                         , 0 , 0 , 3638, 297, 120, 0 ), // #812
  INST(Vcvttps2dq      , VexRm_Lx           , V(F30F00,5B,_,x,I,0,4,FV ), 0                         , 0 , 0 , 3650, 299, 113, 16), // #813
  INST(Vcvttps2qq      , VexRm_Lx           , V(660F00,7A,_,x,_,0,3,HV ), 0                         , 0 , 0 , 3661, 300, 120, 0 ), // #814
  INST(Vcvttps2udq     , VexRm_Lx           , V(000F00,78,_,x,_,0,4,FV ), 0                         , 0 , 0 , 3672, 301, 117, 0 ), // #815
  INST(Vcvttps2uqq     , VexRm_Lx           , V(660F00,78,_,x,_,0,3,HV ), 0                         , 0 , 0 , 3684, 300, 120, 0 ), // #816
  INST(Vcvttsd2si      , VexRm              , V(F20F00,2C,_,I,x,x,3,T1F), 0                         , 0 , 0 , 3696, 302, 114, 17), // #817
  INST(Vcvttsd2usi     , VexRm              , V(F20F00,78,_,I,_,x,3,T1F), 0                         , 0 , 0 , 3707, 303, 66 , 0 ), // #818
  INST(Vcvttss2si      , VexRm              , V(F30F00,2C,_,I,x,x,2,T1F), 0                         , 0 , 0 , 3719, 304, 114, 18), // #819
  INST(Vcvttss2usi     , VexRm              , V(F30F00,78,_,I,_,x,2,T1F), 0                         , 0 , 0 , 3730, 305, 66 , 0 ), // #820
  INST(Vcvtudq2pd      , VexRm_Lx           , V(F30F00,7A,_,x,_,0,3,HV ), 0                         , 0 , 0 , 3742, 306, 117, 0 ), // #821
  INST(Vcvtudq2ps      , VexRm_Lx           , V(F20F00,7A,_,x,_,0,4,FV ), 0                         , 0 , 0 , 3753, 289, 117, 0 ), // #822
  INST(Vcvtuqq2pd      , VexRm_Lx           , V(F30F00,7A,_,x,_,1,4,FV ), 0                         , 0 , 0 , 3764, 283, 120, 0 ), // #823
  INST(Vcvtuqq2ps      , VexRm_Lx           , V(F20F00,7A,_,x,_,1,4,FV ), 0                         , 0 , 0 , 3775, 284, 120, 0 ), // #824
  INST(Vcvtusi2sd      , VexRvm             , V(F20F00,7B,_,I,_,x,2,T1W), 0                         , 0 , 0 , 3786, 307, 66 , 0 ), // #825
  INST(Vcvtusi2ss      , VexRvm             , V(F30F00,7B,_,I,_,x,2,T1W), 0                         , 0 , 0 , 3797, 307, 66 , 0 ), // #826
  INST(Vdbpsadbw       , VexRvmi_Lx         , V(660F3A,42,_,x,_,0,4,FVM), 0                         , 0 , 0 , 3808, 308, 119, 0 ), // #827
  INST(Vdivpd          , VexRvm_Lx          , V(660F00,5E,_,x,I,1,4,FV ), 0                         , 0 , 0 , 3818, 247, 113, 19), // #828
  INST(Vdivps          , VexRvm_Lx          , V(000F00,5E,_,x,I,0,4,FV ), 0                         , 0 , 0 , 3825, 248, 113, 19), // #829
  INST(Vdivsd          , VexRvm             , V(F20F00,5E,_,I,I,1,3,T1S), 0                         , 0 , 0 , 3832, 249, 114, 19), // #830
  INST(Vdivss          , VexRvm             , V(F30F00,5E,_,I,I,0,2,T1S), 0                         , 0 , 0 , 3839, 250, 114, 19), // #831
  INST(Vdppd           , VexRvmi_Lx         , V(660F3A,41,_,x,I,_,_,_  ), 0                         , 0 , 0 , 3846, 309, 115, 19), // #832
  INST(Vdpps           , VexRvmi_Lx         , V(660F3A,40,_,x,I,_,_,_  ), 0                         , 0 , 0 , 3852, 264, 115, 19), // #833
  INST(Verr            , X86M               , O(000F00,00,4,_,_,_,_,_  ), 0                         , 0 , 0 , 3858, 137, 71 , 0 ), // #834
  INST(Verw            , X86M               , O(000F00,00,5,_,_,_,_,_  ), 0                         , 0 , 0 , 3863, 137, 71 , 0 ), // #835
  INST(Vexp2pd         , VexRm              , V(660F38,C8,_,2,_,1,4,FV ), 0                         , 0 , 0 , 3868, 310, 125, 0 ), // #836
  INST(Vexp2ps         , VexRm              , V(660F38,C8,_,2,_,0,4,FV ), 0                         , 0 , 0 , 3876, 311, 125, 0 ), // #837
  INST(Vexpandpd       , VexRm_Lx           , V(660F38,88,_,x,_,1,3,T1S), 0                         , 0 , 0 , 3884, 312, 117, 0 ), // #838
  INST(Vexpandps       , VexRm_Lx           , V(660F38,88,_,x,_,0,2,T1S), 0                         , 0 , 0 , 3894, 312, 117, 0 ), // #839
  INST(Vextractf128    , VexMri             , V(660F3A,19,_,1,0,_,_,_  ), 0                         , 0 , 0 , 3904, 313, 115, 0 ), // #840
  INST(Vextractf32x4   , VexMri_Lx          , V(660F3A,19,_,x,_,0,4,T4 ), 0                         , 0 , 0 , 3917, 314, 117, 0 ), // #841
  INST(Vextractf32x8   , VexMri             , V(660F3A,1B,_,2,_,0,5,T8 ), 0                         , 0 , 0 , 3931, 315, 64 , 0 ), // #842
  INST(Vextractf64x2   , VexMri_Lx          , V(660F3A,19,_,x,_,1,4,T2 ), 0                         , 0 , 0 , 3945, 314, 120, 0 ), // #843
  INST(Vextractf64x4   , VexMri             , V(660F3A,1B,_,2,_,1,5,T4 ), 0                         , 0 , 0 , 3959, 315, 66 , 0 ), // #844
  INST(Vextracti128    , VexMri             , V(660F3A,39,_,1,0,_,_,_  ), 0                         , 0 , 0 , 3973, 313, 121, 0 ), // #845
  INST(Vextracti32x4   , VexMri_Lx          , V(660F3A,39,_,x,_,0,4,T4 ), 0                         , 0 , 0 , 3986, 314, 117, 0 ), // #846
  INST(Vextracti32x8   , VexMri             , V(660F3A,3B,_,2,_,0,5,T8 ), 0                         , 0 , 0 , 4000, 315, 64 , 0 ), // #847
  INST(Vextracti64x2   , VexMri_Lx          , V(660F3A,39,_,x,_,1,4,T2 ), 0                         , 0 , 0 , 4014, 314, 120, 0 ), // #848
  INST(Vextracti64x4   , VexMri             , V(660F3A,3B,_,2,_,1,5,T4 ), 0                         , 0 , 0 , 4028, 315, 66 , 0 ), // #849
  INST(Vextractps      , VexMri             , V(660F3A,17,_,0,I,I,2,T1S), 0                         , 0 , 0 , 4042, 316, 114, 20), // #850
  INST(Vfixupimmpd     , VexRvmi_Lx         , V(660F3A,54,_,x,_,1,4,FV ), 0                         , 0 , 0 , 4053, 317, 117, 0 ), // #851
  INST(Vfixupimmps     , VexRvmi_Lx         , V(660F3A,54,_,x,_,0,4,FV ), 0                         , 0 , 0 , 4065, 318, 117, 0 ), // #852
  INST(Vfixupimmsd     , VexRvmi            , V(660F3A,55,_,I,_,1,3,T1S), 0                         , 0 , 0 , 4077, 319, 66 , 0 ), // #853
  INST(Vfixupimmss     , VexRvmi            , V(660F3A,55,_,I,_,0,2,T1S), 0                         , 0 , 0 , 4089, 320, 66 , 0 ), // #854
  INST(Vfmadd132pd     , VexRvm_Lx          , V(660F38,98,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4101, 321, 126, 0 ), // #855
  INST(Vfmadd132ps     , VexRvm_Lx          , V(660F38,98,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4113, 322, 126, 0 ), // #856
  INST(Vfmadd132sd     , VexRvm             , V(660F38,99,_,I,1,1,3,T1S), 0                         , 0 , 0 , 4125, 323, 127, 0 ), // #857
  INST(Vfmadd132ss     , VexRvm             , V(660F38,99,_,I,0,0,2,T1S), 0                         , 0 , 0 , 4137, 324, 127, 0 ), // #858
  INST(Vfmadd213pd     , VexRvm_Lx          , V(660F38,A8,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4149, 321, 126, 0 ), // #859
  INST(Vfmadd213ps     , VexRvm_Lx          , V(660F38,A8,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4161, 322, 126, 0 ), // #860
  INST(Vfmadd213sd     , VexRvm             , V(660F38,A9,_,I,1,1,3,T1S), 0                         , 0 , 0 , 4173, 323, 127, 0 ), // #861
  INST(Vfmadd213ss     , VexRvm             , V(660F38,A9,_,I,0,0,2,T1S), 0                         , 0 , 0 , 4185, 324, 127, 0 ), // #862
  INST(Vfmadd231pd     , VexRvm_Lx          , V(660F38,B8,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4197, 321, 126, 0 ), // #863
  INST(Vfmadd231ps     , VexRvm_Lx          , V(660F38,B8,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4209, 322, 126, 0 ), // #864
  INST(Vfmadd231sd     , VexRvm             , V(660F38,B9,_,I,1,1,3,T1S), 0                         , 0 , 0 , 4221, 323, 127, 0 ), // #865
  INST(Vfmadd231ss     , VexRvm             , V(660F38,B9,_,I,0,0,2,T1S), 0                         , 0 , 0 , 4233, 324, 127, 0 ), // #866
  INST(Vfmaddpd        , Fma4_Lx            , V(660F3A,69,_,x,x,_,_,_  ), 0                         , 0 , 0 , 4245, 325, 128, 0 ), // #867
  INST(Vfmaddps        , Fma4_Lx            , V(660F3A,68,_,x,x,_,_,_  ), 0                         , 0 , 0 , 4254, 325, 128, 0 ), // #868
  INST(Vfmaddsd        , Fma4               , V(660F3A,6B,_,0,x,_,_,_  ), 0                         , 0 , 0 , 4263, 326, 128, 0 ), // #869
  INST(Vfmaddss        , Fma4               , V(660F3A,6A,_,0,x,_,_,_  ), 0                         , 0 , 0 , 4272, 327, 128, 0 ), // #870
  INST(Vfmaddsub132pd  , VexRvm_Lx          , V(660F38,96,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4281, 321, 126, 0 ), // #871
  INST(Vfmaddsub132ps  , VexRvm_Lx          , V(660F38,96,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4296, 322, 126, 0 ), // #872
  INST(Vfmaddsub213pd  , VexRvm_Lx          , V(660F38,A6,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4311, 321, 126, 0 ), // #873
  INST(Vfmaddsub213ps  , VexRvm_Lx          , V(660F38,A6,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4326, 322, 126, 0 ), // #874
  INST(Vfmaddsub231pd  , VexRvm_Lx          , V(660F38,B6,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4341, 321, 126, 0 ), // #875
  INST(Vfmaddsub231ps  , VexRvm_Lx          , V(660F38,B6,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4356, 322, 126, 0 ), // #876
  INST(Vfmaddsubpd     , Fma4_Lx            , V(660F3A,5D,_,x,x,_,_,_  ), 0                         , 0 , 0 , 4371, 325, 128, 0 ), // #877
  INST(Vfmaddsubps     , Fma4_Lx            , V(660F3A,5C,_,x,x,_,_,_  ), 0                         , 0 , 0 , 4383, 325, 128, 0 ), // #878
  INST(Vfmsub132pd     , VexRvm_Lx          , V(660F38,9A,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4395, 321, 126, 0 ), // #879
  INST(Vfmsub132ps     , VexRvm_Lx          , V(660F38,9A,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4407, 322, 126, 0 ), // #880
  INST(Vfmsub132sd     , VexRvm             , V(660F38,9B,_,I,1,1,3,T1S), 0                         , 0 , 0 , 4419, 323, 127, 0 ), // #881
  INST(Vfmsub132ss     , VexRvm             , V(660F38,9B,_,I,0,0,2,T1S), 0                         , 0 , 0 , 4431, 324, 127, 0 ), // #882
  INST(Vfmsub213pd     , VexRvm_Lx          , V(660F38,AA,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4443, 321, 126, 0 ), // #883
  INST(Vfmsub213ps     , VexRvm_Lx          , V(660F38,AA,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4455, 322, 126, 0 ), // #884
  INST(Vfmsub213sd     , VexRvm             , V(660F38,AB,_,I,1,1,3,T1S), 0                         , 0 , 0 , 4467, 323, 127, 0 ), // #885
  INST(Vfmsub213ss     , VexRvm             , V(660F38,AB,_,I,0,0,2,T1S), 0                         , 0 , 0 , 4479, 324, 127, 0 ), // #886
  INST(Vfmsub231pd     , VexRvm_Lx          , V(660F38,BA,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4491, 321, 126, 0 ), // #887
  INST(Vfmsub231ps     , VexRvm_Lx          , V(660F38,BA,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4503, 322, 126, 0 ), // #888
  INST(Vfmsub231sd     , VexRvm             , V(660F38,BB,_,I,1,1,3,T1S), 0                         , 0 , 0 , 4515, 323, 127, 0 ), // #889
  INST(Vfmsub231ss     , VexRvm             , V(660F38,BB,_,I,0,0,2,T1S), 0                         , 0 , 0 , 4527, 324, 127, 0 ), // #890
  INST(Vfmsubadd132pd  , VexRvm_Lx          , V(660F38,97,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4539, 321, 126, 0 ), // #891
  INST(Vfmsubadd132ps  , VexRvm_Lx          , V(660F38,97,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4554, 322, 126, 0 ), // #892
  INST(Vfmsubadd213pd  , VexRvm_Lx          , V(660F38,A7,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4569, 321, 126, 0 ), // #893
  INST(Vfmsubadd213ps  , VexRvm_Lx          , V(660F38,A7,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4584, 322, 126, 0 ), // #894
  INST(Vfmsubadd231pd  , VexRvm_Lx          , V(660F38,B7,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4599, 321, 126, 0 ), // #895
  INST(Vfmsubadd231ps  , VexRvm_Lx          , V(660F38,B7,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4614, 322, 126, 0 ), // #896
  INST(Vfmsubaddpd     , Fma4_Lx            , V(660F3A,5F,_,x,x,_,_,_  ), 0                         , 0 , 0 , 4629, 325, 128, 0 ), // #897
  INST(Vfmsubaddps     , Fma4_Lx            , V(660F3A,5E,_,x,x,_,_,_  ), 0                         , 0 , 0 , 4641, 325, 128, 0 ), // #898
  INST(Vfmsubpd        , Fma4_Lx            , V(660F3A,6D,_,x,x,_,_,_  ), 0                         , 0 , 0 , 4653, 325, 128, 0 ), // #899
  INST(Vfmsubps        , Fma4_Lx            , V(660F3A,6C,_,x,x,_,_,_  ), 0                         , 0 , 0 , 4662, 325, 128, 0 ), // #900
  INST(Vfmsubsd        , Fma4               , V(660F3A,6F,_,0,x,_,_,_  ), 0                         , 0 , 0 , 4671, 326, 128, 0 ), // #901
  INST(Vfmsubss        , Fma4               , V(660F3A,6E,_,0,x,_,_,_  ), 0                         , 0 , 0 , 4680, 327, 128, 0 ), // #902
  INST(Vfnmadd132pd    , VexRvm_Lx          , V(660F38,9C,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4689, 321, 126, 0 ), // #903
  INST(Vfnmadd132ps    , VexRvm_Lx          , V(660F38,9C,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4702, 322, 126, 0 ), // #904
  INST(Vfnmadd132sd    , VexRvm             , V(660F38,9D,_,I,1,1,3,T1S), 0                         , 0 , 0 , 4715, 323, 127, 0 ), // #905
  INST(Vfnmadd132ss    , VexRvm             , V(660F38,9D,_,I,0,0,2,T1S), 0                         , 0 , 0 , 4728, 324, 127, 0 ), // #906
  INST(Vfnmadd213pd    , VexRvm_Lx          , V(660F38,AC,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4741, 321, 126, 0 ), // #907
  INST(Vfnmadd213ps    , VexRvm_Lx          , V(660F38,AC,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4754, 322, 126, 0 ), // #908
  INST(Vfnmadd213sd    , VexRvm             , V(660F38,AD,_,I,1,1,3,T1S), 0                         , 0 , 0 , 4767, 323, 127, 0 ), // #909
  INST(Vfnmadd213ss    , VexRvm             , V(660F38,AD,_,I,0,0,2,T1S), 0                         , 0 , 0 , 4780, 324, 127, 0 ), // #910
  INST(Vfnmadd231pd    , VexRvm_Lx          , V(660F38,BC,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4793, 321, 126, 0 ), // #911
  INST(Vfnmadd231ps    , VexRvm_Lx          , V(660F38,BC,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4806, 322, 126, 0 ), // #912
  INST(Vfnmadd231sd    , VexRvm             , V(660F38,BC,_,I,1,1,3,T1S), 0                         , 0 , 0 , 4819, 323, 127, 0 ), // #913
  INST(Vfnmadd231ss    , VexRvm             , V(660F38,BC,_,I,0,0,2,T1S), 0                         , 0 , 0 , 4832, 324, 127, 0 ), // #914
  INST(Vfnmaddpd       , Fma4_Lx            , V(660F3A,79,_,x,x,_,_,_  ), 0                         , 0 , 0 , 4845, 325, 128, 0 ), // #915
  INST(Vfnmaddps       , Fma4_Lx            , V(660F3A,78,_,x,x,_,_,_  ), 0                         , 0 , 0 , 4855, 325, 128, 0 ), // #916
  INST(Vfnmaddsd       , Fma4               , V(660F3A,7B,_,0,x,_,_,_  ), 0                         , 0 , 0 , 4865, 326, 128, 0 ), // #917
  INST(Vfnmaddss       , Fma4               , V(660F3A,7A,_,0,x,_,_,_  ), 0                         , 0 , 0 , 4875, 327, 128, 0 ), // #918
  INST(Vfnmsub132pd    , VexRvm_Lx          , V(660F38,9E,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4885, 321, 126, 0 ), // #919
  INST(Vfnmsub132ps    , VexRvm_Lx          , V(660F38,9E,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4898, 322, 126, 0 ), // #920
  INST(Vfnmsub132sd    , VexRvm             , V(660F38,9F,_,I,1,1,3,T1S), 0                         , 0 , 0 , 4911, 323, 127, 0 ), // #921
  INST(Vfnmsub132ss    , VexRvm             , V(660F38,9F,_,I,0,0,2,T1S), 0                         , 0 , 0 , 4924, 324, 127, 0 ), // #922
  INST(Vfnmsub213pd    , VexRvm_Lx          , V(660F38,AE,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4937, 321, 126, 0 ), // #923
  INST(Vfnmsub213ps    , VexRvm_Lx          , V(660F38,AE,_,x,0,0,4,FV ), 0                         , 0 , 0 , 4950, 322, 126, 0 ), // #924
  INST(Vfnmsub213sd    , VexRvm             , V(660F38,AF,_,I,1,1,3,T1S), 0                         , 0 , 0 , 4963, 323, 127, 0 ), // #925
  INST(Vfnmsub213ss    , VexRvm             , V(660F38,AF,_,I,0,0,2,T1S), 0                         , 0 , 0 , 4976, 324, 127, 0 ), // #926
  INST(Vfnmsub231pd    , VexRvm_Lx          , V(660F38,BE,_,x,1,1,4,FV ), 0                         , 0 , 0 , 4989, 321, 126, 0 ), // #927
  INST(Vfnmsub231ps    , VexRvm_Lx          , V(660F38,BE,_,x,0,0,4,FV ), 0                         , 0 , 0 , 5002, 322, 126, 0 ), // #928
  INST(Vfnmsub231sd    , VexRvm             , V(660F38,BF,_,I,1,1,3,T1S), 0                         , 0 , 0 , 5015, 323, 127, 0 ), // #929
  INST(Vfnmsub231ss    , VexRvm             , V(660F38,BF,_,I,0,0,2,T1S), 0                         , 0 , 0 , 5028, 324, 127, 0 ), // #930
  INST(Vfnmsubpd       , Fma4_Lx            , V(660F3A,7D,_,x,x,_,_,_  ), 0                         , 0 , 0 , 5041, 325, 128, 0 ), // #931
  INST(Vfnmsubps       , Fma4_Lx            , V(660F3A,7C,_,x,x,_,_,_  ), 0                         , 0 , 0 , 5051, 325, 128, 0 ), // #932
  INST(Vfnmsubsd       , Fma4               , V(660F3A,7F,_,0,x,_,_,_  ), 0                         , 0 , 0 , 5061, 326, 128, 0 ), // #933
  INST(Vfnmsubss       , Fma4               , V(660F3A,7E,_,0,x,_,_,_  ), 0                         , 0 , 0 , 5071, 327, 128, 0 ), // #934
  INST(Vfpclasspd      , VexRmi_Lx          , V(660F3A,66,_,x,_,1,4,FV ), 0                         , 0 , 0 , 5081, 328, 120, 0 ), // #935
  INST(Vfpclassps      , VexRmi_Lx          , V(660F3A,66,_,x,_,0,4,FV ), 0                         , 0 , 0 , 5092, 329, 120, 0 ), // #936
  INST(Vfpclasssd      , VexRmi_Lx          , V(660F3A,67,_,I,_,1,3,T1S), 0                         , 0 , 0 , 5103, 330, 64 , 0 ), // #937
  INST(Vfpclassss      , VexRmi_Lx          , V(660F3A,67,_,I,_,0,2,T1S), 0                         , 0 , 0 , 5114, 331, 64 , 0 ), // #938
  INST(Vfrczpd         , VexRm_Lx           , V(XOP_M9,81,_,x,0,_,_,_  ), 0                         , 0 , 0 , 5125, 332, 129, 0 ), // #939
  INST(Vfrczps         , VexRm_Lx           , V(XOP_M9,80,_,x,0,_,_,_  ), 0                         , 0 , 0 , 5133, 332, 129, 0 ), // #940
  INST(Vfrczsd         , VexRm              , V(XOP_M9,83,_,0,0,_,_,_  ), 0                         , 0 , 0 , 5141, 333, 129, 0 ), // #941
  INST(Vfrczss         , VexRm              , V(XOP_M9,82,_,0,0,_,_,_  ), 0                         , 0 , 0 , 5149, 334, 129, 0 ), // #942
  INST(Vgatherdpd      , VexRmvRm_VM        , V(660F38,92,_,x,1,_,_,_  ), V(660F38,92,_,x,_,1,3,T1S), 0 , 0 , 5157, 335, 130, 0 ), // #943
  INST(Vgatherdps      , VexRmvRm_VM        , V(660F38,92,_,x,0,_,_,_  ), V(660F38,92,_,x,_,0,2,T1S), 0 , 0 , 5168, 336, 130, 0 ), // #944
  INST(Vgatherpf0dpd   , VexM_VM            , V(660F38,C6,1,2,_,1,3,T1S), 0                         , 0 , 0 , 5179, 337, 131, 0 ), // #945
  INST(Vgatherpf0dps   , VexM_VM            , V(660F38,C6,1,2,_,0,2,T1S), 0                         , 0 , 0 , 5193, 338, 131, 0 ), // #946
  INST(Vgatherpf0qpd   , VexM_VM            , V(660F38,C7,1,2,_,1,3,T1S), 0                         , 0 , 0 , 5207, 339, 131, 0 ), // #947
  INST(Vgatherpf0qps   , VexM_VM            , V(660F38,C7,1,2,_,0,2,T1S), 0                         , 0 , 0 , 5221, 339, 131, 0 ), // #948
  INST(Vgatherpf1dpd   , VexM_VM            , V(660F38,C6,2,2,_,1,3,T1S), 0                         , 0 , 0 , 5235, 337, 131, 0 ), // #949
  INST(Vgatherpf1dps   , VexM_VM            , V(660F38,C6,2,2,_,0,2,T1S), 0                         , 0 , 0 , 5249, 338, 131, 0 ), // #950
  INST(Vgatherpf1qpd   , VexM_VM            , V(660F38,C7,2,2,_,1,3,T1S), 0                         , 0 , 0 , 5263, 339, 131, 0 ), // #951
  INST(Vgatherpf1qps   , VexM_VM            , V(660F38,C7,2,2,_,0,2,T1S), 0                         , 0 , 0 , 5277, 339, 131, 0 ), // #952
  INST(Vgatherqpd      , VexRmvRm_VM        , V(660F38,93,_,x,1,_,_,_  ), V(660F38,93,_,x,_,1,3,T1S), 0 , 0 , 5291, 340, 130, 0 ), // #953
  INST(Vgatherqps      , VexRmvRm_VM        , V(660F38,93,_,x,0,_,_,_  ), V(660F38,93,_,x,_,0,2,T1S), 0 , 0 , 5302, 341, 130, 0 ), // #954
  INST(Vgetexppd       , VexRm_Lx           , V(660F38,42,_,x,_,1,4,FV ), 0                         , 0 , 0 , 5313, 297, 117, 0 ), // #955
  INST(Vgetexpps       , VexRm_Lx           , V(660F38,42,_,x,_,0,4,FV ), 0                         , 0 , 0 , 5323, 301, 117, 0 ), // #956
  INST(Vgetexpsd       , VexRvm             , V(660F38,43,_,I,_,1,3,T1S), 0                         , 0 , 0 , 5333, 342, 66 , 0 ), // #957
  INST(Vgetexpss       , VexRvm             , V(660F38,43,_,I,_,0,2,T1S), 0                         , 0 , 0 , 5343, 343, 66 , 0 ), // #958
  INST(Vgetmantpd      , VexRmi_Lx          , V(660F3A,26,_,x,_,1,4,FV ), 0                         , 0 , 0 , 5353, 344, 117, 0 ), // #959
  INST(Vgetmantps      , VexRmi_Lx          , V(660F3A,26,_,x,_,0,4,FV ), 0                         , 0 , 0 , 5364, 345, 117, 0 ), // #960
  INST(Vgetmantsd      , VexRvmi            , V(660F3A,27,_,I,_,1,3,T1S), 0                         , 0 , 0 , 5375, 346, 66 , 0 ), // #961
  INST(Vgetmantss      , VexRvmi            , V(660F3A,27,_,I,_,0,2,T1S), 0                         , 0 , 0 , 5386, 347, 66 , 0 ), // #962
  INST(Vhaddpd         , VexRvm_Lx          , V(660F00,7C,_,x,I,_,_,_  ), 0                         , 0 , 0 , 5397, 251, 115, 21), // #963
  INST(Vhaddps         , VexRvm_Lx          , V(F20F00,7C,_,x,I,_,_,_  ), 0                         , 0 , 0 , 5405, 251, 115, 21), // #964
  INST(Vhsubpd         , VexRvm_Lx          , V(660F00,7D,_,x,I,_,_,_  ), 0                         , 0 , 0 , 5413, 251, 115, 22), // #965
  INST(Vhsubps         , VexRvm_Lx          , V(F20F00,7D,_,x,I,_,_,_  ), 0                         , 0 , 0 , 5421, 251, 115, 22), // #966
  INST(Vinsertf128     , VexRvmi            , V(660F3A,18,_,1,0,_,_,_  ), 0                         , 0 , 0 , 5429, 348, 115, 0 ), // #967
  INST(Vinsertf32x4    , VexRvmi_Lx         , V(660F3A,18,_,x,_,0,4,T4 ), 0                         , 0 , 0 , 5441, 349, 117, 0 ), // #968
  INST(Vinsertf32x8    , VexRvmi            , V(660F3A,1A,_,2,_,0,5,T8 ), 0                         , 0 , 0 , 5454, 350, 64 , 0 ), // #969
  INST(Vinsertf64x2    , VexRvmi_Lx         , V(660F3A,18,_,x,_,1,4,T2 ), 0                         , 0 , 0 , 5467, 349, 120, 0 ), // #970
  INST(Vinsertf64x4    , VexRvmi            , V(660F3A,1A,_,2,_,1,5,T4 ), 0                         , 0 , 0 , 5480, 350, 66 , 0 ), // #971
  INST(Vinserti128     , VexRvmi            , V(660F3A,38,_,1,0,_,_,_  ), 0                         , 0 , 0 , 5493, 348, 121, 0 ), // #972
  INST(Vinserti32x4    , VexRvmi_Lx         , V(660F3A,38,_,x,_,0,4,T4 ), 0                         , 0 , 0 , 5505, 349, 117, 0 ), // #973
  INST(Vinserti32x8    , VexRvmi            , V(660F3A,3A,_,2,_,0,5,T8 ), 0                         , 0 , 0 , 5518, 350, 64 , 0 ), // #974
  INST(Vinserti64x2    , VexRvmi_Lx         , V(660F3A,38,_,x,_,1,4,T2 ), 0                         , 0 , 0 , 5531, 349, 120, 0 ), // #975
  INST(Vinserti64x4    , VexRvmi            , V(660F3A,3A,_,2,_,1,5,T4 ), 0                         , 0 , 0 , 5544, 350, 66 , 0 ), // #976
  INST(Vinsertps       , VexRvmi            , V(660F3A,21,_,0,I,0,2,T1S), 0                         , 0 , 0 , 5557, 351, 114, 23), // #977
  INST(Vlddqu          , VexRm_Lx           , V(F20F00,F0,_,x,I,_,_,_  ), 0                         , 0 , 0 , 5567, 352, 115, 24), // #978
  INST(Vldmxcsr        , VexM               , V(000F00,AE,2,0,I,_,_,_  ), 0                         , 0 , 0 , 5574, 353, 115, 0 ), // #979
  INST(Vmaskmovdqu     , VexRm_ZDI          , V(660F00,F7,_,0,I,_,_,_  ), 0                         , 0 , 0 , 5583, 354, 115, 25), // #980
  INST(Vmaskmovpd      , VexRvmMvr_Lx       , V(660F38,2D,_,x,0,_,_,_  ), V(660F38,2F,_,x,0,_,_,_  ), 0 , 0 , 5595, 355, 115, 0 ), // #981
  INST(Vmaskmovps      , VexRvmMvr_Lx       , V(660F38,2C,_,x,0,_,_,_  ), V(660F38,2E,_,x,0,_,_,_  ), 0 , 0 , 5606, 356, 115, 0 ), // #982
  INST(Vmaxpd          , VexRvm_Lx          , V(660F00,5F,_,x,I,1,4,FV ), 0                         , 0 , 0 , 5617, 357, 113, 26), // #983
  INST(Vmaxps          , VexRvm_Lx          , V(000F00,5F,_,x,I,0,4,FV ), 0                         , 0 , 0 , 5624, 358, 113, 26), // #984
  INST(Vmaxsd          , VexRvm             , V(F20F00,5F,_,I,I,1,3,T1S), 0                         , 0 , 0 , 5631, 359, 113, 26), // #985
  INST(Vmaxss          , VexRvm             , V(F30F00,5F,_,I,I,0,2,T1S), 0                         , 0 , 0 , 5638, 293, 113, 26), // #986
  INST(Vminpd          , VexRvm_Lx          , V(660F00,5D,_,x,I,1,4,FV ), 0                         , 0 , 0 , 5645, 357, 113, 27), // #987
  INST(Vminps          , VexRvm_Lx          , V(000F00,5D,_,x,I,0,4,FV ), 0                         , 0 , 0 , 5652, 358, 113, 27), // #988
  INST(Vminsd          , VexRvm             , V(F20F00,5D,_,I,I,1,3,T1S), 0                         , 0 , 0 , 5659, 359, 113, 27), // #989
  INST(Vminss          , VexRvm             , V(F30F00,5D,_,I,I,0,2,T1S), 0                         , 0 , 0 , 5666, 293, 113, 27), // #990
  INST(Vmovapd         , VexRmMr_Lx         , V(660F00,28,_,x,I,1,4,FVM), V(660F00,29,_,x,I,1,4,FVM), 0 , 0 , 5673, 360, 113, 28), // #991
  INST(Vmovaps         , VexRmMr_Lx         , V(000F00,28,_,x,I,0,4,FVM), V(000F00,29,_,x,I,0,4,FVM), 0 , 0 , 5681, 361, 113, 28), // #992
  INST(Vmovd           , VexMovdMovq        , V(660F00,6E,_,0,0,0,2,T1S), V(660F00,7E,_,0,0,0,2,T1S), 0 , 0 , 5689, 362, 114, 29), // #993
  INST(Vmovddup        , VexRm_Lx           , V(F20F00,12,_,x,I,1,3,DUP), 0                         , 0 , 0 , 5695, 363, 113, 29), // #994
  INST(Vmovdqa         , VexRmMr_Lx         , V(660F00,6F,_,x,I,_,_,_  ), V(660F00,7F,_,x,I,_,_,_  ), 0 , 0 , 5704, 364, 115, 30), // #995
  INST(Vmovdqa32       , VexRmMr_Lx         , V(660F00,6F,_,x,_,0,4,FVM), V(660F00,7F,_,x,_,0,4,FVM), 0 , 0 , 5712, 365, 117, 0 ), // #996
  INST(Vmovdqa64       , VexRmMr_Lx         , V(660F00,6F,_,x,_,1,4,FVM), V(660F00,7F,_,x,_,1,4,FVM), 0 , 0 , 5722, 366, 117, 0 ), // #997
  INST(Vmovdqu         , VexRmMr_Lx         , V(F30F00,6F,_,x,I,_,_,_  ), V(F30F00,7F,_,x,I,_,_,_  ), 0 , 0 , 5732, 367, 115, 28), // #998
  INST(Vmovdqu16       , VexRmMr_Lx         , V(F20F00,6F,_,x,_,1,4,FVM), V(F20F00,7F,_,x,_,1,4,FVM), 0 , 0 , 5740, 368, 119, 0 ), // #999
  INST(Vmovdqu32       , VexRmMr_Lx         , V(F30F00,6F,_,x,_,0,4,FVM), V(F30F00,7F,_,x,_,0,4,FVM), 0 , 0 , 5750, 369, 117, 0 ), // #1000
  INST(Vmovdqu64       , VexRmMr_Lx         , V(F30F00,6F,_,x,_,1,4,FVM), V(F30F00,7F,_,x,_,1,4,FVM), 0 , 0 , 5760, 370, 117, 0 ), // #1001
  INST(Vmovdqu8        , VexRmMr_Lx         , V(F20F00,6F,_,x,_,0,4,FVM), V(F20F00,7F,_,x,_,0,4,FVM), 0 , 0 , 5770, 371, 119, 0 ), // #1002
  INST(Vmovhlps        , VexRvm             , V(000F00,12,_,0,I,0,_,_  ), 0                         , 0 , 0 , 5779, 372, 114, 26), // #1003
  INST(Vmovhpd         , VexRvmMr           , V(660F00,16,_,0,I,1,3,T1S), V(660F00,17,_,0,I,1,3,T1S), 0 , 0 , 5788, 373, 114, 31), // #1004
  INST(Vmovhps         , VexRvmMr           , V(000F00,16,_,0,I,0,3,T2 ), V(000F00,17,_,0,I,0,3,T2 ), 0 , 0 , 5796, 374, 114, 31), // #1005
  INST(Vmovlhps        , VexRvm             , V(000F00,16,_,0,I,0,_,_  ), 0                         , 0 , 0 , 5804, 372, 114, 26), // #1006
  INST(Vmovlpd         , VexRvmMr           , V(660F00,12,_,0,I,1,3,T1S), V(660F00,13,_,0,I,1,3,T1S), 0 , 0 , 5813, 375, 114, 31), // #1007
  INST(Vmovlps         , VexRvmMr           , V(000F00,12,_,0,I,0,3,T2 ), V(000F00,13,_,0,I,0,3,T2 ), 0 , 0 , 5821, 376, 114, 31), // #1008
  INST(Vmovmskpd       , VexRm_Lx           , V(660F00,50,_,x,I,_,_,_  ), 0                         , 0 , 0 , 5829, 377, 115, 32), // #1009
  INST(Vmovmskps       , VexRm_Lx           , V(000F00,50,_,x,I,_,_,_  ), 0                         , 0 , 0 , 5839, 377, 115, 32), // #1010
  INST(Vmovntdq        , VexMr_Lx           , V(660F00,E7,_,x,I,0,4,FVM), 0                         , 0 , 0 , 5849, 378, 113, 32), // #1011
  INST(Vmovntdqa       , VexRm_Lx           , V(660F38,2A,_,x,I,0,4,FVM), 0                         , 0 , 0 , 5858, 379, 122, 32), // #1012
  INST(Vmovntpd        , VexMr_Lx           , V(660F00,2B,_,x,I,1,4,FVM), 0                         , 0 , 0 , 5868, 378, 113, 25), // #1013
  INST(Vmovntps        , VexMr_Lx           , V(000F00,2B,_,x,I,0,4,FVM), 0                         , 0 , 0 , 5877, 378, 113, 25), // #1014
  INST(Vmovq           , VexMovdMovq        , V(660F00,6E,_,0,I,1,3,T1S), V(660F00,7E,_,0,I,1,3,T1S), 0 , 0 , 5886, 380, 114, 28), // #1015
  INST(Vmovsd          , VexMovssMovsd      , V(F20F00,10,_,I,I,1,3,T1S), V(F20F00,11,_,I,I,1,3,T1S), 0 , 0 , 5892, 381, 114, 33), // #1016
  INST(Vmovshdup       , VexRm_Lx           , V(F30F00,16,_,x,I,0,4,FVM), 0                         , 0 , 0 , 5899, 382, 113, 30), // #1017
  INST(Vmovsldup       , VexRm_Lx           , V(F30F00,12,_,x,I,0,4,FVM), 0                         , 0 , 0 , 5909, 382, 113, 30), // #1018
  INST(Vmovss          , VexMovssMovsd      , V(F30F00,10,_,I,I,0,2,T1S), V(F30F00,11,_,I,I,0,2,T1S), 0 , 0 , 5919, 383, 114, 33), // #1019
  INST(Vmovupd         , VexRmMr_Lx         , V(660F00,10,_,x,I,1,4,FVM), V(660F00,11,_,x,I,1,4,FVM), 0 , 0 , 5926, 384, 113, 34), // #1020
  INST(Vmovups         , VexRmMr_Lx         , V(000F00,10,_,x,I,0,4,FVM), V(000F00,11,_,x,I,0,4,FVM), 0 , 0 , 5934, 385, 113, 34), // #1021
  INST(Vmpsadbw        , VexRvmi_Lx         , V(660F3A,42,_,x,I,_,_,_  ), 0                         , 0 , 0 , 5942, 264, 132, 35), // #1022
  INST(Vmulpd          , VexRvm_Lx          , V(660F00,59,_,x,I,1,4,FV ), 0                         , 0 , 0 , 5951, 247, 113, 36), // #1023
  INST(Vmulps          , VexRvm_Lx          , V(000F00,59,_,x,I,0,4,FV ), 0                         , 0 , 0 , 5958, 248, 113, 36), // #1024
  INST(Vmulsd          , VexRvm_Lx          , V(F20F00,59,_,I,I,1,3,T1S), 0                         , 0 , 0 , 5965, 249, 114, 36), // #1025
  INST(Vmulss          , VexRvm_Lx          , V(F30F00,59,_,I,I,0,2,T1S), 0                         , 0 , 0 , 5972, 250, 114, 36), // #1026
  INST(Vorpd           , VexRvm_Lx          , V(660F00,56,_,x,I,1,4,FV ), 0                         , 0 , 0 , 5979, 259, 118, 37), // #1027
  INST(Vorps           , VexRvm_Lx          , V(000F00,56,_,x,I,0,4,FV ), 0                         , 0 , 0 , 5985, 260, 118, 37), // #1028
  INST(Vp4dpwssd       , VexRm_T1_4X        , V(F20F38,52,_,2,_,0,2,T4X), 0                         , 0 , 0 , 5991, 386, 133, 0 ), // #1029
  INST(Vp4dpwssds      , VexRm_T1_4X        , V(F20F38,53,_,2,_,0,2,T4X), 0                         , 0 , 0 , 6001, 386, 133, 0 ), // #1030
  INST(Vpabsb          , VexRm_Lx           , V(660F38,1C,_,x,I,_,4,FVM), 0                         , 0 , 0 , 6012, 382, 134, 38), // #1031
  INST(Vpabsd          , VexRm_Lx           , V(660F38,1E,_,x,I,0,4,FV ), 0                         , 0 , 0 , 6019, 382, 122, 38), // #1032
  INST(Vpabsq          , VexRm_Lx           , V(660F38,1F,_,x,_,1,4,FV ), 0                         , 0 , 0 , 6026, 312, 117, 0 ), // #1033
  INST(Vpabsw          , VexRm_Lx           , V(660F38,1D,_,x,I,_,4,FVM), 0                         , 0 , 0 , 6033, 382, 134, 39), // #1034
  INST(Vpackssdw       , VexRvm_Lx          , V(660F00,6B,_,x,I,0,4,FV ), 0                         , 0 , 0 , 6040, 258, 134, 40), // #1035
  INST(Vpacksswb       , VexRvm_Lx          , V(660F00,63,_,x,I,I,4,FVM), 0                         , 0 , 0 , 6050, 387, 134, 40), // #1036
  INST(Vpackusdw       , VexRvm_Lx          , V(660F38,2B,_,x,I,0,4,FV ), 0                         , 0 , 0 , 6060, 258, 134, 40), // #1037
  INST(Vpackuswb       , VexRvm_Lx          , V(660F00,67,_,x,I,I,4,FVM), 0                         , 0 , 0 , 6070, 387, 134, 40), // #1038
  INST(Vpaddb          , VexRvm_Lx          , V(660F00,FC,_,x,I,I,4,FVM), 0                         , 0 , 0 , 6080, 387, 134, 40), // #1039
  INST(Vpaddd          , VexRvm_Lx          , V(660F00,FE,_,x,I,0,4,FV ), 0                         , 0 , 0 , 6087, 258, 122, 40), // #1040
  INST(Vpaddq          , VexRvm_Lx          , V(660F00,D4,_,x,I,1,4,FV ), 0                         , 0 , 0 , 6094, 257, 122, 40), // #1041
  INST(Vpaddsb         , VexRvm_Lx          , V(660F00,EC,_,x,I,I,4,FVM), 0                         , 0 , 0 , 6101, 387, 134, 40), // #1042
  INST(Vpaddsw         , VexRvm_Lx          , V(660F00,ED,_,x,I,I,4,FVM), 0                         , 0 , 0 , 6109, 387, 134, 40), // #1043
  INST(Vpaddusb        , VexRvm_Lx          , V(660F00,DC,_,x,I,I,4,FVM), 0                         , 0 , 0 , 6117, 387, 134, 40), // #1044
  INST(Vpaddusw        , VexRvm_Lx          , V(660F00,DD,_,x,I,I,4,FVM), 0                         , 0 , 0 , 6126, 387, 134, 40), // #1045
  INST(Vpaddw          , VexRvm_Lx          , V(660F00,FD,_,x,I,I,4,FVM), 0                         , 0 , 0 , 6135, 387, 134, 40), // #1046
  INST(Vpalignr        , VexRvmi_Lx         , V(660F3A,0F,_,x,I,I,4,FVM), 0                         , 0 , 0 , 6142, 388, 134, 40), // #1047
  INST(Vpand           , VexRvm_Lx          , V(660F00,DB,_,x,I,_,_,_  ), 0                         , 0 , 0 , 6151, 389, 132, 40), // #1048
  INST(Vpandd          , VexRvm_Lx          , V(660F00,DB,_,x,_,0,4,FV ), 0                         , 0 , 0 , 6157, 390, 117, 0 ), // #1049
  INST(Vpandn          , VexRvm_Lx          , V(660F00,DF,_,x,I,_,_,_  ), 0                         , 0 , 0 , 6164, 391, 132, 41), // #1050
  INST(Vpandnd         , VexRvm_Lx          , V(660F00,DF,_,x,_,0,4,FV ), 0                         , 0 , 0 , 6171, 392, 117, 0 ), // #1051
  INST(Vpandnq         , VexRvm_Lx          , V(660F00,DF,_,x,_,1,4,FV ), 0                         , 0 , 0 , 6179, 393, 117, 0 ), // #1052
  INST(Vpandq          , VexRvm_Lx          , V(660F00,DB,_,x,_,1,4,FV ), 0                         , 0 , 0 , 6187, 394, 117, 0 ), // #1053
  INST(Vpavgb          , VexRvm_Lx          , V(660F00,E0,_,x,I,I,4,FVM), 0                         , 0 , 0 , 6194, 387, 134, 42), // #1054
  INST(Vpavgw          , VexRvm_Lx          , V(660F00,E3,_,x,I,I,4,FVM), 0                         , 0 , 0 , 6201, 387, 134, 43), // #1055
  INST(Vpblendd        , VexRvmi_Lx         , V(660F3A,02,_,x,0,_,_,_  ), 0                         , 0 , 0 , 6208, 264, 121, 0 ), // #1056
  INST(Vpblendvb       , VexRvmr            , V(660F3A,4C,_,x,0,_,_,_  ), 0                         , 0 , 0 , 6217, 265, 132, 44), // #1057
  INST(Vpblendw        , VexRvmi_Lx         , V(660F3A,0E,_,x,I,_,_,_  ), 0                         , 0 , 0 , 6227, 264, 132, 42), // #1058
  INST(Vpbroadcastb    , VexRm_Lx           , V(660F38,78,_,x,0,0,0,T1S), 0                         , 0 , 0 , 6236, 395, 135, 0 ), // #1059
  INST(Vpbroadcastd    , VexRm_Lx           , V(660F38,58,_,x,0,0,2,T1S), 0                         , 0 , 0 , 6249, 396, 130, 0 ), // #1060
  INST(Vpbroadcastmb2d , VexRm_Lx           , V(F30F38,3A,_,x,_,0,_,_  ), 0                         , 0 , 0 , 6262, 397, 136, 0 ), // #1061
  INST(Vpbroadcastmb2q , VexRm_Lx           , V(F30F38,2A,_,x,_,1,_,_  ), 0                         , 0 , 0 , 6278, 397, 136, 0 ), // #1062
  INST(Vpbroadcastq    , VexRm_Lx           , V(660F38,59,_,x,0,1,3,T1S), 0                         , 0 , 0 , 6294, 398, 130, 0 ), // #1063
  INST(Vpbroadcastw    , VexRm_Lx           , V(660F38,79,_,x,0,0,1,T1S), 0                         , 0 , 0 , 6307, 399, 135, 0 ), // #1064
  INST(Vpclmulqdq      , VexRvmi            , V(660F3A,44,_,0,I,_,_,_  ), 0                         , 0 , 0 , 6320, 309, 137, 45), // #1065
  INST(Vpcmov          , VexRvrmRvmr_Lx     , V(XOP_M8,A2,_,x,x,_,_,_  ), 0                         , 0 , 0 , 6331, 325, 129, 0 ), // #1066
  INST(Vpcmpb          , VexRvm_Lx          , V(660F3A,3F,_,x,_,0,4,FVM), 0                         , 0 , 0 , 6338, 400, 119, 0 ), // #1067
  INST(Vpcmpd          , VexRvm_Lx          , V(660F3A,1F,_,x,_,0,4,FV ), 0                         , 0 , 0 , 6345, 401, 117, 0 ), // #1068
  INST(Vpcmpeqb        , VexRvm_Lx          , V(660F00,74,_,x,I,I,4,FV ), 0                         , 0 , 0 , 6352, 402, 134, 46), // #1069
  INST(Vpcmpeqd        , VexRvm_Lx          , V(660F00,76,_,x,I,0,4,FVM), 0                         , 0 , 0 , 6361, 403, 122, 46), // #1070
  INST(Vpcmpeqq        , VexRvm_Lx          , V(660F38,29,_,x,I,1,4,FVM), 0                         , 0 , 0 , 6370, 404, 122, 46), // #1071
  INST(Vpcmpeqw        , VexRvm_Lx          , V(660F00,75,_,x,I,I,4,FV ), 0                         , 0 , 0 , 6379, 402, 134, 46), // #1072
  INST(Vpcmpestri      , VexRmi             , V(660F3A,61,_,0,I,_,_,_  ), 0                         , 0 , 0 , 6388, 405, 138, 28), // #1073
  INST(Vpcmpestrm      , VexRmi             , V(660F3A,60,_,0,I,_,_,_  ), 0                         , 0 , 0 , 6399, 406, 138, 28), // #1074
  INST(Vpcmpgtb        , VexRvm_Lx          , V(660F00,64,_,x,I,I,4,FV ), 0                         , 0 , 0 , 6410, 402, 134, 46), // #1075
  INST(Vpcmpgtd        , VexRvm_Lx          , V(660F00,66,_,x,I,0,4,FVM), 0                         , 0 , 0 , 6419, 403, 122, 46), // #1076
  INST(Vpcmpgtq        , VexRvm_Lx          , V(660F38,37,_,x,I,1,4,FVM), 0                         , 0 , 0 , 6428, 404, 122, 46), // #1077
  INST(Vpcmpgtw        , VexRvm_Lx          , V(660F00,65,_,x,I,I,4,FV ), 0                         , 0 , 0 , 6437, 402, 134, 46), // #1078
  INST(Vpcmpistri      , VexRmi             , V(660F3A,63,_,0,I,_,_,_  ), 0                         , 0 , 0 , 6446, 407, 138, 28), // #1079
  INST(Vpcmpistrm      , VexRmi             , V(660F3A,62,_,0,I,_,_,_  ), 0                         , 0 , 0 , 6457, 408, 138, 28), // #1080
  INST(Vpcmpq          , VexRvm_Lx          , V(660F3A,1F,_,x,_,1,4,FV ), 0                         , 0 , 0 , 6468, 409, 117, 0 ), // #1081
  INST(Vpcmpub         , VexRvm_Lx          , V(660F3A,3E,_,x,_,0,4,FVM), 0                         , 0 , 0 , 6475, 400, 119, 0 ), // #1082
  INST(Vpcmpud         , VexRvm_Lx          , V(660F3A,1E,_,x,_,0,4,FV ), 0                         , 0 , 0 , 6483, 401, 117, 0 ), // #1083
  INST(Vpcmpuq         , VexRvm_Lx          , V(660F3A,1E,_,x,_,1,4,FV ), 0                         , 0 , 0 , 6491, 409, 117, 0 ), // #1084
  INST(Vpcmpuw         , VexRvm_Lx          , V(660F3A,3E,_,x,_,1,4,FVM), 0                         , 0 , 0 , 6499, 409, 119, 0 ), // #1085
  INST(Vpcmpw          , VexRvm_Lx          , V(660F3A,3F,_,x,_,1,4,FVM), 0                         , 0 , 0 , 6507, 409, 119, 0 ), // #1086
  INST(Vpcomb          , VexRvmi            , V(XOP_M8,CC,_,0,0,_,_,_  ), 0                         , 0 , 0 , 6514, 309, 129, 0 ), // #1087
  INST(Vpcomd          , VexRvmi            , V(XOP_M8,CE,_,0,0,_,_,_  ), 0                         , 0 , 0 , 6521, 309, 129, 0 ), // #1088
  INST(Vpcompressd     , VexMr_Lx           , V(660F38,8B,_,x,_,0,2,T1S), 0                         , 0 , 0 , 6528, 279, 117, 0 ), // #1089
  INST(Vpcompressq     , VexMr_Lx           , V(660F38,8B,_,x,_,1,3,T1S), 0                         , 0 , 0 , 6540, 279, 117, 0 ), // #1090
  INST(Vpcomq          , VexRvmi            , V(XOP_M8,CF,_,0,0,_,_,_  ), 0                         , 0 , 0 , 6552, 309, 129, 0 ), // #1091
  INST(Vpcomub         , VexRvmi            , V(XOP_M8,EC,_,0,0,_,_,_  ), 0                         , 0 , 0 , 6559, 309, 129, 0 ), // #1092
  INST(Vpcomud         , VexRvmi            , V(XOP_M8,EE,_,0,0,_,_,_  ), 0                         , 0 , 0 , 6567, 309, 129, 0 ), // #1093
  INST(Vpcomuq         , VexRvmi            , V(XOP_M8,EF,_,0,0,_,_,_  ), 0                         , 0 , 0 , 6575, 309, 129, 0 ), // #1094
  INST(Vpcomuw         , VexRvmi            , V(XOP_M8,ED,_,0,0,_,_,_  ), 0                         , 0 , 0 , 6583, 309, 129, 0 ), // #1095
  INST(Vpcomw          , VexRvmi            , V(XOP_M8,CD,_,0,0,_,_,_  ), 0                         , 0 , 0 , 6591, 309, 129, 0 ), // #1096
  INST(Vpconflictd     , VexRm_Lx           , V(660F38,C4,_,x,_,0,4,FV ), 0                         , 0 , 0 , 6598, 410, 136, 0 ), // #1097
  INST(Vpconflictq     , VexRm_Lx           , V(660F38,C4,_,x,_,1,4,FV ), 0                         , 0 , 0 , 6610, 410, 136, 0 ), // #1098
  INST(Vperm2f128      , VexRvmi            , V(660F3A,06,_,1,0,_,_,_  ), 0                         , 0 , 0 , 6622, 411, 115, 0 ), // #1099
  INST(Vperm2i128      , VexRvmi            , V(660F3A,46,_,1,0,_,_,_  ), 0                         , 0 , 0 , 6633, 411, 121, 0 ), // #1100
  INST(Vpermb          , VexRvm_Lx          , V(660F38,8D,_,x,_,0,4,FVM), 0                         , 0 , 0 , 6644, 261, 139, 0 ), // #1101
  INST(Vpermd          , VexRvm_Lx          , V(660F38,36,_,x,0,0,4,FV ), 0                         , 0 , 0 , 6651, 412, 130, 0 ), // #1102
  INST(Vpermi2b        , VexRvm_Lx          , V(660F38,75,_,x,_,0,4,FVM), 0                         , 0 , 0 , 6658, 261, 139, 0 ), // #1103
  INST(Vpermi2d        , VexRvm_Lx          , V(660F38,76,_,x,_,0,4,FV ), 0                         , 0 , 0 , 6667, 413, 117, 0 ), // #1104
  INST(Vpermi2pd       , VexRvm_Lx          , V(660F38,77,_,x,_,1,4,FV ), 0                         , 0 , 0 , 6676, 263, 117, 0 ), // #1105
  INST(Vpermi2ps       , VexRvm_Lx          , V(660F38,77,_,x,_,0,4,FV ), 0                         , 0 , 0 , 6686, 262, 117, 0 ), // #1106
  INST(Vpermi2q        , VexRvm_Lx          , V(660F38,76,_,x,_,1,4,FV ), 0                         , 0 , 0 , 6696, 414, 117, 0 ), // #1107
  INST(Vpermi2w        , VexRvm_Lx          , V(660F38,75,_,x,_,1,4,FVM), 0                         , 0 , 0 , 6705, 415, 119, 0 ), // #1108
  INST(Vpermil2pd      , VexRvrmiRvmri_Lx   , V(660F3A,49,_,x,x,_,_,_  ), 0                         , 0 , 0 , 6714, 416, 129, 0 ), // #1109
  INST(Vpermil2ps      , VexRvrmiRvmri_Lx   , V(660F3A,48,_,x,x,_,_,_  ), 0                         , 0 , 0 , 6725, 416, 129, 0 ), // #1110
  INST(Vpermilpd       , VexRvmRmi_Lx       , V(660F38,0D,_,x,0,1,4,FV ), V(660F3A,05,_,x,0,1,4,FV ), 0 , 0 , 6736, 417, 113, 0 ), // #1111
  INST(Vpermilps       , VexRvmRmi_Lx       , V(660F38,0C,_,x,0,0,4,FV ), V(660F3A,04,_,x,0,0,4,FV ), 0 , 0 , 6746, 418, 113, 0 ), // #1112
  INST(Vpermpd         , VexRmi             , V(660F3A,01,_,1,1,_,_,_  ), 0                         , 0 , 0 , 6756, 419, 121, 0 ), // #1113
  INST(Vpermps         , VexRvm             , V(660F38,16,_,1,0,_,_,_  ), 0                         , 0 , 0 , 6764, 420, 121, 0 ), // #1114
  INST(Vpermq          , VexRvmRmi_Lx       , V(660F38,36,_,x,_,1,4,FV ), V(660F3A,00,_,x,1,1,4,FV ), 0 , 0 , 6772, 421, 130, 0 ), // #1115
  INST(Vpermt2b        , VexRvm_Lx          , V(660F38,7D,_,x,_,0,4,FVM), 0                         , 0 , 0 , 6779, 261, 139, 0 ), // #1116
  INST(Vpermt2d        , VexRvm_Lx          , V(660F38,7E,_,x,_,0,4,FV ), 0                         , 0 , 0 , 6788, 413, 117, 0 ), // #1117
  INST(Vpermt2pd       , VexRvm_Lx          , V(660F38,7F,_,x,_,1,4,FV ), 0                         , 0 , 0 , 6797, 414, 117, 0 ), // #1118
  INST(Vpermt2ps       , VexRvm_Lx          , V(660F38,7F,_,x,_,0,4,FV ), 0                         , 0 , 0 , 6807, 413, 117, 0 ), // #1119
  INST(Vpermt2q        , VexRvm_Lx          , V(660F38,7E,_,x,_,1,4,FV ), 0                         , 0 , 0 , 6817, 414, 117, 0 ), // #1120
  INST(Vpermt2w        , VexRvm_Lx          , V(660F38,7D,_,x,_,1,4,FVM), 0                         , 0 , 0 , 6826, 415, 119, 0 ), // #1121
  INST(Vpermw          , VexRvm_Lx          , V(660F38,8D,_,x,_,1,4,FVM), 0                         , 0 , 0 , 6835, 261, 119, 0 ), // #1122
  INST(Vpexpandd       , VexRm_Lx           , V(660F38,89,_,x,_,0,2,T1S), 0                         , 0 , 0 , 6842, 312, 117, 0 ), // #1123
  INST(Vpexpandq       , VexRm_Lx           , V(660F38,89,_,x,_,1,3,T1S), 0                         , 0 , 0 , 6852, 312, 117, 0 ), // #1124
  INST(Vpextrb         , VexMri             , V(660F3A,14,_,0,0,I,0,T1S), 0                         , 0 , 0 , 6862, 422, 140, 47), // #1125
  INST(Vpextrd         , VexMri             , V(660F3A,16,_,0,0,0,2,T1S), 0                         , 0 , 0 , 6870, 316, 141, 47), // #1126
  INST(Vpextrq         , VexMri             , V(660F3A,16,_,0,1,1,3,T1S), 0                         , 0 , 0 , 6878, 423, 141, 47), // #1127
  INST(Vpextrw         , VexMri             , V(660F3A,15,_,0,0,I,1,T1S), 0                         , 0 , 0 , 6886, 424, 140, 47), // #1128
  INST(Vpgatherdd      , VexRmvRm_VM        , V(660F38,90,_,x,0,_,_,_  ), V(660F38,90,_,x,_,0,2,T1S), 0 , 0 , 6894, 425, 130, 0 ), // #1129
  INST(Vpgatherdq      , VexRmvRm_VM        , V(660F38,90,_,x,1,_,_,_  ), V(660F38,90,_,x,_,1,3,T1S), 0 , 0 , 6905, 426, 130, 0 ), // #1130
  INST(Vpgatherqd      , VexRmvRm_VM        , V(660F38,91,_,x,0,_,_,_  ), V(660F38,91,_,x,_,0,2,T1S), 0 , 0 , 6916, 427, 130, 0 ), // #1131
  INST(Vpgatherqq      , VexRmvRm_VM        , V(660F38,91,_,x,1,_,_,_  ), V(660F38,91,_,x,_,1,3,T1S), 0 , 0 , 6927, 428, 130, 0 ), // #1132
  INST(Vphaddbd        , VexRm              , V(XOP_M9,C2,_,0,0,_,_,_  ), 0                         , 0 , 0 , 6938, 253, 129, 0 ), // #1133
  INST(Vphaddbq        , VexRm              , V(XOP_M9,C3,_,0,0,_,_,_  ), 0                         , 0 , 0 , 6947, 253, 129, 0 ), // #1134
  INST(Vphaddbw        , VexRm              , V(XOP_M9,C1,_,0,0,_,_,_  ), 0                         , 0 , 0 , 6956, 253, 129, 0 ), // #1135
  INST(Vphaddd         , VexRvm_Lx          , V(660F38,02,_,x,I,_,_,_  ), 0                         , 0 , 0 , 6965, 251, 132, 48), // #1136
  INST(Vphadddq        , VexRm              , V(XOP_M9,CB,_,0,0,_,_,_  ), 0                         , 0 , 0 , 6973, 253, 129, 0 ), // #1137
  INST(Vphaddsw        , VexRvm_Lx          , V(660F38,03,_,x,I,_,_,_  ), 0                         , 0 , 0 , 6982, 251, 132, 49), // #1138
  INST(Vphaddubd       , VexRm              , V(XOP_M9,D2,_,0,0,_,_,_  ), 0                         , 0 , 0 , 6991, 253, 129, 0 ), // #1139
  INST(Vphaddubq       , VexRm              , V(XOP_M9,D3,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7001, 253, 129, 0 ), // #1140
  INST(Vphaddubw       , VexRm              , V(XOP_M9,D1,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7011, 253, 129, 0 ), // #1141
  INST(Vphaddudq       , VexRm              , V(XOP_M9,DB,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7021, 253, 129, 0 ), // #1142
  INST(Vphadduwd       , VexRm              , V(XOP_M9,D6,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7031, 253, 129, 0 ), // #1143
  INST(Vphadduwq       , VexRm              , V(XOP_M9,D7,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7041, 253, 129, 0 ), // #1144
  INST(Vphaddw         , VexRvm_Lx          , V(660F38,01,_,x,I,_,_,_  ), 0                         , 0 , 0 , 7051, 251, 132, 50), // #1145
  INST(Vphaddwd        , VexRm              , V(XOP_M9,C6,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7059, 253, 129, 0 ), // #1146
  INST(Vphaddwq        , VexRm              , V(XOP_M9,C7,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7068, 253, 129, 0 ), // #1147
  INST(Vphminposuw     , VexRm              , V(660F38,41,_,0,I,_,_,_  ), 0                         , 0 , 0 , 7077, 253, 115, 51), // #1148
  INST(Vphsubbw        , VexRm              , V(XOP_M9,E1,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7089, 253, 129, 0 ), // #1149
  INST(Vphsubd         , VexRvm_Lx          , V(660F38,06,_,x,I,_,_,_  ), 0                         , 0 , 0 , 7098, 251, 132, 52), // #1150
  INST(Vphsubdq        , VexRm              , V(XOP_M9,E3,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7106, 253, 129, 0 ), // #1151
  INST(Vphsubsw        , VexRvm_Lx          , V(660F38,07,_,x,I,_,_,_  ), 0                         , 0 , 0 , 7115, 251, 132, 53), // #1152
  INST(Vphsubw         , VexRvm_Lx          , V(660F38,05,_,x,I,_,_,_  ), 0                         , 0 , 0 , 7124, 251, 132, 53), // #1153
  INST(Vphsubwd        , VexRm              , V(XOP_M9,E2,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7132, 253, 129, 0 ), // #1154
  INST(Vpinsrb         , VexRvmi            , V(660F3A,20,_,0,0,I,0,T1S), 0                         , 0 , 0 , 7141, 429, 140, 52), // #1155
  INST(Vpinsrd         , VexRvmi            , V(660F3A,22,_,0,0,0,2,T1S), 0                         , 0 , 0 , 7149, 430, 141, 52), // #1156
  INST(Vpinsrq         , VexRvmi            , V(660F3A,22,_,0,1,1,3,T1S), 0                         , 0 , 0 , 7157, 431, 141, 52), // #1157
  INST(Vpinsrw         , VexRvmi            , V(660F00,C4,_,0,0,I,1,T1S), 0                         , 0 , 0 , 7165, 432, 140, 52), // #1158
  INST(Vplzcntd        , VexRm_Lx           , V(660F38,44,_,x,_,0,4,FV ), 0                         , 0 , 0 , 7173, 410, 136, 0 ), // #1159
  INST(Vplzcntq        , VexRm_Lx           , V(660F38,44,_,x,_,1,4,FV ), 0                         , 0 , 0 , 7182, 433, 136, 0 ), // #1160
  INST(Vpmacsdd        , VexRvmr            , V(XOP_M8,9E,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7191, 434, 129, 0 ), // #1161
  INST(Vpmacsdqh       , VexRvmr            , V(XOP_M8,9F,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7200, 434, 129, 0 ), // #1162
  INST(Vpmacsdql       , VexRvmr            , V(XOP_M8,97,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7210, 434, 129, 0 ), // #1163
  INST(Vpmacssdd       , VexRvmr            , V(XOP_M8,8E,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7220, 434, 129, 0 ), // #1164
  INST(Vpmacssdqh      , VexRvmr            , V(XOP_M8,8F,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7230, 434, 129, 0 ), // #1165
  INST(Vpmacssdql      , VexRvmr            , V(XOP_M8,87,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7241, 434, 129, 0 ), // #1166
  INST(Vpmacsswd       , VexRvmr            , V(XOP_M8,86,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7252, 434, 129, 0 ), // #1167
  INST(Vpmacssww       , VexRvmr            , V(XOP_M8,85,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7262, 434, 129, 0 ), // #1168
  INST(Vpmacswd        , VexRvmr            , V(XOP_M8,96,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7272, 434, 129, 0 ), // #1169
  INST(Vpmacsww        , VexRvmr            , V(XOP_M8,95,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7281, 434, 129, 0 ), // #1170
  INST(Vpmadcsswd      , VexRvmr            , V(XOP_M8,A6,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7290, 434, 129, 0 ), // #1171
  INST(Vpmadcswd       , VexRvmr            , V(XOP_M8,B6,_,0,0,_,_,_  ), 0                         , 0 , 0 , 7301, 434, 129, 0 ), // #1172
  INST(Vpmadd52huq     , VexRvm_Lx          , V(660F38,B5,_,x,_,1,4,FV ), 0                         , 0 , 0 , 7311, 414, 142, 0 ), // #1173
  INST(Vpmadd52luq     , VexRvm_Lx          , V(660F38,B4,_,x,_,1,4,FV ), 0                         , 0 , 0 , 7323, 414, 142, 0 ), // #1174
  INST(Vpmaddubsw      , VexRvm_Lx          , V(660F38,04,_,x,I,I,4,FVM), 0                         , 0 , 0 , 7335, 387, 134, 54), // #1175
  INST(Vpmaddwd        , VexRvm_Lx          , V(660F00,F5,_,x,I,I,4,FVM), 0                         , 0 , 0 , 7346, 387, 134, 54), // #1176
  INST(Vpmaskmovd      , VexRvmMvr_Lx       , V(660F38,8C,_,x,0,_,_,_  ), V(660F38,8E,_,x,0,_,_,_  ), 0 , 0 , 7355, 435, 121, 0 ), // #1177
  INST(Vpmaskmovq      , VexRvmMvr_Lx       , V(660F38,8C,_,x,1,_,_,_  ), V(660F38,8E,_,x,1,_,_,_  ), 0 , 0 , 7366, 436, 121, 0 ), // #1178
  INST(Vpmaxsb         , VexRvm_Lx          , V(660F38,3C,_,x,I,I,4,FVM), 0                         , 0 , 0 , 7377, 437, 134, 55), // #1179
  INST(Vpmaxsd         , VexRvm_Lx          , V(660F38,3D,_,x,I,0,4,FV ), 0                         , 0 , 0 , 7385, 260, 122, 55), // #1180
  INST(Vpmaxsq         , VexRvm_Lx          , V(660F38,3D,_,x,_,1,4,FV ), 0                         , 0 , 0 , 7393, 263, 117, 0 ), // #1181
  INST(Vpmaxsw         , VexRvm_Lx          , V(660F00,EE,_,x,I,I,4,FVM), 0                         , 0 , 0 , 7401, 437, 134, 56), // #1182
  INST(Vpmaxub         , VexRvm_Lx          , V(660F00,DE,_,x,I,I,4,FVM), 0                         , 0 , 0 , 7409, 437, 134, 56), // #1183
  INST(Vpmaxud         , VexRvm_Lx          , V(660F38,3F,_,x,I,0,4,FV ), 0                         , 0 , 0 , 7417, 260, 122, 56), // #1184
  INST(Vpmaxuq         , VexRvm_Lx          , V(660F38,3F,_,x,_,1,4,FV ), 0                         , 0 , 0 , 7425, 263, 117, 0 ), // #1185
  INST(Vpmaxuw         , VexRvm_Lx          , V(660F38,3E,_,x,I,I,4,FVM), 0                         , 0 , 0 , 7433, 437, 134, 57), // #1186
  INST(Vpminsb         , VexRvm_Lx          , V(660F38,38,_,x,I,I,4,FVM), 0                         , 0 , 0 , 7441, 437, 134, 57), // #1187
  INST(Vpminsd         , VexRvm_Lx          , V(660F38,39,_,x,I,0,4,FV ), 0                         , 0 , 0 , 7449, 260, 122, 57), // #1188
  INST(Vpminsq         , VexRvm_Lx          , V(660F38,39,_,x,_,1,4,FV ), 0                         , 0 , 0 , 7457, 263, 117, 0 ), // #1189
  INST(Vpminsw         , VexRvm_Lx          , V(660F00,EA,_,x,I,I,4,FVM), 0                         , 0 , 0 , 7465, 437, 134, 58), // #1190
  INST(Vpminub         , VexRvm_Lx          , V(660F00,DA,_,x,I,_,4,FVM), 0                         , 0 , 0 , 7473, 437, 134, 58), // #1191
  INST(Vpminud         , VexRvm_Lx          , V(660F38,3B,_,x,I,0,4,FV ), 0                         , 0 , 0 , 7481, 260, 122, 58), // #1192
  INST(Vpminuq         , VexRvm_Lx          , V(660F38,3B,_,x,_,1,4,FV ), 0                         , 0 , 0 , 7489, 263, 117, 0 ), // #1193
  INST(Vpminuw         , VexRvm_Lx          , V(660F38,3A,_,x,I,_,4,FVM), 0                         , 0 , 0 , 7497, 437, 134, 59), // #1194
  INST(Vpmovb2m        , VexRm_Lx           , V(F30F38,29,_,x,_,0,_,_  ), 0                         , 0 , 0 , 7505, 438, 119, 0 ), // #1195
  INST(Vpmovd2m        , VexRm_Lx           , V(F30F38,39,_,x,_,0,_,_  ), 0                         , 0 , 0 , 7514, 438, 120, 0 ), // #1196
  INST(Vpmovdb         , VexMr_Lx           , V(F30F38,31,_,x,_,0,2,QVM), 0                         , 0 , 0 , 7523, 439, 117, 0 ), // #1197
  INST(Vpmovdw         , VexMr_Lx           , V(F30F38,33,_,x,_,0,3,HVM), 0                         , 0 , 0 , 7531, 440, 117, 0 ), // #1198
  INST(Vpmovm2b        , VexRm_Lx           , V(F30F38,28,_,x,_,0,_,_  ), 0                         , 0 , 0 , 7539, 397, 119, 0 ), // #1199
  INST(Vpmovm2d        , VexRm_Lx           , V(F30F38,38,_,x,_,0,_,_  ), 0                         , 0 , 0 , 7548, 397, 120, 0 ), // #1200
  INST(Vpmovm2q        , VexRm_Lx           , V(F30F38,38,_,x,_,1,_,_  ), 0                         , 0 , 0 , 7557, 397, 120, 0 ), // #1201
  INST(Vpmovm2w        , VexRm_Lx           , V(F30F38,28,_,x,_,1,_,_  ), 0                         , 0 , 0 , 7566, 397, 119, 0 ), // #1202
  INST(Vpmovmskb       , VexRm_Lx           , V(660F00,D7,_,x,I,_,_,_  ), 0                         , 0 , 0 , 7575, 377, 132, 60), // #1203
  INST(Vpmovq2m        , VexRm_Lx           , V(F30F38,39,_,x,_,1,_,_  ), 0                         , 0 , 0 , 7585, 438, 120, 0 ), // #1204
  INST(Vpmovqb         , VexMr_Lx           , V(F30F38,32,_,x,_,0,1,OVM), 0                         , 0 , 0 , 7594, 441, 117, 0 ), // #1205
  INST(Vpmovqd         , VexMr_Lx           , V(F30F38,35,_,x,_,0,3,HVM), 0                         , 0 , 0 , 7602, 440, 117, 0 ), // #1206
  INST(Vpmovqw         , VexMr_Lx           , V(F30F38,34,_,x,_,0,2,QVM), 0                         , 0 , 0 , 7610, 439, 117, 0 ), // #1207
  INST(Vpmovsdb        , VexMr_Lx           , V(F30F38,21,_,x,_,0,2,QVM), 0                         , 0 , 0 , 7618, 439, 117, 0 ), // #1208
  INST(Vpmovsdw        , VexMr_Lx           , V(F30F38,23,_,x,_,0,3,HVM), 0                         , 0 , 0 , 7627, 440, 117, 0 ), // #1209
  INST(Vpmovsqb        , VexMr_Lx           , V(F30F38,22,_,x,_,0,1,OVM), 0                         , 0 , 0 , 7636, 441, 117, 0 ), // #1210
  INST(Vpmovsqd        , VexMr_Lx           , V(F30F38,25,_,x,_,0,3,HVM), 0                         , 0 , 0 , 7645, 440, 117, 0 ), // #1211
  INST(Vpmovsqw        , VexMr_Lx           , V(F30F38,24,_,x,_,0,2,QVM), 0                         , 0 , 0 , 7654, 439, 117, 0 ), // #1212
  INST(Vpmovswb        , VexMr_Lx           , V(F30F38,20,_,x,_,0,3,HVM), 0                         , 0 , 0 , 7663, 440, 119, 0 ), // #1213
  INST(Vpmovsxbd       , VexRm_Lx           , V(660F38,21,_,x,I,I,2,QVM), 0                         , 0 , 0 , 7672, 442, 122, 61), // #1214
  INST(Vpmovsxbq       , VexRm_Lx           , V(660F38,22,_,x,I,I,1,OVM), 0                         , 0 , 0 , 7682, 443, 122, 61), // #1215
  INST(Vpmovsxbw       , VexRm_Lx           , V(660F38,20,_,x,I,I,3,HVM), 0                         , 0 , 0 , 7692, 444, 134, 61), // #1216
  INST(Vpmovsxdq       , VexRm_Lx           , V(660F38,25,_,x,I,0,3,HVM), 0                         , 0 , 0 , 7702, 445, 122, 61), // #1217
  INST(Vpmovsxwd       , VexRm_Lx           , V(660F38,23,_,x,I,I,3,HVM), 0                         , 0 , 0 , 7712, 444, 122, 61), // #1218
  INST(Vpmovsxwq       , VexRm_Lx           , V(660F38,24,_,x,I,I,2,QVM), 0                         , 0 , 0 , 7722, 442, 122, 61), // #1219
  INST(Vpmovusdb       , VexMr_Lx           , V(F30F38,11,_,x,_,0,2,QVM), 0                         , 0 , 0 , 7732, 439, 117, 0 ), // #1220
  INST(Vpmovusdw       , VexMr_Lx           , V(F30F38,13,_,x,_,0,3,HVM), 0                         , 0 , 0 , 7742, 440, 117, 0 ), // #1221
  INST(Vpmovusqb       , VexMr_Lx           , V(F30F38,12,_,x,_,0,1,OVM), 0                         , 0 , 0 , 7752, 441, 117, 0 ), // #1222
  INST(Vpmovusqd       , VexMr_Lx           , V(F30F38,15,_,x,_,0,3,HVM), 0                         , 0 , 0 , 7762, 440, 117, 0 ), // #1223
  INST(Vpmovusqw       , VexMr_Lx           , V(F30F38,14,_,x,_,0,2,QVM), 0                         , 0 , 0 , 7772, 439, 117, 0 ), // #1224
  INST(Vpmovuswb       , VexMr_Lx           , V(F30F38,10,_,x,_,0,3,HVM), 0                         , 0 , 0 , 7782, 440, 119, 0 ), // #1225
  INST(Vpmovw2m        , VexRm_Lx           , V(F30F38,29,_,x,_,1,_,_  ), 0                         , 0 , 0 , 7792, 438, 119, 0 ), // #1226
  INST(Vpmovwb         , VexMr_Lx           , V(F30F38,30,_,x,_,0,3,HVM), 0                         , 0 , 0 , 7801, 440, 119, 0 ), // #1227
  INST(Vpmovzxbd       , VexRm_Lx           , V(660F38,31,_,x,I,I,2,QVM), 0                         , 0 , 0 , 7809, 442, 122, 18), // #1228
  INST(Vpmovzxbq       , VexRm_Lx           , V(660F38,32,_,x,I,I,1,OVM), 0                         , 0 , 0 , 7819, 443, 122, 18), // #1229
  INST(Vpmovzxbw       , VexRm_Lx           , V(660F38,30,_,x,I,I,3,HVM), 0                         , 0 , 0 , 7829, 444, 134, 18), // #1230
  INST(Vpmovzxdq       , VexRm_Lx           , V(660F38,35,_,x,I,0,3,HVM), 0                         , 0 , 0 , 7839, 445, 122, 18), // #1231
  INST(Vpmovzxwd       , VexRm_Lx           , V(660F38,33,_,x,I,I,3,HVM), 0                         , 0 , 0 , 7849, 444, 122, 18), // #1232
  INST(Vpmovzxwq       , VexRm_Lx           , V(660F38,34,_,x,I,I,2,QVM), 0                         , 0 , 0 , 7859, 442, 122, 18), // #1233
  INST(Vpmuldq         , VexRvm_Lx          , V(660F38,28,_,x,I,1,4,FV ), 0                         , 0 , 0 , 7869, 257, 122, 62), // #1234
  INST(Vpmulhrsw       , VexRvm_Lx          , V(660F38,0B,_,x,I,I,4,FVM), 0                         , 0 , 0 , 7877, 387, 134, 62), // #1235
  INST(Vpmulhuw        , VexRvm_Lx          , V(660F00,E4,_,x,I,I,4,FVM), 0                         , 0 , 0 , 7887, 387, 134, 63), // #1236
  INST(Vpmulhw         , VexRvm_Lx          , V(660F00,E5,_,x,I,I,4,FVM), 0                         , 0 , 0 , 7896, 387, 134, 63), // #1237
  INST(Vpmulld         , VexRvm_Lx          , V(660F38,40,_,x,I,0,4,FV ), 0                         , 0 , 0 , 7904, 258, 122, 63), // #1238
  INST(Vpmullq         , VexRvm_Lx          , V(660F38,40,_,x,_,1,4,FV ), 0                         , 0 , 0 , 7912, 263, 120, 0 ), // #1239
  INST(Vpmullw         , VexRvm_Lx          , V(660F00,D5,_,x,I,I,4,FVM), 0                         , 0 , 0 , 7920, 387, 134, 62), // #1240
  INST(Vpmultishiftqb  , VexRvm_Lx          , V(660F38,83,_,x,_,1,4,FV ), 0                         , 0 , 0 , 7928, 263, 139, 0 ), // #1241
  INST(Vpmuludq        , VexRvm_Lx          , V(660F00,F4,_,x,I,1,4,FV ), 0                         , 0 , 0 , 7943, 257, 122, 64), // #1242
  INST(Vpopcntd        , VexRm              , V(660F38,55,_,2,_,0,4,FVM), 0                         , 0 , 0 , 7952, 446, 143, 0 ), // #1243
  INST(Vpopcntq        , VexRm              , V(660F38,55,_,2,_,1,4,FVM), 0                         , 0 , 0 , 7961, 447, 143, 0 ), // #1244
  INST(Vpor            , VexRvm_Lx          , V(660F00,EB,_,x,I,_,_,_  ), 0                         , 0 , 0 , 7970, 389, 132, 65), // #1245
  INST(Vpord           , VexRvm_Lx          , V(660F00,EB,_,x,_,0,4,FV ), 0                         , 0 , 0 , 7975, 390, 117, 0 ), // #1246
  INST(Vporq           , VexRvm_Lx          , V(660F00,EB,_,x,_,1,4,FV ), 0                         , 0 , 0 , 7981, 394, 117, 0 ), // #1247
  INST(Vpperm          , VexRvrmRvmr        , V(XOP_M8,A3,_,0,x,_,_,_  ), 0                         , 0 , 0 , 7987, 448, 129, 0 ), // #1248
  INST(Vprold          , VexVmi_Lx          , V(660F00,72,1,x,_,0,4,FV ), 0                         , 0 , 0 , 7994, 449, 117, 0 ), // #1249
  INST(Vprolq          , VexVmi_Lx          , V(660F00,72,1,x,_,1,4,FV ), 0                         , 0 , 0 , 8001, 450, 117, 0 ), // #1250
  INST(Vprolvd         , VexRvm_Lx          , V(660F38,15,_,x,_,0,4,FV ), 0                         , 0 , 0 , 8008, 262, 117, 0 ), // #1251
  INST(Vprolvq         , VexRvm_Lx          , V(660F38,15,_,x,_,1,4,FV ), 0                         , 0 , 0 , 8016, 263, 117, 0 ), // #1252
  INST(Vprord          , VexVmi_Lx          , V(660F00,72,0,x,_,0,4,FV ), 0                         , 0 , 0 , 8024, 449, 117, 0 ), // #1253
  INST(Vprorq          , VexVmi_Lx          , V(660F00,72,0,x,_,1,4,FV ), 0                         , 0 , 0 , 8031, 450, 117, 0 ), // #1254
  INST(Vprorvd         , VexRvm_Lx          , V(660F38,14,_,x,_,0,4,FV ), 0                         , 0 , 0 , 8038, 262, 117, 0 ), // #1255
  INST(Vprorvq         , VexRvm_Lx          , V(660F38,14,_,x,_,1,4,FV ), 0                         , 0 , 0 , 8046, 263, 117, 0 ), // #1256
  INST(Vprotb          , VexRvmRmvRmi       , V(XOP_M9,90,_,0,x,_,_,_  ), V(XOP_M8,C0,_,0,x,_,_,_  ), 0 , 0 , 8054, 451, 129, 0 ), // #1257
  INST(Vprotd          , VexRvmRmvRmi       , V(XOP_M9,92,_,0,x,_,_,_  ), V(XOP_M8,C2,_,0,x,_,_,_  ), 0 , 0 , 8061, 452, 129, 0 ), // #1258
  INST(Vprotq          , VexRvmRmvRmi       , V(XOP_M9,93,_,0,x,_,_,_  ), V(XOP_M8,C3,_,0,x,_,_,_  ), 0 , 0 , 8068, 453, 129, 0 ), // #1259
  INST(Vprotw          , VexRvmRmvRmi       , V(XOP_M9,91,_,0,x,_,_,_  ), V(XOP_M8,C1,_,0,x,_,_,_  ), 0 , 0 , 8075, 454, 129, 0 ), // #1260
  INST(Vpsadbw         , VexRvm_Lx          , V(660F00,F6,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8082, 455, 134, 66), // #1261
  INST(Vpscatterdd     , VexMr_VM           , V(660F38,A0,_,x,_,0,2,T1S), 0                         , 0 , 0 , 8090, 456, 117, 0 ), // #1262
  INST(Vpscatterdq     , VexMr_VM           , V(660F38,A0,_,x,_,1,3,T1S), 0                         , 0 , 0 , 8102, 456, 117, 0 ), // #1263
  INST(Vpscatterqd     , VexMr_VM           , V(660F38,A1,_,x,_,0,2,T1S), 0                         , 0 , 0 , 8114, 457, 117, 0 ), // #1264
  INST(Vpscatterqq     , VexMr_VM           , V(660F38,A1,_,x,_,1,3,T1S), 0                         , 0 , 0 , 8126, 458, 117, 0 ), // #1265
  INST(Vpshab          , VexRvmRmv          , V(XOP_M9,98,_,0,x,_,_,_  ), 0                         , 0 , 0 , 8138, 459, 129, 0 ), // #1266
  INST(Vpshad          , VexRvmRmv          , V(XOP_M9,9A,_,0,x,_,_,_  ), 0                         , 0 , 0 , 8145, 459, 129, 0 ), // #1267
  INST(Vpshaq          , VexRvmRmv          , V(XOP_M9,9B,_,0,x,_,_,_  ), 0                         , 0 , 0 , 8152, 459, 129, 0 ), // #1268
  INST(Vpshaw          , VexRvmRmv          , V(XOP_M9,99,_,0,x,_,_,_  ), 0                         , 0 , 0 , 8159, 459, 129, 0 ), // #1269
  INST(Vpshlb          , VexRvmRmv          , V(XOP_M9,94,_,0,x,_,_,_  ), 0                         , 0 , 0 , 8166, 459, 129, 0 ), // #1270
  INST(Vpshld          , VexRvmRmv          , V(XOP_M9,96,_,0,x,_,_,_  ), 0                         , 0 , 0 , 8173, 459, 129, 0 ), // #1271
  INST(Vpshlq          , VexRvmRmv          , V(XOP_M9,97,_,0,x,_,_,_  ), 0                         , 0 , 0 , 8180, 459, 129, 0 ), // #1272
  INST(Vpshlw          , VexRvmRmv          , V(XOP_M9,95,_,0,x,_,_,_  ), 0                         , 0 , 0 , 8187, 459, 129, 0 ), // #1273
  INST(Vpshufb         , VexRvm_Lx          , V(660F38,00,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8194, 387, 134, 67), // #1274
  INST(Vpshufd         , VexRmi_Lx          , V(660F00,70,_,x,I,0,4,FV ), 0                         , 0 , 0 , 8202, 460, 122, 20), // #1275
  INST(Vpshufhw        , VexRmi_Lx          , V(F30F00,70,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8210, 461, 134, 20), // #1276
  INST(Vpshuflw        , VexRmi_Lx          , V(F20F00,70,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8219, 461, 134, 20), // #1277
  INST(Vpsignb         , VexRvm_Lx          , V(660F38,08,_,x,I,_,_,_  ), 0                         , 0 , 0 , 8228, 251, 132, 68), // #1278
  INST(Vpsignd         , VexRvm_Lx          , V(660F38,0A,_,x,I,_,_,_  ), 0                         , 0 , 0 , 8236, 251, 132, 68), // #1279
  INST(Vpsignw         , VexRvm_Lx          , V(660F38,09,_,x,I,_,_,_  ), 0                         , 0 , 0 , 8244, 251, 132, 68), // #1280
  INST(Vpslld          , VexRvmVmi_Lx       , V(660F00,F2,_,x,I,0,4,128), V(660F00,72,6,x,I,0,4,FV ), 0 , 0 , 8252, 462, 122, 68), // #1281
  INST(Vpslldq         , VexEvexVmi_Lx      , V(660F00,73,7,x,I,I,4,FVM), 0                         , 0 , 0 , 8259, 463, 134, 68), // #1282
  INST(Vpsllq          , VexRvmVmi_Lx       , V(660F00,F3,_,x,I,1,4,128), V(660F00,73,6,x,I,1,4,FV ), 0 , 0 , 8267, 464, 122, 68), // #1283
  INST(Vpsllvd         , VexRvm_Lx          , V(660F38,47,_,x,0,0,4,FV ), 0                         , 0 , 0 , 8274, 258, 130, 0 ), // #1284
  INST(Vpsllvq         , VexRvm_Lx          , V(660F38,47,_,x,1,1,4,FV ), 0                         , 0 , 0 , 8282, 257, 130, 0 ), // #1285
  INST(Vpsllvw         , VexRvm_Lx          , V(660F38,12,_,x,_,1,4,FVM), 0                         , 0 , 0 , 8290, 261, 119, 0 ), // #1286
  INST(Vpsllw          , VexRvmVmi_Lx       , V(660F00,F1,_,x,I,I,4,FVM), V(660F00,71,6,x,I,I,4,FVM), 0 , 0 , 8298, 465, 134, 69), // #1287
  INST(Vpsrad          , VexRvmVmi_Lx       , V(660F00,E2,_,x,I,0,4,128), V(660F00,72,4,x,I,0,4,FV ), 0 , 0 , 8305, 466, 122, 69), // #1288
  INST(Vpsraq          , VexRvmVmi_Lx       , V(660F00,E2,_,x,_,1,4,128), V(660F00,72,4,x,_,1,4,FV ), 0 , 0 , 8312, 467, 117, 0 ), // #1289
  INST(Vpsravd         , VexRvm_Lx          , V(660F38,46,_,x,0,0,4,FV ), 0                         , 0 , 0 , 8319, 258, 130, 0 ), // #1290
  INST(Vpsravq         , VexRvm_Lx          , V(660F38,46,_,x,_,1,4,FV ), 0                         , 0 , 0 , 8327, 263, 117, 0 ), // #1291
  INST(Vpsravw         , VexRvm_Lx          , V(660F38,11,_,x,_,1,4,FVM), 0                         , 0 , 0 , 8335, 261, 119, 0 ), // #1292
  INST(Vpsraw          , VexRvmVmi_Lx       , V(660F00,E1,_,x,I,I,4,128), V(660F00,71,4,x,I,I,4,FVM), 0 , 0 , 8343, 468, 134, 70), // #1293
  INST(Vpsrld          , VexRvmVmi_Lx       , V(660F00,D2,_,x,I,0,4,128), V(660F00,72,2,x,I,0,4,FV ), 0 , 0 , 8350, 469, 122, 70), // #1294
  INST(Vpsrldq         , VexEvexVmi_Lx      , V(660F00,73,3,x,I,I,4,FVM), 0                         , 0 , 0 , 8357, 463, 134, 70), // #1295
  INST(Vpsrlq          , VexRvmVmi_Lx       , V(660F00,D3,_,x,I,1,4,128), V(660F00,73,2,x,I,1,4,FV ), 0 , 0 , 8365, 470, 122, 70), // #1296
  INST(Vpsrlvd         , VexRvm_Lx          , V(660F38,45,_,x,0,0,4,FV ), 0                         , 0 , 0 , 8372, 258, 130, 0 ), // #1297
  INST(Vpsrlvq         , VexRvm_Lx          , V(660F38,45,_,x,1,1,4,FV ), 0                         , 0 , 0 , 8380, 257, 130, 0 ), // #1298
  INST(Vpsrlvw         , VexRvm_Lx          , V(660F38,10,_,x,_,1,4,FVM), 0                         , 0 , 0 , 8388, 261, 119, 0 ), // #1299
  INST(Vpsrlw          , VexRvmVmi_Lx       , V(660F00,D1,_,x,I,I,4,128), V(660F00,71,2,x,I,I,4,FVM), 0 , 0 , 8396, 471, 134, 71), // #1300
  INST(Vpsubb          , VexRvm_Lx          , V(660F00,F8,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8403, 472, 134, 71), // #1301
  INST(Vpsubd          , VexRvm_Lx          , V(660F00,FA,_,x,I,0,4,FV ), 0                         , 0 , 0 , 8410, 473, 122, 71), // #1302
  INST(Vpsubq          , VexRvm_Lx          , V(660F00,FB,_,x,I,1,4,FV ), 0                         , 0 , 0 , 8417, 474, 122, 71), // #1303
  INST(Vpsubsb         , VexRvm_Lx          , V(660F00,E8,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8424, 472, 134, 71), // #1304
  INST(Vpsubsw         , VexRvm_Lx          , V(660F00,E9,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8432, 472, 134, 71), // #1305
  INST(Vpsubusb        , VexRvm_Lx          , V(660F00,D8,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8440, 472, 134, 71), // #1306
  INST(Vpsubusw        , VexRvm_Lx          , V(660F00,D9,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8449, 472, 134, 71), // #1307
  INST(Vpsubw          , VexRvm_Lx          , V(660F00,F9,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8458, 472, 134, 71), // #1308
  INST(Vpternlogd      , VexRvmi_Lx         , V(660F3A,25,_,x,_,0,4,FV ), 0                         , 0 , 0 , 8465, 475, 117, 0 ), // #1309
  INST(Vpternlogq      , VexRvmi_Lx         , V(660F3A,25,_,x,_,1,4,FV ), 0                         , 0 , 0 , 8476, 476, 117, 0 ), // #1310
  INST(Vptest          , VexRm_Lx           , V(660F38,17,_,x,I,_,_,_  ), 0                         , 0 , 0 , 8487, 477, 138, 72), // #1311
  INST(Vptestmb        , VexRvm_Lx          , V(660F38,26,_,x,_,0,4,FVM), 0                         , 0 , 0 , 8494, 478, 119, 0 ), // #1312
  INST(Vptestmd        , VexRvm_Lx          , V(660F38,27,_,x,_,0,4,FV ), 0                         , 0 , 0 , 8503, 479, 117, 0 ), // #1313
  INST(Vptestmq        , VexRvm_Lx          , V(660F38,27,_,x,_,1,4,FV ), 0                         , 0 , 0 , 8512, 480, 117, 0 ), // #1314
  INST(Vptestmw        , VexRvm_Lx          , V(660F38,26,_,x,_,1,4,FVM), 0                         , 0 , 0 , 8521, 478, 119, 0 ), // #1315
  INST(Vptestnmb       , VexRvm_Lx          , V(F30F38,26,_,x,_,0,4,FVM), 0                         , 0 , 0 , 8530, 478, 119, 0 ), // #1316
  INST(Vptestnmd       , VexRvm_Lx          , V(F30F38,27,_,x,_,0,4,FV ), 0                         , 0 , 0 , 8540, 479, 117, 0 ), // #1317
  INST(Vptestnmq       , VexRvm_Lx          , V(F30F38,27,_,x,_,1,4,FV ), 0                         , 0 , 0 , 8550, 480, 117, 0 ), // #1318
  INST(Vptestnmw       , VexRvm_Lx          , V(F30F38,26,_,x,_,1,4,FVM), 0                         , 0 , 0 , 8560, 478, 119, 0 ), // #1319
  INST(Vpunpckhbw      , VexRvm_Lx          , V(660F00,68,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8570, 387, 134, 73), // #1320
  INST(Vpunpckhdq      , VexRvm_Lx          , V(660F00,6A,_,x,I,0,4,FV ), 0                         , 0 , 0 , 8581, 258, 122, 73), // #1321
  INST(Vpunpckhqdq     , VexRvm_Lx          , V(660F00,6D,_,x,I,1,4,FV ), 0                         , 0 , 0 , 8592, 257, 122, 73), // #1322
  INST(Vpunpckhwd      , VexRvm_Lx          , V(660F00,69,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8604, 387, 134, 73), // #1323
  INST(Vpunpcklbw      , VexRvm_Lx          , V(660F00,60,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8615, 387, 134, 73), // #1324
  INST(Vpunpckldq      , VexRvm_Lx          , V(660F00,62,_,x,I,0,4,FV ), 0                         , 0 , 0 , 8626, 258, 122, 73), // #1325
  INST(Vpunpcklqdq     , VexRvm_Lx          , V(660F00,6C,_,x,I,1,4,FV ), 0                         , 0 , 0 , 8637, 257, 122, 73), // #1326
  INST(Vpunpcklwd      , VexRvm_Lx          , V(660F00,61,_,x,I,I,4,FVM), 0                         , 0 , 0 , 8649, 387, 134, 73), // #1327
  INST(Vpxor           , VexRvm_Lx          , V(660F00,EF,_,x,I,_,_,_  ), 0                         , 0 , 0 , 8660, 391, 132, 22), // #1328
  INST(Vpxord          , VexRvm_Lx          , V(660F00,EF,_,x,_,0,4,FV ), 0                         , 0 , 0 , 8666, 392, 117, 0 ), // #1329
  INST(Vpxorq          , VexRvm_Lx          , V(660F00,EF,_,x,_,1,4,FV ), 0                         , 0 , 0 , 8673, 393, 117, 0 ), // #1330
  INST(Vrangepd        , VexRvmi_Lx         , V(660F3A,50,_,x,_,1,4,FV ), 0                         , 0 , 0 , 8680, 481, 120, 0 ), // #1331
  INST(Vrangeps        , VexRvmi_Lx         , V(660F3A,50,_,x,_,0,4,FV ), 0                         , 0 , 0 , 8689, 482, 120, 0 ), // #1332
  INST(Vrangesd        , VexRvmi            , V(660F3A,51,_,I,_,1,3,T1S), 0                         , 0 , 0 , 8698, 346, 64 , 0 ), // #1333
  INST(Vrangess        , VexRvmi            , V(660F3A,51,_,I,_,0,2,T1S), 0                         , 0 , 0 , 8707, 347, 64 , 0 ), // #1334
  INST(Vrcp14pd        , VexRm_Lx           , V(660F38,4C,_,x,_,1,4,FV ), 0                         , 0 , 0 , 8716, 433, 117, 0 ), // #1335
  INST(Vrcp14ps        , VexRm_Lx           , V(660F38,4C,_,x,_,0,4,FV ), 0                         , 0 , 0 , 8725, 410, 117, 0 ), // #1336
  INST(Vrcp14sd        , VexRvm             , V(660F38,4D,_,I,_,1,3,T1S), 0                         , 0 , 0 , 8734, 483, 66 , 0 ), // #1337
  INST(Vrcp14ss        , VexRvm             , V(660F38,4D,_,I,_,0,2,T1S), 0                         , 0 , 0 , 8743, 484, 66 , 0 ), // #1338
  INST(Vrcp28pd        , VexRm              , V(660F38,CA,_,2,_,1,4,FV ), 0                         , 0 , 0 , 8752, 310, 125, 0 ), // #1339
  INST(Vrcp28ps        , VexRm              , V(660F38,CA,_,2,_,0,4,FV ), 0                         , 0 , 0 , 8761, 311, 125, 0 ), // #1340
  INST(Vrcp28sd        , VexRvm             , V(660F38,CB,_,I,_,1,3,T1S), 0                         , 0 , 0 , 8770, 342, 125, 0 ), // #1341
  INST(Vrcp28ss        , VexRvm             , V(660F38,CB,_,I,_,0,2,T1S), 0                         , 0 , 0 , 8779, 343, 125, 0 ), // #1342
  INST(Vrcpps          , VexRm_Lx           , V(000F00,53,_,x,I,_,_,_  ), 0                         , 0 , 0 , 8788, 332, 115, 74), // #1343
  INST(Vrcpss          , VexRvm             , V(F30F00,53,_,I,I,_,_,_  ), 0                         , 0 , 0 , 8795, 485, 115, 75), // #1344
  INST(Vreducepd       , VexRmi_Lx          , V(660F3A,56,_,x,_,1,4,FV ), 0                         , 0 , 0 , 8802, 450, 120, 0 ), // #1345
  INST(Vreduceps       , VexRmi_Lx          , V(660F3A,56,_,x,_,0,4,FV ), 0                         , 0 , 0 , 8812, 449, 120, 0 ), // #1346
  INST(Vreducesd       , VexRvmi            , V(660F3A,57,_,I,_,1,3,T1S), 0                         , 0 , 0 , 8822, 486, 64 , 0 ), // #1347
  INST(Vreducess       , VexRvmi            , V(660F3A,57,_,I,_,0,2,T1S), 0                         , 0 , 0 , 8832, 487, 64 , 0 ), // #1348
  INST(Vrndscalepd     , VexRmi_Lx          , V(660F3A,09,_,x,_,1,4,FV ), 0                         , 0 , 0 , 8842, 344, 117, 0 ), // #1349
  INST(Vrndscaleps     , VexRmi_Lx          , V(660F3A,08,_,x,_,0,4,FV ), 0                         , 0 , 0 , 8854, 345, 117, 0 ), // #1350
  INST(Vrndscalesd     , VexRvmi            , V(660F3A,0B,_,I,_,1,3,T1S), 0                         , 0 , 0 , 8866, 346, 66 , 0 ), // #1351
  INST(Vrndscaless     , VexRvmi            , V(660F3A,0A,_,I,_,0,2,T1S), 0                         , 0 , 0 , 8878, 347, 66 , 0 ), // #1352
  INST(Vroundpd        , VexRmi_Lx          , V(660F3A,09,_,x,I,_,_,_  ), 0                         , 0 , 0 , 8890, 488, 115, 76), // #1353
  INST(Vroundps        , VexRmi_Lx          , V(660F3A,08,_,x,I,_,_,_  ), 0                         , 0 , 0 , 8899, 488, 115, 76), // #1354
  INST(Vroundsd        , VexRvmi            , V(660F3A,0B,_,I,I,_,_,_  ), 0                         , 0 , 0 , 8908, 489, 115, 77), // #1355
  INST(Vroundss        , VexRvmi            , V(660F3A,0A,_,I,I,_,_,_  ), 0                         , 0 , 0 , 8917, 490, 115, 77), // #1356
  INST(Vrsqrt14pd      , VexRm_Lx           , V(660F38,4E,_,x,_,1,4,FV ), 0                         , 0 , 0 , 8926, 433, 117, 0 ), // #1357
  INST(Vrsqrt14ps      , VexRm_Lx           , V(660F38,4E,_,x,_,0,4,FV ), 0                         , 0 , 0 , 8937, 410, 117, 0 ), // #1358
  INST(Vrsqrt14sd      , VexRvm             , V(660F38,4F,_,I,_,1,3,T1S), 0                         , 0 , 0 , 8948, 483, 66 , 0 ), // #1359
  INST(Vrsqrt14ss      , VexRvm             , V(660F38,4F,_,I,_,0,2,T1S), 0                         , 0 , 0 , 8959, 484, 66 , 0 ), // #1360
  INST(Vrsqrt28pd      , VexRm              , V(660F38,CC,_,2,_,1,4,FV ), 0                         , 0 , 0 , 8970, 310, 125, 0 ), // #1361
  INST(Vrsqrt28ps      , VexRm              , V(660F38,CC,_,2,_,0,4,FV ), 0                         , 0 , 0 , 8981, 311, 125, 0 ), // #1362
  INST(Vrsqrt28sd      , VexRvm             , V(660F38,CD,_,I,_,1,3,T1S), 0                         , 0 , 0 , 8992, 342, 125, 0 ), // #1363
  INST(Vrsqrt28ss      , VexRvm             , V(660F38,CD,_,I,_,0,2,T1S), 0                         , 0 , 0 , 9003, 343, 125, 0 ), // #1364
  INST(Vrsqrtps        , VexRm_Lx           , V(000F00,52,_,x,I,_,_,_  ), 0                         , 0 , 0 , 9014, 332, 115, 78), // #1365
  INST(Vrsqrtss        , VexRvm             , V(F30F00,52,_,I,I,_,_,_  ), 0                         , 0 , 0 , 9023, 485, 115, 79), // #1366
  INST(Vscalefpd       , VexRvm_Lx          , V(660F38,2C,_,x,_,1,4,FV ), 0                         , 0 , 0 , 9032, 491, 117, 0 ), // #1367
  INST(Vscalefps       , VexRvm_Lx          , V(660F38,2C,_,x,_,0,4,FV ), 0                         , 0 , 0 , 9042, 492, 117, 0 ), // #1368
  INST(Vscalefsd       , VexRvm             , V(660F38,2D,_,I,_,1,3,T1S), 0                         , 0 , 0 , 9052, 493, 66 , 0 ), // #1369
  INST(Vscalefss       , VexRvm             , V(660F38,2D,_,I,_,0,2,T1S), 0                         , 0 , 0 , 9062, 494, 66 , 0 ), // #1370
  INST(Vscatterdpd     , VexMr_Lx           , V(660F38,A2,_,x,_,1,3,T1S), 0                         , 0 , 0 , 9072, 495, 117, 0 ), // #1371
  INST(Vscatterdps     , VexMr_Lx           , V(660F38,A2,_,x,_,0,2,T1S), 0                         , 0 , 0 , 9084, 456, 117, 0 ), // #1372
  INST(Vscatterpf0dpd  , VexM_VM            , V(660F38,C6,5,2,_,1,3,T1S), 0                         , 0 , 0 , 9096, 337, 131, 0 ), // #1373
  INST(Vscatterpf0dps  , VexM_VM            , V(660F38,C6,5,2,_,0,2,T1S), 0                         , 0 , 0 , 9111, 338, 131, 0 ), // #1374
  INST(Vscatterpf0qpd  , VexM_VM            , V(660F38,C7,5,2,_,1,3,T1S), 0                         , 0 , 0 , 9126, 339, 131, 0 ), // #1375
  INST(Vscatterpf0qps  , VexM_VM            , V(660F38,C7,5,2,_,0,2,T1S), 0                         , 0 , 0 , 9141, 339, 131, 0 ), // #1376
  INST(Vscatterpf1dpd  , VexM_VM            , V(660F38,C6,6,2,_,1,3,T1S), 0                         , 0 , 0 , 9156, 337, 131, 0 ), // #1377
  INST(Vscatterpf1dps  , VexM_VM            , V(660F38,C6,6,2,_,0,2,T1S), 0                         , 0 , 0 , 9171, 338, 131, 0 ), // #1378
  INST(Vscatterpf1qpd  , VexM_VM            , V(660F38,C7,6,2,_,1,3,T1S), 0                         , 0 , 0 , 9186, 339, 131, 0 ), // #1379
  INST(Vscatterpf1qps  , VexM_VM            , V(660F38,C7,6,2,_,0,2,T1S), 0                         , 0 , 0 , 9201, 339, 131, 0 ), // #1380
  INST(Vscatterqpd     , VexMr_Lx           , V(660F38,A3,_,x,_,1,3,T1S), 0                         , 0 , 0 , 9216, 458, 117, 0 ), // #1381
  INST(Vscatterqps     , VexMr_Lx           , V(660F38,A3,_,x,_,0,2,T1S), 0                         , 0 , 0 , 9228, 457, 117, 0 ), // #1382
  INST(Vshuff32x4      , VexRvmi_Lx         , V(660F3A,23,_,x,_,0,4,FV ), 0                         , 0 , 0 , 9240, 496, 117, 0 ), // #1383
  INST(Vshuff64x2      , VexRvmi_Lx         , V(660F3A,23,_,x,_,1,4,FV ), 0                         , 0 , 0 , 9251, 497, 117, 0 ), // #1384
  INST(Vshufi32x4      , VexRvmi_Lx         , V(660F3A,43,_,x,_,0,4,FV ), 0                         , 0 , 0 , 9262, 496, 117, 0 ), // #1385
  INST(Vshufi64x2      , VexRvmi_Lx         , V(660F3A,43,_,x,_,1,4,FV ), 0                         , 0 , 0 , 9273, 497, 117, 0 ), // #1386
  INST(Vshufpd         , VexRvmi_Lx         , V(660F00,C6,_,x,I,1,4,FV ), 0                         , 0 , 0 , 9284, 498, 113, 80), // #1387
  INST(Vshufps         , VexRvmi_Lx         , V(000F00,C6,_,x,I,0,4,FV ), 0                         , 0 , 0 , 9292, 499, 113, 80), // #1388
  INST(Vsqrtpd         , VexRm_Lx           , V(660F00,51,_,x,I,1,4,FV ), 0                         , 0 , 0 , 9300, 500, 113, 81), // #1389
  INST(Vsqrtps         , VexRm_Lx           , V(000F00,51,_,x,I,0,4,FV ), 0                         , 0 , 0 , 9308, 281, 113, 81), // #1390
  INST(Vsqrtsd         , VexRvm             , V(F20F00,51,_,I,I,1,3,T1S), 0                         , 0 , 0 , 9316, 249, 114, 82), // #1391
  INST(Vsqrtss         , VexRvm             , V(F30F00,51,_,I,I,0,2,T1S), 0                         , 0 , 0 , 9324, 250, 114, 82), // #1392
  INST(Vstmxcsr        , VexM               , V(000F00,AE,3,0,I,_,_,_  ), 0                         , 0 , 0 , 9332, 501, 115, 0 ), // #1393
  INST(Vsubpd          , VexRvm_Lx          , V(660F00,5C,_,x,I,1,4,FV ), 0                         , 0 , 0 , 9341, 247, 113, 19), // #1394
  INST(Vsubps          , VexRvm_Lx          , V(000F00,5C,_,x,I,0,4,FV ), 0                         , 0 , 0 , 9348, 248, 113, 19), // #1395
  INST(Vsubsd          , VexRvm             , V(F20F00,5C,_,I,I,1,3,T1S), 0                         , 0 , 0 , 9355, 249, 114, 19), // #1396
  INST(Vsubss          , VexRvm             , V(F30F00,5C,_,I,I,0,2,T1S), 0                         , 0 , 0 , 9362, 250, 114, 19), // #1397
  INST(Vtestpd         , VexRm_Lx           , V(660F38,0F,_,x,0,_,_,_  ), 0                         , 0 , 0 , 9369, 477, 138, 0 ), // #1398
  INST(Vtestps         , VexRm_Lx           , V(660F38,0E,_,x,0,_,_,_  ), 0                         , 0 , 0 , 9377, 477, 138, 0 ), // #1399
  INST(Vucomisd        , VexRm              , V(660F00,2E,_,I,I,1,3,T1S), 0                         , 0 , 0 , 9385, 277, 123, 11), // #1400
  INST(Vucomiss        , VexRm              , V(000F00,2E,_,I,I,0,2,T1S), 0                         , 0 , 0 , 9394, 278, 123, 11), // #1401
  INST(Vunpckhpd       , VexRvm_Lx          , V(660F00,15,_,x,I,1,4,FV ), 0                         , 0 , 0 , 9403, 257, 113, 6 ), // #1402
  INST(Vunpckhps       , VexRvm_Lx          , V(000F00,15,_,x,I,0,4,FV ), 0                         , 0 , 0 , 9413, 258, 113, 6 ), // #1403
  INST(Vunpcklpd       , VexRvm_Lx          , V(660F00,14,_,x,I,1,4,FV ), 0                         , 0 , 0 , 9423, 257, 113, 6 ), // #1404
  INST(Vunpcklps       , VexRvm_Lx          , V(000F00,14,_,x,I,0,4,FV ), 0                         , 0 , 0 , 9433, 258, 113, 6 ), // #1405
  INST(Vxorpd          , VexRvm_Lx          , V(660F00,57,_,x,I,1,4,FV ), 0                         , 0 , 0 , 9443, 474, 118, 83), // #1406
  INST(Vxorps          , VexRvm_Lx          , V(000F00,57,_,x,I,0,4,FV ), 0                         , 0 , 0 , 9450, 473, 118, 83), // #1407
  INST(Vzeroall        , VexOp              , V(000F00,77,_,1,I,_,_,_  ), 0                         , 0 , 0 , 9457, 502, 144, 0 ), // #1408
  INST(Vzeroupper      , VexOp              , V(000F00,77,_,0,I,_,_,_  ), 0                         , 0 , 0 , 9466, 502, 144, 0 ), // #1409
  INST(Wbinvd          , X86Op              , O(000F00,09,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9477, 34 , 23 , 0 ), // #1410
  INST(Wrfsbase        , X86M               , O(F30F00,AE,2,_,x,_,_,_  ), 0                         , 0 , 0 , 9484, 503, 101, 0 ), // #1411
  INST(Wrgsbase        , X86M               , O(F30F00,AE,3,_,x,_,_,_  ), 0                         , 0 , 0 , 9493, 503, 101, 0 ), // #1412
  INST(Wrmsr           , X86Op              , O(000F00,30,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9502, 504, 145, 0 ), // #1413
  INST(Xabort          , X86Op_O_I8         , O(000000,C6,7,_,_,_,_,_  ), 0                         , 0 , 0 , 9508, 99 , 146, 0 ), // #1414
  INST(Xadd            , X86Xadd            , O(000F00,C0,_,_,x,_,_,_  ), 0                         , 0 , 0 , 9515, 505, 147, 0 ), // #1415
  INST(Xbegin          , X86JmpRel          , O(000000,C7,7,_,_,_,_,_  ), 0                         , 0 , 0 , 9520, 506, 146, 0 ), // #1416
  INST(Xchg            , X86Xchg            , O(000000,86,_,_,x,_,_,_  ), 0                         , 0 , 0 , 434 , 507, 0  , 0 ), // #1417
  INST(Xend            , X86Op              , O(000F01,D5,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9527, 34 , 146, 0 ), // #1418
  INST(Xgetbv          , X86Op              , O(000F01,D0,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9532, 228, 148, 0 ), // #1419
  INST(Xlatb           , X86Op              , O(000000,D7,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9539, 34 , 45 , 0 ), // #1420
  INST(Xor             , X86Arith           , O(000000,30,6,_,x,_,_,_  ), 0                         , 0 , 0 , 8662, 244, 1  , 0 ), // #1421
  INST(Xorpd           , ExtRm              , O(660F00,57,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9444, 192, 4  , 83), // #1422
  INST(Xorps           , ExtRm              , O(000F00,57,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9451, 192, 5  , 83), // #1423
  INST(Xrstor          , X86M_Only          , O(000F00,AE,5,_,_,_,_,_  ), 0                         , 0 , 0 , 1105, 508, 148, 0 ), // #1424
  INST(Xrstor64        , X86M_Only          , O(000F00,AE,5,_,1,_,_,_  ), 0                         , 0 , 0 , 1113, 509, 148, 0 ), // #1425
  INST(Xrstors         , X86M_Only          , O(000F00,C7,3,_,_,_,_,_  ), 0                         , 0 , 0 , 9545, 508, 149, 0 ), // #1426
  INST(Xrstors64       , X86M_Only          , O(000F00,C7,3,_,1,_,_,_  ), 0                         , 0 , 0 , 9553, 509, 149, 0 ), // #1427
  INST(Xsave           , X86M_Only          , O(000F00,AE,4,_,_,_,_,_  ), 0                         , 0 , 0 , 1123, 510, 148, 0 ), // #1428
  INST(Xsave64         , X86M_Only          , O(000F00,AE,4,_,1,_,_,_  ), 0                         , 0 , 0 , 1130, 511, 148, 0 ), // #1429
  INST(Xsavec          , X86M_Only          , O(000F00,C7,4,_,_,_,_,_  ), 0                         , 0 , 0 , 9563, 510, 150, 0 ), // #1430
  INST(Xsavec64        , X86M_Only          , O(000F00,C7,4,_,1,_,_,_  ), 0                         , 0 , 0 , 9570, 511, 150, 0 ), // #1431
  INST(Xsaveopt        , X86M_Only          , O(000F00,AE,6,_,_,_,_,_  ), 0                         , 0 , 0 , 9579, 510, 151, 0 ), // #1432
  INST(Xsaveopt64      , X86M_Only          , O(000F00,AE,6,_,1,_,_,_  ), 0                         , 0 , 0 , 9588, 511, 151, 0 ), // #1433
  INST(Xsaves          , X86M_Only          , O(000F00,C7,5,_,_,_,_,_  ), 0                         , 0 , 0 , 9599, 510, 149, 0 ), // #1434
  INST(Xsaves64        , X86M_Only          , O(000F00,C7,5,_,1,_,_,_  ), 0                         , 0 , 0 , 9606, 511, 149, 0 ), // #1435
  INST(Xsetbv          , X86Op              , O(000F01,D1,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9615, 504, 152, 0 ), // #1436
  INST(Xtest           , X86Op              , O(000F01,D6,_,_,_,_,_,_  ), 0                         , 0 , 0 , 9622, 34 , 153, 0 )  // #1437
  // ${instData:End}
};
#undef NAME_DATA_INDEX
#undef INST

// ${altOpCodeData:Begin}
// ------------------- Automatically generated, do not edit -------------------
const uint32_t X86InstDB::altOpCodeData[] = {
  0                         , // #0 [ref=1298x]
  O(660F00,1B,_,_,_,_,_,_  ), // #1 [ref=1x]
  O(000F00,BA,4,_,x,_,_,_  ), // #2 [ref=1x]
  O(000F00,BA,7,_,x,_,_,_  ), // #3 [ref=1x]
  O(000F00,BA,6,_,x,_,_,_  ), // #4 [ref=1x]
  O(000F00,BA,5,_,x,_,_,_  ), // #5 [ref=1x]
  O(000000,48,_,_,x,_,_,_  ), // #6 [ref=1x]
  O(660F00,78,0,_,_,_,_,_  ), // #7 [ref=1x]
  O_FPU(00,00DF,5)          , // #8 [ref=1x]
  O_FPU(00,00DF,7)          , // #9 [ref=1x]
  O_FPU(00,00DD,1)          , // #10 [ref=1x]
  O_FPU(00,00DB,5)          , // #11 [ref=1x]
  O_FPU(00,DFE0,_)          , // #12 [ref=1x]
  O(000000,DB,7,_,_,_,_,_  ), // #13 [ref=1x]
  O_FPU(9B,DFE0,_)          , // #14 [ref=1x]
  O(000000,E4,_,_,_,_,_,_  ), // #15 [ref=1x]
  O(000000,40,_,_,x,_,_,_  ), // #16 [ref=1x]
  O(F20F00,78,_,_,_,_,_,_  ), // #17 [ref=1x]
  O(000000,77,_,_,_,_,_,_  ), // #18 [ref=2x]
  O(000000,73,_,_,_,_,_,_  ), // #19 [ref=3x]
  O(000000,72,_,_,_,_,_,_  ), // #20 [ref=3x]
  O(000000,76,_,_,_,_,_,_  ), // #21 [ref=2x]
  O(000000,74,_,_,_,_,_,_  ), // #22 [ref=2x]
  O(000000,E3,_,_,_,_,_,_  ), // #23 [ref=1x]
  O(000000,7F,_,_,_,_,_,_  ), // #24 [ref=2x]
  O(000000,7D,_,_,_,_,_,_  ), // #25 [ref=2x]
  O(000000,7C,_,_,_,_,_,_  ), // #26 [ref=2x]
  O(000000,7E,_,_,_,_,_,_  ), // #27 [ref=2x]
  O(000000,EB,_,_,_,_,_,_  ), // #28 [ref=1x]
  O(000000,75,_,_,_,_,_,_  ), // #29 [ref=2x]
  O(000000,71,_,_,_,_,_,_  ), // #30 [ref=1x]
  O(000000,7B,_,_,_,_,_,_  ), // #31 [ref=2x]
  O(000000,79,_,_,_,_,_,_  ), // #32 [ref=1x]
  O(000000,70,_,_,_,_,_,_  ), // #33 [ref=1x]
  O(000000,7A,_,_,_,_,_,_  ), // #34 [ref=2x]
  O(000000,78,_,_,_,_,_,_  ), // #35 [ref=1x]
  V(660F00,92,_,0,0,_,_,_  ), // #36 [ref=1x]
  V(F20F00,92,_,0,0,_,_,_  ), // #37 [ref=1x]
  V(F20F00,92,_,0,1,_,_,_  ), // #38 [ref=1x]
  V(000F00,92,_,0,0,_,_,_  ), // #39 [ref=1x]
  O(000000,E2,_,_,_,_,_,_  ), // #40 [ref=1x]
  O(000000,E1,_,_,_,_,_,_  ), // #41 [ref=1x]
  O(000000,E0,_,_,_,_,_,_  ), // #42 [ref=1x]
  O(660F00,29,_,_,_,_,_,_  ), // #43 [ref=1x]
  O(000F00,29,_,_,_,_,_,_  ), // #44 [ref=1x]
  O(000F38,F1,_,_,x,_,_,_  ), // #45 [ref=1x]
  O(000F00,7E,_,_,_,_,_,_  ), // #46 [ref=1x]
  O(660F00,7F,_,_,_,_,_,_  ), // #47 [ref=1x]
  O(F30F00,7F,_,_,_,_,_,_  ), // #48 [ref=1x]
  O(660F00,17,_,_,_,_,_,_  ), // #49 [ref=1x]
  O(000F00,17,_,_,_,_,_,_  ), // #50 [ref=1x]
  O(660F00,13,_,_,_,_,_,_  ), // #51 [ref=1x]
  O(000F00,13,_,_,_,_,_,_  ), // #52 [ref=1x]
  O(660F00,E7,_,_,_,_,_,_  ), // #53 [ref=1x]
  O(660F00,2B,_,_,_,_,_,_  ), // #54 [ref=1x]
  O(000F00,2B,_,_,_,_,_,_  ), // #55 [ref=1x]
  O(000F00,E7,_,_,_,_,_,_  ), // #56 [ref=1x]
  O(F20F00,2B,_,_,_,_,_,_  ), // #57 [ref=1x]
  O(F30F00,2B,_,_,_,_,_,_  ), // #58 [ref=1x]
  O(000F00,7E,_,_,x,_,_,_  ), // #59 [ref=1x]
  O(F20F00,11,_,_,_,_,_,_  ), // #60 [ref=1x]
  O(F30F00,11,_,_,_,_,_,_  ), // #61 [ref=1x]
  O(660F00,11,_,_,_,_,_,_  ), // #62 [ref=1x]
  O(000F00,11,_,_,_,_,_,_  ), // #63 [ref=1x]
  O(000000,E6,_,_,_,_,_,_  ), // #64 [ref=1x]
  O(000F3A,15,_,_,_,_,_,_  ), // #65 [ref=1x]
  O(000000,58,_,_,_,_,_,_  ), // #66 [ref=1x]
  O(000F00,72,6,_,_,_,_,_  ), // #67 [ref=1x]
  O(660F00,73,7,_,_,_,_,_  ), // #68 [ref=1x]
  O(000F00,73,6,_,_,_,_,_  ), // #69 [ref=1x]
  O(000F00,71,6,_,_,_,_,_  ), // #70 [ref=1x]
  O(000F00,72,4,_,_,_,_,_  ), // #71 [ref=1x]
  O(000F00,71,4,_,_,_,_,_  ), // #72 [ref=1x]
  O(000F00,72,2,_,_,_,_,_  ), // #73 [ref=1x]
  O(660F00,73,3,_,_,_,_,_  ), // #74 [ref=1x]
  O(000F00,73,2,_,_,_,_,_  ), // #75 [ref=1x]
  O(000F00,71,2,_,_,_,_,_  ), // #76 [ref=1x]
  O(000000,50,_,_,_,_,_,_  ), // #77 [ref=1x]
  O(000000,F6,_,_,x,_,_,_  ), // #78 [ref=1x]
  V(660F38,92,_,x,_,1,3,T1S), // #79 [ref=1x]
  V(660F38,92,_,x,_,0,2,T1S), // #80 [ref=1x]
  V(660F38,93,_,x,_,1,3,T1S), // #81 [ref=1x]
  V(660F38,93,_,x,_,0,2,T1S), // #82 [ref=1x]
  V(660F38,2F,_,x,0,_,_,_  ), // #83 [ref=1x]
  V(660F38,2E,_,x,0,_,_,_  ), // #84 [ref=1x]
  V(660F00,29,_,x,I,1,4,FVM), // #85 [ref=1x]
  V(000F00,29,_,x,I,0,4,FVM), // #86 [ref=1x]
  V(660F00,7E,_,0,0,0,2,T1S), // #87 [ref=1x]
  V(660F00,7F,_,x,I,_,_,_  ), // #88 [ref=1x]
  V(660F00,7F,_,x,_,0,4,FVM), // #89 [ref=1x]
  V(660F00,7F,_,x,_,1,4,FVM), // #90 [ref=1x]
  V(F30F00,7F,_,x,I,_,_,_  ), // #91 [ref=1x]
  V(F20F00,7F,_,x,_,1,4,FVM), // #92 [ref=1x]
  V(F30F00,7F,_,x,_,0,4,FVM), // #93 [ref=1x]
  V(F30F00,7F,_,x,_,1,4,FVM), // #94 [ref=1x]
  V(F20F00,7F,_,x,_,0,4,FVM), // #95 [ref=1x]
  V(660F00,17,_,0,I,1,3,T1S), // #96 [ref=1x]
  V(000F00,17,_,0,I,0,3,T2 ), // #97 [ref=1x]
  V(660F00,13,_,0,I,1,3,T1S), // #98 [ref=1x]
  V(000F00,13,_,0,I,0,3,T2 ), // #99 [ref=1x]
  V(660F00,7E,_,0,I,1,3,T1S), // #100 [ref=1x]
  V(F20F00,11,_,I,I,1,3,T1S), // #101 [ref=1x]
  V(F30F00,11,_,I,I,0,2,T1S), // #102 [ref=1x]
  V(660F00,11,_,x,I,1,4,FVM), // #103 [ref=1x]
  V(000F00,11,_,x,I,0,4,FVM), // #104 [ref=1x]
  V(660F3A,05,_,x,0,1,4,FV ), // #105 [ref=1x]
  V(660F3A,04,_,x,0,0,4,FV ), // #106 [ref=1x]
  V(660F3A,00,_,x,1,1,4,FV ), // #107 [ref=1x]
  V(660F38,90,_,x,_,0,2,T1S), // #108 [ref=1x]
  V(660F38,90,_,x,_,1,3,T1S), // #109 [ref=1x]
  V(660F38,91,_,x,_,0,2,T1S), // #110 [ref=1x]
  V(660F38,91,_,x,_,1,3,T1S), // #111 [ref=1x]
  V(660F38,8E,_,x,0,_,_,_  ), // #112 [ref=1x]
  V(660F38,8E,_,x,1,_,_,_  ), // #113 [ref=1x]
  V(XOP_M8,C0,_,0,x,_,_,_  ), // #114 [ref=1x]
  V(XOP_M8,C2,_,0,x,_,_,_  ), // #115 [ref=1x]
  V(XOP_M8,C3,_,0,x,_,_,_  ), // #116 [ref=1x]
  V(XOP_M8,C1,_,0,x,_,_,_  ), // #117 [ref=1x]
  V(660F00,72,6,x,I,0,4,FV ), // #118 [ref=1x]
  V(660F00,73,6,x,I,1,4,FV ), // #119 [ref=1x]
  V(660F00,71,6,x,I,I,4,FVM), // #120 [ref=1x]
  V(660F00,72,4,x,I,0,4,FV ), // #121 [ref=1x]
  V(660F00,72,4,x,_,1,4,FV ), // #122 [ref=1x]
  V(660F00,71,4,x,I,I,4,FVM), // #123 [ref=1x]
  V(660F00,72,2,x,I,0,4,FV ), // #124 [ref=1x]
  V(660F00,73,2,x,I,1,4,FV ), // #125 [ref=1x]
  V(660F00,71,2,x,I,I,4,FVM)  // #126 [ref=1x]
};
// ----------------------------------------------------------------------------
// ${altOpCodeData:End}

#undef O_FPU
#undef O
#undef V

// ${commonData:Begin}
// ------------------- Automatically generated, do not edit -------------------
#define F(FLAG) X86Inst::kFlag##FLAG
#define CONTROL(TYPE) Inst::kControl##TYPE
#define SINGLE_REG(TYPE) X86Inst::kSingleReg##TYPE
const X86Inst::CommonData X86InstDB::commonData[] = {
  { F(UseR)                                               , 0  , 0  , 0  , 0  , 0 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #0 [ref=1x]
  { F(UseX)|F(FixedReg)                                   , 0  , 0  , 0  , 376, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #1 [ref=4x]
  { F(UseX)|F(FixedReg)                                   , 0  , 0  , 0  , 377, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #2 [ref=2x]
  { F(UseX)|F(Lock)|F(XAcquire)|F(XRelease)               , 0  , 0  , 0  , 16 , 12, CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #3 [ref=3x]
  { F(UseX)                                               , 0  , 0  , 0  , 180, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #4 [ref=2x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 0  , 339, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #5 [ref=38x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 0  , 378, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #6 [ref=6x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 0  , 379, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #7 [ref=6x]
  { F(UseW)|F(Vec)                                        , 0  , 0  , 0  , 89 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #8 [ref=2x]
  { F(UseW)|F(Vec)                                        , 0  , 0  , 0  , 221, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #9 [ref=1x]
  { F(UseX)|F(Lock)|F(XAcquire)|F(XRelease)               , 0  , 0  , 0  , 28 , 12, CONTROL(Regular)      , SINGLE_REG(RO)  , 0 }, // #10 [ref=1x]
  { F(UseW)|F(Vex)                                        , 0  , 0  , 0  , 278, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #11 [ref=3x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 0  , 339, 1 , CONTROL(Regular)      , SINGLE_REG(RO)  , 0 }, // #12 [ref=12x]
  { F(UseX)                                               , 0  , 0  , 0  , 380, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #13 [ref=1x]
  { F(UseW)|F(Vex)                                        , 0  , 0  , 0  , 280, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #14 [ref=5x]
  { F(UseW)|F(Vex)                                        , 0  , 0  , 0  , 177, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #15 [ref=12x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 0  , 341, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #16 [ref=12x]
  { F(UseX)|F(FixedReg)|F(Vec)                            , 0  , 0  , 0  , 381, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #17 [ref=4x]
  { F(UseR)                                               , 0  , 0  , 0  , 282, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #18 [ref=3x]
  { F(UseW)|F(Mib)                                        , 0  , 0  , 0  , 382, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #19 [ref=1x]
  { F(UseW)                                               , 0  , 0  , 0  , 383, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #20 [ref=1x]
  { F(UseW)                                               , 0  , 0  , 1  , 284, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #21 [ref=1x]
  { F(UseW)|F(Mib)                                        , 0  , 0  , 0  , 384, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #22 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 0  , 286, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #23 [ref=1x]
  { F(UseW)                                               , 0  , 0  , 0  , 176, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #24 [ref=5x]
  { F(UseX)                                               , 0  , 0  , 0  , 385, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #25 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 2  , 128, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #26 [ref=1x]
  { F(UseX)|F(Lock)|F(XAcquire)|F(XRelease)               , 0  , 0  , 3  , 132, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #27 [ref=1x]
  { F(UseX)|F(Lock)|F(XAcquire)|F(XRelease)               , 0  , 0  , 4  , 132, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #28 [ref=1x]
  { F(UseX)|F(Lock)|F(XAcquire)|F(XRelease)               , 0  , 0  , 5  , 132, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #29 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 0  , 288, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #30 [ref=1x]
  { F(UseX)|F(FixedReg)                                   , 0  , 0  , 0  , 386, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #31 [ref=1x]
  { F(UseW)|F(FixedReg)                                   , 0  , 0  , 0  , 387, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #32 [ref=1x]
  { F(UseX)|F(FixedReg)                                   , 0  , 0  , 0  , 388, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #33 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 0  , 296, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #34 [ref=65x]
  { F(UseR)                                               , 0  , 0  , 0  , 389, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #35 [ref=16x]
  { F(UseR)|F(FixedRM)                                    , 0  , 0  , 0  , 390, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #36 [ref=1x]
  { F(UseX)                                               , 0  , 0  , 0  , 179, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #37 [ref=30x]
  { F(UseR)                                               , 0  , 0  , 0  , 40 , 12, CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #38 [ref=1x]
  { F(UseX)|F(FixedRM)|F(Rep)|F(Repne)                    , 0  , 0  , 0  , 391, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #39 [ref=1x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 0  , 392, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #40 [ref=1x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 0  , 393, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #41 [ref=2x]
  { F(UseX)|F(FixedReg)|F(Lock)|F(XAcquire)|F(XRelease)   , 0  , 0  , 0  , 136, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #42 [ref=1x]
  { F(UseX)|F(FixedReg)|F(Lock)|F(XAcquire)|F(XRelease)   , 0  , 0  , 0  , 394, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #43 [ref=1x]
  { F(UseX)|F(FixedReg)|F(Lock)|F(XAcquire)|F(XRelease)   , 0  , 0  , 0  , 395, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #44 [ref=1x]
  { F(UseR)|F(Vec)                                        , 0  , 0  , 0  , 396, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #45 [ref=2x]
  { F(UseR)|F(Vec)                                        , 0  , 0  , 0  , 397, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #46 [ref=2x]
  { F(UseX)|F(FixedReg)                                   , 0  , 0  , 0  , 398, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #47 [ref=1x]
  { F(UseW)|F(FixedReg)                                   , 0  , 0  , 0  , 399, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #48 [ref=1x]
  { F(UseX)                                               , 0  , 0  , 0  , 290, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #49 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 0  , 87 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #50 [ref=9x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 0  , 89 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #51 [ref=12x]
  { F(UseW)|F(Mmx)|F(Vec)                                 , 0  , 8  , 0  , 400, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #52 [ref=2x]
  { F(UseW)|F(Mmx)|F(Vec)                                 , 0  , 16 , 0  , 401, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #53 [ref=1x]
  { F(UseW)|F(Mmx)|F(Vec)                                 , 0  , 8  , 0  , 401, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #54 [ref=1x]
  { F(UseW)|F(Mmx)|F(Vec)                                 , 0  , 8  , 0  , 402, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #55 [ref=2x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 0  , 403, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #56 [ref=2x]
  { F(UseW)|F(Vec)                                        , 0  , 4  , 0  , 87 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #57 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 0  , 404, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #58 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 4  , 0  , 404, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #59 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 0  , 260, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #60 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 0  , 405, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #61 [ref=2x]
  { F(UseW)|F(FixedReg)                                   , 0  , 0  , 0  , 406, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #62 [ref=1x]
  { F(UseX)|F(FixedReg)                                   , 0  , 0  , 0  , 407, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #63 [ref=1x]
  { F(UseX)|F(Lock)|F(XAcquire)|F(XRelease)               , 0  , 0  , 6  , 292, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #64 [ref=1x]
  { F(UseX)|F(FixedReg)                                   , 0  , 0  , 0  , 140, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #65 [ref=2x]
  { F(UseR)|F(Mmx)                                        , 0  , 0  , 0  , 296, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #66 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 0  , 408, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #67 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 0  , 409, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #68 [ref=2x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 7  , 294, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #69 [ref=1x]
  { F(UseA)|F(FixedReg)|F(FpuM32)|F(FpuM64)               , 0  , 0  , 0  , 182, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #70 [ref=5x]
  { F(UseX)                                               , 0  , 0  , 0  , 296, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #71 [ref=7x]
  { F(UseR)|F(FpuM80)                                     , 0  , 0  , 0  , 410, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #72 [ref=1x]
  { F(UseW)|F(FpuM80)                                     , 0  , 0  , 0  , 411, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #73 [ref=1x]
  { F(UseX)                                               , 0  , 0  , 0  , 297, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #74 [ref=9x]
  { F(UseR)|F(FpuM32)|F(FpuM64)                           , 0  , 0  , 0  , 298, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #75 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 0  , 301, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #76 [ref=4x]
  { F(UseR)|F(FpuM16)|F(FpuM32)                           , 0  , 0  , 0  , 412, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #77 [ref=8x]
  { F(UseR)|F(FpuM16)|F(FpuM32)|F(FpuM64)                 , 0  , 0  , 8  , 413, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #78 [ref=1x]
  { F(UseW)|F(FpuM16)|F(FpuM32)                           , 0  , 0  , 0  , 414, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #79 [ref=1x]
  { F(UseW)|F(FpuM16)|F(FpuM32)|F(FpuM64)                 , 0  , 0  , 9  , 415, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #80 [ref=1x]
  { F(UseW)|F(FpuM16)|F(FpuM32)|F(FpuM64)                 , 0  , 0  , 10 , 415, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #81 [ref=1x]
  { F(UseR)|F(FpuM32)|F(FpuM64)|F(FpuM80)                 , 0  , 0  , 11 , 416, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #82 [ref=1x]
  { F(UseR)|F(FpuM16)                                     , 0  , 0  , 0  , 417, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #83 [ref=1x]
  { F(UseX)|F(FixedReg)|F(FpuM32)|F(FpuM64)               , 0  , 0  , 0  , 185, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #84 [ref=1x]
  { F(UseW)                                               , 0  , 0  , 0  , 418, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #85 [ref=7x]
  { F(UseW)|F(FpuM16)                                     , 0  , 0  , 0  , 419, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #86 [ref=2x]
  { F(UseW)|F(FixedReg)|F(FpuM16)                         , 0  , 0  , 12 , 420, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #87 [ref=1x]
  { F(UseW)|F(FpuM32)|F(FpuM64)                           , 0  , 0  , 0  , 421, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #88 [ref=1x]
  { F(UseW)|F(FpuM32)|F(FpuM64)|F(FpuM80)                 , 0  , 0  , 13 , 422, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #89 [ref=1x]
  { F(UseW)|F(FixedReg)|F(FpuM16)                         , 0  , 0  , 14 , 420, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #90 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 0  , 300, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #91 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 0  , 423, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #92 [ref=1x]
  { F(UseW)                                               , 0  , 0  , 0  , 424, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #93 [ref=1x]
  { F(UseA)|F(FixedReg)                                   , 0  , 0  , 0  , 52 , 10, CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #94 [ref=1x]
  { F(UseW)|F(FixedReg)                                   , 0  , 0  , 15 , 425, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #95 [ref=1x]
  { F(UseX)|F(Lock)|F(XAcquire)|F(XRelease)               , 0  , 0  , 16 , 292, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #96 [ref=1x]
  { F(UseW)|F(FixedRM)|F(Rep)|F(Repne)                    , 0  , 0  , 0  , 426, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #97 [ref=1x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 17 , 302, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #98 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 0  , 427, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #99 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 0  , 428, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #100 [ref=8x]
  { F(UseR)                                               , 0  , 0  , 0  , 304, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #101 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 0  , 296, 1 , CONTROL(Return)       , SINGLE_REG(None), 0 }, // #102 [ref=3x]
  { F(UseR)                                               , 0  , 0  , 0  , 429, 1 , CONTROL(Return)       , SINGLE_REG(None), 0 }, // #103 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 18 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #104 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 19 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #105 [ref=3x]
  { F(UseR)                                               , 0  , 0  , 20 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #106 [ref=3x]
  { F(UseR)                                               , 0  , 0  , 21 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #107 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 22 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #108 [ref=2x]
  { F(UseR)|F(FixedReg)                                   , 0  , 0  , 23 , 306, 2 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #109 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 24 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #110 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 25 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #111 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 26 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #112 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 27 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #113 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 28 , 308, 2 , CONTROL(Jump)         , SINGLE_REG(None), 0 }, // #114 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 29 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #115 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 30 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #116 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 31 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #117 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 32 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #118 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 33 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #119 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 34 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #120 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 35 , 430, 1 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #121 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 431, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #122 [ref=27x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 36 , 310, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #123 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 37 , 312, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #124 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 38 , 314, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #125 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 39 , 316, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #126 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 432, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #127 [ref=4x]
  { F(UseR)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 433, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #128 [ref=8x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 434, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #129 [ref=8x]
  { F(UseW)|F(FixedReg)                                   , 0  , 0  , 0  , 435, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #130 [ref=1x]
  { F(UseW)                                               , 0  , 0  , 0  , 318, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #131 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 0  , 230, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #132 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 0  , 436, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #133 [ref=1x]
  { F(UseX)                                               , 0  , 0  , 0  , 320, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #134 [ref=2x]
  { F(UseW)                                               , 0  , 0  , 0  , 437, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #135 [ref=1x]
  { F(UseX)                                               , 0  , 0  , 0  , 188, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #136 [ref=3x]
  { F(UseR)                                               , 0  , 0  , 0  , 438, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #137 [ref=5x]
  { F(UseW)|F(FixedRM)|F(Rep)|F(Repne)                    , 0  , 0  , 0  , 439, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #138 [ref=1x]
  { F(UseX)|F(FixedReg)                                   , 0  , 0  , 40 , 322, 2 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #139 [ref=1x]
  { F(UseX)|F(FixedReg)                                   , 0  , 0  , 41 , 322, 2 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #140 [ref=1x]
  { F(UseX)|F(FixedReg)                                   , 0  , 0  , 42 , 322, 2 , CONTROL(Branch)       , SINGLE_REG(None), 0 }, // #141 [ref=1x]
  { F(UseW)                                               , 0  , 0  , 0  , 324, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #142 [ref=1x]
  { F(UseR)|F(FixedRM)|F(Vec)                             , 0  , 0  , 0  , 440, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #143 [ref=1x]
  { F(UseR)|F(FixedRM)|F(Mmx)                             , 0  , 0  , 0  , 441, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #144 [ref=1x]
  { F(UseR)|F(FixedRM)                                    , 0  , 0  , 0  , 442, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #145 [ref=2x]
  { F(UseW)|F(XRelease)                                   , 0  , 0  , 0  , 0  , 16, CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #146 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 43 , 89 , 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #147 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 44 , 89 , 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #148 [ref=1x]
  { F(UseW)                                               , 0  , 0  , 45 , 77 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #149 [ref=1x]
  { F(UseW)|F(Mmx)|F(Vec)                                 , 0  , 16 , 46 , 326, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #150 [ref=1x]
  { F(UseW)|F(Mmx)|F(Vec)                                 , 0  , 8  , 0  , 443, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #151 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 47 , 89 , 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #152 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 48 , 89 , 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #153 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 0  , 444, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #154 [ref=1x]
  { F(UseW)|F(Vec)                                        , 8  , 8  , 49 , 236, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #155 [ref=1x]
  { F(UseW)|F(Vec)                                        , 8  , 8  , 50 , 236, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #156 [ref=1x]
  { F(UseW)|F(Vec)                                        , 8  , 8  , 0  , 444, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #157 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 51 , 236, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #158 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 52 , 236, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #159 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 0  , 445, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #160 [ref=2x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 53 , 90 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #161 [ref=1x]
  { F(UseW)                                               , 0  , 8  , 0  , 81 , 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #162 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 54 , 90 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #163 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 55 , 90 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #164 [ref=1x]
  { F(UseW)|F(Mmx)                                        , 0  , 8  , 56 , 446, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #165 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 57 , 88 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #166 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 4  , 58 , 239, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #167 [ref=1x]
  { F(UseW)|F(Mmx)|F(Vec)                                 , 0  , 16 , 59 , 83 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #168 [ref=1x]
  { F(UseW)|F(Mmx)|F(Vec)                                 , 0  , 16 , 0  , 447, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #169 [ref=1x]
  { F(UseX)|F(FixedRM)|F(Rep)|F(Repne)                    , 0  , 0  , 0  , 448, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #170 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 60 , 87 , 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #171 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 4  , 61 , 328, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #172 [ref=1x]
  { F(UseW)                                               , 0  , 0  , 0  , 330, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #173 [ref=2x]
  { F(UseW)                                               , 0  , 0  , 0  , 449, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #174 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 62 , 89 , 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #175 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 63 , 89 , 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #176 [ref=1x]
  { F(UseA)|F(FixedReg)                                   , 0  , 0  , 0  , 52 , 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #177 [ref=1x]
  { F(UseW)|F(FixedReg)|F(Vex)                            , 0  , 0  , 0  , 332, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #178 [ref=1x]
  { F(UseR)|F(FixedReg)                                   , 0  , 0  , 0  , 450, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #179 [ref=1x]
  { F(UseR)|F(FixedReg)                                   , 0  , 0  , 0  , 451, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #180 [ref=1x]
  { F(UseX)|F(Lock)|F(XAcquire)|F(XRelease)               , 0  , 0  , 0  , 293, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #181 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 0  , 334, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #182 [ref=1x]
  { F(UseX)|F(Lock)|F(XAcquire)|F(XRelease)               , 0  , 0  , 0  , 16 , 12, CONTROL(Regular)      , SINGLE_REG(RO)  , 0 }, // #183 [ref=1x]
  { F(UseR)|F(FixedReg)                                   , 0  , 0  , 64 , 452, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #184 [ref=1x]
  { F(UseR)|F(FixedRM)|F(Rep)|F(Repne)                    , 0  , 0  , 0  , 453, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #185 [ref=1x]
  { F(UseW)|F(Mmx)|F(Vec)                                 , 0  , 0  , 0  , 336, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #186 [ref=3x]
  { F(UseX)|F(Mmx)|F(Vec)                                 , 0  , 0  , 0  , 338, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #187 [ref=37x]
  { F(UseX)|F(Mmx)|F(Vec)                                 , 0  , 0  , 0  , 340, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #188 [ref=1x]
  { F(UseX)|F(Mmx)|F(Vec)                                 , 0  , 0  , 0  , 338, 2 , CONTROL(Regular)      , SINGLE_REG(RO)  , 0 }, // #189 [ref=6x]
  { F(UseX)|F(Mmx)|F(Vec)                                 , 0  , 0  , 0  , 338, 2 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #190 [ref=16x]
  { F(UseX)|F(Mmx)                                        , 0  , 0  , 0  , 338, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #191 [ref=18x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 0  , 339, 1 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #192 [ref=4x]
  { F(UseR)|F(FixedReg)|F(Vec)                            , 0  , 0  , 0  , 454, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #193 [ref=1x]
  { F(UseR)|F(FixedReg)|F(Vec)                            , 0  , 0  , 0  , 455, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #194 [ref=1x]
  { F(UseR)|F(FixedReg)|F(Vec)                            , 0  , 0  , 0  , 456, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #195 [ref=1x]
  { F(UseR)|F(FixedReg)|F(Vec)                            , 0  , 0  , 0  , 457, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #196 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 0  , 458, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #197 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 0  , 459, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #198 [ref=1x]
  { F(UseW)|F(Mmx)|F(Vec)                                 , 0  , 8  , 65 , 342, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #199 [ref=1x]
  { F(UseW)|F(Mmx)                                        , 0  , 8  , 0  , 336, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #200 [ref=6x]
  { F(UseW)|F(Mmx)                                        , 0  , 0  , 0  , 336, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #201 [ref=2x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 0  , 460, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #202 [ref=1x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 0  , 461, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #203 [ref=1x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 0  , 462, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #204 [ref=1x]
  { F(UseX)|F(Mmx)|F(Vec)                                 , 0  , 0  , 0  , 463, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #205 [ref=1x]
  { F(UseW)|F(Mmx)|F(Vec)                                 , 0  , 8  , 0  , 464, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #206 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 0  , 260, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #207 [ref=4x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 0  , 263, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #208 [ref=2x]
  { F(UseW)|F(FixedReg)                                   , 0  , 0  , 66 , 144, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #209 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 0  , 429, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #210 [ref=6x]
  { F(UseW)|F(Vec)                                        , 0  , 16 , 0  , 221, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #211 [ref=5x]
  { F(UseW)|F(Mmx)                                        , 0  , 8  , 0  , 465, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #212 [ref=1x]
  { F(UseX)|F(Mmx)|F(Vec)                                 , 0  , 0  , 67 , 344, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #213 [ref=1x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 68 , 466, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #214 [ref=1x]
  { F(UseX)|F(Mmx)|F(Vec)                                 , 0  , 0  , 69 , 344, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #215 [ref=1x]
  { F(UseX)|F(Mmx)|F(Vec)                                 , 0  , 0  , 70 , 344, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #216 [ref=1x]
  { F(UseX)|F(Mmx)|F(Vec)                                 , 0  , 0  , 71 , 344, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #217 [ref=1x]
  { F(UseX)|F(Mmx)|F(Vec)                                 , 0  , 0  , 72 , 344, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #218 [ref=1x]
  { F(UseX)|F(Mmx)|F(Vec)                                 , 0  , 0  , 73 , 344, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #219 [ref=1x]
  { F(UseX)|F(Vec)                                        , 0  , 0  , 74 , 466, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #220 [ref=1x]
  { F(UseX)|F(Mmx)|F(Vec)                                 , 0  , 0  , 75 , 344, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #221 [ref=1x]
  { F(UseX)|F(Mmx)|F(Vec)                                 , 0  , 0  , 76 , 344, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #222 [ref=1x]
  { F(UseR)|F(Vec)                                        , 0  , 0  , 0  , 372, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #223 [ref=1x]
  { F(UseA)|F(FixedReg)                                   , 0  , 0  , 77 , 148, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #224 [ref=1x]
  { F(UseX)|F(FixedReg)                                   , 0  , 0  , 0  , 467, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #225 [ref=8x]
  { F(UseW)|F(Vec)                                        , 0  , 4  , 0  , 260, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #226 [ref=3x]
  { F(UseW)                                               , 0  , 8  , 0  , 468, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #227 [ref=2x]
  { F(UseW)|F(FixedReg)                                   , 0  , 0  , 0  , 469, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #228 [ref=3x]
  { F(UseW)                                               , 0  , 8  , 0  , 470, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #229 [ref=2x]
  { F(UseW)|F(FixedReg)                                   , 0  , 0  , 0  , 471, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #230 [ref=1x]
  { F(UseW)|F(FixedReg)                                   , 0  , 0  , 0  , 472, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #231 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 0  , 346, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #232 [ref=1x]
  { F(UseW)|F(Vex)                                        , 0  , 0  , 0  , 348, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #233 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 0  , 473, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #234 [ref=1x]
  { F(UseW)|F(Vec)                                        , 0  , 4  , 0  , 474, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #235 [ref=1x]
  { F(UseR)|F(FixedReg)                                   , 0  , 0  , 0  , 475, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #236 [ref=1x]
  { F(UseR)|F(FixedRM)|F(Rep)|F(Repne)                    , 0  , 0  , 0  , 476, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #237 [ref=1x]
  { F(UseW)                                               , 0  , 1  , 0  , 477, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #238 [ref=30x]
  { F(UseX)|F(FixedReg)                                   , 0  , 0  , 0  , 191, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #239 [ref=2x]
  { F(UseW)                                               , 0  , 0  , 0  , 478, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #240 [ref=3x]
  { F(UseW)|F(Vec)                                        , 0  , 8  , 0  , 87 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #241 [ref=1x]
  { F(UseW)                                               , 0  , 0  , 0  , 479, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #242 [ref=1x]
  { F(UseX)|F(FixedRM)|F(Rep)|F(Repne)                    , 0  , 0  , 0  , 480, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #243 [ref=1x]
  { F(UseX)|F(Lock)|F(XAcquire)|F(XRelease)               , 0  , 0  , 0  , 16 , 12, CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #244 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 78 , 70 , 7 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #245 [ref=1x]
  { F(UseX)|F(Vec)|F(Evex)|F(Avx512T4X)|F(Avx512KZ)       , 0  , 0  , 0  , 481, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #246 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_ER_SAE_B64)  , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #247 [ref=4x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_ER_SAE_B32)  , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #248 [ref=4x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_ER_SAE)      , 0  , 0  , 0  , 482, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #249 [ref=6x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_ER_SAE)      , 0  , 0  , 0  , 483, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #250 [ref=5x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 194, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #251 [ref=15x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 194, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #252 [ref=4x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 89 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #253 [ref=17x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 221, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #254 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B32)                , 0  , 0  , 0  , 197, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #255 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B64)                , 0  , 0  , 0  , 197, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #256 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B64)         , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #257 [ref=10x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B32)         , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #258 [ref=12x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B64)         , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(RO)  , 0 }, // #259 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B32)         , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(RO)  , 0 }, // #260 [ref=6x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #261 [ref=9x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B32)                , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #262 [ref=5x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B64)                , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #263 [ref=12x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 197, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #264 [ref=6x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 350, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #265 [ref=3x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 484, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #266 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 485, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #267 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 486, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #268 [ref=4x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 487, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #269 [ref=4x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 488, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #270 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 485, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #271 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 352, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #272 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_SAE_B64)     , 0  , 0  , 0  , 200, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #273 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_SAE_B32)     , 0  , 0  , 0  , 200, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #274 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_SAE)         , 0  , 0  , 0  , 489, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #275 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_SAE)         , 0  , 0  , 0  , 490, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #276 [ref=1x]
  { F(UseR)|F(Vec)|F(Vex)|F(Evex)|F(Avx512SAE)            , 0  , 0  , 0  , 396, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #277 [ref=2x]
  { F(UseR)|F(Vec)|F(Vex)|F(Evex)|F(Avx512SAE)            , 0  , 0  , 0  , 397, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #278 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 203, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #279 [ref=4x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B32)         , 0  , 0  , 0  , 206, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #280 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_ER_SAE_B32)  , 0  , 0  , 0  , 209, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #281 [ref=3x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_ER_SAE_B64)  , 0  , 0  , 0  , 354, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #282 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_ER_SAE_B64)         , 0  , 0  , 0  , 209, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #283 [ref=4x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_ER_SAE_B64)         , 0  , 0  , 0  , 354, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #284 [ref=3x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_SAE)         , 0  , 0  , 0  , 206, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #285 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_ER_SAE_B32)  , 0  , 0  , 0  , 206, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #286 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_SAE)         , 0  , 0  , 0  , 212, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #287 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_ER_SAE_B32)         , 0  , 0  , 0  , 206, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #288 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_ER_SAE_B32)         , 0  , 0  , 0  , 209, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #289 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512ER_SAE)         , 0  , 0  , 0  , 403, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #290 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512ER_SAE)                , 0  , 0  , 0  , 403, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #291 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512ER_SAE)         , 0  , 0  , 0  , 491, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #292 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_SAE)         , 0  , 0  , 0  , 483, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #293 [ref=3x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512ER_SAE)         , 0  , 0  , 0  , 405, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #294 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512ER_SAE)                , 0  , 0  , 0  , 405, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #295 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_SAE_B64)     , 0  , 0  , 0  , 354, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #296 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE_B64)            , 0  , 0  , 0  , 209, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #297 [ref=3x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE_B64)            , 0  , 0  , 0  , 354, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #298 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_SAE_B32)     , 0  , 0  , 0  , 209, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #299 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE_B32)            , 0  , 0  , 0  , 206, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #300 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE_B32)            , 0  , 0  , 0  , 209, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #301 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512SAE)            , 0  , 0  , 0  , 403, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #302 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512SAE)                   , 0  , 0  , 0  , 403, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #303 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512SAE)            , 0  , 0  , 0  , 405, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #304 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512SAE)                   , 0  , 0  , 0  , 405, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #305 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B32)                , 0  , 0  , 0  , 206, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #306 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512ER_SAE)                , 0  , 0  , 0  , 491, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #307 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 197, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #308 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 197, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #309 [ref=10x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE_B64)            , 0  , 0  , 0  , 93 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #310 [ref=3x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE_B32)            , 0  , 0  , 0  , 93 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #311 [ref=3x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 209, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #312 [ref=5x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 213, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #313 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 492, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #314 [ref=4x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 214, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #315 [ref=4x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 0  , 409, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #316 [ref=2x]
  { F(UseX)|F(Vec)|F(Evex)|F(Avx512KZ_SAE_B64)            , 0  , 0  , 0  , 215, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #317 [ref=1x]
  { F(UseX)|F(Vec)|F(Evex)|F(Avx512KZ_SAE_B32)            , 0  , 0  , 0  , 215, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #318 [ref=1x]
  { F(UseX)|F(Vec)|F(Evex)|F(Avx512KZ_SAE)                , 0  , 0  , 0  , 493, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #319 [ref=1x]
  { F(UseX)|F(Vec)|F(Evex)|F(Avx512KZ_SAE)                , 0  , 0  , 0  , 494, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #320 [ref=1x]
  { F(UseX)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_ER_SAE_B64)  , 0  , 0  , 0  , 218, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #321 [ref=18x]
  { F(UseX)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_ER_SAE_B32)  , 0  , 0  , 0  , 218, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #322 [ref=18x]
  { F(UseX)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_ER_SAE)      , 0  , 0  , 0  , 495, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #323 [ref=12x]
  { F(UseX)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_ER_SAE)      , 0  , 0  , 0  , 496, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #324 [ref=12x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 152, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #325 [ref=13x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 356, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #326 [ref=4x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 358, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #327 [ref=4x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512K_B64)                 , 0  , 0  , 0  , 497, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #328 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512K_B32)                 , 0  , 0  , 0  , 497, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #329 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512K)                     , 0  , 0  , 0  , 498, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #330 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512K)                     , 0  , 0  , 0  , 499, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #331 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 209, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #332 [ref=4x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 87 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #333 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 260, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #334 [ref=1x]
  { F(UseX)|F(Vec)|F(Vsib)|F(Vex)|F(Evex)|F(Avx512K)      , 0  , 0  , 79 , 113, 5 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #335 [ref=1x]
  { F(UseX)|F(Vec)|F(Vsib)|F(Vex)|F(Evex)|F(Avx512K)      , 0  , 0  , 80 , 118, 5 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #336 [ref=1x]
  { F(UseR)|F(Vsib)|F(Evex)|F(Avx512K)                    , 0  , 0  , 0  , 500, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #337 [ref=4x]
  { F(UseR)|F(Vsib)|F(Evex)|F(Avx512K)                    , 0  , 0  , 0  , 501, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #338 [ref=4x]
  { F(UseR)|F(Vsib)|F(Evex)|F(Avx512K)                    , 0  , 0  , 0  , 502, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #339 [ref=8x]
  { F(UseX)|F(Vec)|F(Vsib)|F(Vex)|F(Evex)|F(Avx512K)      , 0  , 0  , 81 , 123, 5 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #340 [ref=1x]
  { F(UseX)|F(Vec)|F(Vsib)|F(Vex)|F(Evex)|F(Avx512K)      , 0  , 0  , 82 , 156, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #341 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE)                , 0  , 0  , 0  , 482, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #342 [ref=3x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE)                , 0  , 0  , 0  , 483, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #343 [ref=3x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE_B64)            , 0  , 0  , 0  , 221, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #344 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE_B32)            , 0  , 0  , 0  , 221, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #345 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE)                , 0  , 0  , 0  , 503, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #346 [ref=3x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE)                , 0  , 0  , 0  , 504, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #347 [ref=3x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 360, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #348 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 360, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #349 [ref=4x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 505, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #350 [ref=4x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 0  , 504, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #351 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 230, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #352 [ref=1x]
  { F(UseR)|F(Vex)                                        , 0  , 0  , 0  , 436, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #353 [ref=1x]
  { F(UseR)|F(FixedRM)|F(Vec)|F(Vex)                      , 0  , 0  , 0  , 440, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #354 [ref=1x]
  { F(UseA)|F(Vec)|F(Vex)                                 , 0  , 0  , 83 , 160, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #355 [ref=1x]
  { F(UseA)|F(Vec)|F(Vex)                                 , 0  , 0  , 84 , 160, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #356 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_SAE_B64)     , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #357 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_SAE_B32)     , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #358 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_SAE)         , 0  , 0  , 0  , 482, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #359 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 85 , 89 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #360 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 86 , 89 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #361 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 87 , 362, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #362 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 224, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #363 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 88 , 89 , 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #364 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 89 , 89 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #365 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 90 , 89 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #366 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 91 , 89 , 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #367 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 92 , 89 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #368 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 93 , 89 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #369 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 94 , 89 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #370 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 95 , 89 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #371 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 0  , 238, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #372 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 96 , 364, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #373 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 97 , 364, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #374 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 98 , 364, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #375 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 99 , 364, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #376 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 506, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #377 [ref=3x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 0  , 227, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #378 [ref=3x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 0  , 230, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #379 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 100, 233, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #380 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 101, 236, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #381 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 209, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #382 [ref=5x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 102, 239, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #383 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 103, 89 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #384 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 104, 89 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #385 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512T4X)|F(Avx512KZ)       , 0  , 0  , 0  , 507, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #386 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #387 [ref=21x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 197, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #388 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 194, 2 , CONTROL(Regular)      , SINGLE_REG(RO)  , 0 }, // #389 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B32)                , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(RO)  , 0 }, // #390 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 194, 2 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #391 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B32)                , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #392 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B64)                , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #393 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B64)                , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(RO)  , 0 }, // #394 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 508, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #395 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 509, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #396 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)                                , 0  , 0  , 0  , 510, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #397 [ref=6x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 242, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #398 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 511, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #399 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512K)                     , 0  , 0  , 0  , 245, 3 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #400 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512K_B32)                 , 0  , 0  , 0  , 245, 3 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #401 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512K)              , 0  , 0  , 0  , 248, 3 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #402 [ref=4x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512K_B32)          , 0  , 0  , 0  , 248, 3 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #403 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512K_B64)          , 0  , 0  , 0  , 248, 3 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #404 [ref=2x]
  { F(UseR)|F(FixedReg)|F(Vec)|F(Vex)                     , 0  , 0  , 0  , 454, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #405 [ref=1x]
  { F(UseR)|F(FixedReg)|F(Vec)|F(Vex)                     , 0  , 0  , 0  , 455, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #406 [ref=1x]
  { F(UseR)|F(FixedReg)|F(Vec)|F(Vex)                     , 0  , 0  , 0  , 456, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #407 [ref=1x]
  { F(UseR)|F(FixedReg)|F(Vec)|F(Vex)                     , 0  , 0  , 0  , 457, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #408 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512K_B64)                 , 0  , 0  , 0  , 245, 3 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #409 [ref=4x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B32)                , 0  , 0  , 0  , 209, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #410 [ref=5x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 198, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #411 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B32)         , 0  , 0  , 0  , 195, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #412 [ref=1x]
  { F(UseX)|F(Vec)|F(Evex)|F(Avx512KZ_B32)                , 0  , 0  , 0  , 218, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #413 [ref=3x]
  { F(UseX)|F(Vec)|F(Evex)|F(Avx512KZ_B64)                , 0  , 0  , 0  , 218, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #414 [ref=5x]
  { F(UseX)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 218, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #415 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 164, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #416 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B64)         , 0  , 0  , 105, 95 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #417 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B64)         , 0  , 0  , 106, 95 , 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #418 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 168, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #419 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 169, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #420 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B64)         , 0  , 0  , 107, 168, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #421 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 0  , 458, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #422 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 0  , 459, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #423 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 0  , 343, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #424 [ref=1x]
  { F(UseX)|F(Vec)|F(Vsib)|F(Vex)|F(Evex)|F(Avx512K)      , 0  , 0  , 108, 118, 5 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #425 [ref=1x]
  { F(UseX)|F(Vec)|F(Vsib)|F(Vex)|F(Evex)|F(Avx512K)      , 0  , 0  , 109, 113, 5 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #426 [ref=1x]
  { F(UseX)|F(Vec)|F(Vsib)|F(Vex)|F(Evex)|F(Avx512K)      , 0  , 0  , 110, 156, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #427 [ref=1x]
  { F(UseX)|F(Vec)|F(Vsib)|F(Vex)|F(Evex)|F(Avx512K)      , 0  , 0  , 111, 123, 5 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #428 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 512, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #429 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 513, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #430 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 514, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #431 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 515, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #432 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B64)                , 0  , 0  , 0  , 209, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #433 [ref=3x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 350, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #434 [ref=12x]
  { F(UseA)|F(Vec)|F(Vex)                                 , 0  , 0  , 112, 160, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #435 [ref=1x]
  { F(UseA)|F(Vec)|F(Vex)                                 , 0  , 0  , 113, 160, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #436 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(RO)  , 0 }, // #437 [ref=8x]
  { F(UseW)|F(Vec)|F(Evex)                                , 0  , 0  , 0  , 516, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #438 [ref=4x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 251, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #439 [ref=6x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 254, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #440 [ref=9x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 257, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #441 [ref=3x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 260, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #442 [ref=4x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 263, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #443 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 206, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #444 [ref=4x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 266, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #445 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B32)                , 0  , 0  , 0  , 93 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #446 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B64)                , 0  , 0  , 0  , 93 , 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #447 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 152, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #448 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B32)                , 0  , 0  , 0  , 221, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #449 [ref=3x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B64)                , 0  , 0  , 0  , 221, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #450 [ref=3x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 114, 366, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #451 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 115, 366, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #452 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 116, 366, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #453 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 117, 366, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #454 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #455 [ref=1x]
  { F(UseW)|F(Vec)|F(Vsib)|F(Evex)|F(Avx512K)             , 0  , 0  , 0  , 269, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #456 [ref=3x]
  { F(UseW)|F(Vec)|F(Vsib)|F(Evex)|F(Avx512K)             , 0  , 0  , 0  , 368, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #457 [ref=2x]
  { F(UseW)|F(Vec)|F(Vsib)|F(Evex)|F(Avx512K)             , 0  , 0  , 0  , 272, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #458 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 370, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #459 [ref=8x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B32)         , 0  , 0  , 0  , 221, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #460 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 221, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #461 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B32)         , 0  , 0  , 118, 101, 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #462 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)                         , 0  , 0  , 0  , 221, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #463 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B64)         , 0  , 0  , 119, 101, 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #464 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 120, 101, 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #465 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B32)         , 0  , 0  , 121, 101, 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #466 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B64)                , 0  , 0  , 122, 107, 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #467 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 123, 101, 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #468 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B32)         , 0  , 0  , 124, 101, 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #469 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B64)         , 0  , 0  , 125, 101, 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #470 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 126, 101, 6 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #471 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ)             , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #472 [ref=6x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B32)         , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #473 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B64)         , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(WO)  , 0 }, // #474 [ref=2x]
  { F(UseX)|F(Vec)|F(Evex)|F(Avx512KZ_B32)                , 0  , 0  , 0  , 215, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #475 [ref=1x]
  { F(UseX)|F(Vec)|F(Evex)|F(Avx512KZ_B64)                , 0  , 0  , 0  , 215, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #476 [ref=1x]
  { F(UseR)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 372, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #477 [ref=3x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512K)                     , 0  , 0  , 0  , 275, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #478 [ref=4x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512K_B32)                 , 0  , 0  , 0  , 275, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #479 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512K_B64)                 , 0  , 0  , 0  , 275, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #480 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE_B64)            , 0  , 0  , 0  , 197, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #481 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_SAE_B32)            , 0  , 0  , 0  , 197, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #482 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 482, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #483 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 483, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #484 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 483, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #485 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 503, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #486 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ)                    , 0  , 0  , 0  , 504, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #487 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 221, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #488 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 503, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #489 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 504, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #490 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_ER_SAE_B64)         , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #491 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_ER_SAE_B32)         , 0  , 0  , 0  , 194, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #492 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_ER_SAE)             , 0  , 0  , 0  , 482, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #493 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_ER_SAE)             , 0  , 0  , 0  , 483, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #494 [ref=1x]
  { F(UseW)|F(Vec)|F(Vsib)|F(Evex)|F(Avx512K)             , 0  , 0  , 0  , 374, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #495 [ref=1x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B32)                , 0  , 0  , 0  , 198, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #496 [ref=2x]
  { F(UseW)|F(Vec)|F(Evex)|F(Avx512KZ_B64)                , 0  , 0  , 0  , 198, 2 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #497 [ref=2x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B32)         , 0  , 0  , 0  , 197, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #498 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_B64)         , 0  , 0  , 0  , 197, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #499 [ref=1x]
  { F(UseW)|F(Vec)|F(Vex)|F(Evex)|F(Avx512KZ_ER_SAE_B64)  , 0  , 0  , 0  , 209, 3 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #500 [ref=1x]
  { F(UseW)|F(Vex)                                        , 0  , 0  , 0  , 479, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #501 [ref=1x]
  { F(UseR)|F(Vec)|F(Vex)                                 , 0  , 0  , 0  , 296, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #502 [ref=2x]
  { F(UseR)                                               , 0  , 0  , 0  , 517, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #503 [ref=2x]
  { F(UseR)|F(FixedReg)                                   , 0  , 0  , 0  , 518, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #504 [ref=2x]
  { F(UseX)|F(UseXX)|F(Lock)|F(XAcquire)|F(XRelease)      , 0  , 0  , 0  , 172, 4 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #505 [ref=1x]
  { F(UseR)                                               , 0  , 0  , 0  , 519, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #506 [ref=1x]
  { F(UseX)|F(UseXX)|F(Lock)                              , 0  , 0  , 0  , 62 , 8 , CONTROL(Regular)      , SINGLE_REG(RO)  , 0 }, // #507 [ref=1x]
  { F(UseR)|F(FixedReg)                                   , 0  , 0  , 0  , 520, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #508 [ref=2x]
  { F(UseR)|F(FixedReg)                                   , 0  , 0  , 0  , 521, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #509 [ref=2x]
  { F(UseW)|F(FixedReg)                                   , 0  , 0  , 0  , 522, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }, // #510 [ref=4x]
  { F(UseW)|F(FixedReg)                                   , 0  , 0  , 0  , 523, 1 , CONTROL(Regular)      , SINGLE_REG(None), 0 }  // #511 [ref=4x]
};
#undef SINGLE_REG
#undef CONTROL
#undef F
// ----------------------------------------------------------------------------
// ${commonData:End}

// ${operationData:Begin}
// ------------------- Automatically generated, do not edit -------------------
#define OP_FLAG(F) uint32_t(X86Inst::kOperation##F)
#define FEATURE(F) uint32_t(CpuInfo::kX86Feature##F)
#define SPECIAL(F) uint32_t(x86::kSpecialReg_##F)
const X86Inst::OperationData X86InstDB::operationData[] = {
  { 0, { 0 }, 0, 0 }, // #0 [ref=29x]
  { 0, { 0 }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #1 [ref=33x]
  { 0, { 0 }, SPECIAL(FLAGS_CF), SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #2 [ref=2x]
  { 0, { FEATURE(ADX) }, SPECIAL(FLAGS_CF), SPECIAL(FLAGS_CF) }, // #3 [ref=1x]
  { 0, { FEATURE(SSE2) }, 0, 0 }, // #4 [ref=62x]
  { 0, { FEATURE(SSE) }, 0, 0 }, // #5 [ref=43x]
  { 0, { FEATURE(SSE3) }, 0, 0 }, // #6 [ref=11x]
  { 0, { FEATURE(ADX) }, SPECIAL(FLAGS_OF), SPECIAL(FLAGS_OF) }, // #7 [ref=1x]
  { 0, { FEATURE(AESNI) }, 0, 0 }, // #8 [ref=6x]
  { 0, { FEATURE(BMI) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #9 [ref=6x]
  { 0, { 0 }, 0, SPECIAL(FLAGS_ZF) }, // #10 [ref=1x]
  { 0, { FEATURE(TBM) }, 0, 0 }, // #11 [ref=9x]
  { 0, { FEATURE(SSE4_1) }, 0, 0 }, // #12 [ref=47x]
  { 0, { FEATURE(MPX) }, 0, 0 }, // #13 [ref=7x]
  { 0, { 0 }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) }, // #14 [ref=1x]
  { 0, { 0 }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) }, // #15 [ref=3x]
  { 0, { FEATURE(BMI2) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #16 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(SMAP) }, 0, SPECIAL(FLAGS_AC) }, // #17 [ref=2x]
  { 0, { 0 }, 0, SPECIAL(FLAGS_CF) }, // #18 [ref=2x]
  { 0, { 0 }, 0, SPECIAL(FLAGS_DF) }, // #19 [ref=2x]
  { OP_FLAG(Volatile), { FEATURE(CLFLUSH) }, 0, 0 }, // #20 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(CLFLUSHOPT) }, 0, 0 }, // #21 [ref=1x]
  { OP_FLAG(Volatile), { 0 }, 0, SPECIAL(FLAGS_IF) }, // #22 [ref=2x]
  { OP_FLAG(Volatile) | OP_FLAG(Privileged), { 0 }, 0, 0 }, // #23 [ref=14x]
  { OP_FLAG(Volatile), { FEATURE(CLWB) }, 0, 0 }, // #24 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(CLZERO) }, 0, 0 }, // #25 [ref=1x]
  { 0, { 0 }, SPECIAL(FLAGS_CF), SPECIAL(FLAGS_CF) }, // #26 [ref=1x]
  { 0, { FEATURE(CMOV) }, SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_ZF), 0 }, // #27 [ref=4x]
  { 0, { FEATURE(CMOV) }, SPECIAL(FLAGS_CF), 0 }, // #28 [ref=6x]
  { 0, { FEATURE(CMOV) }, SPECIAL(FLAGS_ZF), 0 }, // #29 [ref=4x]
  { 0, { FEATURE(CMOV) }, SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF), 0 }, // #30 [ref=4x]
  { 0, { FEATURE(CMOV) }, SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_SF), 0 }, // #31 [ref=4x]
  { 0, { FEATURE(CMOV) }, SPECIAL(FLAGS_OF), 0 }, // #32 [ref=2x]
  { 0, { FEATURE(CMOV) }, SPECIAL(FLAGS_PF), 0 }, // #33 [ref=4x]
  { 0, { FEATURE(CMOV) }, SPECIAL(FLAGS_SF), 0 }, // #34 [ref=2x]
  { 0, { 0 }, SPECIAL(FLAGS_DF), SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #35 [ref=2x]
  { OP_FLAG(Volatile), { FEATURE(I486) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #36 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(CMPXCHG16B) }, 0, SPECIAL(FLAGS_ZF) }, // #37 [ref=1x]
  { 0, { FEATURE(CMPXCHG8B) }, 0, SPECIAL(FLAGS_ZF) }, // #38 [ref=1x]
  { 0, { FEATURE(SSE2) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #39 [ref=2x]
  { 0, { FEATURE(SSE) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #40 [ref=2x]
  { OP_FLAG(Volatile), { FEATURE(I486) }, 0, 0 }, // #41 [ref=1x]
  { 0, { FEATURE(SSE4_2) }, 0, 0 }, // #42 [ref=2x]
  { 0, { 0 }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #43 [ref=2x]
  { OP_FLAG(Volatile), { FEATURE(MMX) }, 0, 0 }, // #44 [ref=1x]
  { OP_FLAG(Volatile), { 0 }, 0, 0 }, // #45 [ref=23x]
  { 0, { FEATURE(SSE4A) }, 0, 0 }, // #46 [ref=4x]
  { 0, { 0 }, 0, SPECIAL(X87SW_C0) | SPECIAL(X87SW_C1) | SPECIAL(X87SW_C2) | SPECIAL(X87SW_C3) }, // #47 [ref=80x]
  { 0, { FEATURE(CMOV) }, 0, SPECIAL(X87SW_C0) | SPECIAL(X87SW_C1) | SPECIAL(X87SW_C2) | SPECIAL(X87SW_C3) }, // #48 [ref=8x]
  { 0, { 0 }, 0, SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_ZF) | SPECIAL(X87SW_C1) }, // #49 [ref=4x]
  { OP_FLAG(Volatile), { FEATURE(3DNOW) }, 0, 0 }, // #50 [ref=1x]
  { 0, { FEATURE(SSE3) }, 0, SPECIAL(X87SW_C0) | SPECIAL(X87SW_C1) | SPECIAL(X87SW_C2) | SPECIAL(X87SW_C3) }, // #51 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(FXSR) }, 0, SPECIAL(X87SW_C0) | SPECIAL(X87SW_C1) | SPECIAL(X87SW_C2) | SPECIAL(X87SW_C3) }, // #52 [ref=2x]
  { OP_FLAG(Volatile), { FEATURE(FXSR) }, 0, 0 }, // #53 [ref=2x]
  { OP_FLAG(Volatile), { 0 }, SPECIAL(FLAGS_OF), 0 }, // #54 [ref=1x]
  { OP_FLAG(Volatile) | OP_FLAG(Privileged), { FEATURE(I486) }, 0, 0 }, // #55 [ref=3x]
  { 0, { 0 }, SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_ZF), 0 }, // #56 [ref=8x]
  { 0, { 0 }, SPECIAL(FLAGS_CF), 0 }, // #57 [ref=12x]
  { 0, { 0 }, SPECIAL(FLAGS_ZF), 0 }, // #58 [ref=10x]
  { 0, { 0 }, SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF), 0 }, // #59 [ref=8x]
  { 0, { 0 }, SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_SF), 0 }, // #60 [ref=8x]
  { 0, { 0 }, SPECIAL(FLAGS_OF), 0 }, // #61 [ref=4x]
  { 0, { 0 }, SPECIAL(FLAGS_PF), 0 }, // #62 [ref=8x]
  { 0, { 0 }, SPECIAL(FLAGS_SF), 0 }, // #63 [ref=4x]
  { 0, { FEATURE(AVX512_DQ) }, 0, 0 }, // #64 [ref=23x]
  { 0, { FEATURE(AVX512_BW) }, 0, 0 }, // #65 [ref=22x]
  { 0, { FEATURE(AVX512_F) }, 0, 0 }, // #66 [ref=37x]
  { 0, { FEATURE(AVX512_DQ) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #67 [ref=3x]
  { 0, { FEATURE(AVX512_BW) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #68 [ref=4x]
  { 0, { FEATURE(AVX512_F) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #69 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(LAHFSAHF) }, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF), 0 }, // #70 [ref=1x]
  { OP_FLAG(Volatile), { 0 }, 0, SPECIAL(FLAGS_ZF) }, // #71 [ref=4x]
  { OP_FLAG(Barrier) | OP_FLAG(Volatile), { FEATURE(SSE2) }, 0, 0 }, // #72 [ref=2x]
  { 0, { 0 }, SPECIAL(FLAGS_DF), 0 }, // #73 [ref=3x]
  { 0, { FEATURE(LZCNT) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #74 [ref=1x]
  { 0, { FEATURE(MMX2) }, 0, 0 }, // #75 [ref=3x]
  { OP_FLAG(Volatile) | OP_FLAG(Privileged), { FEATURE(MONITOR) }, 0, 0 }, // #76 [ref=2x]
  { OP_FLAG(Volatile), { FEATURE(MONITORX) }, 0, 0 }, // #77 [ref=2x]
  { OP_FLAG(MovCrDr), { 0 }, 0, 0 }, // #78 [ref=1x]
  { 0, { FEATURE(MOVBE) }, 0, 0 }, // #79 [ref=1x]
  { 0, { FEATURE(MMX), FEATURE(SSE2) }, 0, 0 }, // #80 [ref=46x]
  { OP_FLAG(MovSsSd), { FEATURE(SSE2) }, 0, 0 }, // #81 [ref=1x]
  { OP_FLAG(MovSsSd), { FEATURE(SSE) }, 0, 0 }, // #82 [ref=1x]
  { 0, { FEATURE(BMI2) }, 0, 0 }, // #83 [ref=7x]
  { 0, { 0 }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #84 [ref=1x]
  { 0, { FEATURE(SSSE3) }, 0, 0 }, // #85 [ref=15x]
  { 0, { FEATURE(MMX2), FEATURE(SSE2) }, 0, 0 }, // #86 [ref=10x]
  { 0, { FEATURE(3DNOW) }, 0, 0 }, // #87 [ref=19x]
  { 0, { FEATURE(PCLMULQDQ) }, 0, 0 }, // #88 [ref=1x]
  { 0, { FEATURE(SSE4_2) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #89 [ref=4x]
  { OP_FLAG(Volatile), { FEATURE(PCOMMIT) }, 0, 0 }, // #90 [ref=1x]
  { 0, { FEATURE(MMX2), FEATURE(SSE2), FEATURE(SSE4_1) }, 0, 0 }, // #91 [ref=1x]
  { 0, { FEATURE(3DNOW2) }, 0, 0 }, // #92 [ref=5x]
  { 0, { FEATURE(GEODE) }, 0, 0 }, // #93 [ref=2x]
  { 0, { FEATURE(POPCNT) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #94 [ref=1x]
  { OP_FLAG(Prefetch), { FEATURE(3DNOW) }, 0, 0 }, // #95 [ref=1x]
  { OP_FLAG(Prefetch), { FEATURE(MMX2) }, 0, 0 }, // #96 [ref=4x]
  { OP_FLAG(Prefetch), { FEATURE(PREFETCHW) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #97 [ref=1x]
  { OP_FLAG(Prefetch), { FEATURE(PREFETCHWT1) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #98 [ref=1x]
  { 0, { FEATURE(SSE4_1) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #99 [ref=1x]
  { 0, { 0 }, SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF), SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) }, // #100 [ref=2x]
  { OP_FLAG(Volatile), { FEATURE(FSGSBASE) }, 0, 0 }, // #101 [ref=4x]
  { OP_FLAG(Volatile) | OP_FLAG(Privileged), { FEATURE(MSR) }, SPECIAL(MSR), 0 }, // #102 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(RDRAND) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #103 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(RDSEED) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #104 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(RDTSC) }, 0, 0 }, // #105 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(RDTSCP) }, 0, 0 }, // #106 [ref=1x]
  { 0, { 0 }, 0, SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) }, // #107 [ref=2x]
  { OP_FLAG(Volatile), { 0 }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #108 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(LAHFSAHF) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #109 [ref=1x]
  { OP_FLAG(Barrier) | OP_FLAG(Volatile), { FEATURE(MMX2) }, 0, 0 }, // #110 [ref=1x]
  { 0, { FEATURE(SHA) }, 0, 0 }, // #111 [ref=7x]
  { 0, { FEATURE(AVX512_4FMAPS) }, 0, 0 }, // #112 [ref=2x]
  { 0, { FEATURE(AVX), FEATURE(AVX512_F), FEATURE(AVX512_VL) }, 0, 0 }, // #113 [ref=46x]
  { 0, { FEATURE(AVX), FEATURE(AVX512_F) }, 0, 0 }, // #114 [ref=32x]
  { 0, { FEATURE(AVX) }, 0, 0 }, // #115 [ref=35x]
  { 0, { FEATURE(AESNI), FEATURE(AVX) }, 0, 0 }, // #116 [ref=6x]
  { 0, { FEATURE(AVX512_F), FEATURE(AVX512_VL) }, 0, 0 }, // #117 [ref=112x]
  { 0, { FEATURE(AVX), FEATURE(AVX512_DQ), FEATURE(AVX512_VL) }, 0, 0 }, // #118 [ref=8x]
  { 0, { FEATURE(AVX512_BW), FEATURE(AVX512_VL) }, 0, 0 }, // #119 [ref=26x]
  { 0, { FEATURE(AVX512_DQ), FEATURE(AVX512_VL) }, 0, 0 }, // #120 [ref=30x]
  { 0, { FEATURE(AVX2) }, 0, 0 }, // #121 [ref=9x]
  { 0, { FEATURE(AVX), FEATURE(AVX2), FEATURE(AVX512_F), FEATURE(AVX512_VL) }, 0, 0 }, // #122 [ref=39x]
  { 0, { FEATURE(AVX), FEATURE(AVX512_F) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #123 [ref=4x]
  { 0, { FEATURE(AVX512_F), FEATURE(AVX512_VL), FEATURE(F16C) }, 0, 0 }, // #124 [ref=2x]
  { 0, { FEATURE(AVX512_ERI) }, 0, 0 }, // #125 [ref=10x]
  { 0, { FEATURE(AVX512_F), FEATURE(AVX512_VL), FEATURE(FMA) }, 0, 0 }, // #126 [ref=36x]
  { 0, { FEATURE(AVX512_F), FEATURE(FMA) }, 0, 0 }, // #127 [ref=24x]
  { 0, { FEATURE(FMA4) }, 0, 0 }, // #128 [ref=20x]
  { 0, { FEATURE(XOP) }, 0, 0 }, // #129 [ref=55x]
  { 0, { FEATURE(AVX2), FEATURE(AVX512_F), FEATURE(AVX512_VL) }, 0, 0 }, // #130 [ref=17x]
  { 0, { FEATURE(AVX512_PFI) }, 0, 0 }, // #131 [ref=16x]
  { 0, { FEATURE(AVX), FEATURE(AVX2) }, 0, 0 }, // #132 [ref=17x]
  { 0, { FEATURE(AVX512_4VNNIW) }, 0, 0 }, // #133 [ref=2x]
  { 0, { FEATURE(AVX), FEATURE(AVX2), FEATURE(AVX512_BW), FEATURE(AVX512_VL) }, 0, 0 }, // #134 [ref=54x]
  { 0, { FEATURE(AVX2), FEATURE(AVX512_BW), FEATURE(AVX512_VL) }, 0, 0 }, // #135 [ref=2x]
  { 0, { FEATURE(AVX512_CDI), FEATURE(AVX512_VL) }, 0, 0 }, // #136 [ref=6x]
  { 0, { FEATURE(AVX), FEATURE(PCLMULQDQ) }, 0, 0 }, // #137 [ref=1x]
  { 0, { FEATURE(AVX) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #138 [ref=7x]
  { 0, { FEATURE(AVX512_VBMI), FEATURE(AVX512_VL) }, 0, 0 }, // #139 [ref=4x]
  { 0, { FEATURE(AVX), FEATURE(AVX512_BW) }, 0, 0 }, // #140 [ref=4x]
  { 0, { FEATURE(AVX), FEATURE(AVX512_DQ) }, 0, 0 }, // #141 [ref=4x]
  { 0, { FEATURE(AVX512_IFMA), FEATURE(AVX512_VL) }, 0, 0 }, // #142 [ref=2x]
  { 0, { FEATURE(AVX512_VPOPCNTDQ) }, 0, 0 }, // #143 [ref=2x]
  { OP_FLAG(Volatile), { FEATURE(AVX) }, 0, 0 }, // #144 [ref=2x]
  { OP_FLAG(Volatile) | OP_FLAG(Privileged), { FEATURE(MSR) }, 0, SPECIAL(MSR) }, // #145 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(RTM) }, 0, 0 }, // #146 [ref=3x]
  { 0, { FEATURE(I486) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }, // #147 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(XSAVE) }, SPECIAL(XCR), 0 }, // #148 [ref=5x]
  { OP_FLAG(Volatile), { FEATURE(XSAVES) }, SPECIAL(XCR), 0 }, // #149 [ref=4x]
  { OP_FLAG(Volatile), { FEATURE(XSAVEC) }, SPECIAL(XCR), 0 }, // #150 [ref=2x]
  { OP_FLAG(Volatile), { FEATURE(XSAVEOPT) }, SPECIAL(XCR), 0 }, // #151 [ref=2x]
  { OP_FLAG(Volatile) | OP_FLAG(Privileged), { FEATURE(XSAVE) }, 0, SPECIAL(XCR) }, // #152 [ref=1x]
  { OP_FLAG(Volatile), { FEATURE(TSX) }, 0, SPECIAL(FLAGS_AF) | SPECIAL(FLAGS_CF) | SPECIAL(FLAGS_OF) | SPECIAL(FLAGS_PF) | SPECIAL(FLAGS_SF) | SPECIAL(FLAGS_ZF) }  // #153 [ref=1x]
};
#undef SPECIAL
#undef FEATURE
#undef OP_FLAG
// ----------------------------------------------------------------------------
// ${operationData:End}

// ${sseToAvxData:Begin}
// ------------------- Automatically generated, do not edit -------------------
const X86Inst::SseToAvxData X86InstDB::sseToAvxData[] = {
  { X86Inst::kSseToAvxNone     , 0    }, // #0 [ref=955x]
  { X86Inst::kSseToAvxExtend   , 727  }, // #1 [ref=6x]
  { X86Inst::kSseToAvxExtend   , 726  }, // #2 [ref=8x]
  { X86Inst::kSseToAvxMove     , 726  }, // #3 [ref=2x]
  { X86Inst::kSseToAvxExtend   , 725  }, // #4 [ref=2x]
  { X86Inst::kSseToAvxBlend    , 725  }, // #5 [ref=2x]
  { X86Inst::kSseToAvxExtend   , 673  }, // #6 [ref=6x]
  { X86Inst::kSseToAvxExtend   , 672  }, // #7 [ref=2x]
  { X86Inst::kSseToAvxMove     , 669  }, // #8 [ref=4x]
  { X86Inst::kSseToAvxMove     , 668  }, // #9 [ref=3x]
  { X86Inst::kSseToAvxMove     , 667  }, // #10 [ref=1x]
  { X86Inst::kSseToAvxMove     , 674  }, // #11 [ref=3x]
  { X86Inst::kSseToAvxExtend   , 674  }, // #12 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 675  }, // #13 [ref=3x]
  { X86Inst::kSseToAvxMove     , 675  }, // #14 [ref=1x]
  { X86Inst::kSseToAvxMove     , 676  }, // #15 [ref=1x]
  { X86Inst::kSseToAvxMove     , 678  }, // #16 [ref=1x]
  { X86Inst::kSseToAvxMove     , 680  }, // #17 [ref=1x]
  { X86Inst::kSseToAvxMove     , 681  }, // #18 [ref=7x]
  { X86Inst::kSseToAvxExtend   , 683  }, // #19 [ref=10x]
  { X86Inst::kSseToAvxMove     , 697  }, // #20 [ref=4x]
  { X86Inst::kSseToAvxExtend   , 710  }, // #21 [ref=2x]
  { X86Inst::kSseToAvxExtend   , 709  }, // #22 [ref=3x]
  { X86Inst::kSseToAvxExtend   , 714  }, // #23 [ref=1x]
  { X86Inst::kSseToAvxMove     , 618  }, // #24 [ref=1x]
  { X86Inst::kSseToAvxMove     , 599  }, // #25 [ref=3x]
  { X86Inst::kSseToAvxExtend   , 600  }, // #26 [ref=6x]
  { X86Inst::kSseToAvxExtend   , 599  }, // #27 [ref=4x]
  { X86Inst::kSseToAvxMove     , 596  }, // #28 [ref=8x]
  { X86Inst::kSseToAvxMove     , 595  }, // #29 [ref=2x]
  { X86Inst::kSseToAvxMove     , 594  }, // #30 [ref=3x]
  { X86Inst::kSseToAvxMoveIfMem, 600  }, // #31 [ref=4x]
  { X86Inst::kSseToAvxMove     , 600  }, // #32 [ref=4x]
  { X86Inst::kSseToAvxMoveIfMem, 594  }, // #33 [ref=2x]
  { X86Inst::kSseToAvxMove     , 592  }, // #34 [ref=2x]
  { X86Inst::kSseToAvxExtend   , 591  }, // #35 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 590  }, // #36 [ref=4x]
  { X86Inst::kSseToAvxExtend   , 583  }, // #37 [ref=2x]
  { X86Inst::kSseToAvxMove     , 583  }, // #38 [ref=2x]
  { X86Inst::kSseToAvxMove     , 584  }, // #39 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 584  }, // #40 [ref=14x]
  { X86Inst::kSseToAvxExtend   , 585  }, // #41 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 587  }, // #42 [ref=2x]
  { X86Inst::kSseToAvxExtend   , 586  }, // #43 [ref=1x]
  { X86Inst::kSseToAvxBlend    , 587  }, // #44 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 593  }, // #45 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 596  }, // #46 [ref=8x]
  { X86Inst::kSseToAvxMove     , 637  }, // #47 [ref=4x]
  { X86Inst::kSseToAvxExtend   , 623  }, // #48 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 624  }, // #49 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 630  }, // #50 [ref=1x]
  { X86Inst::kSseToAvxMove     , 632  }, // #51 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 633  }, // #52 [ref=5x]
  { X86Inst::kSseToAvxExtend   , 634  }, // #53 [ref=2x]
  { X86Inst::kSseToAvxExtend   , 649  }, // #54 [ref=2x]
  { X86Inst::kSseToAvxExtend   , 651  }, // #55 [ref=2x]
  { X86Inst::kSseToAvxExtend   , 652  }, // #56 [ref=3x]
  { X86Inst::kSseToAvxExtend   , 653  }, // #57 [ref=3x]
  { X86Inst::kSseToAvxExtend   , 654  }, // #58 [ref=3x]
  { X86Inst::kSseToAvxExtend   , 655  }, // #59 [ref=1x]
  { X86Inst::kSseToAvxMove     , 663  }, // #60 [ref=1x]
  { X86Inst::kSseToAvxMove     , 673  }, // #61 [ref=6x]
  { X86Inst::kSseToAvxExtend   , 681  }, // #62 [ref=3x]
  { X86Inst::kSseToAvxExtend   , 680  }, // #63 [ref=3x]
  { X86Inst::kSseToAvxExtend   , 682  }, // #64 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 677  }, // #65 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 685  }, // #66 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 697  }, // #67 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 696  }, // #68 [ref=6x]
  { X86Inst::kSseToAvxExtend   , 699  }, // #69 [ref=2x]
  { X86Inst::kSseToAvxExtend   , 703  }, // #70 [ref=4x]
  { X86Inst::kSseToAvxExtend   , 706  }, // #71 [ref=9x]
  { X86Inst::kSseToAvxMove     , 707  }, // #72 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 715  }, // #73 [ref=8x]
  { X86Inst::kSseToAvxMove     , 722  }, // #74 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 722  }, // #75 [ref=1x]
  { X86Inst::kSseToAvxMove     , 717  }, // #76 [ref=2x]
  { X86Inst::kSseToAvxExtend   , 717  }, // #77 [ref=2x]
  { X86Inst::kSseToAvxMove     , 724  }, // #78 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 724  }, // #79 [ref=1x]
  { X86Inst::kSseToAvxExtend   , 693  }, // #80 [ref=2x]
  { X86Inst::kSseToAvxMove     , 690  }, // #81 [ref=2x]
  { X86Inst::kSseToAvxExtend   , 690  }, // #82 [ref=2x]
  { X86Inst::kSseToAvxExtend   , -16  }  // #83 [ref=2x]
};
// ----------------------------------------------------------------------------
// ${sseToAvxData:End}

// ============================================================================
// [asmjit::X86Inst - Id <-> Name]
// ============================================================================

#ifndef ASMJIT_DISABLE_TEXT
// ${nameData:Begin}
// ------------------- Automatically generated, do not edit -------------------
const char X86InstDB::nameData[] =
  "\0" "aaa\0" "aad\0" "aam\0" "aas\0" "adc\0" "adcx\0" "adox\0" "arpl\0"
  "bextr\0" "blcfill\0" "blci\0" "blcic\0" "blcmsk\0" "blcs\0" "blsfill\0"
  "blsi\0" "blsic\0" "blsmsk\0" "blsr\0" "bndcl\0" "bndcn\0" "bndcu\0"
  "bndldx\0" "bndmk\0" "bndmov\0" "bndstx\0" "bound\0" "bsf\0" "bsr\0"
  "bswap\0" "bt\0" "btc\0" "btr\0" "bts\0" "bzhi\0" "cbw\0" "cdq\0" "cdqe\0"
  "clac\0" "clc\0" "cld\0" "clflush\0" "clflushopt\0" "cli\0" "clts\0" "clwb\0"
  "clzero\0" "cmc\0" "cmova\0" "cmovae\0" "cmovc\0" "cmovg\0" "cmovge\0"
  "cmovl\0" "cmovle\0" "cmovna\0" "cmovnae\0" "cmovnc\0" "cmovng\0" "cmovnge\0"
  "cmovnl\0" "cmovnle\0" "cmovno\0" "cmovnp\0" "cmovns\0" "cmovnz\0" "cmovo\0"
  "cmovp\0" "cmovpe\0" "cmovpo\0" "cmovs\0" "cmovz\0" "cmp\0" "cmps\0"
  "cmpxchg\0" "cmpxchg16b\0" "cmpxchg8b\0" "cpuid\0" "cqo\0" "crc32\0"
  "cvtpd2pi\0" "cvtpi2pd\0" "cvtpi2ps\0" "cvtps2pi\0" "cvttpd2pi\0"
  "cvttps2pi\0" "cwd\0" "cwde\0" "daa\0" "das\0" "f2xm1\0" "fabs\0" "faddp\0"
  "fbld\0" "fbstp\0" "fchs\0" "fclex\0" "fcmovb\0" "fcmovbe\0" "fcmove\0"
  "fcmovnb\0" "fcmovnbe\0" "fcmovne\0" "fcmovnu\0" "fcmovu\0" "fcom\0"
  "fcomi\0" "fcomip\0" "fcomp\0" "fcompp\0" "fcos\0" "fdecstp\0" "fdiv\0"
  "fdivp\0" "fdivr\0" "fdivrp\0" "femms\0" "ffree\0" "fiadd\0" "ficom\0"
  "ficomp\0" "fidiv\0" "fidivr\0" "fild\0" "fimul\0" "fincstp\0" "finit\0"
  "fist\0" "fistp\0" "fisttp\0" "fisub\0" "fisubr\0" "fld\0" "fld1\0" "fldcw\0"
  "fldenv\0" "fldl2e\0" "fldl2t\0" "fldlg2\0" "fldln2\0" "fldpi\0" "fldz\0"
  "fmulp\0" "fnclex\0" "fninit\0" "fnop\0" "fnsave\0" "fnstcw\0" "fnstenv\0"
  "fnstsw\0" "fpatan\0" "fprem\0" "fprem1\0" "fptan\0" "frndint\0" "frstor\0"
  "fsave\0" "fscale\0" "fsin\0" "fsincos\0" "fsqrt\0" "fst\0" "fstcw\0"
  "fstenv\0" "fstp\0" "fstsw\0" "fsubp\0" "fsubrp\0" "ftst\0" "fucom\0"
  "fucomi\0" "fucomip\0" "fucomp\0" "fucompp\0" "fwait\0" "fxam\0" "fxch\0"
  "fxrstor\0" "fxrstor64\0" "fxsave\0" "fxsave64\0" "fxtract\0" "fyl2x\0"
  "fyl2xp1\0" "hlt\0" "inc\0" "ins\0" "insertq\0" "int3\0" "into\0" "invlpg\0"
  "invpcid\0" "iret\0" "iretd\0" "iretq\0" "iretw\0" "ja\0" "jae\0" "jb\0"
  "jbe\0" "jc\0" "je\0" "jecxz\0" "jg\0" "jge\0" "jl\0" "jle\0" "jmp\0" "jna\0"
  "jnae\0" "jnb\0" "jnbe\0" "jnc\0" "jne\0" "jng\0" "jnge\0" "jnl\0" "jnle\0"
  "jno\0" "jnp\0" "jns\0" "jnz\0" "jo\0" "jp\0" "jpe\0" "jpo\0" "js\0" "jz\0"
  "kaddb\0" "kaddd\0" "kaddq\0" "kaddw\0" "kandb\0" "kandd\0" "kandnb\0"
  "kandnd\0" "kandnq\0" "kandnw\0" "kandq\0" "kandw\0" "kmovb\0" "kmovw\0"
  "knotb\0" "knotd\0" "knotq\0" "knotw\0" "korb\0" "kord\0" "korq\0"
  "kortestb\0" "kortestd\0" "kortestq\0" "kortestw\0" "korw\0" "kshiftlb\0"
  "kshiftld\0" "kshiftlq\0" "kshiftlw\0" "kshiftrb\0" "kshiftrd\0" "kshiftrq\0"
  "kshiftrw\0" "ktestb\0" "ktestd\0" "ktestq\0" "ktestw\0" "kunpckbw\0"
  "kunpckdq\0" "kunpckwd\0" "kxnorb\0" "kxnord\0" "kxnorq\0" "kxnorw\0"
  "kxorb\0" "kxord\0" "kxorq\0" "kxorw\0" "lahf\0" "lar\0" "lds\0" "lea\0"
  "leave\0" "les\0" "lfence\0" "lfs\0" "lgdt\0" "lgs\0" "lidt\0" "lldt\0"
  "lmsw\0" "lods\0" "loop\0" "loope\0" "loopne\0" "lsl\0" "ltr\0" "lzcnt\0"
  "mfence\0" "monitor\0" "monitorx\0" "movdq2q\0" "movnti\0" "movntq\0"
  "movntsd\0" "movntss\0" "movq2dq\0" "movsx\0" "movsxd\0" "movzx\0" "mulx\0"
  "mwait\0" "mwaitx\0" "neg\0" "not\0" "out\0" "outs\0" "pause\0" "pavgusb\0"
  "pcommit\0" "pdep\0" "pext\0" "pf2id\0" "pf2iw\0" "pfacc\0" "pfadd\0"
  "pfcmpeq\0" "pfcmpge\0" "pfcmpgt\0" "pfmax\0" "pfmin\0" "pfmul\0" "pfnacc\0"
  "pfpnacc\0" "pfrcp\0" "pfrcpit1\0" "pfrcpit2\0" "pfrcpv\0" "pfrsqit1\0"
  "pfrsqrt\0" "pfrsqrtv\0" "pfsub\0" "pfsubr\0" "pi2fd\0" "pi2fw\0" "pmulhrw\0"
  "pop\0" "popa\0" "popad\0" "popcnt\0" "popf\0" "popfd\0" "popfq\0"
  "prefetch\0" "prefetchnta\0" "prefetcht0\0" "prefetcht1\0" "prefetcht2\0"
  "prefetchw\0" "prefetchwt1\0" "pshufw\0" "pswapd\0" "push\0" "pusha\0"
  "pushad\0" "pushf\0" "pushfd\0" "pushfq\0" "rcl\0" "rcr\0" "rdfsbase\0"
  "rdgsbase\0" "rdmsr\0" "rdpmc\0" "rdrand\0" "rdseed\0" "rdtsc\0" "rdtscp\0"
  "rol\0" "ror\0" "rorx\0" "rsm\0" "sahf\0" "sal\0" "sar\0" "sarx\0" "sbb\0"
  "scas\0" "seta\0" "setae\0" "setb\0" "setbe\0" "setc\0" "sete\0" "setg\0"
  "setge\0" "setl\0" "setle\0" "setna\0" "setnae\0" "setnb\0" "setnbe\0"
  "setnc\0" "setne\0" "setng\0" "setnge\0" "setnl\0" "setnle\0" "setno\0"
  "setnp\0" "setns\0" "setnz\0" "seto\0" "setp\0" "setpe\0" "setpo\0" "sets\0"
  "setz\0" "sfence\0" "sgdt\0" "sha1msg1\0" "sha1msg2\0" "sha1nexte\0"
  "sha1rnds4\0" "sha256msg1\0" "sha256msg2\0" "sha256rnds2\0" "shl\0" "shlx\0"
  "shr\0" "shrd\0" "shrx\0" "sidt\0" "sldt\0" "smsw\0" "stac\0" "stc\0" "sti\0"
  "stos\0" "str\0" "swapgs\0" "syscall\0" "sysenter\0" "sysexit\0"
  "sysexit64\0" "sysret\0" "sysret64\0" "t1mskc\0" "tzcnt\0" "tzmsk\0" "ud2\0"
  "v4fmaddps\0" "v4fnmaddps\0" "vaddpd\0" "vaddps\0" "vaddsd\0" "vaddss\0"
  "vaddsubpd\0" "vaddsubps\0" "vaesdec\0" "vaesdeclast\0" "vaesenc\0"
  "vaesenclast\0" "vaesimc\0" "vaeskeygenassist\0" "valignd\0" "valignq\0"
  "vandnpd\0" "vandnps\0" "vandpd\0" "vandps\0" "vblendmb\0" "vblendmd\0"
  "vblendmpd\0" "vblendmps\0" "vblendmq\0" "vblendmw\0" "vblendpd\0"
  "vblendps\0" "vblendvpd\0" "vblendvps\0" "vbroadcastf128\0"
  "vbroadcastf32x2\0" "vbroadcastf32x4\0" "vbroadcastf32x8\0"
  "vbroadcastf64x2\0" "vbroadcastf64x4\0" "vbroadcasti128\0"
  "vbroadcasti32x2\0" "vbroadcasti32x4\0" "vbroadcasti32x8\0"
  "vbroadcasti64x2\0" "vbroadcasti64x4\0" "vbroadcastsd\0" "vbroadcastss\0"
  "vcmppd\0" "vcmpps\0" "vcmpsd\0" "vcmpss\0" "vcomisd\0" "vcomiss\0"
  "vcompresspd\0" "vcompressps\0" "vcvtdq2pd\0" "vcvtdq2ps\0" "vcvtpd2dq\0"
  "vcvtpd2ps\0" "vcvtpd2qq\0" "vcvtpd2udq\0" "vcvtpd2uqq\0" "vcvtph2ps\0"
  "vcvtps2dq\0" "vcvtps2pd\0" "vcvtps2ph\0" "vcvtps2qq\0" "vcvtps2udq\0"
  "vcvtps2uqq\0" "vcvtqq2pd\0" "vcvtqq2ps\0" "vcvtsd2si\0" "vcvtsd2ss\0"
  "vcvtsd2usi\0" "vcvtsi2sd\0" "vcvtsi2ss\0" "vcvtss2sd\0" "vcvtss2si\0"
  "vcvtss2usi\0" "vcvttpd2dq\0" "vcvttpd2qq\0" "vcvttpd2udq\0" "vcvttpd2uqq\0"
  "vcvttps2dq\0" "vcvttps2qq\0" "vcvttps2udq\0" "vcvttps2uqq\0" "vcvttsd2si\0"
  "vcvttsd2usi\0" "vcvttss2si\0" "vcvttss2usi\0" "vcvtudq2pd\0" "vcvtudq2ps\0"
  "vcvtuqq2pd\0" "vcvtuqq2ps\0" "vcvtusi2sd\0" "vcvtusi2ss\0" "vdbpsadbw\0"
  "vdivpd\0" "vdivps\0" "vdivsd\0" "vdivss\0" "vdppd\0" "vdpps\0" "verr\0"
  "verw\0" "vexp2pd\0" "vexp2ps\0" "vexpandpd\0" "vexpandps\0" "vextractf128\0"
  "vextractf32x4\0" "vextractf32x8\0" "vextractf64x2\0" "vextractf64x4\0"
  "vextracti128\0" "vextracti32x4\0" "vextracti32x8\0" "vextracti64x2\0"
  "vextracti64x4\0" "vextractps\0" "vfixupimmpd\0" "vfixupimmps\0"
  "vfixupimmsd\0" "vfixupimmss\0" "vfmadd132pd\0" "vfmadd132ps\0"
  "vfmadd132sd\0" "vfmadd132ss\0" "vfmadd213pd\0" "vfmadd213ps\0"
  "vfmadd213sd\0" "vfmadd213ss\0" "vfmadd231pd\0" "vfmadd231ps\0"
  "vfmadd231sd\0" "vfmadd231ss\0" "vfmaddpd\0" "vfmaddps\0" "vfmaddsd\0"
  "vfmaddss\0" "vfmaddsub132pd\0" "vfmaddsub132ps\0" "vfmaddsub213pd\0"
  "vfmaddsub213ps\0" "vfmaddsub231pd\0" "vfmaddsub231ps\0" "vfmaddsubpd\0"
  "vfmaddsubps\0" "vfmsub132pd\0" "vfmsub132ps\0" "vfmsub132sd\0"
  "vfmsub132ss\0" "vfmsub213pd\0" "vfmsub213ps\0" "vfmsub213sd\0"
  "vfmsub213ss\0" "vfmsub231pd\0" "vfmsub231ps\0" "vfmsub231sd\0"
  "vfmsub231ss\0" "vfmsubadd132pd\0" "vfmsubadd132ps\0" "vfmsubadd213pd\0"
  "vfmsubadd213ps\0" "vfmsubadd231pd\0" "vfmsubadd231ps\0" "vfmsubaddpd\0"
  "vfmsubaddps\0" "vfmsubpd\0" "vfmsubps\0" "vfmsubsd\0" "vfmsubss\0"
  "vfnmadd132pd\0" "vfnmadd132ps\0" "vfnmadd132sd\0" "vfnmadd132ss\0"
  "vfnmadd213pd\0" "vfnmadd213ps\0" "vfnmadd213sd\0" "vfnmadd213ss\0"
  "vfnmadd231pd\0" "vfnmadd231ps\0" "vfnmadd231sd\0" "vfnmadd231ss\0"
  "vfnmaddpd\0" "vfnmaddps\0" "vfnmaddsd\0" "vfnmaddss\0" "vfnmsub132pd\0"
  "vfnmsub132ps\0" "vfnmsub132sd\0" "vfnmsub132ss\0" "vfnmsub213pd\0"
  "vfnmsub213ps\0" "vfnmsub213sd\0" "vfnmsub213ss\0" "vfnmsub231pd\0"
  "vfnmsub231ps\0" "vfnmsub231sd\0" "vfnmsub231ss\0" "vfnmsubpd\0"
  "vfnmsubps\0" "vfnmsubsd\0" "vfnmsubss\0" "vfpclasspd\0" "vfpclassps\0"
  "vfpclasssd\0" "vfpclassss\0" "vfrczpd\0" "vfrczps\0" "vfrczsd\0" "vfrczss\0"
  "vgatherdpd\0" "vgatherdps\0" "vgatherpf0dpd\0" "vgatherpf0dps\0"
  "vgatherpf0qpd\0" "vgatherpf0qps\0" "vgatherpf1dpd\0" "vgatherpf1dps\0"
  "vgatherpf1qpd\0" "vgatherpf1qps\0" "vgatherqpd\0" "vgatherqps\0"
  "vgetexppd\0" "vgetexpps\0" "vgetexpsd\0" "vgetexpss\0" "vgetmantpd\0"
  "vgetmantps\0" "vgetmantsd\0" "vgetmantss\0" "vhaddpd\0" "vhaddps\0"
  "vhsubpd\0" "vhsubps\0" "vinsertf128\0" "vinsertf32x4\0" "vinsertf32x8\0"
  "vinsertf64x2\0" "vinsertf64x4\0" "vinserti128\0" "vinserti32x4\0"
  "vinserti32x8\0" "vinserti64x2\0" "vinserti64x4\0" "vinsertps\0" "vlddqu\0"
  "vldmxcsr\0" "vmaskmovdqu\0" "vmaskmovpd\0" "vmaskmovps\0" "vmaxpd\0"
  "vmaxps\0" "vmaxsd\0" "vmaxss\0" "vminpd\0" "vminps\0" "vminsd\0" "vminss\0"
  "vmovapd\0" "vmovaps\0" "vmovd\0" "vmovddup\0" "vmovdqa\0" "vmovdqa32\0"
  "vmovdqa64\0" "vmovdqu\0" "vmovdqu16\0" "vmovdqu32\0" "vmovdqu64\0"
  "vmovdqu8\0" "vmovhlps\0" "vmovhpd\0" "vmovhps\0" "vmovlhps\0" "vmovlpd\0"
  "vmovlps\0" "vmovmskpd\0" "vmovmskps\0" "vmovntdq\0" "vmovntdqa\0"
  "vmovntpd\0" "vmovntps\0" "vmovq\0" "vmovsd\0" "vmovshdup\0" "vmovsldup\0"
  "vmovss\0" "vmovupd\0" "vmovups\0" "vmpsadbw\0" "vmulpd\0" "vmulps\0"
  "vmulsd\0" "vmulss\0" "vorpd\0" "vorps\0" "vp4dpwssd\0" "vp4dpwssds\0"
  "vpabsb\0" "vpabsd\0" "vpabsq\0" "vpabsw\0" "vpackssdw\0" "vpacksswb\0"
  "vpackusdw\0" "vpackuswb\0" "vpaddb\0" "vpaddd\0" "vpaddq\0" "vpaddsb\0"
  "vpaddsw\0" "vpaddusb\0" "vpaddusw\0" "vpaddw\0" "vpalignr\0" "vpand\0"
  "vpandd\0" "vpandn\0" "vpandnd\0" "vpandnq\0" "vpandq\0" "vpavgb\0"
  "vpavgw\0" "vpblendd\0" "vpblendvb\0" "vpblendw\0" "vpbroadcastb\0"
  "vpbroadcastd\0" "vpbroadcastmb2d\0" "vpbroadcastmb2q\0" "vpbroadcastq\0"
  "vpbroadcastw\0" "vpclmulqdq\0" "vpcmov\0" "vpcmpb\0" "vpcmpd\0" "vpcmpeqb\0"
  "vpcmpeqd\0" "vpcmpeqq\0" "vpcmpeqw\0" "vpcmpestri\0" "vpcmpestrm\0"
  "vpcmpgtb\0" "vpcmpgtd\0" "vpcmpgtq\0" "vpcmpgtw\0" "vpcmpistri\0"
  "vpcmpistrm\0" "vpcmpq\0" "vpcmpub\0" "vpcmpud\0" "vpcmpuq\0" "vpcmpuw\0"
  "vpcmpw\0" "vpcomb\0" "vpcomd\0" "vpcompressd\0" "vpcompressq\0" "vpcomq\0"
  "vpcomub\0" "vpcomud\0" "vpcomuq\0" "vpcomuw\0" "vpcomw\0" "vpconflictd\0"
  "vpconflictq\0" "vperm2f128\0" "vperm2i128\0" "vpermb\0" "vpermd\0"
  "vpermi2b\0" "vpermi2d\0" "vpermi2pd\0" "vpermi2ps\0" "vpermi2q\0"
  "vpermi2w\0" "vpermil2pd\0" "vpermil2ps\0" "vpermilpd\0" "vpermilps\0"
  "vpermpd\0" "vpermps\0" "vpermq\0" "vpermt2b\0" "vpermt2d\0" "vpermt2pd\0"
  "vpermt2ps\0" "vpermt2q\0" "vpermt2w\0" "vpermw\0" "vpexpandd\0"
  "vpexpandq\0" "vpextrb\0" "vpextrd\0" "vpextrq\0" "vpextrw\0" "vpgatherdd\0"
  "vpgatherdq\0" "vpgatherqd\0" "vpgatherqq\0" "vphaddbd\0" "vphaddbq\0"
  "vphaddbw\0" "vphaddd\0" "vphadddq\0" "vphaddsw\0" "vphaddubd\0"
  "vphaddubq\0" "vphaddubw\0" "vphaddudq\0" "vphadduwd\0" "vphadduwq\0"
  "vphaddw\0" "vphaddwd\0" "vphaddwq\0" "vphminposuw\0" "vphsubbw\0"
  "vphsubd\0" "vphsubdq\0" "vphsubsw\0" "vphsubw\0" "vphsubwd\0" "vpinsrb\0"
  "vpinsrd\0" "vpinsrq\0" "vpinsrw\0" "vplzcntd\0" "vplzcntq\0" "vpmacsdd\0"
  "vpmacsdqh\0" "vpmacsdql\0" "vpmacssdd\0" "vpmacssdqh\0" "vpmacssdql\0"
  "vpmacsswd\0" "vpmacssww\0" "vpmacswd\0" "vpmacsww\0" "vpmadcsswd\0"
  "vpmadcswd\0" "vpmadd52huq\0" "vpmadd52luq\0" "vpmaddubsw\0" "vpmaddwd\0"
  "vpmaskmovd\0" "vpmaskmovq\0" "vpmaxsb\0" "vpmaxsd\0" "vpmaxsq\0" "vpmaxsw\0"
  "vpmaxub\0" "vpmaxud\0" "vpmaxuq\0" "vpmaxuw\0" "vpminsb\0" "vpminsd\0"
  "vpminsq\0" "vpminsw\0" "vpminub\0" "vpminud\0" "vpminuq\0" "vpminuw\0"
  "vpmovb2m\0" "vpmovd2m\0" "vpmovdb\0" "vpmovdw\0" "vpmovm2b\0" "vpmovm2d\0"
  "vpmovm2q\0" "vpmovm2w\0" "vpmovmskb\0" "vpmovq2m\0" "vpmovqb\0" "vpmovqd\0"
  "vpmovqw\0" "vpmovsdb\0" "vpmovsdw\0" "vpmovsqb\0" "vpmovsqd\0" "vpmovsqw\0"
  "vpmovswb\0" "vpmovsxbd\0" "vpmovsxbq\0" "vpmovsxbw\0" "vpmovsxdq\0"
  "vpmovsxwd\0" "vpmovsxwq\0" "vpmovusdb\0" "vpmovusdw\0" "vpmovusqb\0"
  "vpmovusqd\0" "vpmovusqw\0" "vpmovuswb\0" "vpmovw2m\0" "vpmovwb\0"
  "vpmovzxbd\0" "vpmovzxbq\0" "vpmovzxbw\0" "vpmovzxdq\0" "vpmovzxwd\0"
  "vpmovzxwq\0" "vpmuldq\0" "vpmulhrsw\0" "vpmulhuw\0" "vpmulhw\0" "vpmulld\0"
  "vpmullq\0" "vpmullw\0" "vpmultishiftqb\0" "vpmuludq\0" "vpopcntd\0"
  "vpopcntq\0" "vpor\0" "vpord\0" "vporq\0" "vpperm\0" "vprold\0" "vprolq\0"
  "vprolvd\0" "vprolvq\0" "vprord\0" "vprorq\0" "vprorvd\0" "vprorvq\0"
  "vprotb\0" "vprotd\0" "vprotq\0" "vprotw\0" "vpsadbw\0" "vpscatterdd\0"
  "vpscatterdq\0" "vpscatterqd\0" "vpscatterqq\0" "vpshab\0" "vpshad\0"
  "vpshaq\0" "vpshaw\0" "vpshlb\0" "vpshld\0" "vpshlq\0" "vpshlw\0" "vpshufb\0"
  "vpshufd\0" "vpshufhw\0" "vpshuflw\0" "vpsignb\0" "vpsignd\0" "vpsignw\0"
  "vpslld\0" "vpslldq\0" "vpsllq\0" "vpsllvd\0" "vpsllvq\0" "vpsllvw\0"
  "vpsllw\0" "vpsrad\0" "vpsraq\0" "vpsravd\0" "vpsravq\0" "vpsravw\0"
  "vpsraw\0" "vpsrld\0" "vpsrldq\0" "vpsrlq\0" "vpsrlvd\0" "vpsrlvq\0"
  "vpsrlvw\0" "vpsrlw\0" "vpsubb\0" "vpsubd\0" "vpsubq\0" "vpsubsb\0"
  "vpsubsw\0" "vpsubusb\0" "vpsubusw\0" "vpsubw\0" "vpternlogd\0"
  "vpternlogq\0" "vptest\0" "vptestmb\0" "vptestmd\0" "vptestmq\0" "vptestmw\0"
  "vptestnmb\0" "vptestnmd\0" "vptestnmq\0" "vptestnmw\0" "vpunpckhbw\0"
  "vpunpckhdq\0" "vpunpckhqdq\0" "vpunpckhwd\0" "vpunpcklbw\0" "vpunpckldq\0"
  "vpunpcklqdq\0" "vpunpcklwd\0" "vpxor\0" "vpxord\0" "vpxorq\0" "vrangepd\0"
  "vrangeps\0" "vrangesd\0" "vrangess\0" "vrcp14pd\0" "vrcp14ps\0" "vrcp14sd\0"
  "vrcp14ss\0" "vrcp28pd\0" "vrcp28ps\0" "vrcp28sd\0" "vrcp28ss\0" "vrcpps\0"
  "vrcpss\0" "vreducepd\0" "vreduceps\0" "vreducesd\0" "vreducess\0"
  "vrndscalepd\0" "vrndscaleps\0" "vrndscalesd\0" "vrndscaless\0" "vroundpd\0"
  "vroundps\0" "vroundsd\0" "vroundss\0" "vrsqrt14pd\0" "vrsqrt14ps\0"
  "vrsqrt14sd\0" "vrsqrt14ss\0" "vrsqrt28pd\0" "vrsqrt28ps\0" "vrsqrt28sd\0"
  "vrsqrt28ss\0" "vrsqrtps\0" "vrsqrtss\0" "vscalefpd\0" "vscalefps\0"
  "vscalefsd\0" "vscalefss\0" "vscatterdpd\0" "vscatterdps\0"
  "vscatterpf0dpd\0" "vscatterpf0dps\0" "vscatterpf0qpd\0" "vscatterpf0qps\0"
  "vscatterpf1dpd\0" "vscatterpf1dps\0" "vscatterpf1qpd\0" "vscatterpf1qps\0"
  "vscatterqpd\0" "vscatterqps\0" "vshuff32x4\0" "vshuff64x2\0" "vshufi32x4\0"
  "vshufi64x2\0" "vshufpd\0" "vshufps\0" "vsqrtpd\0" "vsqrtps\0" "vsqrtsd\0"
  "vsqrtss\0" "vstmxcsr\0" "vsubpd\0" "vsubps\0" "vsubsd\0" "vsubss\0"
  "vtestpd\0" "vtestps\0" "vucomisd\0" "vucomiss\0" "vunpckhpd\0" "vunpckhps\0"
  "vunpcklpd\0" "vunpcklps\0" "vxorpd\0" "vxorps\0" "vzeroall\0" "vzeroupper\0"
  "wbinvd\0" "wrfsbase\0" "wrgsbase\0" "wrmsr\0" "xabort\0" "xadd\0" "xbegin\0"
  "xend\0" "xgetbv\0" "xlatb\0" "xrstors\0" "xrstors64\0" "xsavec\0"
  "xsavec64\0" "xsaveopt\0" "xsaveopt64\0" "xsaves\0" "xsaves64\0" "xsetbv\0"
  "xtest";

enum {
  kX86InstMaxLength = 16
};

struct InstNameAZ {
  uint16_t start;
  uint16_t end;
};

static const InstNameAZ X86InstNameAZ[26] = {
  { X86Inst::kIdAaa       , X86Inst::kIdArpl       + 1 },
  { X86Inst::kIdBextr     , X86Inst::kIdBzhi       + 1 },
  { X86Inst::kIdCall      , X86Inst::kIdCwde       + 1 },
  { X86Inst::kIdDaa       , X86Inst::kIdDpps       + 1 },
  { X86Inst::kIdEmms      , X86Inst::kIdExtrq      + 1 },
  { X86Inst::kIdF2xm1     , X86Inst::kIdFyl2xp1    + 1 },
  { X86Inst::kIdNone      , X86Inst::kIdNone       + 1 },
  { X86Inst::kIdHaddpd    , X86Inst::kIdHsubps     + 1 },
  { X86Inst::kIdIdiv      , X86Inst::kIdIretw      + 1 },
  { X86Inst::kIdJa        , X86Inst::kIdJz         + 1 },
  { X86Inst::kIdKaddb     , X86Inst::kIdKxorw      + 1 },
  { X86Inst::kIdLahf      , X86Inst::kIdLzcnt      + 1 },
  { X86Inst::kIdMaskmovdqu, X86Inst::kIdMwaitx     + 1 },
  { X86Inst::kIdNeg       , X86Inst::kIdNot        + 1 },
  { X86Inst::kIdOr        , X86Inst::kIdOuts       + 1 },
  { X86Inst::kIdPabsb     , X86Inst::kIdPxor       + 1 },
  { X86Inst::kIdNone      , X86Inst::kIdNone       + 1 },
  { X86Inst::kIdRcl       , X86Inst::kIdRsqrtss    + 1 },
  { X86Inst::kIdSahf      , X86Inst::kIdSysret64   + 1 },
  { X86Inst::kIdT1mskc    , X86Inst::kIdTzmsk      + 1 },
  { X86Inst::kIdUcomisd   , X86Inst::kIdUnpcklps   + 1 },
  { X86Inst::kIdV4fmaddps , X86Inst::kIdVzeroupper + 1 },
  { X86Inst::kIdWbinvd    , X86Inst::kIdWrmsr      + 1 },
  { X86Inst::kIdXabort    , X86Inst::kIdXtest      + 1 },
  { X86Inst::kIdNone      , X86Inst::kIdNone       + 1 },
  { X86Inst::kIdNone      , X86Inst::kIdNone       + 1 }
};
// ----------------------------------------------------------------------------
// ${nameData:End}

uint32_t X86Inst::getIdByName(const char* name, size_t len) noexcept {
  if (ASMJIT_UNLIKELY(!name))
    return Inst::kIdNone;

  if (len == Globals::kNullTerminated)
    len = std::strlen(name);

  if (ASMJIT_UNLIKELY(len == 0 || len > kX86InstMaxLength))
    return Inst::kIdNone;

  uint32_t prefix = uint32_t(name[0]) - 'a';
  if (ASMJIT_UNLIKELY(prefix > 'z' - 'a'))
    return Inst::kIdNone;

  uint32_t index = X86InstNameAZ[prefix].start;
  if (ASMJIT_UNLIKELY(!index))
    return Inst::kIdNone;

  const char* nameData = X86InstDB::nameData;
  const X86Inst* instData = X86InstDB::instData;

  const X86Inst* base = instData + index;
  const X86Inst* end  = instData + X86InstNameAZ[prefix].end;

  for (size_t lim = (size_t)(end - base); lim != 0; lim >>= 1) {
    const X86Inst* cur = base + (lim >> 1);
    int result = StringUtils::cmpInstName(nameData + cur[0].getNameDataIndex(), name, len);

    if (result < 0) {
      base = cur + 1;
      lim--;
      continue;
    }

    if (result > 0)
      continue;

    return uint32_t((size_t)(cur - instData));
  }

  return Inst::kIdNone;
}

const char* X86Inst::getNameById(uint32_t id) noexcept {
  if (ASMJIT_UNLIKELY(id >= X86Inst::_kIdCount))
    return nullptr;
  return X86Inst::getInst(id).getName();
}
#else
const char X86InstDB::nameData[] = "";
#endif

// ============================================================================
// [asmjit::X86Inst - Validation]
// ============================================================================

#ifndef ASMJIT_DISABLE_INST_API
// ${signatureData:Begin}
// ------------------- Automatically generated, do not edit -------------------
#define FLAG(flag) X86Inst::kOp##flag
#define MEM(mem) X86Inst::kMemOp##mem
#define OSIGNATURE(flags, memFlags, extFlags, regId) \
  { uint32_t(flags), uint16_t(memFlags), uint8_t(extFlags), uint8_t(regId) }
const X86Inst::OSignature X86InstDB::oSignatureData[] = {
  OSIGNATURE(0, 0, 0, 0xFF),
  OSIGNATURE(FLAG(W) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Mem), MEM(Any) | MEM(M8), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(GpbLo) | FLAG(GpbHi), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Gpw) | FLAG(Seg) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpw), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Gpd) | FLAG(Seg) | FLAG(Mem), MEM(Any) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpd), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Gpq) | FLAG(Seg) | FLAG(Cr) | FLAG(Dr) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpq), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Mem), MEM(M8), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(I8) | FLAG(U8), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Gpw) | FLAG(Mem), MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(I16) | FLAG(U16), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Gpd) | FLAG(Mem), MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(I32) | FLAG(U32), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(I32), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Gpq), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Seg) | FLAG(Cr) | FLAG(Dr) | FLAG(Mem) | FLAG(I64) | FLAG(U64), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(GpbLo) | FLAG(GpbHi), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(Any) | MEM(M8), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Gpw), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Seg) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Gpd), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Seg) | FLAG(Mem), MEM(Any) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mem), MEM(Any) | MEM(M16) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Seg), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Seg), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(Any) | MEM(M16) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Cr) | FLAG(Dr), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Cr) | FLAG(Dr), 0, 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Mem), MEM(M8), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Gpw) | FLAG(Mem), MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Gpd) | FLAG(Mem), MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpq) | FLAG(I32), 0, 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M16) | MEM(M32) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(I8), 0, 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Mem), MEM(Any) | MEM(M8), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Gpw) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Gpd) | FLAG(Mem), MEM(Any) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(GpbLo) | FLAG(GpbHi), 0, 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Gpw), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Gpd), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(Any) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Gpq), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M8) | MEM(M16) | MEM(M32) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Gpq), 0, 0, 0x01),
  OSIGNATURE(FLAG(R) | FLAG(Gpq) | FLAG(Mem) | FLAG(I32) | FLAG(U32), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Mem), MEM(M8), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpw) | FLAG(Mem), MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpd) | FLAG(Mem), MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M16) | MEM(M32) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Mem), MEM(Any) | MEM(M8), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpw) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpd) | FLAG(Mem), MEM(Any) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Implicit) | FLAG(Gpw), 0, 0, 0x01),
  OSIGNATURE(FLAG(W) | FLAG(Implicit) | FLAG(Gpw), 0, 0, 0x04),
  OSIGNATURE(FLAG(W) | FLAG(Implicit) | FLAG(Gpd), 0, 0, 0x04),
  OSIGNATURE(FLAG(X) | FLAG(Implicit) | FLAG(Gpd), 0, 0, 0x01),
  OSIGNATURE(FLAG(W) | FLAG(Implicit) | FLAG(Gpq), 0, 0, 0x04),
  OSIGNATURE(FLAG(X) | FLAG(Implicit) | FLAG(Gpq), 0, 0, 0x01),
  OSIGNATURE(FLAG(R) | FLAG(Gpw) | FLAG(Mem) | FLAG(I8) | FLAG(I16), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpd) | FLAG(Mem) | FLAG(I8) | FLAG(I32), MEM(Any) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpq) | FLAG(Mem) | FLAG(I8) | FLAG(I32), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(I8) | FLAG(I16) | FLAG(U16), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(I8) | FLAG(I32) | FLAG(U32), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(I8) | FLAG(I32), 0, 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Gpw) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x01),
  OSIGNATURE(FLAG(X) | FLAG(Gpd) | FLAG(Mem), MEM(Any) | MEM(M32), 0, 0x01),
  OSIGNATURE(FLAG(X) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x01),
  OSIGNATURE(FLAG(X) | FLAG(Mem), MEM(Any) | MEM(M8), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mem), MEM(Any) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpq) | FLAG(Mm) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Gpq) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Xmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M128), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mem), MEM(Any) | MEM(M128), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Ymm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Ymm) | FLAG(Mem), MEM(Any) | MEM(M256), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mem), MEM(Any) | MEM(M256), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Ymm), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Zmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Zmm) | FLAG(Mem), MEM(Any) | MEM(M512), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mem), MEM(Any) | MEM(M512), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Zmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm) | FLAG(Mem) | FLAG(U8), MEM(Any) | MEM(M128), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(Any) | MEM(M128), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(U8), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Ymm) | FLAG(Mem) | FLAG(U8), MEM(Any) | MEM(M256), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(Any) | MEM(M256), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Zmm) | FLAG(Mem) | FLAG(U8), MEM(Any) | MEM(M512), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(Any) | MEM(M512), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Xmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Vm), MEM(Vm32x), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Ymm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Vm), MEM(Vm32y), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Zmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Vm), MEM(Vm32z), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Vm), MEM(Vm64x), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Vm), MEM(Vm64y), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Vm), MEM(Vm64z), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(GpbLo), 0, 0, 0x01),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(Gpw), 0, 0, 0x01),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(Gpd), 0, 0, 0x01),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(Gpq), 0, 0, 0x01),
  OSIGNATURE(FLAG(X) | FLAG(Implicit) | FLAG(Gpw), 0, 0, 0x04),
  OSIGNATURE(FLAG(X) | FLAG(Implicit) | FLAG(Gpd), 0, 0, 0x04),
  OSIGNATURE(FLAG(X) | FLAG(Implicit) | FLAG(Gpq), 0, 0, 0x04),
  OSIGNATURE(FLAG(W) | FLAG(Gpw) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M16) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Seg), 0, 0, 0x1A),
  OSIGNATURE(FLAG(W) | FLAG(Seg), 0, 0, 0x60),
  OSIGNATURE(FLAG(R) | FLAG(Gpw) | FLAG(Gpq) | FLAG(Mem) | FLAG(I8) | FLAG(I16) | FLAG(I32), MEM(Any) | MEM(M16) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Seg), 0, 0, 0x1E),
  OSIGNATURE(FLAG(R) | FLAG(Seg), 0, 0, 0x60),
  OSIGNATURE(FLAG(R) | FLAG(Vm), MEM(Vm64x) | MEM(Vm64y), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Mem), MEM(Any) | MEM(M128), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Mem), MEM(Any) | MEM(M256), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(U4), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(M32) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Fp), 0, 0, 0x01),
  OSIGNATURE(FLAG(R) | FLAG(Fp), 0, 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Fp), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Fp), 0, 0, 0x01),
  OSIGNATURE(FLAG(X) | FLAG(Mem), MEM(M32) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(Any) | MEM(M48), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(Any) | MEM(M80), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(GpbLo) | FLAG(U8), 0, 0, 0x02),
  OSIGNATURE(FLAG(W) | FLAG(K) | FLAG(Xmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(K) | FLAG(Ymm), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(K), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M128), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Ymm) | FLAG(Mem), MEM(Any) | MEM(M256), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Zmm) | FLAG(Mem), MEM(Any) | MEM(M512), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpq) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Xmm) | FLAG(Ymm), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M256), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Vm), MEM(Vm32x), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Vm), MEM(Vm32y), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Vm), MEM(Vm32z), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Vm), MEM(Vm64x), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Vm), MEM(Vm64y), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Vm), MEM(Vm64z), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Bnd), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Bnd), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Bnd) | FLAG(Mem), MEM(Any), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mem), MEM(Any), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpq) | FLAG(Mem) | FLAG(I32) | FLAG(I64) | FLAG(Rel32), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Mem), MEM(M8) | MEM(M16) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Gpq) | FLAG(Mem), MEM(M8) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Gpw) | FLAG(Gpd), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Fp) | FLAG(Mem), MEM(M32) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(Gpw) | FLAG(Gpd), 0, 0, 0x02),
  OSIGNATURE(FLAG(R) | FLAG(I32) | FLAG(I64) | FLAG(Rel8), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(Gpd) | FLAG(Gpq), 0, 0, 0x02),
  OSIGNATURE(FLAG(R) | FLAG(Gpq) | FLAG(Mem) | FLAG(I32) | FLAG(I64) | FLAG(Rel8) | FLAG(Rel32), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq) | FLAG(K) | FLAG(Mem), MEM(Any) | MEM(M8), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M8), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(K), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpd) | FLAG(Gpq) | FLAG(K) | FLAG(Mem), MEM(Any) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Gpd) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpq) | FLAG(K) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq) | FLAG(K) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Implicit) | FLAG(Gpw) | FLAG(Gpd), 0, 0, 0x02),
  OSIGNATURE(FLAG(X) | FLAG(Implicit) | FLAG(Gpd) | FLAG(Gpq), 0, 0, 0x02),
  OSIGNATURE(FLAG(W) | FLAG(Gpd) | FLAG(Gpq), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mm) | FLAG(Xmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpd) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mm) | FLAG(Xmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Gpw) | FLAG(Mem), MEM(M8) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(Gpd), 0, 0, 0x04),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(Gpq), 0, 0, 0x04),
  OSIGNATURE(FLAG(R) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Mem), MEM(M16) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mm) | FLAG(Mem), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Mm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mm) | FLAG(Mem) | FLAG(U8), MEM(Any) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(U16), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm) | FLAG(Mem), MEM(M32) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm) | FLAG(Mem), MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm) | FLAG(Ymm) | FLAG(Mem), MEM(M128) | MEM(M256), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm) | FLAG(U8), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Vm), MEM(Vm64x) | MEM(Vm64y), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm) | FLAG(Ymm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(Xmm), 0, 0, 0x01),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(Mib), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(Any), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mem), MEM(Mib), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Gpd) | FLAG(Gpq), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(Mem), MEM(BaseOnly) | MEM(Ds), 0, 0x01),
  OSIGNATURE(FLAG(X) | FLAG(Implicit) | FLAG(Mem), MEM(BaseOnly) | MEM(Ds), 0, 0x40),
  OSIGNATURE(FLAG(X) | FLAG(Implicit) | FLAG(Mem), MEM(BaseOnly) | MEM(Es), 0, 0x80),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(Gpq), 0, 0, 0x02),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(Gpq), 0, 0, 0x08),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(Gpd), 0, 0, 0x02),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(Gpd), 0, 0, 0x08),
  OSIGNATURE(FLAG(W) | FLAG(Implicit) | FLAG(Gpd), 0, 0, 0x08),
  OSIGNATURE(FLAG(X) | FLAG(Implicit) | FLAG(Gpd), 0, 0, 0x02),
  OSIGNATURE(FLAG(R) | FLAG(Gpd) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M32) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mem), MEM(Any) | MEM(M80), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(M16) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(M16) | MEM(M32) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mem), MEM(M16) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Mem), MEM(M16) | MEM(M32) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Fp) | FLAG(Mem), MEM(M32) | MEM(M64) | MEM(M80), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Gpw) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x01),
  OSIGNATURE(FLAG(W) | FLAG(Fp) | FLAG(Mem), MEM(M32) | MEM(M64), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Fp) | FLAG(Mem), MEM(M32) | MEM(M64) | MEM(M80), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(GpbLo) | FLAG(Gpw) | FLAG(Gpd), 0, 0, 0x01),
  OSIGNATURE(FLAG(R) | FLAG(Gpw) | FLAG(U8), 0, 0, 0x04),
  OSIGNATURE(FLAG(W) | FLAG(Mem), MEM(BaseOnly) | MEM(Es), 0, 0x80),
  OSIGNATURE(FLAG(R) | FLAG(Gpw), 0, 0, 0x04),
  OSIGNATURE(FLAG(R) | FLAG(I32) | FLAG(I64) | FLAG(Rel8) | FLAG(Rel32), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Implicit) | FLAG(GpbHi), 0, 0, 0x01),
  OSIGNATURE(FLAG(W) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(Any) | MEM(M8) | MEM(M16) | MEM(M32) | MEM(M48) | MEM(M64) | MEM(M80) | MEM(M128) | MEM(M256) | MEM(M512) | MEM(M1024), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Implicit) | FLAG(GpbLo) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq), 0, 0, 0x01),
  OSIGNATURE(FLAG(X) | FLAG(Implicit) | FLAG(Mem), MEM(BaseOnly) | MEM(Ds), 0, 0x80),
  OSIGNATURE(FLAG(R) | FLAG(GpbLo) | FLAG(Gpw) | FLAG(Gpd), 0, 0, 0x01),
  OSIGNATURE(FLAG(R) | FLAG(Mem), MEM(BaseOnly) | MEM(Ds), 0, 0x40),
  OSIGNATURE(FLAG(W) | FLAG(Implicit) | FLAG(Gpd), 0, 0, 0x02),
  OSIGNATURE(FLAG(W) | FLAG(Implicit) | FLAG(Xmm), 0, 0, 0x01),
  OSIGNATURE(FLAG(R) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq) | FLAG(Mem), MEM(Any) | MEM(M8), 0, 0x00),
  OSIGNATURE(FLAG(X) | FLAG(Mm) | FLAG(Xmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Implicit) | FLAG(Gpd), 0, 0, 0x01),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(GpbHi), 0, 0, 0x01),
  OSIGNATURE(FLAG(R) | FLAG(Implicit) | FLAG(GpbLo) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq), 0, 0, 0x01),
  OSIGNATURE(FLAG(W) | FLAG(Ymm) | FLAG(Zmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(W) | FLAG(Xmm) | FLAG(Ymm) | FLAG(Zmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Ymm) | FLAG(Zmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm) | FLAG(Ymm) | FLAG(Zmm) | FLAG(Mem), MEM(M128) | MEM(M256) | MEM(M512), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(GpbLo) | FLAG(GpbHi) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M8), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpd) | FLAG(Gpq) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M32), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpw) | FLAG(Gpd) | FLAG(Gpq) | FLAG(Xmm) | FLAG(Mem), MEM(Any) | MEM(M16), 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Xmm) | FLAG(Ymm) | FLAG(Zmm), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(Gpd) | FLAG(Gpq), 0, 0, 0x00),
  OSIGNATURE(FLAG(R) | FLAG(I32) | FLAG(I64) | FLAG(Rel32), 0, 0, 0x00)
};
#undef OSIGNATURE
#undef MEM
#undef FLAG

#define ISIGNATURE(count, x86, x64, implicit, o0, o1, o2, o3, o4, o5) \
  { count, (x86 ? uint8_t(X86Inst::kArchMaskX86) : uint8_t(0)) |      \
           (x64 ? uint8_t(X86Inst::kArchMaskX64) : uint8_t(0)) ,      \
    implicit,                                                         \
    0,                                                                \
    { o0, o1, o2, o3, o4, o5 }                                        \
  }
const X86Inst::ISignature X86InstDB::iSignatureData[] = {
  ISIGNATURE(2, 1, 1, 0, 1  , 2  , 0  , 0  , 0  , 0  ), // #0   {W:r8lo|r8hi|m8|mem, R:r8lo|r8hi}
  ISIGNATURE(2, 1, 1, 0, 3  , 4  , 0  , 0  , 0  , 0  ), //      {W:r16|m16|mem|sreg, R:r16}
  ISIGNATURE(2, 1, 1, 0, 5  , 6  , 0  , 0  , 0  , 0  ), //      {W:r32|m32|mem|sreg, R:r32}
  ISIGNATURE(2, 0, 1, 0, 7  , 8  , 0  , 0  , 0  , 0  ), //      {W:r64|m64|mem|sreg|creg|dreg, R:r64}
  ISIGNATURE(2, 1, 1, 0, 9  , 10 , 0  , 0  , 0  , 0  ), //      {W:r8lo|r8hi|m8, R:i8|u8}
  ISIGNATURE(2, 1, 1, 0, 11 , 12 , 0  , 0  , 0  , 0  ), //      {W:r16|m16, R:i16|u16}
  ISIGNATURE(2, 1, 1, 0, 13 , 14 , 0  , 0  , 0  , 0  ), //      {W:r32|m32, R:i32|u32}
  ISIGNATURE(2, 0, 1, 0, 15 , 16 , 0  , 0  , 0  , 0  ), //      {W:r64|m64|mem, R:i32}
  ISIGNATURE(2, 0, 1, 0, 17 , 18 , 0  , 0  , 0  , 0  ), //      {W:r64, R:i64|u64|m64|mem|sreg|creg|dreg}
  ISIGNATURE(2, 1, 1, 0, 19 , 20 , 0  , 0  , 0  , 0  ), //      {W:r8lo|r8hi, R:m8|mem}
  ISIGNATURE(2, 1, 1, 0, 21 , 22 , 0  , 0  , 0  , 0  ), //      {W:r16, R:m16|mem|sreg}
  ISIGNATURE(2, 1, 1, 0, 23 , 24 , 0  , 0  , 0  , 0  ), //      {W:r32, R:m32|mem|sreg}
  ISIGNATURE(2, 1, 1, 0, 25 , 26 , 0  , 0  , 0  , 0  ), //      {W:m16|mem|m64, R:sreg}
  ISIGNATURE(2, 1, 1, 0, 27 , 28 , 0  , 0  , 0  , 0  ), //      {W:sreg, R:m16|mem|m64}
  ISIGNATURE(2, 1, 0, 0, 23 , 29 , 0  , 0  , 0  , 0  ), //      {W:r32, R:creg|dreg}
  ISIGNATURE(2, 1, 0, 0, 30 , 6  , 0  , 0  , 0  , 0  ), //      {W:creg|dreg, R:r32}
  ISIGNATURE(2, 1, 1, 0, 31 , 10 , 0  , 0  , 0  , 0  ), // #16  {X:r8lo|r8hi|m8, R:i8|u8}
  ISIGNATURE(2, 1, 1, 0, 32 , 12 , 0  , 0  , 0  , 0  ), //      {X:r16|m16, R:i16|u16}
  ISIGNATURE(2, 1, 1, 0, 33 , 14 , 0  , 0  , 0  , 0  ), //      {X:r32|m32, R:i32|u32}
  ISIGNATURE(2, 0, 1, 0, 34 , 35 , 0  , 0  , 0  , 0  ), //      {X:r64|m64|mem, R:i32|r64}
  ISIGNATURE(2, 1, 1, 0, 36 , 37 , 0  , 0  , 0  , 0  ), //      {X:r16|m16|r32|m32|r64|m64|mem, R:i8}
  ISIGNATURE(2, 1, 1, 0, 38 , 2  , 0  , 0  , 0  , 0  ), //      {X:r8lo|r8hi|m8|mem, R:r8lo|r8hi}
  ISIGNATURE(2, 1, 1, 0, 39 , 4  , 0  , 0  , 0  , 0  ), //      {X:r16|m16|mem, R:r16}
  ISIGNATURE(2, 1, 1, 0, 40 , 6  , 0  , 0  , 0  , 0  ), //      {X:r32|m32|mem, R:r32}
  ISIGNATURE(2, 1, 1, 0, 41 , 20 , 0  , 0  , 0  , 0  ), //      {X:r8lo|r8hi, R:m8|mem}
  ISIGNATURE(2, 1, 1, 0, 42 , 43 , 0  , 0  , 0  , 0  ), //      {X:r16, R:m16|mem}
  ISIGNATURE(2, 1, 1, 0, 44 , 45 , 0  , 0  , 0  , 0  ), //      {X:r32, R:m32|mem}
  ISIGNATURE(2, 0, 1, 0, 46 , 47 , 0  , 0  , 0  , 0  ), //      {X:r64, R:m64|mem}
  ISIGNATURE(2, 1, 1, 0, 48 , 10 , 0  , 0  , 0  , 0  ), // #28  {X:r8lo|r8hi|m8|r16|m16|r32|m32|r64|m64|mem, R:i8|u8}
  ISIGNATURE(2, 1, 1, 0, 32 , 12 , 0  , 0  , 0  , 0  ), //      {X:r16|m16, R:i16|u16}
  ISIGNATURE(2, 1, 1, 0, 33 , 14 , 0  , 0  , 0  , 0  ), //      {X:r32|m32, R:i32|u32}
  ISIGNATURE(2, 0, 1, 0, 49 , 14 , 0  , 0  , 0  , 0  ), //      {X:rax, R:u32|i32}
  ISIGNATURE(2, 0, 1, 0, 46 , 50 , 0  , 0  , 0  , 0  ), //      {X:r64, R:u32|i32|r64|m64|mem}
  ISIGNATURE(2, 0, 1, 0, 51 , 35 , 0  , 0  , 0  , 0  ), //      {X:m64|mem, R:i32|r64}
  ISIGNATURE(2, 1, 1, 0, 38 , 2  , 0  , 0  , 0  , 0  ), //      {X:r8lo|r8hi|m8|mem, R:r8lo|r8hi}
  ISIGNATURE(2, 1, 1, 0, 39 , 4  , 0  , 0  , 0  , 0  ), //      {X:r16|m16|mem, R:r16}
  ISIGNATURE(2, 1, 1, 0, 40 , 6  , 0  , 0  , 0  , 0  ), //      {X:r32|m32|mem, R:r32}
  ISIGNATURE(2, 1, 1, 0, 41 , 20 , 0  , 0  , 0  , 0  ), //      {X:r8lo|r8hi, R:m8|mem}
  ISIGNATURE(2, 1, 1, 0, 42 , 43 , 0  , 0  , 0  , 0  ), //      {X:r16, R:m16|mem}
  ISIGNATURE(2, 1, 1, 0, 44 , 45 , 0  , 0  , 0  , 0  ), //      {X:r32, R:m32|mem}
  ISIGNATURE(2, 1, 1, 0, 52 , 10 , 0  , 0  , 0  , 0  ), // #40  {R:r8lo|r8hi|m8, R:i8|u8}
  ISIGNATURE(2, 1, 1, 0, 53 , 12 , 0  , 0  , 0  , 0  ), //      {R:r16|m16, R:i16|u16}
  ISIGNATURE(2, 1, 1, 0, 54 , 14 , 0  , 0  , 0  , 0  ), //      {R:r32|m32, R:i32|u32}
  ISIGNATURE(2, 0, 1, 0, 55 , 35 , 0  , 0  , 0  , 0  ), //      {R:r64|m64|mem, R:i32|r64}
  ISIGNATURE(2, 1, 1, 0, 56 , 37 , 0  , 0  , 0  , 0  ), //      {R:r16|m16|r32|m32|r64|m64|mem, R:i8}
  ISIGNATURE(2, 1, 1, 0, 57 , 2  , 0  , 0  , 0  , 0  ), //      {R:r8lo|r8hi|m8|mem, R:r8lo|r8hi}
  ISIGNATURE(2, 1, 1, 0, 58 , 4  , 0  , 0  , 0  , 0  ), //      {R:r16|m16|mem, R:r16}
  ISIGNATURE(2, 1, 1, 0, 59 , 6  , 0  , 0  , 0  , 0  ), //      {R:r32|m32|mem, R:r32}
  ISIGNATURE(2, 1, 1, 0, 2  , 20 , 0  , 0  , 0  , 0  ), //      {R:r8lo|r8hi, R:m8|mem}
  ISIGNATURE(2, 1, 1, 0, 4  , 43 , 0  , 0  , 0  , 0  ), //      {R:r16, R:m16|mem}
  ISIGNATURE(2, 1, 1, 0, 6  , 45 , 0  , 0  , 0  , 0  ), //      {R:r32, R:m32|mem}
  ISIGNATURE(2, 0, 1, 0, 8  , 47 , 0  , 0  , 0  , 0  ), //      {R:r64, R:m64|mem}
  ISIGNATURE(2, 1, 1, 1, 60 , 57 , 0  , 0  , 0  , 0  ), // #52  {X:<ax>, R:r8lo|r8hi|m8|mem}
  ISIGNATURE(3, 1, 1, 2, 61 , 60 , 58 , 0  , 0  , 0  ), //      {W:<dx>, X:<ax>, R:r16|m16|mem}
  ISIGNATURE(3, 1, 1, 2, 62 , 63 , 59 , 0  , 0  , 0  ), //      {W:<edx>, X:<eax>, R:r32|m32|mem}
  ISIGNATURE(3, 0, 1, 2, 64 , 65 , 55 , 0  , 0  , 0  ), //      {W:<rdx>, X:<rax>, R:r64|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 42 , 66 , 0  , 0  , 0  , 0  ), //      {X:r16, R:r16|m16|mem|i8|i16}
  ISIGNATURE(2, 1, 1, 0, 44 , 67 , 0  , 0  , 0  , 0  ), //      {X:r32, R:r32|m32|mem|i8|i32}
  ISIGNATURE(2, 0, 1, 0, 46 , 68 , 0  , 0  , 0  , 0  ), //      {X:r64, R:r64|m64|mem|i8|i32}
  ISIGNATURE(3, 1, 1, 0, 21 , 58 , 69 , 0  , 0  , 0  ), //      {W:r16, R:r16|m16|mem, R:i8|i16|u16}
  ISIGNATURE(3, 1, 1, 0, 23 , 59 , 70 , 0  , 0  , 0  ), //      {W:r32, R:r32|m32|mem, R:i8|i32|u32}
  ISIGNATURE(3, 0, 1, 0, 17 , 55 , 71 , 0  , 0  , 0  ), //      {W:r64, R:r64|m64|mem, R:i8|i32}
  ISIGNATURE(2, 1, 1, 0, 39 , 42 , 0  , 0  , 0  , 0  ), // #62  {X:r16|m16|mem, X:r16}
  ISIGNATURE(2, 1, 1, 0, 40 , 44 , 0  , 0  , 0  , 0  ), //      {X:r32|m32|mem, X:r32}
  ISIGNATURE(2, 0, 1, 0, 34 , 46 , 0  , 0  , 0  , 0  ), //      {X:r64|m64|mem, X:r64}
  ISIGNATURE(2, 1, 1, 0, 42 , 72 , 0  , 0  , 0  , 0  ), //      {X:r16, X:ax|m16|mem}
  ISIGNATURE(2, 1, 1, 0, 44 , 73 , 0  , 0  , 0  , 0  ), //      {X:r32, X:eax|m32|mem}
  ISIGNATURE(2, 0, 1, 0, 46 , 74 , 0  , 0  , 0  , 0  ), //      {X:r64, X:rax|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 38 , 41 , 0  , 0  , 0  , 0  ), //      {X:r8lo|r8hi|m8|mem, X:r8lo|r8hi}
  ISIGNATURE(2, 1, 1, 0, 41 , 75 , 0  , 0  , 0  , 0  ), //      {X:r8lo|r8hi, X:m8|mem}
  ISIGNATURE(2, 1, 1, 0, 52 , 10 , 0  , 0  , 0  , 0  ), // #70  {R:r8lo|r8hi|m8, R:i8|u8}
  ISIGNATURE(2, 1, 1, 0, 53 , 12 , 0  , 0  , 0  , 0  ), //      {R:r16|m16, R:i16|u16}
  ISIGNATURE(2, 1, 1, 0, 54 , 14 , 0  , 0  , 0  , 0  ), //      {R:r32|m32, R:i32|u32}
  ISIGNATURE(2, 0, 1, 0, 55 , 35 , 0  , 0  , 0  , 0  ), //      {R:r64|m64|mem, R:i32|r64}
  ISIGNATURE(2, 1, 1, 0, 57 , 2  , 0  , 0  , 0  , 0  ), //      {R:r8lo|r8hi|m8|mem, R:r8lo|r8hi}
  ISIGNATURE(2, 1, 1, 0, 58 , 4  , 0  , 0  , 0  , 0  ), //      {R:r16|m16|mem, R:r16}
  ISIGNATURE(2, 1, 1, 0, 59 , 6  , 0  , 0  , 0  , 0  ), //      {R:r32|m32|mem, R:r32}
  ISIGNATURE(2, 1, 1, 0, 21 , 43 , 0  , 0  , 0  , 0  ), // #77  {W:r16, R:m16|mem}
  ISIGNATURE(2, 1, 1, 0, 23 , 45 , 0  , 0  , 0  , 0  ), //      {W:r32, R:m32|mem}
  ISIGNATURE(2, 0, 1, 0, 17 , 47 , 0  , 0  , 0  , 0  ), //      {W:r64, R:m64|mem}
  ISIGNATURE(2, 1, 1, 0, 76 , 4  , 0  , 0  , 0  , 0  ), //      {W:m16|mem, R:r16}
  ISIGNATURE(2, 1, 1, 0, 77 , 6  , 0  , 0  , 0  , 0  ), // #81  {W:m32|mem, R:r32}
  ISIGNATURE(2, 0, 1, 0, 78 , 8  , 0  , 0  , 0  , 0  ), //      {W:m64|mem, R:r64}
  ISIGNATURE(2, 1, 1, 0, 79 , 80 , 0  , 0  , 0  , 0  ), // #83  {W:mm, R:mm|m64|mem|r64|xmm}
  ISIGNATURE(2, 1, 1, 0, 81 , 82 , 0  , 0  , 0  , 0  ), //      {W:m64|mem|r64|xmm, R:mm}
  ISIGNATURE(2, 0, 1, 0, 15 , 83 , 0  , 0  , 0  , 0  ), //      {W:r64|m64|mem, R:xmm}
  ISIGNATURE(2, 0, 1, 0, 84 , 55 , 0  , 0  , 0  , 0  ), //      {W:xmm, R:r64|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 84 , 85 , 0  , 0  , 0  , 0  ), // #87  {W:xmm, R:xmm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 78 , 83 , 0  , 0  , 0  , 0  ), // #88  {W:m64|mem, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 84 , 86 , 0  , 0  , 0  , 0  ), // #89  {W:xmm, R:xmm|m128|mem}
  ISIGNATURE(2, 1, 1, 0, 87 , 83 , 0  , 0  , 0  , 0  ), // #90  {W:m128|mem, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 88 , 89 , 0  , 0  , 0  , 0  ), //      {W:ymm, R:ymm|m256|mem}
  ISIGNATURE(2, 1, 1, 0, 90 , 91 , 0  , 0  , 0  , 0  ), //      {W:m256|mem, R:ymm}
  ISIGNATURE(2, 1, 1, 0, 92 , 93 , 0  , 0  , 0  , 0  ), // #93  {W:zmm, R:zmm|m512|mem}
  ISIGNATURE(2, 1, 1, 0, 94 , 95 , 0  , 0  , 0  , 0  ), //      {W:m512|mem, R:zmm}
  ISIGNATURE(3, 1, 1, 0, 84 , 83 , 96 , 0  , 0  , 0  ), // #95  {W:xmm, R:xmm, R:xmm|m128|mem|u8}
  ISIGNATURE(3, 1, 1, 0, 84 , 97 , 98 , 0  , 0  , 0  ), //      {W:xmm, R:m128|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 88 , 91 , 99 , 0  , 0  , 0  ), //      {W:ymm, R:ymm, R:ymm|m256|mem|u8}
  ISIGNATURE(3, 1, 1, 0, 88 , 100, 98 , 0  , 0  , 0  ), //      {W:ymm, R:m256|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 92 , 95 , 101, 0  , 0  , 0  ), //      {W:zmm, R:zmm, R:zmm|m512|mem|u8}
  ISIGNATURE(3, 1, 1, 0, 92 , 102, 98 , 0  , 0  , 0  ), //      {W:zmm, R:m512|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 84 , 83 , 96 , 0  , 0  , 0  ), // #101 {W:xmm, R:xmm, R:u8|xmm|m128|mem}
  ISIGNATURE(3, 1, 1, 0, 88 , 91 , 96 , 0  , 0  , 0  ), //      {W:ymm, R:ymm, R:u8|xmm|m128|mem}
  ISIGNATURE(3, 1, 1, 0, 84 , 97 , 98 , 0  , 0  , 0  ), //      {W:xmm, R:m128|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 88 , 100, 98 , 0  , 0  , 0  ), //      {W:ymm, R:m256|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 92 , 95 , 96 , 0  , 0  , 0  ), //      {W:zmm, R:zmm, R:xmm|m128|mem|u8}
  ISIGNATURE(3, 1, 1, 0, 92 , 102, 98 , 0  , 0  , 0  ), //      {W:zmm, R:m512|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 84 , 83 , 96 , 0  , 0  , 0  ), // #107 {W:xmm, R:xmm, R:xmm|m128|mem|u8}
  ISIGNATURE(3, 1, 1, 0, 84 , 97 , 98 , 0  , 0  , 0  ), //      {W:xmm, R:m128|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 88 , 91 , 96 , 0  , 0  , 0  ), //      {W:ymm, R:ymm, R:xmm|m128|mem|u8}
  ISIGNATURE(3, 1, 1, 0, 88 , 100, 98 , 0  , 0  , 0  ), //      {W:ymm, R:m256|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 92 , 95 , 96 , 0  , 0  , 0  ), //      {W:zmm, R:zmm, R:xmm|m128|mem|u8}
  ISIGNATURE(3, 1, 1, 0, 92 , 102, 98 , 0  , 0  , 0  ), //      {W:zmm, R:m512|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 103, 104, 103, 0  , 0  , 0  ), // #113 {X:xmm, R:vm32x, X:xmm}
  ISIGNATURE(3, 1, 1, 0, 105, 104, 105, 0  , 0  , 0  ), //      {X:ymm, R:vm32x, X:ymm}
  ISIGNATURE(2, 1, 1, 0, 103, 104, 0  , 0  , 0  , 0  ), //      {X:xmm, R:vm32x}
  ISIGNATURE(2, 1, 1, 0, 105, 106, 0  , 0  , 0  , 0  ), //      {X:ymm, R:vm32y}
  ISIGNATURE(2, 1, 1, 0, 107, 108, 0  , 0  , 0  , 0  ), //      {X:zmm, R:vm32z}
  ISIGNATURE(3, 1, 1, 0, 103, 104, 103, 0  , 0  , 0  ), // #118 {X:xmm, R:vm32x, X:xmm}
  ISIGNATURE(3, 1, 1, 0, 105, 106, 105, 0  , 0  , 0  ), //      {X:ymm, R:vm32y, X:ymm}
  ISIGNATURE(2, 1, 1, 0, 103, 104, 0  , 0  , 0  , 0  ), //      {X:xmm, R:vm32x}
  ISIGNATURE(2, 1, 1, 0, 105, 106, 0  , 0  , 0  , 0  ), //      {X:ymm, R:vm32y}
  ISIGNATURE(2, 1, 1, 0, 107, 108, 0  , 0  , 0  , 0  ), //      {X:zmm, R:vm32z}
  ISIGNATURE(3, 1, 1, 0, 103, 109, 103, 0  , 0  , 0  ), // #123 {X:xmm, R:vm64x, X:xmm}
  ISIGNATURE(3, 1, 1, 0, 105, 110, 105, 0  , 0  , 0  ), //      {X:ymm, R:vm64y, X:ymm}
  ISIGNATURE(2, 1, 1, 0, 103, 109, 0  , 0  , 0  , 0  ), //      {X:xmm, R:vm64x}
  ISIGNATURE(2, 1, 1, 0, 105, 110, 0  , 0  , 0  , 0  ), //      {X:ymm, R:vm64y}
  ISIGNATURE(2, 1, 1, 0, 107, 111, 0  , 0  , 0  , 0  ), //      {X:zmm, R:vm64z}
  ISIGNATURE(2, 1, 1, 0, 56 , 98 , 0  , 0  , 0  , 0  ), // #128 {R:r16|m16|r32|m32|r64|m64|mem, R:u8}
  ISIGNATURE(2, 1, 1, 0, 58 , 4  , 0  , 0  , 0  , 0  ), //      {R:r16|m16|mem, R:r16}
  ISIGNATURE(2, 1, 1, 0, 59 , 6  , 0  , 0  , 0  , 0  ), //      {R:r32|m32|mem, R:r32}
  ISIGNATURE(2, 0, 1, 0, 55 , 8  , 0  , 0  , 0  , 0  ), //      {R:r64|m64|mem, R:r64}
  ISIGNATURE(2, 1, 1, 0, 36 , 98 , 0  , 0  , 0  , 0  ), // #132 {X:r16|m16|r32|m32|r64|m64|mem, R:u8}
  ISIGNATURE(2, 1, 1, 0, 39 , 4  , 0  , 0  , 0  , 0  ), //      {X:r16|m16|mem, R:r16}
  ISIGNATURE(2, 1, 1, 0, 40 , 6  , 0  , 0  , 0  , 0  ), //      {X:r32|m32|mem, R:r32}
  ISIGNATURE(2, 0, 1, 0, 34 , 8  , 0  , 0  , 0  , 0  ), //      {X:r64|m64|mem, R:r64}
  ISIGNATURE(3, 1, 1, 1, 38 , 2  , 112, 0  , 0  , 0  ), // #136 {X:r8lo|r8hi|m8|mem, R:r8lo|r8hi, R:<al>}
  ISIGNATURE(3, 1, 1, 1, 39 , 4  , 113, 0  , 0  , 0  ), //      {X:r16|m16|mem, R:r16, R:<ax>}
  ISIGNATURE(3, 1, 1, 1, 40 , 6  , 114, 0  , 0  , 0  ), //      {X:r32|m32|mem, R:r32, R:<eax>}
  ISIGNATURE(3, 0, 1, 1, 34 , 8  , 115, 0  , 0  , 0  ), //      {X:r64|m64|mem, R:r64, R:<rax>}
  ISIGNATURE(2, 1, 1, 1, 60 , 57 , 0  , 0  , 0  , 0  ), // #140 {X:<ax>, R:r8lo|r8hi|m8|mem}
  ISIGNATURE(3, 1, 1, 2, 116, 60 , 58 , 0  , 0  , 0  ), //      {X:<dx>, X:<ax>, R:r16|m16|mem}
  ISIGNATURE(3, 1, 1, 2, 117, 63 , 59 , 0  , 0  , 0  ), //      {X:<edx>, X:<eax>, R:r32|m32|mem}
  ISIGNATURE(3, 0, 1, 2, 118, 65 , 55 , 0  , 0  , 0  ), //      {X:<rdx>, X:<rax>, R:r64|m64|mem}
  ISIGNATURE(1, 1, 1, 0, 119, 0  , 0  , 0  , 0  , 0  ), // #144 {W:r16|m16|r64|m64|mem}
  ISIGNATURE(1, 1, 0, 0, 13 , 0  , 0  , 0  , 0  , 0  ), //      {W:r32|m32}
  ISIGNATURE(1, 1, 0, 0, 120, 0  , 0  , 0  , 0  , 0  ), //      {W:ds|es|ss}
  ISIGNATURE(1, 1, 1, 0, 121, 0  , 0  , 0  , 0  , 0  ), //      {W:fs|gs}
  ISIGNATURE(1, 1, 1, 0, 122, 0  , 0  , 0  , 0  , 0  ), // #148 {R:r16|m16|r64|m64|mem|i8|i16|i32}
  ISIGNATURE(1, 1, 0, 0, 54 , 0  , 0  , 0  , 0  , 0  ), //      {R:r32|m32}
  ISIGNATURE(1, 1, 0, 0, 123, 0  , 0  , 0  , 0  , 0  ), //      {R:cs|ss|ds|es}
  ISIGNATURE(1, 1, 1, 0, 124, 0  , 0  , 0  , 0  , 0  ), //      {R:fs|gs}
  ISIGNATURE(4, 1, 1, 0, 84 , 83 , 83 , 86 , 0  , 0  ), // #152 {W:xmm, R:xmm, R:xmm, R:xmm|m128|mem}
  ISIGNATURE(4, 1, 1, 0, 84 , 83 , 97 , 83 , 0  , 0  ), //      {W:xmm, R:xmm, R:m128|mem, R:xmm}
  ISIGNATURE(4, 1, 1, 0, 88 , 91 , 91 , 89 , 0  , 0  ), //      {W:ymm, R:ymm, R:ymm, R:ymm|m256|mem}
  ISIGNATURE(4, 1, 1, 0, 88 , 91 , 100, 91 , 0  , 0  ), //      {W:ymm, R:ymm, R:m256|mem, R:ymm}
  ISIGNATURE(3, 1, 1, 0, 103, 125, 103, 0  , 0  , 0  ), // #156 {X:xmm, R:vm64x|vm64y, X:xmm}
  ISIGNATURE(2, 1, 1, 0, 103, 109, 0  , 0  , 0  , 0  ), //      {X:xmm, R:vm64x}
  ISIGNATURE(2, 1, 1, 0, 105, 110, 0  , 0  , 0  , 0  ), //      {X:ymm, R:vm64y}
  ISIGNATURE(2, 1, 1, 0, 107, 111, 0  , 0  , 0  , 0  ), //      {X:zmm, R:vm64z}
  ISIGNATURE(3, 1, 1, 0, 126, 83 , 83 , 0  , 0  , 0  ), // #160 {X:m128|mem, R:xmm, R:xmm}
  ISIGNATURE(3, 1, 1, 0, 127, 91 , 91 , 0  , 0  , 0  ), //      {X:m256|mem, R:ymm, R:ymm}
  ISIGNATURE(3, 1, 1, 0, 84 , 83 , 97 , 0  , 0  , 0  ), //      {W:xmm, R:xmm, R:m128|mem}
  ISIGNATURE(3, 1, 1, 0, 88 , 91 , 100, 0  , 0  , 0  ), //      {W:ymm, R:ymm, R:m256|mem}
  ISIGNATURE(5, 1, 1, 0, 84 , 83 , 86 , 83 , 128, 0  ), // #164 {W:xmm, R:xmm, R:xmm|m128|mem, R:xmm, R:u4}
  ISIGNATURE(5, 1, 1, 0, 84 , 83 , 83 , 97 , 128, 0  ), //      {W:xmm, R:xmm, R:xmm, R:m128|mem, R:u4}
  ISIGNATURE(5, 1, 1, 0, 88 , 91 , 89 , 91 , 128, 0  ), //      {W:ymm, R:ymm, R:ymm|m256|mem, R:ymm, R:u4}
  ISIGNATURE(5, 1, 1, 0, 88 , 91 , 91 , 100, 128, 0  ), //      {W:ymm, R:ymm, R:ymm, R:m256|mem, R:u4}
  ISIGNATURE(3, 1, 1, 0, 88 , 89 , 98 , 0  , 0  , 0  ), // #168 {W:ymm, R:ymm|m256|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 88 , 91 , 89 , 0  , 0  , 0  ), // #169 {W:ymm, R:ymm, R:ymm|m256|mem}
  ISIGNATURE(3, 1, 1, 0, 92 , 95 , 101, 0  , 0  , 0  ), //      {W:zmm, R:zmm, R:zmm|m512|mem|u8}
  ISIGNATURE(3, 1, 1, 0, 92 , 102, 98 , 0  , 0  , 0  ), //      {W:zmm, R:m512|mem, R:u8}
  ISIGNATURE(2, 1, 1, 0, 38 , 41 , 0  , 0  , 0  , 0  ), // #172 {X:r8lo|r8hi|m8|mem, X:r8lo|r8hi}
  ISIGNATURE(2, 1, 1, 0, 39 , 42 , 0  , 0  , 0  , 0  ), //      {X:r16|m16|mem, X:r16}
  ISIGNATURE(2, 1, 1, 0, 40 , 44 , 0  , 0  , 0  , 0  ), //      {X:r32|m32|mem, X:r32}
  ISIGNATURE(2, 0, 1, 0, 34 , 46 , 0  , 0  , 0  , 0  ), //      {X:r64|m64|mem, X:r64}
  ISIGNATURE(2, 1, 1, 0, 21 , 58 , 0  , 0  , 0  , 0  ), // #176 {W:r16, R:r16|m16|mem}
  ISIGNATURE(2, 1, 1, 0, 23 , 59 , 0  , 0  , 0  , 0  ), // #177 {W:r32, R:r32|m32|mem}
  ISIGNATURE(2, 0, 1, 0, 17 , 55 , 0  , 0  , 0  , 0  ), //      {W:r64, R:r64|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 42 , 58 , 0  , 0  , 0  , 0  ), // #179 {X:r16, R:r16|m16|mem}
  ISIGNATURE(2, 1, 1, 0, 44 , 59 , 0  , 0  , 0  , 0  ), // #180 {X:r32, R:r32|m32|mem}
  ISIGNATURE(2, 0, 1, 0, 46 , 55 , 0  , 0  , 0  , 0  ), //      {X:r64, R:r64|m64|mem}
  ISIGNATURE(1, 1, 1, 0, 129, 0  , 0  , 0  , 0  , 0  ), // #182 {R:m32|m64}
  ISIGNATURE(2, 1, 1, 0, 130, 131, 0  , 0  , 0  , 0  ), //      {X:fp0, R:fp}
  ISIGNATURE(2, 1, 1, 0, 132, 133, 0  , 0  , 0  , 0  ), //      {X:fp, R:fp0}
  ISIGNATURE(1, 1, 1, 0, 134, 0  , 0  , 0  , 0  , 0  ), // #185 {X:m32|m64}
  ISIGNATURE(2, 1, 1, 0, 130, 131, 0  , 0  , 0  , 0  ), //      {X:fp0, R:fp}
  ISIGNATURE(2, 1, 1, 0, 132, 133, 0  , 0  , 0  , 0  ), //      {X:fp, R:fp0}
  ISIGNATURE(2, 1, 1, 0, 42 , 45 , 0  , 0  , 0  , 0  ), // #188 {X:r16, R:m32|mem}
  ISIGNATURE(2, 1, 1, 0, 44 , 135, 0  , 0  , 0  , 0  ), //      {X:r32, R:m48|mem}
  ISIGNATURE(2, 0, 1, 0, 46 , 136, 0  , 0  , 0  , 0  ), //      {X:r64, R:m80|mem}
  ISIGNATURE(3, 1, 1, 0, 39 , 4  , 137, 0  , 0  , 0  ), // #191 {X:r16|m16|mem, R:r16, R:u8|cl}
  ISIGNATURE(3, 1, 1, 0, 40 , 6  , 137, 0  , 0  , 0  ), //      {X:r32|m32|mem, R:r32, R:u8|cl}
  ISIGNATURE(3, 0, 1, 0, 34 , 8  , 137, 0  , 0  , 0  ), //      {X:r64|m64|mem, R:r64, R:u8|cl}
  ISIGNATURE(3, 1, 1, 0, 84 , 83 , 86 , 0  , 0  , 0  ), // #194 {W:xmm, R:xmm, R:xmm|m128|mem}
  ISIGNATURE(3, 1, 1, 0, 88 , 91 , 89 , 0  , 0  , 0  ), // #195 {W:ymm, R:ymm, R:ymm|m256|mem}
  ISIGNATURE(3, 1, 1, 0, 92 , 95 , 93 , 0  , 0  , 0  ), //      {W:zmm, R:zmm, R:zmm|m512|mem}
  ISIGNATURE(4, 1, 1, 0, 84 , 83 , 86 , 98 , 0  , 0  ), // #197 {W:xmm, R:xmm, R:xmm|m128|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 88 , 91 , 89 , 98 , 0  , 0  ), // #198 {W:ymm, R:ymm, R:ymm|m256|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 92 , 95 , 93 , 98 , 0  , 0  ), //      {W:zmm, R:zmm, R:zmm|m512|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 138, 83 , 86 , 98 , 0  , 0  ), // #200 {W:xmm|k, R:xmm, R:xmm|m128|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 139, 91 , 89 , 98 , 0  , 0  ), //      {W:ymm|k, R:ymm, R:ymm|m256|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 140, 95 , 93 , 98 , 0  , 0  ), //      {W:k, R:zmm, R:zmm|m512|mem, R:u8}
  ISIGNATURE(2, 1, 1, 0, 141, 83 , 0  , 0  , 0  , 0  ), // #203 {W:xmm|m128|mem, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 142, 91 , 0  , 0  , 0  , 0  ), //      {W:ymm|m256|mem, R:ymm}
  ISIGNATURE(2, 1, 1, 0, 143, 95 , 0  , 0  , 0  , 0  ), //      {W:zmm|m512|mem, R:zmm}
  ISIGNATURE(2, 1, 1, 0, 84 , 85 , 0  , 0  , 0  , 0  ), // #206 {W:xmm, R:xmm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 88 , 86 , 0  , 0  , 0  , 0  ), //      {W:ymm, R:xmm|m128|mem}
  ISIGNATURE(2, 1, 1, 0, 92 , 89 , 0  , 0  , 0  , 0  ), //      {W:zmm, R:ymm|m256|mem}
  ISIGNATURE(2, 1, 1, 0, 84 , 86 , 0  , 0  , 0  , 0  ), // #209 {W:xmm, R:xmm|m128|mem}
  ISIGNATURE(2, 1, 1, 0, 88 , 89 , 0  , 0  , 0  , 0  ), //      {W:ymm, R:ymm|m256|mem}
  ISIGNATURE(2, 1, 1, 0, 92 , 93 , 0  , 0  , 0  , 0  ), //      {W:zmm, R:zmm|m512|mem}
  ISIGNATURE(3, 1, 1, 0, 144, 83 , 98 , 0  , 0  , 0  ), // #212 {W:xmm|m64|mem, R:xmm, R:u8}
  ISIGNATURE(3, 1, 1, 0, 141, 91 , 98 , 0  , 0  , 0  ), // #213 {W:xmm|m128|mem, R:ymm, R:u8}
  ISIGNATURE(3, 1, 1, 0, 142, 95 , 98 , 0  , 0  , 0  ), // #214 {W:ymm|m256|mem, R:zmm, R:u8}
  ISIGNATURE(4, 1, 1, 0, 103, 83 , 86 , 98 , 0  , 0  ), // #215 {X:xmm, R:xmm, R:xmm|m128|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 105, 91 , 89 , 98 , 0  , 0  ), //      {X:ymm, R:ymm, R:ymm|m256|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 107, 95 , 93 , 98 , 0  , 0  ), //      {X:zmm, R:zmm, R:zmm|m512|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 103, 83 , 86 , 0  , 0  , 0  ), // #218 {X:xmm, R:xmm, R:xmm|m128|mem}
  ISIGNATURE(3, 1, 1, 0, 105, 91 , 89 , 0  , 0  , 0  ), //      {X:ymm, R:ymm, R:ymm|m256|mem}
  ISIGNATURE(3, 1, 1, 0, 107, 95 , 93 , 0  , 0  , 0  ), //      {X:zmm, R:zmm, R:zmm|m512|mem}
  ISIGNATURE(3, 1, 1, 0, 84 , 86 , 98 , 0  , 0  , 0  ), // #221 {W:xmm, R:xmm|m128|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 88 , 89 , 98 , 0  , 0  , 0  ), //      {W:ymm, R:ymm|m256|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 92 , 93 , 98 , 0  , 0  , 0  ), //      {W:zmm, R:zmm|m512|mem, R:u8}
  ISIGNATURE(2, 1, 1, 0, 84 , 85 , 0  , 0  , 0  , 0  ), // #224 {W:xmm, R:xmm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 88 , 89 , 0  , 0  , 0  , 0  ), //      {W:ymm, R:ymm|m256|mem}
  ISIGNATURE(2, 1, 1, 0, 92 , 93 , 0  , 0  , 0  , 0  ), //      {W:zmm, R:zmm|m512|mem}
  ISIGNATURE(2, 1, 1, 0, 87 , 83 , 0  , 0  , 0  , 0  ), // #227 {W:m128|mem, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 90 , 91 , 0  , 0  , 0  , 0  ), //      {W:m256|mem, R:ymm}
  ISIGNATURE(2, 1, 1, 0, 94 , 95 , 0  , 0  , 0  , 0  ), //      {W:m512|mem, R:zmm}
  ISIGNATURE(2, 1, 1, 0, 84 , 97 , 0  , 0  , 0  , 0  ), // #230 {W:xmm, R:m128|mem}
  ISIGNATURE(2, 1, 1, 0, 88 , 100, 0  , 0  , 0  , 0  ), //      {W:ymm, R:m256|mem}
  ISIGNATURE(2, 1, 1, 0, 92 , 102, 0  , 0  , 0  , 0  ), //      {W:zmm, R:m512|mem}
  ISIGNATURE(2, 0, 1, 0, 15 , 83 , 0  , 0  , 0  , 0  ), // #233 {W:r64|m64|mem, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 84 , 145, 0  , 0  , 0  , 0  ), //      {W:xmm, R:xmm|m64|mem|r64}
  ISIGNATURE(2, 1, 1, 0, 78 , 83 , 0  , 0  , 0  , 0  ), //      {W:m64|mem, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 78 , 83 , 0  , 0  , 0  , 0  ), // #236 {W:m64|mem, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 84 , 47 , 0  , 0  , 0  , 0  ), //      {W:xmm, R:m64|mem}
  ISIGNATURE(3, 1, 1, 0, 84 , 83 , 83 , 0  , 0  , 0  ), // #238 {W:xmm, R:xmm, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 77 , 83 , 0  , 0  , 0  , 0  ), // #239 {W:m32|mem, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 84 , 45 , 0  , 0  , 0  , 0  ), //      {W:xmm, R:m32|mem}
  ISIGNATURE(3, 1, 1, 0, 84 , 83 , 83 , 0  , 0  , 0  ), //      {W:xmm, R:xmm, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 146, 145, 0  , 0  , 0  , 0  ), // #242 {W:xmm|ymm, R:xmm|m64|mem|r64}
  ISIGNATURE(2, 0, 1, 0, 92 , 8  , 0  , 0  , 0  , 0  ), //      {W:zmm, R:r64}
  ISIGNATURE(2, 1, 1, 0, 92 , 85 , 0  , 0  , 0  , 0  ), //      {W:zmm, R:xmm|m64|mem}
  ISIGNATURE(4, 1, 1, 0, 140, 83 , 86 , 98 , 0  , 0  ), // #245 {W:k, R:xmm, R:xmm|m128|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 140, 91 , 89 , 98 , 0  , 0  ), //      {W:k, R:ymm, R:ymm|m256|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 140, 95 , 93 , 98 , 0  , 0  ), //      {W:k, R:zmm, R:zmm|m512|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 138, 83 , 86 , 0  , 0  , 0  ), // #248 {W:xmm|k, R:xmm, R:xmm|m128|mem}
  ISIGNATURE(3, 1, 1, 0, 139, 91 , 89 , 0  , 0  , 0  ), //      {W:ymm|k, R:ymm, R:ymm|m256|mem}
  ISIGNATURE(3, 1, 1, 0, 140, 95 , 93 , 0  , 0  , 0  ), //      {W:k, R:zmm, R:zmm|m512|mem}
  ISIGNATURE(2, 1, 1, 0, 147, 83 , 0  , 0  , 0  , 0  ), // #251 {W:xmm|m32|mem, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 144, 91 , 0  , 0  , 0  , 0  ), //      {W:xmm|m64|mem, R:ymm}
  ISIGNATURE(2, 1, 1, 0, 141, 95 , 0  , 0  , 0  , 0  ), //      {W:xmm|m128|mem, R:zmm}
  ISIGNATURE(2, 1, 1, 0, 144, 83 , 0  , 0  , 0  , 0  ), // #254 {W:xmm|m64|mem, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 141, 91 , 0  , 0  , 0  , 0  ), //      {W:xmm|m128|mem, R:ymm}
  ISIGNATURE(2, 1, 1, 0, 142, 95 , 0  , 0  , 0  , 0  ), //      {W:ymm|m256|mem, R:zmm}
  ISIGNATURE(2, 1, 1, 0, 148, 83 , 0  , 0  , 0  , 0  ), // #257 {W:xmm|m16|mem, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 147, 91 , 0  , 0  , 0  , 0  ), //      {W:xmm|m32|mem, R:ymm}
  ISIGNATURE(2, 1, 1, 0, 144, 95 , 0  , 0  , 0  , 0  ), //      {W:xmm|m64|mem, R:zmm}
  ISIGNATURE(2, 1, 1, 0, 84 , 149, 0  , 0  , 0  , 0  ), // #260 {W:xmm, R:xmm|m32|mem}
  ISIGNATURE(2, 1, 1, 0, 88 , 85 , 0  , 0  , 0  , 0  ), //      {W:ymm, R:xmm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 92 , 86 , 0  , 0  , 0  , 0  ), //      {W:zmm, R:xmm|m128|mem}
  ISIGNATURE(2, 1, 1, 0, 84 , 150, 0  , 0  , 0  , 0  ), // #263 {W:xmm, R:xmm|m16|mem}
  ISIGNATURE(2, 1, 1, 0, 88 , 149, 0  , 0  , 0  , 0  ), //      {W:ymm, R:xmm|m32|mem}
  ISIGNATURE(2, 1, 1, 0, 92 , 85 , 0  , 0  , 0  , 0  ), //      {W:zmm, R:xmm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 84 , 85 , 0  , 0  , 0  , 0  ), // #266 {W:xmm, R:xmm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 88 , 86 , 0  , 0  , 0  , 0  ), //      {W:ymm, R:xmm|m128|mem}
  ISIGNATURE(2, 1, 1, 0, 92 , 151, 0  , 0  , 0  , 0  ), //      {W:zmm, R:xmm|m256|mem}
  ISIGNATURE(2, 1, 1, 0, 152, 83 , 0  , 0  , 0  , 0  ), // #269 {W:vm32x, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 153, 91 , 0  , 0  , 0  , 0  ), //      {W:vm32y, R:ymm}
  ISIGNATURE(2, 1, 1, 0, 154, 95 , 0  , 0  , 0  , 0  ), //      {W:vm32z, R:zmm}
  ISIGNATURE(2, 1, 1, 0, 155, 83 , 0  , 0  , 0  , 0  ), // #272 {W:vm64x, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 156, 91 , 0  , 0  , 0  , 0  ), //      {W:vm64y, R:ymm}
  ISIGNATURE(2, 1, 1, 0, 157, 95 , 0  , 0  , 0  , 0  ), //      {W:vm64z, R:zmm}
  ISIGNATURE(3, 1, 1, 0, 140, 83 , 86 , 0  , 0  , 0  ), // #275 {W:k, R:xmm, R:xmm|m128|mem}
  ISIGNATURE(3, 1, 1, 0, 140, 91 , 89 , 0  , 0  , 0  ), //      {W:k, R:ymm, R:ymm|m256|mem}
  ISIGNATURE(3, 1, 1, 0, 140, 95 , 93 , 0  , 0  , 0  ), //      {W:k, R:zmm, R:zmm|m512|mem}
  ISIGNATURE(3, 1, 1, 0, 23 , 6  , 59 , 0  , 0  , 0  ), // #278 {W:r32, R:r32, R:r32|m32|mem}
  ISIGNATURE(3, 0, 1, 0, 17 , 8  , 55 , 0  , 0  , 0  ), //      {W:r64, R:r64, R:r64|m64|mem}
  ISIGNATURE(3, 1, 1, 0, 23 , 59 , 6  , 0  , 0  , 0  ), // #280 {W:r32, R:r32|m32|mem, R:r32}
  ISIGNATURE(3, 0, 1, 0, 17 , 55 , 8  , 0  , 0  , 0  ), //      {W:r64, R:r64|m64|mem, R:r64}
  ISIGNATURE(2, 1, 0, 0, 158, 59 , 0  , 0  , 0  , 0  ), // #282 {R:bnd, R:r32|m32|mem}
  ISIGNATURE(2, 0, 1, 0, 158, 55 , 0  , 0  , 0  , 0  ), //      {R:bnd, R:r64|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 159, 160, 0  , 0  , 0  , 0  ), // #284 {W:bnd, R:bnd|mem}
  ISIGNATURE(2, 1, 1, 0, 161, 158, 0  , 0  , 0  , 0  ), //      {W:mem, R:bnd}
  ISIGNATURE(2, 1, 0, 0, 4  , 45 , 0  , 0  , 0  , 0  ), // #286 {R:r16, R:m32|mem}
  ISIGNATURE(2, 1, 0, 0, 6  , 47 , 0  , 0  , 0  , 0  ), //      {R:r32, R:m64|mem}
  ISIGNATURE(1, 1, 1, 0, 162, 0  , 0  , 0  , 0  , 0  ), // #288 {R:rel32|r64|m64|mem}
  ISIGNATURE(1, 1, 0, 0, 59 , 0  , 0  , 0  , 0  , 0  ), //      {R:r32|m32|mem}
  ISIGNATURE(2, 1, 1, 0, 44 , 163, 0  , 0  , 0  , 0  ), // #290 {X:r32, R:r8lo|r8hi|m8|r16|m16|r32|m32}
  ISIGNATURE(2, 0, 1, 0, 46 , 164, 0  , 0  , 0  , 0  ), //      {X:r64, R:r8lo|r8hi|m8|r64|m64}
  ISIGNATURE(1, 1, 0, 0, 165, 0  , 0  , 0  , 0  , 0  ), // #292 {X:r16|r32}
  ISIGNATURE(1, 1, 1, 0, 48 , 0  , 0  , 0  , 0  , 0  ), // #293 {X:r8lo|r8hi|m8|r16|m16|r32|m32|r64|m64|mem}
  ISIGNATURE(3, 1, 1, 0, 103, 98 , 98 , 0  , 0  , 0  ), // #294 {X:xmm, R:u8, R:u8}
  ISIGNATURE(2, 1, 1, 0, 103, 83 , 0  , 0  , 0  , 0  ), //      {X:xmm, R:xmm}
  ISIGNATURE(0, 1, 1, 0, 0  , 0  , 0  , 0  , 0  , 0  ), // #296 {}
  ISIGNATURE(1, 1, 1, 0, 132, 0  , 0  , 0  , 0  , 0  ), // #297 {X:fp}
  ISIGNATURE(0, 1, 1, 0, 0  , 0  , 0  , 0  , 0  , 0  ), // #298 {}
  ISIGNATURE(1, 1, 1, 0, 166, 0  , 0  , 0  , 0  , 0  ), //      {R:m32|m64|fp}
  ISIGNATURE(0, 1, 1, 0, 0  , 0  , 0  , 0  , 0  , 0  ), // #300 {}
  ISIGNATURE(1, 1, 1, 0, 131, 0  , 0  , 0  , 0  , 0  ), // #301 {R:fp}
  ISIGNATURE(2, 1, 1, 0, 103, 83 , 0  , 0  , 0  , 0  ), // #302 {X:xmm, R:xmm}
  ISIGNATURE(4, 1, 1, 0, 103, 83 , 98 , 98 , 0  , 0  ), //      {X:xmm, R:xmm, R:u8, R:u8}
  ISIGNATURE(2, 1, 0, 0, 6  , 97 , 0  , 0  , 0  , 0  ), // #304 {R:r32, R:m128|mem}
  ISIGNATURE(2, 0, 1, 0, 8  , 97 , 0  , 0  , 0  , 0  ), //      {R:r64, R:m128|mem}
  ISIGNATURE(2, 1, 0, 1, 167, 168, 0  , 0  , 0  , 0  ), // #306 {R:<cx|ecx>, R:rel8}
  ISIGNATURE(2, 0, 1, 1, 169, 168, 0  , 0  , 0  , 0  ), //      {R:<ecx|rcx>, R:rel8}
  ISIGNATURE(1, 1, 1, 0, 170, 0  , 0  , 0  , 0  , 0  ), // #308 {R:rel8|rel32|r64|m64|mem}
  ISIGNATURE(1, 1, 0, 0, 59 , 0  , 0  , 0  , 0  , 0  ), //      {R:r32|m32|mem}
  ISIGNATURE(2, 1, 1, 0, 140, 171, 0  , 0  , 0  , 0  ), // #310 {W:k, R:k|m8|mem|r32|r64|r8lo|r8hi|r16}
  ISIGNATURE(2, 1, 1, 0, 172, 173, 0  , 0  , 0  , 0  ), //      {W:m8|mem|r32|r64|r8lo|r8hi|r16, R:k}
  ISIGNATURE(2, 1, 1, 0, 140, 174, 0  , 0  , 0  , 0  ), // #312 {W:k, R:k|m32|mem|r32|r64}
  ISIGNATURE(2, 1, 1, 0, 175, 173, 0  , 0  , 0  , 0  ), //      {W:m32|mem|r32|r64, R:k}
  ISIGNATURE(2, 1, 1, 0, 140, 176, 0  , 0  , 0  , 0  ), // #314 {W:k, R:k|m64|mem|r64}
  ISIGNATURE(2, 1, 1, 0, 15 , 173, 0  , 0  , 0  , 0  ), //      {W:m64|mem|r64, R:k}
  ISIGNATURE(2, 1, 1, 0, 140, 177, 0  , 0  , 0  , 0  ), // #316 {W:k, R:k|m16|mem|r32|r64|r16}
  ISIGNATURE(2, 1, 1, 0, 178, 173, 0  , 0  , 0  , 0  ), //      {W:m16|mem|r32|r64|r16, R:k}
  ISIGNATURE(2, 1, 1, 0, 21 , 58 , 0  , 0  , 0  , 0  ), // #318 {W:r16, R:r16|m16|mem}
  ISIGNATURE(2, 1, 1, 0, 23 , 179, 0  , 0  , 0  , 0  ), //      {W:r32, R:r32|m16|mem|r16}
  ISIGNATURE(2, 1, 0, 0, 42 , 45 , 0  , 0  , 0  , 0  ), // #320 {X:r16, R:m32|mem}
  ISIGNATURE(2, 1, 0, 0, 44 , 135, 0  , 0  , 0  , 0  ), //      {X:r32, R:m48|mem}
  ISIGNATURE(2, 1, 0, 1, 180, 168, 0  , 0  , 0  , 0  ), // #322 {X:<cx|ecx>, R:rel8}
  ISIGNATURE(2, 0, 1, 1, 181, 168, 0  , 0  , 0  , 0  ), //      {X:<ecx|rcx>, R:rel8}
  ISIGNATURE(2, 1, 1, 0, 21 , 58 , 0  , 0  , 0  , 0  ), // #324 {W:r16, R:r16|m16|mem}
  ISIGNATURE(2, 1, 1, 0, 182, 179, 0  , 0  , 0  , 0  ), //      {W:r32|r64, R:r32|m16|mem|r16}
  ISIGNATURE(2, 1, 1, 0, 183, 184, 0  , 0  , 0  , 0  ), // #326 {W:mm|xmm, R:r32|m32|mem|r64}
  ISIGNATURE(2, 1, 1, 0, 175, 185, 0  , 0  , 0  , 0  ), //      {W:r32|m32|mem|r64, R:mm|xmm}
  ISIGNATURE(2, 1, 1, 0, 84 , 149, 0  , 0  , 0  , 0  ), // #328 {W:xmm, R:xmm|m32|mem}
  ISIGNATURE(2, 1, 1, 0, 77 , 83 , 0  , 0  , 0  , 0  ), //      {W:m32|mem, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 21 , 52 , 0  , 0  , 0  , 0  ), // #330 {W:r16, R:r8lo|r8hi|m8}
  ISIGNATURE(2, 1, 1, 0, 182, 186, 0  , 0  , 0  , 0  ), //      {W:r32|r64, R:r8lo|r8hi|m8|r16|m16}
  ISIGNATURE(4, 1, 1, 1, 23 , 23 , 59 , 187, 0  , 0  ), // #332 {W:r32, W:r32, R:r32|m32|mem, R:<edx>}
  ISIGNATURE(4, 0, 1, 1, 17 , 17 , 55 , 188, 0  , 0  ), //      {W:r64, W:r64, R:r64|m64|mem, R:<rdx>}
  ISIGNATURE(0, 1, 1, 0, 0  , 0  , 0  , 0  , 0  , 0  ), // #334 {}
  ISIGNATURE(1, 1, 1, 0, 189, 0  , 0  , 0  , 0  , 0  ), //      {R:r16|m16|r32|m32}
  ISIGNATURE(2, 1, 1, 0, 79 , 190, 0  , 0  , 0  , 0  ), // #336 {W:mm, R:mm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 84 , 86 , 0  , 0  , 0  , 0  ), //      {W:xmm, R:xmm|m128|mem}
  ISIGNATURE(2, 1, 1, 0, 191, 190, 0  , 0  , 0  , 0  ), // #338 {X:mm, R:mm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 103, 86 , 0  , 0  , 0  , 0  ), // #339 {X:xmm, R:xmm|m128|mem}
  ISIGNATURE(3, 1, 1, 0, 191, 190, 98 , 0  , 0  , 0  ), // #340 {X:mm, R:mm|m64|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 103, 86 , 98 , 0  , 0  , 0  ), // #341 {X:xmm, R:xmm|m128|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 182, 82 , 98 , 0  , 0  , 0  ), // #342 {W:r32|r64, R:mm, R:u8}
  ISIGNATURE(3, 1, 1, 0, 178, 83 , 98 , 0  , 0  , 0  ), // #343 {W:r32|r64|m16|mem|r16, R:xmm, R:u8}
  ISIGNATURE(2, 1, 1, 0, 191, 192, 0  , 0  , 0  , 0  ), // #344 {X:mm, R:u8|mm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 103, 96 , 0  , 0  , 0  , 0  ), //      {X:xmm, R:u8|xmm|m128|mem}
  ISIGNATURE(0, 1, 1, 0, 0  , 0  , 0  , 0  , 0  , 0  ), // #346 {}
  ISIGNATURE(1, 1, 1, 0, 193, 0  , 0  , 0  , 0  , 0  ), //      {R:u16}
  ISIGNATURE(3, 1, 1, 0, 23 , 59 , 98 , 0  , 0  , 0  ), // #348 {W:r32, R:r32|m32|mem, R:u8}
  ISIGNATURE(3, 0, 1, 0, 17 , 55 , 98 , 0  , 0  , 0  ), //      {W:r64, R:r64|m64|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 84 , 83 , 86 , 83 , 0  , 0  ), // #350 {W:xmm, R:xmm, R:xmm|m128|mem, R:xmm}
  ISIGNATURE(4, 1, 1, 0, 88 , 91 , 89 , 91 , 0  , 0  ), //      {W:ymm, R:ymm, R:ymm|m256|mem, R:ymm}
  ISIGNATURE(2, 1, 1, 0, 146, 194, 0  , 0  , 0  , 0  ), // #352 {W:xmm|ymm, R:m32|xmm|m64}
  ISIGNATURE(2, 1, 1, 0, 92 , 195, 0  , 0  , 0  , 0  ), //      {W:zmm, R:xmm|m64}
  ISIGNATURE(2, 1, 1, 0, 84 , 196, 0  , 0  , 0  , 0  ), // #354 {W:xmm, R:xmm|m128|ymm|m256}
  ISIGNATURE(2, 1, 1, 0, 88 , 93 , 0  , 0  , 0  , 0  ), //      {W:ymm, R:zmm|m512|mem}
  ISIGNATURE(4, 1, 1, 0, 84 , 83 , 83 , 85 , 0  , 0  ), // #356 {W:xmm, R:xmm, R:xmm, R:xmm|m64|mem}
  ISIGNATURE(4, 1, 1, 0, 84 , 83 , 47 , 83 , 0  , 0  ), //      {W:xmm, R:xmm, R:m64|mem, R:xmm}
  ISIGNATURE(4, 1, 1, 0, 84 , 83 , 83 , 149, 0  , 0  ), // #358 {W:xmm, R:xmm, R:xmm, R:xmm|m32|mem}
  ISIGNATURE(4, 1, 1, 0, 84 , 83 , 45 , 83 , 0  , 0  ), //      {W:xmm, R:xmm, R:m32|mem, R:xmm}
  ISIGNATURE(4, 1, 1, 0, 88 , 91 , 86 , 98 , 0  , 0  ), // #360 {W:ymm, R:ymm, R:xmm|m128|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 92 , 95 , 86 , 98 , 0  , 0  ), //      {W:zmm, R:zmm, R:xmm|m128|mem, R:u8}
  ISIGNATURE(2, 1, 1, 0, 175, 83 , 0  , 0  , 0  , 0  ), // #362 {W:r32|m32|mem|r64, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 84 , 184, 0  , 0  , 0  , 0  ), //      {W:xmm, R:r32|m32|mem|r64}
  ISIGNATURE(2, 1, 1, 0, 78 , 83 , 0  , 0  , 0  , 0  ), // #364 {W:m64|mem, R:xmm}
  ISIGNATURE(3, 1, 1, 0, 84 , 83 , 47 , 0  , 0  , 0  ), //      {W:xmm, R:xmm, R:m64|mem}
  ISIGNATURE(3, 1, 1, 0, 84 , 83 , 96 , 0  , 0  , 0  ), // #366 {W:xmm, R:xmm, R:xmm|m128|mem|u8}
  ISIGNATURE(3, 1, 1, 0, 84 , 97 , 197, 0  , 0  , 0  ), //      {W:xmm, R:m128|mem, R:u8|xmm}
  ISIGNATURE(2, 1, 1, 0, 198, 83 , 0  , 0  , 0  , 0  ), // #368 {W:vm64x|vm64y, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 157, 91 , 0  , 0  , 0  , 0  ), //      {W:vm64z, R:ymm}
  ISIGNATURE(3, 1, 1, 0, 84 , 83 , 86 , 0  , 0  , 0  ), // #370 {W:xmm, R:xmm, R:xmm|m128|mem}
  ISIGNATURE(3, 1, 1, 0, 84 , 97 , 83 , 0  , 0  , 0  ), //      {W:xmm, R:m128|mem, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 83 , 86 , 0  , 0  , 0  , 0  ), // #372 {R:xmm, R:xmm|m128|mem}
  ISIGNATURE(2, 1, 1, 0, 91 , 89 , 0  , 0  , 0  , 0  ), //      {R:ymm, R:ymm|m256|mem}
  ISIGNATURE(2, 1, 1, 0, 152, 199, 0  , 0  , 0  , 0  ), // #374 {W:vm32x, R:xmm|ymm}
  ISIGNATURE(2, 1, 1, 0, 153, 95 , 0  , 0  , 0  , 0  ), //      {W:vm32y, R:zmm}
  ISIGNATURE(1, 1, 0, 1, 60 , 0  , 0  , 0  , 0  , 0  ), // #376 {X:<ax>}
  ISIGNATURE(2, 1, 0, 1, 60 , 98 , 0  , 0  , 0  , 0  ), // #377 {X:<ax>, R:u8}
  ISIGNATURE(2, 1, 1, 0, 103, 85 , 0  , 0  , 0  , 0  ), // #378 {X:xmm, R:xmm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 103, 149, 0  , 0  , 0  , 0  ), // #379 {X:xmm, R:xmm|m32|mem}
  ISIGNATURE(2, 1, 0, 0, 39 , 4  , 0  , 0  , 0  , 0  ), // #380 {X:r16|m16|mem, R:r16}
  ISIGNATURE(3, 1, 1, 1, 103, 86 , 200, 0  , 0  , 0  ), // #381 {X:xmm, R:xmm|m128|mem, R:<xmm0>}
  ISIGNATURE(2, 1, 1, 0, 159, 201, 0  , 0  , 0  , 0  ), // #382 {W:bnd, R:mib}
  ISIGNATURE(2, 1, 1, 0, 159, 202, 0  , 0  , 0  , 0  ), // #383 {W:bnd, R:mem}
  ISIGNATURE(2, 1, 1, 0, 203, 158, 0  , 0  , 0  , 0  ), // #384 {W:mib, R:bnd}
  ISIGNATURE(1, 1, 1, 0, 204, 0  , 0  , 0  , 0  , 0  ), // #385 {X:r32|r64}
  ISIGNATURE(1, 1, 1, 1, 60 , 0  , 0  , 0  , 0  , 0  ), // #386 {X:<ax>}
  ISIGNATURE(2, 1, 1, 2, 62 , 114, 0  , 0  , 0  , 0  ), // #387 {W:<edx>, R:<eax>}
  ISIGNATURE(1, 0, 1, 1, 65 , 0  , 0  , 0  , 0  , 0  ), // #388 {X:<rax>}
  ISIGNATURE(1, 1, 1, 0, 202, 0  , 0  , 0  , 0  , 0  ), // #389 {R:mem}
  ISIGNATURE(1, 1, 1, 1, 205, 0  , 0  , 0  , 0  , 0  ), // #390 {R:<ds:[zax]>}
  ISIGNATURE(2, 1, 1, 2, 206, 207, 0  , 0  , 0  , 0  ), // #391 {X:<ds:[zsi]>, X:<es:[zdi]>}
  ISIGNATURE(3, 1, 1, 0, 103, 85 , 98 , 0  , 0  , 0  ), // #392 {X:xmm, R:xmm|m64|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 103, 149, 98 , 0  , 0  , 0  ), // #393 {X:xmm, R:xmm|m32|mem, R:u8}
  ISIGNATURE(5, 0, 1, 4, 126, 118, 65 , 208, 209, 0  ), // #394 {X:m128|mem, X:<rdx>, X:<rax>, R:<rcx>, R:<rbx>}
  ISIGNATURE(5, 1, 1, 4, 51 , 117, 63 , 210, 211, 0  ), // #395 {X:m64|mem, X:<edx>, X:<eax>, R:<ecx>, R:<ebx>}
  ISIGNATURE(2, 1, 1, 0, 83 , 85 , 0  , 0  , 0  , 0  ), // #396 {R:xmm, R:xmm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 83 , 149, 0  , 0  , 0  , 0  ), // #397 {R:xmm, R:xmm|m32|mem}
  ISIGNATURE(4, 1, 1, 4, 63 , 212, 213, 62 , 0  , 0  ), // #398 {X:<eax>, W:<ebx>, X:<ecx>, W:<edx>}
  ISIGNATURE(2, 0, 1, 2, 64 , 115, 0  , 0  , 0  , 0  ), // #399 {W:<rdx>, R:<rax>}
  ISIGNATURE(2, 1, 1, 0, 79 , 86 , 0  , 0  , 0  , 0  ), // #400 {W:mm, R:xmm|m128|mem}
  ISIGNATURE(2, 1, 1, 0, 84 , 190, 0  , 0  , 0  , 0  ), // #401 {W:xmm, R:mm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 79 , 85 , 0  , 0  , 0  , 0  ), // #402 {W:mm, R:xmm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 182, 85 , 0  , 0  , 0  , 0  ), // #403 {W:r32|r64, R:xmm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 84 , 214, 0  , 0  , 0  , 0  ), // #404 {W:xmm, R:r32|m32|mem|r64|m64}
  ISIGNATURE(2, 1, 1, 0, 182, 149, 0  , 0  , 0  , 0  ), // #405 {W:r32|r64, R:xmm|m32|mem}
  ISIGNATURE(2, 1, 1, 2, 61 , 113, 0  , 0  , 0  , 0  ), // #406 {W:<dx>, R:<ax>}
  ISIGNATURE(1, 1, 1, 1, 63 , 0  , 0  , 0  , 0  , 0  ), // #407 {X:<eax>}
  ISIGNATURE(2, 1, 1, 0, 193, 98 , 0  , 0  , 0  , 0  ), // #408 {R:u16, R:u8}
  ISIGNATURE(3, 1, 1, 0, 175, 83 , 98 , 0  , 0  , 0  ), // #409 {W:r32|m32|mem|r64, R:xmm, R:u8}
  ISIGNATURE(1, 1, 1, 0, 136, 0  , 0  , 0  , 0  , 0  ), // #410 {R:m80|mem}
  ISIGNATURE(1, 1, 1, 0, 215, 0  , 0  , 0  , 0  , 0  ), // #411 {W:m80|mem}
  ISIGNATURE(1, 1, 1, 0, 216, 0  , 0  , 0  , 0  , 0  ), // #412 {R:m16|m32}
  ISIGNATURE(1, 1, 1, 0, 217, 0  , 0  , 0  , 0  , 0  ), // #413 {R:m16|m32|m64}
  ISIGNATURE(1, 1, 1, 0, 218, 0  , 0  , 0  , 0  , 0  ), // #414 {W:m16|m32}
  ISIGNATURE(1, 1, 1, 0, 219, 0  , 0  , 0  , 0  , 0  ), // #415 {W:m16|m32|m64}
  ISIGNATURE(1, 1, 1, 0, 220, 0  , 0  , 0  , 0  , 0  ), // #416 {R:m32|m64|m80|fp}
  ISIGNATURE(1, 1, 1, 0, 43 , 0  , 0  , 0  , 0  , 0  ), // #417 {R:m16|mem}
  ISIGNATURE(1, 1, 1, 0, 161, 0  , 0  , 0  , 0  , 0  ), // #418 {W:mem}
  ISIGNATURE(1, 1, 1, 0, 76 , 0  , 0  , 0  , 0  , 0  ), // #419 {W:m16|mem}
  ISIGNATURE(1, 1, 1, 0, 221, 0  , 0  , 0  , 0  , 0  ), // #420 {W:ax|m16|mem}
  ISIGNATURE(1, 1, 1, 0, 222, 0  , 0  , 0  , 0  , 0  ), // #421 {W:m32|m64|fp}
  ISIGNATURE(1, 1, 1, 0, 223, 0  , 0  , 0  , 0  , 0  ), // #422 {W:m32|m64|m80|fp}
  ISIGNATURE(1, 0, 1, 0, 202, 0  , 0  , 0  , 0  , 0  ), // #423 {R:mem}
  ISIGNATURE(1, 0, 1, 0, 161, 0  , 0  , 0  , 0  , 0  ), // #424 {W:mem}
  ISIGNATURE(2, 1, 1, 0, 224, 225, 0  , 0  , 0  , 0  ), // #425 {W:al|ax|eax, R:u8|dx}
  ISIGNATURE(2, 1, 1, 0, 226, 227, 0  , 0  , 0  , 0  ), // #426 {W:es:[zdi], R:dx}
  ISIGNATURE(1, 1, 1, 0, 98 , 0  , 0  , 0  , 0  , 0  ), // #427 {R:u8}
  ISIGNATURE(0, 1, 0, 0, 0  , 0  , 0  , 0  , 0  , 0  ), // #428 {}
  ISIGNATURE(0, 0, 1, 0, 0  , 0  , 0  , 0  , 0  , 0  ), // #429 {}
  ISIGNATURE(1, 1, 1, 0, 228, 0  , 0  , 0  , 0  , 0  ), // #430 {R:rel8|rel32}
  ISIGNATURE(3, 1, 1, 0, 140, 173, 173, 0  , 0  , 0  ), // #431 {W:k, R:k, R:k}
  ISIGNATURE(2, 1, 1, 0, 140, 173, 0  , 0  , 0  , 0  ), // #432 {W:k, R:k}
  ISIGNATURE(2, 1, 1, 0, 173, 173, 0  , 0  , 0  , 0  ), // #433 {R:k, R:k}
  ISIGNATURE(3, 1, 1, 0, 140, 173, 98 , 0  , 0  , 0  ), // #434 {W:k, R:k, R:u8}
  ISIGNATURE(1, 1, 1, 1, 229, 0  , 0  , 0  , 0  , 0  ), // #435 {W:<ah>}
  ISIGNATURE(1, 1, 1, 0, 45 , 0  , 0  , 0  , 0  , 0  ), // #436 {R:m32|mem}
  ISIGNATURE(2, 1, 1, 0, 230, 231, 0  , 0  , 0  , 0  ), // #437 {W:r16|r32|r64, R:mem|m8|m16|m32|m48|m64|m80|m128|m256|m512|m1024}
  ISIGNATURE(1, 1, 1, 0, 232, 0  , 0  , 0  , 0  , 0  ), // #438 {R:r16|m16|mem|r32|r64}
  ISIGNATURE(2, 1, 1, 2, 233, 206, 0  , 0  , 0  , 0  ), // #439 {W:<al|ax|eax|rax>, X:<ds:[zsi]>}
  ISIGNATURE(3, 1, 1, 1, 83 , 83 , 234, 0  , 0  , 0  ), // #440 {R:xmm, R:xmm, X:<ds:[zdi]>}
  ISIGNATURE(3, 1, 1, 1, 82 , 82 , 234, 0  , 0  , 0  ), // #441 {R:mm, R:mm, X:<ds:[zdi]>}
  ISIGNATURE(3, 1, 1, 3, 205, 210, 187, 0  , 0  , 0  ), // #442 {R:<ds:[zax]>, R:<ecx>, R:<edx>}
  ISIGNATURE(2, 1, 1, 0, 79 , 83 , 0  , 0  , 0  , 0  ), // #443 {W:mm, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 84 , 83 , 0  , 0  , 0  , 0  ), // #444 {W:xmm, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 182, 83 , 0  , 0  , 0  , 0  ), // #445 {W:r32|r64, R:xmm}
  ISIGNATURE(2, 1, 1, 0, 78 , 82 , 0  , 0  , 0  , 0  ), // #446 {W:m64|mem, R:mm}
  ISIGNATURE(2, 1, 1, 0, 84 , 82 , 0  , 0  , 0  , 0  ), // #447 {W:xmm, R:mm}
  ISIGNATURE(2, 1, 1, 2, 207, 206, 0  , 0  , 0  , 0  ), // #448 {X:<es:[zdi]>, X:<ds:[zsi]>}
  ISIGNATURE(2, 0, 1, 0, 17 , 59 , 0  , 0  , 0  , 0  ), // #449 {W:r64, R:r32|m32|mem}
  ISIGNATURE(2, 1, 1, 2, 114, 210, 0  , 0  , 0  , 0  ), // #450 {R:<eax>, R:<ecx>}
  ISIGNATURE(3, 1, 1, 3, 114, 210, 211, 0  , 0  , 0  ), // #451 {R:<eax>, R:<ecx>, R:<ebx>}
  ISIGNATURE(2, 1, 1, 0, 225, 235, 0  , 0  , 0  , 0  ), // #452 {R:u8|dx, R:al|ax|eax}
  ISIGNATURE(2, 1, 1, 0, 227, 236, 0  , 0  , 0  , 0  ), // #453 {R:dx, R:ds:[zsi]}
  ISIGNATURE(6, 1, 1, 3, 83 , 86 , 98 , 237, 114, 187), // #454 {R:xmm, R:xmm|m128|mem, R:u8, W:<ecx>, R:<eax>, R:<edx>}
  ISIGNATURE(6, 1, 1, 3, 83 , 86 , 98 , 238, 114, 187), // #455 {R:xmm, R:xmm|m128|mem, R:u8, W:<xmm0>, R:<eax>, R:<edx>}
  ISIGNATURE(4, 1, 1, 1, 83 , 86 , 98 , 237, 0  , 0  ), // #456 {R:xmm, R:xmm|m128|mem, R:u8, W:<ecx>}
  ISIGNATURE(4, 1, 1, 1, 83 , 86 , 98 , 238, 0  , 0  ), // #457 {R:xmm, R:xmm|m128|mem, R:u8, W:<xmm0>}
  ISIGNATURE(3, 1, 1, 0, 172, 83 , 98 , 0  , 0  , 0  ), // #458 {W:r32|m8|mem|r64|r8lo|r8hi|r16, R:xmm, R:u8}
  ISIGNATURE(3, 0, 1, 0, 15 , 83 , 98 , 0  , 0  , 0  ), // #459 {W:r64|m64|mem, R:xmm, R:u8}
  ISIGNATURE(3, 1, 1, 0, 103, 239, 98 , 0  , 0  , 0  ), // #460 {X:xmm, R:r32|m8|mem|r64|r8lo|r8hi|r16, R:u8}
  ISIGNATURE(3, 1, 1, 0, 103, 184, 98 , 0  , 0  , 0  ), // #461 {X:xmm, R:r32|m32|mem|r64, R:u8}
  ISIGNATURE(3, 0, 1, 0, 103, 55 , 98 , 0  , 0  , 0  ), // #462 {X:xmm, R:r64|m64|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 240, 232, 98 , 0  , 0  , 0  ), // #463 {X:mm|xmm, R:r32|m16|mem|r64|r16, R:u8}
  ISIGNATURE(2, 1, 1, 0, 182, 185, 0  , 0  , 0  , 0  ), // #464 {W:r32|r64, R:mm|xmm}
  ISIGNATURE(3, 1, 1, 0, 79 , 190, 98 , 0  , 0  , 0  ), // #465 {W:mm, R:mm|m64|mem, R:u8}
  ISIGNATURE(2, 1, 1, 0, 103, 98 , 0  , 0  , 0  , 0  ), // #466 {X:xmm, R:u8}
  ISIGNATURE(2, 1, 1, 0, 48 , 137, 0  , 0  , 0  , 0  ), // #467 {X:r8lo|r8hi|m8|r16|m16|r32|m32|r64|m64|mem, R:cl|u8}
  ISIGNATURE(1, 0, 1, 0, 182, 0  , 0  , 0  , 0  , 0  ), // #468 {W:r32|r64}
  ISIGNATURE(3, 1, 1, 3, 62 , 241, 210, 0  , 0  , 0  ), // #469 {W:<edx>, W:<eax>, R:<ecx>}
  ISIGNATURE(1, 1, 1, 0, 230, 0  , 0  , 0  , 0  , 0  ), // #470 {W:r16|r32|r64}
  ISIGNATURE(2, 1, 1, 2, 62 , 241, 0  , 0  , 0  , 0  ), // #471 {W:<edx>, W:<eax>}
  ISIGNATURE(3, 1, 1, 3, 62 , 241, 237, 0  , 0  , 0  ), // #472 {W:<edx>, W:<eax>, W:<ecx>}
  ISIGNATURE(3, 1, 1, 0, 84 , 85 , 98 , 0  , 0  , 0  ), // #473 {W:xmm, R:xmm|m64|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 84 , 149, 98 , 0  , 0  , 0  ), // #474 {W:xmm, R:xmm|m32|mem, R:u8}
  ISIGNATURE(1, 1, 1, 1, 242, 0  , 0  , 0  , 0  , 0  ), // #475 {R:<ah>}
  ISIGNATURE(2, 1, 1, 2, 243, 207, 0  , 0  , 0  , 0  ), // #476 {R:<al|ax|eax|rax>, X:<es:[zdi]>}
  ISIGNATURE(1, 1, 1, 0, 1  , 0  , 0  , 0  , 0  , 0  ), // #477 {W:r8lo|r8hi|m8|mem}
  ISIGNATURE(1, 1, 1, 0, 178, 0  , 0  , 0  , 0  , 0  ), // #478 {W:r16|m16|mem|r32|r64}
  ISIGNATURE(1, 1, 1, 0, 77 , 0  , 0  , 0  , 0  , 0  ), // #479 {W:m32|mem}
  ISIGNATURE(2, 1, 1, 2, 207, 243, 0  , 0  , 0  , 0  ), // #480 {X:<es:[zdi]>, R:<al|ax|eax|rax>}
  ISIGNATURE(6, 1, 1, 0, 107, 95 , 95 , 95 , 95 , 97 ), // #481 {X:zmm, R:zmm, R:zmm, R:zmm, R:zmm, R:m128|mem}
  ISIGNATURE(3, 1, 1, 0, 84 , 83 , 85 , 0  , 0  , 0  ), // #482 {W:xmm, R:xmm, R:xmm|m64|mem}
  ISIGNATURE(3, 1, 1, 0, 84 , 83 , 149, 0  , 0  , 0  ), // #483 {W:xmm, R:xmm, R:xmm|m32|mem}
  ISIGNATURE(2, 1, 1, 0, 88 , 97 , 0  , 0  , 0  , 0  ), // #484 {W:ymm, R:m128|mem}
  ISIGNATURE(2, 1, 1, 0, 244, 85 , 0  , 0  , 0  , 0  ), // #485 {W:ymm|zmm, R:xmm|m64|mem}
  ISIGNATURE(2, 1, 1, 0, 244, 97 , 0  , 0  , 0  , 0  ), // #486 {W:ymm|zmm, R:m128|mem}
  ISIGNATURE(2, 1, 1, 0, 92 , 100, 0  , 0  , 0  , 0  ), // #487 {W:zmm, R:m256|mem}
  ISIGNATURE(2, 1, 1, 0, 245, 85 , 0  , 0  , 0  , 0  ), // #488 {W:xmm|ymm|zmm, R:xmm|m64|mem}
  ISIGNATURE(4, 1, 1, 0, 138, 83 , 85 , 98 , 0  , 0  ), // #489 {W:xmm|k, R:xmm, R:xmm|m64|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 138, 83 , 149, 98 , 0  , 0  ), // #490 {W:xmm|k, R:xmm, R:xmm|m32|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 84 , 83 , 214, 0  , 0  , 0  ), // #491 {W:xmm, R:xmm, R:r32|m32|mem|r64|m64}
  ISIGNATURE(3, 1, 1, 0, 141, 246, 98 , 0  , 0  , 0  ), // #492 {W:xmm|m128|mem, R:ymm|zmm, R:u8}
  ISIGNATURE(4, 1, 1, 0, 103, 83 , 85 , 98 , 0  , 0  ), // #493 {X:xmm, R:xmm, R:xmm|m64|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 103, 83 , 149, 98 , 0  , 0  ), // #494 {X:xmm, R:xmm, R:xmm|m32|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 103, 83 , 85 , 0  , 0  , 0  ), // #495 {X:xmm, R:xmm, R:xmm|m64|mem}
  ISIGNATURE(3, 1, 1, 0, 103, 83 , 149, 0  , 0  , 0  ), // #496 {X:xmm, R:xmm, R:xmm|m32|mem}
  ISIGNATURE(3, 1, 1, 0, 140, 247, 98 , 0  , 0  , 0  ), // #497 {W:k, R:xmm|m128|ymm|m256|zmm|m512, R:u8}
  ISIGNATURE(3, 1, 1, 0, 140, 85 , 98 , 0  , 0  , 0  ), // #498 {W:k, R:xmm|m64|mem, R:u8}
  ISIGNATURE(3, 1, 1, 0, 140, 149, 98 , 0  , 0  , 0  ), // #499 {W:k, R:xmm|m32|mem, R:u8}
  ISIGNATURE(1, 1, 1, 0, 106, 0  , 0  , 0  , 0  , 0  ), // #500 {R:vm32y}
  ISIGNATURE(1, 1, 1, 0, 108, 0  , 0  , 0  , 0  , 0  ), // #501 {R:vm32z}
  ISIGNATURE(1, 1, 1, 0, 111, 0  , 0  , 0  , 0  , 0  ), // #502 {R:vm64z}
  ISIGNATURE(4, 1, 1, 0, 84 , 83 , 85 , 98 , 0  , 0  ), // #503 {W:xmm, R:xmm, R:xmm|m64|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 84 , 83 , 149, 98 , 0  , 0  ), // #504 {W:xmm, R:xmm, R:xmm|m32|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 92 , 95 , 89 , 98 , 0  , 0  ), // #505 {W:zmm, R:zmm, R:ymm|m256|mem, R:u8}
  ISIGNATURE(2, 1, 1, 0, 182, 199, 0  , 0  , 0  , 0  ), // #506 {W:r32|r64, R:xmm|ymm}
  ISIGNATURE(6, 1, 1, 0, 92 , 95 , 95 , 95 , 95 , 97 ), // #507 {W:zmm, R:zmm, R:zmm, R:zmm, R:zmm, R:m128|mem}
  ISIGNATURE(2, 1, 1, 0, 245, 248, 0  , 0  , 0  , 0  ), // #508 {W:xmm|ymm|zmm, R:xmm|m8|mem|r32|r64|r8lo|r8hi|r16}
  ISIGNATURE(2, 1, 1, 0, 245, 249, 0  , 0  , 0  , 0  ), // #509 {W:xmm|ymm|zmm, R:xmm|m32|mem|r32|r64}
  ISIGNATURE(2, 1, 1, 0, 245, 173, 0  , 0  , 0  , 0  ), // #510 {W:xmm|ymm|zmm, R:k}
  ISIGNATURE(2, 1, 1, 0, 245, 250, 0  , 0  , 0  , 0  ), // #511 {W:xmm|ymm|zmm, R:xmm|m16|mem|r32|r64|r16}
  ISIGNATURE(4, 1, 1, 0, 84 , 83 , 239, 98 , 0  , 0  ), // #512 {W:xmm, R:xmm, R:r32|m8|mem|r64|r8lo|r8hi|r16, R:u8}
  ISIGNATURE(4, 1, 1, 0, 84 , 83 , 184, 98 , 0  , 0  ), // #513 {W:xmm, R:xmm, R:r32|m32|mem|r64, R:u8}
  ISIGNATURE(4, 0, 1, 0, 84 , 83 , 55 , 98 , 0  , 0  ), // #514 {W:xmm, R:xmm, R:r64|m64|mem, R:u8}
  ISIGNATURE(4, 1, 1, 0, 84 , 83 , 232, 98 , 0  , 0  ), // #515 {W:xmm, R:xmm, R:r32|m16|mem|r64|r16, R:u8}
  ISIGNATURE(2, 1, 1, 0, 140, 251, 0  , 0  , 0  , 0  ), // #516 {W:k, R:xmm|ymm|zmm}
  ISIGNATURE(1, 0, 1, 0, 252, 0  , 0  , 0  , 0  , 0  ), // #517 {R:r32|r64}
  ISIGNATURE(3, 1, 1, 3, 187, 114, 210, 0  , 0  , 0  ), // #518 {R:<edx>, R:<eax>, R:<ecx>}
  ISIGNATURE(1, 1, 1, 0, 253, 0  , 0  , 0  , 0  , 0  ), // #519 {R:rel16|rel32}
  ISIGNATURE(3, 1, 1, 2, 202, 187, 114, 0  , 0  , 0  ), // #520 {R:mem, R:<edx>, R:<eax>}
  ISIGNATURE(3, 0, 1, 2, 202, 187, 114, 0  , 0  , 0  ), // #521 {R:mem, R:<edx>, R:<eax>}
  ISIGNATURE(3, 1, 1, 2, 161, 187, 114, 0  , 0  , 0  ), // #522 {W:mem, R:<edx>, R:<eax>}
  ISIGNATURE(3, 0, 1, 2, 161, 187, 114, 0  , 0  , 0  )  // #523 {W:mem, R:<edx>, R:<eax>}
};
#undef ISIGNATURE
// ----------------------------------------------------------------------------
// ${signatureData:End}
#endif

// ============================================================================
// [asmjit::X86Inst - MiscData]
// ============================================================================

#define CC_TO_INST(inst) {               \
  inst##o, inst##no, inst##b , inst##ae, \
  inst##e, inst##ne, inst##be, inst##a , \
  inst##s, inst##ns, inst##pe, inst##po, \
  inst##l, inst##ge, inst##le, inst##g   \
}

const X86Inst::MiscData X86InstDB::miscData = {
  CC_TO_INST(X86Inst::kIdJ),
  CC_TO_INST(X86Inst::kIdSet),
  CC_TO_INST(X86Inst::kIdCmov),

  // ReversedCond[]:
  {
    x86::kCondO, x86::kCondNO, x86::kCondA , x86::kCondBE, // O|NO|B |AE
    x86::kCondE, x86::kCondNE, x86::kCondAE, x86::kCondB , // E|NE|BE|A
    x86::kCondS, x86::kCondNS, x86::kCondPE, x86::kCondPO, // S|NS|PE|PO
    x86::kCondG, x86::kCondLE, x86::kCondGE, x86::kCondL   // L|GE|LE|G
  }
};

#undef CC_TO_INST

// ============================================================================
// [asmjit::X86Inst - Test]
// ============================================================================

#if defined(ASMJIT_BUILD_TEST)
UNIT(x86_inst_bits) {
  INFO("Checking validity of X86Inst enums");

  // Cross-validate prefixes.
  EXPECT(X86Inst::kOptionRex  == 0x40000000U, "REX prefix must be at 0x40000000");
  EXPECT(X86Inst::kOptionVex3 == 0x00000400U, "VEX3 prefix must be at 0x00000400");
  EXPECT(X86Inst::kOptionEvex == 0x00001000U, "EVEX prefix must be at 0x00001000");

  // These could be combined together to form a valid REX prefix, they must match.
  EXPECT(int(X86Inst::kOptionOpCodeB) == int(X86Inst::kOpCode_B));
  EXPECT(int(X86Inst::kOptionOpCodeX) == int(X86Inst::kOpCode_X));
  EXPECT(int(X86Inst::kOptionOpCodeR) == int(X86Inst::kOpCode_R));
  EXPECT(int(X86Inst::kOptionOpCodeW) == int(X86Inst::kOpCode_W));

  uint32_t rex_rb = (X86Inst::kOpCode_R >> X86Inst::kOpCode_REX_Shift) |
                    (X86Inst::kOpCode_B >> X86Inst::kOpCode_REX_Shift) | 0x40;
  uint32_t rex_rw = (X86Inst::kOpCode_R >> X86Inst::kOpCode_REX_Shift) |
                    (X86Inst::kOpCode_W >> X86Inst::kOpCode_REX_Shift) | 0x40;
  EXPECT(rex_rb == 0x45, "kOpCode_R|B must form a valid REX prefix 0x45 if combined with 0x40");
  EXPECT(rex_rw == 0x4C, "kOpCode_R|W must form a valid REX prefix 0x4C if combined with 0x40");
}
#endif

#if defined(ASMJIT_BUILD_TEST) && !defined(ASMJIT_DISABLE_TEXT)
UNIT(x86_inst_names) {
  // All known instructions should be matched.
  INFO("Matching all X86/X64 instructions");
  for (uint32_t a = 0; a < X86Inst::_kIdCount; a++) {
    uint32_t b = X86Inst::getIdByName(X86Inst::getInst(a).getName());
    EXPECT(a == b,
      "Should match existing instruction \"%s\" {id:%u} != \"%s\" {id:%u}",
        X86Inst::getInst(a).getName(), a,
        X86Inst::getInst(b).getName(), b);
  }

  // Everything else should return `Inst::kIdNone`.
  INFO("Trying to look-up instructions that don't exist");
  EXPECT(X86Inst::getIdByName(nullptr)  == Inst::kIdNone, "Should return Inst::kIdNone for null input");
  EXPECT(X86Inst::getIdByName("")       == Inst::kIdNone, "Should return Inst::kIdNone for empty string");
  EXPECT(X86Inst::getIdByName("_")      == Inst::kIdNone, "Should return Inst::kIdNone for unknown instruction");
  EXPECT(X86Inst::getIdByName("123xyz") == Inst::kIdNone, "Should return Inst::kIdNone for unknown instruction");
}
#endif

ASMJIT_END_NAMESPACE

// [Guard]
#endif // ASMJIT_BUILD_X86
