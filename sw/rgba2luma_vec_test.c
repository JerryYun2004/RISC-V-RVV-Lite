#include <stdint.h>

extern void putchar_uart(char c);
extern void rgba2luma_vec(uint32_t *in, uint32_t *out, uint32_t n);

static void print_u32_hex(uint32_t x) {
  const char *hex = "0123456789abcdef";
  for (int i = 7; i >= 0; --i) putchar_uart(hex[(x >> (i*4)) & 0xF]);
}

static void print_str(const char *s) {
  while (*s) putchar_uart(*s++);
}

#define NPIX 512

static uint32_t in_pixels[NPIX];
static uint32_t out_luma32[NPIX];   // <-- 32-bit output (no vse8.v)

int main(void) {
  for (int i = 0; i < NPIX; i++) {
    uint32_t rr = (i * 3) & 0xFF;
    uint32_t gg = (i * 5) & 0xFF;
    uint32_t bb = (i * 7) & 0xFF;
    in_pixels[i] = (0xAAu << 24) | (bb << 16) | (gg << 8) | (rr << 0);
  }

  rgba2luma_vec(in_pixels, out_luma32, NPIX);

  // checksum (use low 8 bits only, same meaning as original)
  uint32_t sum = 0;
  for (int i = 0; i < NPIX; i++) sum += (out_luma32[i] & 0xFFu);

  print_str("[rgba2luma_vec] sum=");
  print_u32_hex(sum);
  print_str("\n");

  return 0;
}