ENTRY(_start)

MEMORY
{
  RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 64K
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
   * Fixed planar benchmark buffers for small tests.
   * Must match min_tb_planar.cpp.
   *
   * PLANAR_NPIX = 16
   * each plane = 16 * 4 = 64 bytes = 0x40
   *
   * Keep these far enough above program/data/bss so they do not overlap.
   */
  . = 0x00001000;
  .in_r (NOLOAD) : ALIGN(4) {
    *(.in_r)
  } > RAM

  . = 0x00002000;
  .in_g (NOLOAD) : ALIGN(4) {
    *(.in_g)
  } > RAM

  . = 0x00003000;
  .in_b (NOLOAD) : ALIGN(4) {
    *(.in_b)
  } > RAM

  . = 0x00004000;
  .out_luma32 (NOLOAD) : ALIGN(4) {
    *(.out_luma32)
  } > RAM

  . = ALIGN(8);
  _stack_top = ORIGIN(RAM) + LENGTH(RAM);

  ASSERT(SIZEOF(.in_r) <= 64, ".in_r exceeds 16 x 4 bytes")
  ASSERT(SIZEOF(.in_g) <= 64, ".in_g exceeds 16 x 4 bytes")
  ASSERT(SIZEOF(.in_b) <= 64, ".in_b exceeds 16 x 4 bytes")
  ASSERT(SIZEOF(.out_luma32) <= 64, ".out_luma32 exceeds 16 x 4 bytes")
}