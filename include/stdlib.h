#ifndef STDLIB_H_INCLUDED
#define STDLIB_H_INCLUDED

#include "stddef.h"
#include "stdint.h"

/** Converts a string to an unsigned integer value, similar the standard atoi. */
unsigned atou(const char *str, size_t length);
/** 64-bit xorshift* random number generator. */
uint64_t xorshift64star(uint64_t *seed);

#endif
