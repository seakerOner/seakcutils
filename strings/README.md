# mystrings

This module provides low-level string utilities focused on **ASCII-only**
manipulation.

There is no locale handling, no Unicode normalization, and no implicit
character classification.
Everything operates on raw bytes with explicit rules.

## Design Principles

- ASCII-only by design
- Explicit, bit-level operations
- No hidden allocations
- No implicit mutability
- No locale or libc character helpers

## Why ASCII?

ASCII is simple, stable, and fully understood.
For systems code, tooling, and low-level pipelines, ASCII is often sufficient
and predictable.

This module assumes the developer knows what they are manipulating.

## Case Conversion

Uppercase and lowercase conversions rely on the structural properties of ASCII
(bit 5), not on lookup tables or locale-dependent behavior.

This is intentional.

## API
```c
#define SEAK_PRINT_BITS(num_bits, bits)                                         \
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
```

## UTF Support

UTF-8 / UTF-16 support is **not currently implemented**.

It may be added in the future as a separate module, with explicit APIs and
clear ownership rules.

