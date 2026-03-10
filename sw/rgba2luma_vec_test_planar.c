#include <stdint.h>

extern void putchar_uart(char c);
extern void rgba2luma_vec(uint32_t *r, uint32_t *g, uint32_t *b,
                          uint32_t *out, uint32_t n);

static void print_u32_hex(uint32_t x) {
  const char *hex = "0123456789abcdef";
  for (int i = 7; i >= 0; --i) putchar_uart(hex[(x >> (i * 4)) & 0xF]);
}

static void print_str(const char *s) {
  while (*s) putchar_uart(*s++);
}

#define NPIX 100000

/*
 * Planar RGB layout, one 32-bit word per channel sample.
 * Only the low 8 bits are meaningful.
 *
 * The testbench preloads these arrays before reset release.
 */
static uint32_t in_r[NPIX] __attribute__((section(".in_r")));
static uint32_t in_g[NPIX] __attribute__((section(".in_g")));
static uint32_t in_b[NPIX] __attribute__((section(".in_b")));
static uint32_t out_luma32[NPIX] __attribute__((section(".out_luma32")));

int main(void) {
  rgba2luma_vec(in_r, in_g, in_b, out_luma32, NPIX);

  uint32_t sum = 0;
  for (int i = 0; i < NPIX; i++) sum += (out_luma32[i] & 0xFFu);

  print_str("[rgb2luma_vec_planar] sum=");
  print_u32_hex(sum);
  print_str("\n");

  return 0;
}
