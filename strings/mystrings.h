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

void ascii2bits(ascii c, uint8_t bits[8]);

// prints ascii character in hex and binary format
void print_ascii_stats(ascii letter);

void ascii_to_uppercase(ascii *string, size_t num_c);

void ascii_to_lowercase(ascii *string, size_t num_c);

#define ASCII_START 0x20
#define ASCII_MAX 0x7E

#endif // !MYSTRINGS_H
