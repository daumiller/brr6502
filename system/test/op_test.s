; xa -bt32768 system/test/op_test.s -o op_test.rom

; manually specifying relative branching (don't want to define a zillion different unique labels)
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
; a       ADC $BEEF    Absolute
; a,x     ADC $BE00,x  Absolute Indexed X
; a,y     ADC $BE00,y  Absolute Indexed Y
; #       ADC #$FF     Immediate
; zp      ADC $32      Zero Page
; (zp,x)  ADC ($30,x)  Zero Page Indexed Indirect
; zp,x    ADC $30,x    Zero Page Indexed X
; (zp)    ADC ($32)    Zero Page Indirect
; (zp),y  ADC ($32),y  Zero Page Indirect Indexed Y
ADC_TESTS:
enter_test_section(#1)
  ; immediate tests
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

  ; absolute
  enter_test_subsection(#4)
  ldx #31 : stx $1234
  lda #31 : clc : adc $1234 ; 31+31 = 62
  n_clear_or_failure
  v_clear_or_failure
  z_clear_or_failure
  c_clear_or_failure
  cmp #62
  eq_or_failure
  leave_test_subsection

  ; absolute indexed x
  enter_test_subsection(#5)
  ldx #41 : stx $4331
  ldx #$31
  lda #41 : clc : adc $4300,x ; 41+41 = 82
  n_clear_or_failure
  v_clear_or_failure
  z_clear_or_failure
  c_clear_or_failure
  cmp #82
  eq_or_failure
  leave_test_subsection

  ; absolute indexed y
  enter_test_subsection(#6)
  ldx #51 : stx $4343
  ldy #$43
  lda #51 : clc : adc $4300,y ; 51+51 = 102
  n_clear_or_failure
  v_clear_or_failure
  z_clear_or_failure
  c_clear_or_failure
  cmp #102
  eq_or_failure
  leave_test_subsection

  ; zero page
  enter_test_subsection(#7)
  ldx #61 : stx $00AA
  lda #61 : clc : adc $AA ; 61+61 = 122
  n_clear_or_failure
  v_clear_or_failure
  z_clear_or_failure
  c_clear_or_failure
  cmp #122
  eq_or_failure
  leave_test_subsection

  ; zero page indexed indirect
  enter_test_subsection(#8)
  ldx #40  : stx $4331
  ldx #$31 : stx $00BB
  ldx #$43 : stx $00BC
  ldx #$0B
  lda #60 : clc : adc ($B0,x) ; 40+60 = 100
  n_clear_or_failure
  v_clear_or_failure
  z_clear_or_failure
  c_clear_or_failure
  cmp #100
  eq_or_failure
  leave_test_subsection

  ; zero page indexed
  enter_test_subsection(#9)
  ldx #30  : stx $00CC
  ldx #$0C
  lda #70 : clc : adc $C0,x ; 30+70 = 100
  n_clear_or_failure
  v_clear_or_failure
  z_clear_or_failure
  c_clear_or_failure
  cmp #100
  eq_or_failure
  leave_test_subsection

  ; zero page indirect
  enter_test_subsection(#10)
  ldx #20  : stx $00CC
  ldx #$CC : stx $00DD
  lda #80  : clc : adc ($DD) ; 20+80 = 100
  n_clear_or_failure
  v_clear_or_failure
  z_clear_or_failure
  c_clear_or_failure
  cmp #100
  eq_or_failure
  leave_test_subsection

  ; zero page indirect indexed
  enter_test_subsection(#11)
  ldx #10  : stx $5678
  ldx #$70 : stx $00EE
  ldx #$56 : stx $00EF
  ldy #$08
  lda #90  : clc : adc ($EE),y ; 10+90 = 100
  n_clear_or_failure
  v_clear_or_failure
  z_clear_or_failure
  c_clear_or_failure
  cmp #100
  eq_or_failure
  leave_test_subsection

leave_test_section

bra success

failure:
  ; end up here if we failed a test
  ; we should have pushed a pair of test identifier bytes that are available on the stack
  lda #$00
  ldx #$FA
  ldy #$11
  nop
  bra failure

success:
  ; we passed all tests!
  lda #$00
  ldx #$94
  ldy #$55
  nop
  bra success

vectorTable:
  .dsb ($FFFA - vectorTable), $00
  .word entryPoint ; NMI   Vector
  .word entryPoint ; RESET Vector
  .word entryPoint ; IRQ   Vector
