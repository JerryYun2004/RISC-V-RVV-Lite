#include <stdint.h>

extern void putchar_uart(char c);

static void print_u32_hex(uint32_t x) {
  const char *hex = "0123456789abcdef";
  for (int i = 7; i >= 0; --i) putchar_uart(hex[(x >> (i * 4)) & 0xF]);
}

static void print_str(const char *s) {
  while (*s) putchar_uart(*s++);
}

#define NPIX 16

/*
 * Planar RGB layout, one 32-bit word per channel sample.
 * Only the low 8 bits are meaningful.
 *
 * These arrays are intentionally left uninitialized in C.
 * The testbench preloads them before reset release.
 */
static uint32_t in_r[NPIX] __attribute__((section(".in_r")));
static uint32_t in_g[NPIX] __attribute__((section(".in_g")));
static uint32_t in_b[NPIX] __attribute__((section(".in_b")));
static uint32_t out_luma32[NPIX] __attribute__((section(".out_luma32")));

static inline uint32_t rgb2luma_scalar_u32(uint32_t r, uint32_t g, uint32_t b) {
  r &= 0xFFu;
  g &= 0xFFu;
  b &= 0xFFu;
  return ((77u * r + 150u * g + 29u * b) >> 8) & 0xFFu;
}

int main(void) {
  for (int i = 0; i < NPIX; i++) {
    out_luma32[i] = rgb2luma_scalar_u32(in_r[i], in_g[i], in_b[i]);
  }

  uint32_t sum = 0;
  for (int i = 0; i < NPIX; i++) sum += (out_luma32[i] & 0xFFu);

  print_str("[rgb2luma_scalar_planar] sum=");
  print_u32_hex(sum);
  print_str("\n");

  return 0;
}
