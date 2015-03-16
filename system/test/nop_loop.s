; xa -bt32768 system/test/nop_loop.s -o nop_loop.rom

upperHalf:
  .dsb $04, $00 ; first four bytes Zero (IO address space)

entryPoint:
  nop
  bra entryPoint

vectorTable:
  .dsb ($FFFA - vectorTable), $00
  .word entryPoint
  .word entryPoint
  .word entryPoint
