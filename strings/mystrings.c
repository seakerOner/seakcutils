
#include "mystrings.h"
#include <stdio.h>

void ascii2bits(ascii *string, uint8_t bits[8]) {
  for (int x = 0; x < 8; x += 1) {
    bits[x] = (*string >> x) & 1;
  }
}
// prints ascii character in hex and binary format
void print_ascii_stats(ascii letter) {
  ascii a_bits[8];
  ascii2bits(&letter, a_bits);

  printf("CHARACTER: '%c' | HEX: %x | BITS: ", letter, letter);
  SEAK_PRINT_BITS(8, a_bits);
}

void ascii_to_uppercase(ascii *string, size_t num_c) {
  for (size_t x = 0; x < num_c; x += 1)
    if ((string[x] >> 5) & 0b11 && string[x] <= 0x7A)
      string[x] = (string[x] - 0x20);
}

void ascii_to_lowercase(ascii *string, size_t num_c) {
  for (size_t x = 0; x < num_c; x += 1)
    if ((string[x] >> 5) & 0b10 && string[x] <= 0x5A)
      string[x] = (string[x] + 0x20);
}
