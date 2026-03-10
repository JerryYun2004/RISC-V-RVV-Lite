ENTRY(_start)

MEMORY
{
  RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 4M
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
   * For NPIX = 100000:
   * each plane = 100000 * 4 = 400000 bytes = 0x61A80
   */

  . = 0x00010000;
  .in_r (NOLOAD) : ALIGN(4) {
    *(.in_r)
  } > RAM

  . = 0x00080000;
  .in_g (NOLOAD) : ALIGN(4) {
    *(.in_g)
  } > RAM

  . = 0x000F0000;
  .in_b (NOLOAD) : ALIGN(4) {
    *(.in_b)
  } > RAM

  . = 0x00160000;
  .out_luma32 (NOLOAD) : ALIGN(4) {
    *(.out_luma32)
  } > RAM

  . = ALIGN(8);
  _stack_top = ORIGIN(RAM) + LENGTH(RAM);

  ASSERT(SIZEOF(.in_r) <= 400000,  ".in_r exceeds 100000 x 4 bytes")
  ASSERT(SIZEOF(.in_g) <= 400000,  ".in_g exceeds 100000 x 4 bytes")
  ASSERT(SIZEOF(.in_b) <= 400000,  ".in_b exceeds 100000 x 4 bytes")
  ASSERT(SIZEOF(.out_luma32) <= 400000, ".out_luma32 exceeds 100000 x 4 bytes")
}