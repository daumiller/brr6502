; xa -bt32768 system/test/op_test.s -o op_test.rom

; WHY CAN'T I MANUALLY SPECIFY A RELATIVE BRANCH OFFSET!!?!?
; "BRA $03" -> BRA PC+3, instead of attempting an BRA $0003....
#define z_clear_or_failure .byt $D0, $03 : jmp failure
#define z_set_or_failure   .byt $F0, $03 : jmp failure
#define c_clear_or_failure .byt $90, $03 : jmp failure
#define c_set_or_failure   .byt $B0, $03 : jmp failure
#define v_clear_or_failure .byt $50, $03 : jmp failure
#define v_set_or_failure   .byt $70, $03 : jmp failure
#define n_clear_or_failure .byt $10, $03 : jmp failure
#define n_set_or_failure   .byt $30, $03 : jmp failure

#define eq_or_failure  .byt $F0, $03 : jmp failure
#define ne_or_failure  .byt $D0, $03 : jmp failure

#define clear_status_with_x       ldx #$00 : phx : plp
#define clear_status_with_y       ldy #$00 : phy : plp
#define enter_test_section(x)     lda x : pha
#define leave_test_section        pla
#define enter_test_subsection(x)  lda x : pha
#define leave_test_subsection     pla

upperHalf:
  .dsb $04, $00 ; first four bytes Zero (IO address space)
entryPoint:
  ldx #$FF : txs ; prep stack pointer

; ADC - Add with Carry
; =====================
; N V Z C
ADC_TESTS:
enter_test_section(#$01)
  enter_test_subsection(#1)
  lda #$00 : clc : adc #$00  ; 0 + 0
  n_clear_or_failure ; ensure P.NEGATIVE == 0
  v_clear_or_failure ; ensure P.OVERFLOW == 0
  z_set_or_failure   ; ensure P.ZERO     == 1
  c_clear_or_failure ; ensure P.CARRY    == 0
  cmp #$00
  eq_or_failure      ; ensure (0+0) == 0
  leave_test_subsection

  enter_test_subsection(#2)
  lda #$0F : clc : adc #$70  ; 15+112=127 ||  15+112=127
  n_clear_or_failure ; ensure P.NEGATIVE == 0
  v_clear_or_failure ; ensure P.OVERFLOW == 0
  z_clear_or_failure ; ensure P.ZERO     == 0
  c_clear_or_failure ; ensure P.CARRY    == 0
  cmp #$7F
  eq_or_failure         ; ensure ($0F + $70) == $7F
  leave_test_subsection

  enter_test_subsection(#3)
  lda #$F8 : clc : adc #$88  ; 248+136 =0x180  ||  -8 + -120 =0x80
  n_set_or_failure   ; ensure P.NEGATIVE == 1
  v_clear_or_failure ; ensure P.OVERFLOW == 0  signed -128 in range
  z_clear_or_failure ; ensure P.ZERO     == 0
  c_set_or_failure   ; ensure P.CARRY    == 1  unsigned 384 out of range
  cmp #$80
  eq_or_failure      ; esnure ($F8 + $88) == $180
  leave_test_subsection
leave_test_section

bra success

failure:
  ; end up here if we failed a test
  ; we should have pushed a pair of test identifier bytes that are available on the stack
  nop
  bra failure

success:
  ; we passed all tests!
  lda #$AA
  lda #$55
  bra success

vectorTable:
  .dsb ($FFFA - vectorTable), $00
  .word entryPoint ; NMI   Vector
  .word entryPoint ; RESET Vector
  .word entryPoint ; IRQ   Vector
