ENTRY(_start)

MEMORY
{
  RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 32M
}

SECTIONS
{
  .text : {
    *(.text.init)
    *(.text*)
  } > RAM

  .rodata : {
    *(.rodata*)
  } > RAM

  .data : {
    *(.data*)
  } > RAM

  .bss (NOLOAD) : {
    *(.bss*)
    *(COMMON)
  } > RAM

  /*
   * Fixed planar benchmark buffers.
   * These addresses must match the preload constants in min_tb.cpp
   * and min_tb_vec.cpp.
   *
   * For NPIX = 1000000:
   * each plane = 1000000 * 4 = 4000000 bytes = 0x003D0900
   */

  . = 0x00100000;
  .in_r (NOLOAD) : ALIGN(4) {
    *(.in_r)
  } > RAM

  . = 0x00500000;
  .in_g (NOLOAD) : ALIGN(4) {
    *(.in_g)
  } > RAM

  . = 0x00900000;
  .in_b (NOLOAD) : ALIGN(4) {
    *(.in_b)
  } > RAM

  . = 0x00D00000;
  .out_luma32 (NOLOAD) : ALIGN(4) {
    *(.out_luma32)
  } > RAM

  . = ALIGN(8);
  _stack_top = ORIGIN(RAM) + LENGTH(RAM);

  ASSERT(SIZEOF(.in_r) <= 4000000,  ".in_r exceeds 1000000 x 4 bytes")
  ASSERT(SIZEOF(.in_g) <= 4000000,  ".in_g exceeds 1000000 x 4 bytes")
  ASSERT(SIZEOF(.in_b) <= 4000000,  ".in_b exceeds 1000000 x 4 bytes")
  ASSERT(SIZEOF(.out_luma32) <= 4000000, ".out_luma32 exceeds 1000000 x 4 bytes")
}