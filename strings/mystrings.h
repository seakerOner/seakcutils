#ifndef MYSTRINGS_H
#define MYSTRINGS_H

#include <stdint.h>
#include <string.h>

#define SEAK_PRINT_BITS(num_bits, bits)                                             \
  for (size_t x = num_bits; x > 0; x--) {                                      \
    printf("%d", bits[x - 1]);                                                 \
    if (x - 1 == 0)                                                            \
      printf("\n");                                                            \
  }

typedef uint8_t ascii;

void ascii2bits(ascii *string, uint8_t bits[8]);

// prints ascii character in hex and binary format
void print_ascii_stats(ascii letter);

void ascii_to_uppercase(ascii *string, size_t num_c);

void ascii_to_lowercase(ascii *string, size_t num_c);

#define ASCII_START 0x20
#define ASCII_MAX 0x7E

#endif // !MYSTRINGS_H
