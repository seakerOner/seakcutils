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
/*
------------------------------------------------------------------------------
mystrings.h — ASCII Utilities and Bit Manipulation

This header provides basic ASCII string manipulation and character analysis
functions, as well as utilities to inspect the binary representation of ASCII
characters.

------------------------------------------------------------------------------
TYPES

ascii
  Typedef for uint8_t representing a single ASCII character.

------------------------------------------------------------------------------
MACROS

SEAK_PRINT_BITS(num_bits, bits)
  Prints the bits of an ASCII character or integer in reverse order (MSB->LSB).

  num_bits : number of bits to print (typically 8 for ASCII)
  bits     : array of bits (uint8_t) to print

ASCII_START
  The first printable ASCII character (0x20, space).

ASCII_MAX
  The last printable ASCII character (0x7E, '~').

------------------------------------------------------------------------------
FUNCTIONS

void ascii2bits(ascii c, uint8_t bits[8])
  Converts an ASCII character into its 8-bit binary representation.

  c    : ASCII character to convert
  bits : array of 8 bytes where each bit will be stored (LSB at index 0)

  Example:
    ascii2bits('A', bits);
    // bits will contain: 0 0 0 0 0 0 1 0 1 (binary representation of 'A')

void print_ascii_stats(ascii letter)
  Prints an ASCII character along with its hexadecimal and binary
representation.

  letter : ASCII character to print

  Output format:
    CHARACTER: 'A' | HEX: 41 | BITS: 01000001

void ascii_to_uppercase(ascii *string, size_t num_c)
  Converts a null-terminated ASCII string to uppercase in place.

  string : pointer to the ASCII string
  num_c  : number of characters in the string to convert

  Notes:
    - Only affects lowercase letters 'a'-'z'.
    - Does not allocate new memory; conversion is in place.

void ascii_to_lowercase(ascii *string, size_t num_c)
  Converts a null-terminated ASCII string to lowercase in place.

  string : pointer to the ASCII string
  num_c  : number of characters in the string to convert

  Notes:
    - Only affects uppercase letters 'A'-'Z'.
    - Does not allocate new memory; conversion is in place.

------------------------------------------------------------------------------
EXAMPLES

ascii c = 'A';
uint8_t bits[8];
ascii2bits(c, bits);
print_ascii_stats(c);

ascii str[] = "Hello, World!";
ascii_to_uppercase(str, 13); // converts str to "HELLO, WORLD!"
ascii_to_lowercase(str, 13); // converts back to "hello, world!"

------------------------------------------------------------------------------
WARNINGS

- Functions operate only on ASCII printable characters.
- `ascii2bits` and `print_ascii_stats` assume 8-bit ASCII.
- `ascii_to_uppercase` and `ascii_to_lowercase` do not handle UTF-8 or extended
ASCII.
------------------------------------------------------------------------------
*/
#ifndef MYSTRINGS_H
#define MYSTRINGS_H

#include <stdint.h>
#include <string.h>

#define SEAK_PRINT_BITS(num_bits, bits)                                        \
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

#if (defined(MYSTRINGS_IMPLEMENTATION))
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
#endif
