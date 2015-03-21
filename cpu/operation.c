#include "cpu/internal.h"

#define OPCODE_LINE(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
  OP_CODE_##a, OP_CODE_##b, OP_CODE_##c, OP_CODE_##d, OP_CODE_##e, OP_CODE_##f, OP_CODE_##g, OP_CODE_##h, \
  OP_CODE_##i, OP_CODE_##j, OP_CODE_##k, OP_CODE_##l, OP_CODE_##m, OP_CODE_##n, OP_CODE_##o, OP_CODE_##p

#define ABS   ADDRESS_MODE_ABSOLUTE                      //  a
#define IAX   ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT     // (a,x)
#define AX    ADDRESS_MODE_ABSOLUTE_INDEXED_X            //  a,x
#define AY    ADDRESS_MODE_ABSOLUTE_INDEXED_Y            //  a,y
#define IA    ADDRESS_MODE_ABSOLUTE_INDRECT              // (a)
#define ACC   ADDRESS_MODE_ACCUMULATOR                   //  A
#define IMM   ADDRESS_MODE_IMMEDIATE                     //  #
#define IMP   ADDRESS_MODE_IMPLIED                       //  i
#define REL   ADDRESS_MODE_RELATIVE                      //  r
#define ST    ADDRESS_MODE_STACK                         //  s
#define ZP    ADDRESS_MODE_ZERO_PAGE                     //  zp
#define IZPX  ADDRESS_MODE_ZERO_PAGE_INDEXED_INDIRECT    // (zp,x)
#define ZPX   ADDRESS_MODE_ZERO_PAGE_INDEXED_X           //  zp,x
#define ZPY   ADDRESS_MODE_ZERO_PAGE_INDEXED_Y           //  zp,y
#define IZP   ADDRESS_MODE_ZERO_PAGE_INDIRECT            // (zp)
#define IZPY  ADDRESS_MODE_ZERO_PAGE_INDIRECT_INDEXED_Y  // (zp),y
#define ZPR   ADDRESS_MODE_ZERO_PAGE_RELATIVE            //  zp,r
#define xxa   ADDRESS_MODE_xxx                           //  invalid

OpCode op_code_table[256] = {
  //  Low 4b    0    1    2    3    4    5    6    7     8    9    A    B    C    D    E    F      // High 4b
  OPCODE_LINE( BRK, ORA, xxx, xxx, TSB, ORA, ASL, RMB0, PHP, ORA, ASL, xxx, TSB, ORA, ASL, BBR0 ), //    0
  OPCODE_LINE( BPL, ORA, ORA, xxx, TRB, ORA, ASL, RMB1, CLC, ORA, INC, xxx, TRB, ORA, ASL, BBR1 ), //    1
  OPCODE_LINE( JSR, AND, xxx, xxx, BIT, AND, ROL, RMB2, PLP, AND, ROL, xxx, BIT, AND, ROL, BBR2 ), //    2
  OPCODE_LINE( BMI, AND, AND, xxx, BIT, AND, ROL, RMB3, SEC, AND, DEC, xxx, BIT, AND, ROL, BBR3 ), //    3
  OPCODE_LINE( RTI, XOR, xxx, xxx, xxx, XOR, LSR, RMB4, PHA, XOR, LSR, xxx, JMP, XOR, LSR, BBR4 ), //    4
  OPCODE_LINE( BVC, XOR, XOR, xxx, xxx, XOR, LSR, RMB5, CLI, XOR, PHY, xxx, xxx, XOR, LSR, BBR5 ), //    5
  OPCODE_LINE( RTS, ADC, xxx, xxx, STZ, ADC, ROR, RMB6, PLA, ADC, ROR, xxx, JMP, ADC, ROR, BBR6 ), //    6
  OPCODE_LINE( BVS, ADC, ADC, xxx, STZ, ADC, ROR, RMB7, SEI, ADC, PLY, xxx, JMP, ADC, ROR, BBR7 ), //    7
  OPCODE_LINE( BRA, STA, xxx, xxx, STY, STA, STX, SMB0, DEY, BIT, TXA, xxx, STY, STA, STX, BBS0 ), //    8
  OPCODE_LINE( BCC, STA, STA, xxx, STY, STA, STX, SMB1, TYA, STA, TXS, xxx, STZ, STA, STZ, BBS1 ), //    9
  OPCODE_LINE( LDY, LDA, LDX, xxx, LDY, LDA, LDX, SMB2, TAY, LDA, TAX, xxx, LDY, LDA, LDX, BBS2 ), //    A
  OPCODE_LINE( BCS, LDA, LDA, xxx, LDY, LDA, LDX, SMB3, CLV, LDA, TSX, xxx, LDY, LDA, LDX, BBS3 ), //    B
  OPCODE_LINE( CPY, CMP, xxx, xxx, CPY, CMP, DEC, SMB4, INY, CMP, DEX, WAI, CPY, CMP, DEC, BBS4 ), //    C
  OPCODE_LINE( BNE, CMP, CMP, xxx, xxx, CMP, DEC, SMB5, CLD, CMP, PHX, STP, xxx, CMP, DEC, BBS5 ), //    D
  OPCODE_LINE( CPX, SBC, xxx, xxx, CPX, SBC, INC, SMB6, INX, SBC, NOP, xxx, CPX, SBC, INC, BBS6 ), //    E
  OPCODE_LINE( BEQ, SBC, SBC, xxx, xxx, SBC, INC, SMB7, SED, SBC, PLX, xxx, xxx, SBC, INC, BBS7 )  //    F
};

AddressMode address_mode_table[256] = {
// 0     1    2    3    4    5    6    7    8    9    A    B    C    D    E    F    //  High
  ABS, IZPX, xxa, xxa,  ZP,  ZP,  ZP,  ZP,  ST, IMM, ACC, xxa, ABS, ABS, ABS, ZPR,  //   0
  REL, IZPY, IZP, xxa,  ZP, ZPX, ZPX,  ZP, IMP,  AY, ACC, xxa, ABS,  AX,  AX, ZPR,  //   1
  ABS, IZPX, xxa, xxa,  ZP,  ZP,  ZP,  ZP,  ST, IMM, ACC, xxa, ABS, ABS, ABS, ZPR,  //   2
  REL, IZPY, IZP, xxa, ZPX, ZPX, ZPX,  ZP, IMP,  AY, ACC, xxa,  AX,  AX,  AX, ZPR,  //   3
   ST, IZPX, xxa, xxa, xxa,  ZP,  ZP,  ZP,  ST, IMM, ACC, xxa, ABS, ABS, ABS, ZPR,  //   4
  REL, IZPY, IZP, xxa, xxa, ZPX, ZPX,  ZP, IMP,  AY,  ST, xxa, xxa,  AX,  AX, ZPR,  //   5
   ST, IZPX, xxa, xxa,  ZP,  ZP,  ZP,  ZP,  ST, IMM, ACC, xxa,  IA, ABS, ABS, ZPR,  //   6
  REL, IZPY, IZP, xxa, ZPX, ZPX, ZPX,  ZP, IMP,  AY,  ST, xxa, IAX,  AX,  AX, ZPR,  //   7
  REL, IZPX, xxa, xxa,  ZP,  ZP,  ZP,  ZP, IMP, IMM, IMP, xxa, ABS, ABS, ABS, ZPR,  //   8
  REL, IZPY, IZP, xxa, ZPX, ZPX, ZPY,  ZP, IMP,  AY, IMP, xxa, ABS,  AX,  AX, ZPR,  //   9
  IMM, IZPX, IMM, xxa,  ZP,  ZP,  ZP,  ZP, IMP, IMM, IMP, xxa, ABS, ABS, ABS, ZPR,  //   A
  REL, IZPY, IZP, xxa, ZPX, ZPX, ZPY,  ZP, IMP,  AY, IMP, xxa,  AX,  AX,  AX, ZPR,  //   B
  IMM, IZPX, xxa, xxa,  ZP,  ZP,  ZP,  ZP, IMP, IMM, IMP, IMP, ABS, ABS, ABS, ZPR,  //   C
  REL, IZPY, IZP, xxa, xxa, ZPX, ZPX,  ZP, IMP,  AY,  ST, IMP, xxa,  AX,  AX, ZPR,  //   D
  IMM, IZPX, xxa, xxa,  ZP,  ZP,  ZP,  ZP, IMP, IMM, IMP, xxa, ABS, ABS, ABS, ZPR,  //   E
  REL, IZPY, IZP, xxa, xxa, ZPX, ZPX,  ZP, IMP,  AY,  ST, xxa, xxa,  AX,  AX, ZPR   //   F
};
