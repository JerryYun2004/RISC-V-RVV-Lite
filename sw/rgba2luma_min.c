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

static uint32_t in_pixels[NPIX];
static uint32_t out_luma32[NPIX];   // match vector benchmark memory traffic

// 0xAABBGGRR -> Y in low 8 bits of a 32-bit word
static inline uint32_t rgba2luma_scalar_u32(uint32_t p) {
  uint32_t r = (p >> 0)  & 0xFFu;
  uint32_t g = (p >> 8)  & 0xFFu;
  uint32_t b = (p >> 16) & 0xFFu;
  uint32_t y = (77u * r + 150u * g + 29u * b) >> 8;
  return y & 0xFFu;
}

int main(void) {
  for (int i = 0; i < NPIX; i++) {
    uint32_t rr = (i * 3) & 0xFFu;
    uint32_t gg = (i * 5) & 0xFFu;
    uint32_t bb = (i * 7) & 0xFFu;
    in_pixels[i] = (0xAAu << 24) | (bb << 16) | (gg << 8) | (rr << 0);
  }

  for (int i = 0; i < NPIX; i++) {
    out_luma32[i] = rgba2luma_scalar_u32(in_pixels[i]);
  }

  // checksum: low 8 bits only, exactly like vector benchmark meaning
  uint32_t sum = 0;
  for (int i = 0; i < NPIX; i++) sum += (out_luma32[i] & 0xFFu);

  print_str("[rgba2luma_scalar] sum=");
  print_u32_hex(sum);
  print_str("\n");

  return 0;
}