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
   * These addresses must match the preload constants in min_tb_planar.cpp
   * and min_tb_vec_planar.cpp.
   */
  . = 0x00010000;
  .in_r (NOLOAD) : ALIGN(4) {
    *(.in_r)
  } > RAM

  . = 0x00018000;
  .in_g (NOLOAD) : ALIGN(4) {
    *(.in_g)
  } > RAM

  . = 0x00020000;
  .in_b (NOLOAD) : ALIGN(4) {
    *(.in_b)
  } > RAM

  . = 0x00028000;
  .out_luma32 (NOLOAD) : ALIGN(4) {
    *(.out_luma32)
  } > RAM

  . = ALIGN(8);
  _stack_top = ORIGIN(RAM) + LENGTH(RAM);

  ASSERT(SIZEOF(.in_r) <= 32000, ".in_r exceeds 8000 x 4 bytes")
  ASSERT(SIZEOF(.in_g) <= 32000, ".in_g exceeds 8000 x 4 bytes")
  ASSERT(SIZEOF(.in_b) <= 32000, ".in_b exceeds 8000 x 4 bytes")
  ASSERT(SIZEOF(.out_luma32) <= 32000, ".out_luma32 exceeds 8000 x 4 bytes")
}
