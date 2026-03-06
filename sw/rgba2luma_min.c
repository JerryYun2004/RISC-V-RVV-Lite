#include <stdint.h>

// If your platform already provides UART/printf, use it.
// Otherwise, replace these with your platform's print mechanism.
extern void putchar_uart(char c);

static void print_u32_hex(uint32_t x) {
  const char *hex = "0123456789abcdef";
  for (int i = 7; i >= 0; --i) {
    putchar_uart(hex[(x >> (i*4)) & 0xF]);
  }
}

static void print_str(const char *s) {
  while (*s) putchar_uart(*s++);
}

// Minimal deterministic input
#define NPIX  (16)

static uint32_t in_pixels[NPIX];
static uint8_t  out_luma[NPIX];

// 0xAABBGGRR -> Y
static inline uint8_t rgba2luma(uint32_t p) {
  uint32_t r = (p >> 0)  & 0xFF;
  uint32_t g = (p >> 8)  & 0xFF;
  uint32_t b = (p >> 16) & 0xFF;
  uint32_t y = (77*r + 150*g + 29*b) >> 8;
  return (uint8_t)y;
}

int main(void) {
  // init input
  for (int i = 0; i < NPIX; i++) {
    uint32_t rr = (i * 3) & 0xFF;
    uint32_t gg = (i * 5) & 0xFF;
    uint32_t bb = (i * 7) & 0xFF;
    in_pixels[i] = (0xAAu << 24) | (bb << 16) | (gg << 8) | (rr << 0);
  }

  // warm-up (optional)
  volatile uint32_t sink = 0;
  for (int i = 0; i < 16; i++) sink += rgba2luma(in_pixels[i]);

  uint32_t c0 = 0;

  for (int i = 0; i < NPIX; i++) {
    out_luma[i] = rgba2luma(in_pixels[i]);
  }

  uint32_t c1 = 0;

  // checksum so compiler can’t “optimize away”
  uint32_t sum = 0;
  for (int i = 0; i < NPIX; i++) sum += out_luma[i];

  print_str("[rgba2luma] cycles=");
  print_u32_hex(c1 - c0);
  print_str(" sum=");
  print_u32_hex(sum);
  print_str("\n");

  return 0;
}