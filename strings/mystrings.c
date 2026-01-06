// Copyright 2025 Seaker <seakerone@proton.me>

// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#include "mystrings.h"
#include <stdio.h>

void ascii2bits(ascii c, uint8_t bits[8]) {
  for (int x = 0; x < 8; x += 1) {
    bits[x] = (c >> x) & 1;
  }
}
// prints ascii character in hex and binary format
void print_ascii_stats(ascii letter) {
  ascii a_bits[8];
  ascii2bits(letter, a_bits);

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
