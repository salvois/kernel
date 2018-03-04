/*
FreeDOS-32 kernel
Copyright (C) 2008-2018  Salvatore ISAJA

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include "kernel.h"

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *c1 = (const uint8_t *) s1;
    const uint8_t *c2 = (const uint8_t *) s2;
    if (IS_ALIGNED(c1, sizeof (int)) && IS_ALIGNED(c2, sizeof (int))) {
        for (; n >= sizeof(int) && *(const int *) c1 == *(const int *) c2; c1 += sizeof(int), c2 += sizeof(int), n -= sizeof(int)) {
        }
    }
    while (n--) {
        if (*c1 != *c2) return *c1 - *c2;
        c1++;
        c2++;
    }
    return 0;
}

void *memcpy(void *s1, const void *s2, size_t n) {
    uint8_t *c1 = (uint8_t *) s1;
    const uint8_t *c2 = (const uint8_t *) s2;
    if (IS_ALIGNED(c1, sizeof(int)) && IS_ALIGNED(c2, sizeof(int))) {
        for (; n >= sizeof(int); c1 += sizeof(int), c2 += sizeof(int), n -= sizeof(int)) {
            *(int *) c1 = *(int *) c2;
        }
    }
    while (n--) {
        *c1++ = *c2++;
    }
    return s1;
}

void *memzero(void *s, size_t n) {
    uint8_t *c1 = (uint8_t *) s;
    if (IS_ALIGNED(c1, sizeof (int))) {
        for (; n >= sizeof(int); c1 += sizeof (int), n -= sizeof (int)) {
            *(int *) c1 = 0;
        }
    }
    while (n--) *c1++ = 0;
    return s;
}

int strcmp(const char *s1, const char *s2) {
    while (true) {
        int c1 = *s1;
        int c2 = *s2;
        if (c1 != c2) return c1 - c2;
        if (c1 == 0) return 0;
        s1++;
        s2++;
    }
}

unsigned stringToUnsigned(const char *str, size_t length) {
    unsigned result = 0;
    unsigned mult = 1;
    for (int i = length - 1; i >= 0; i--) {
        unsigned d = str[i] - '0';
        result += d * mult;
        mult *= 10;
    }
    return result;
}

// Courtesy of http://en.wikipedia.org/wiki/Linear_congruential_generator
int rand_r(uint32_t *seed) {
    *seed = 1103515245 * *seed + 12345;
    return *seed & INT32_MAX;
}

// Courtesy of https://en.wikipedia.org/wiki/Xorshift#xorshift.2A
uint64_t xorshift64star(uint64_t *seed) {
    uint64_t x = *seed;
    x ^= x >> 12; // a
    x ^= x << 25; // b
    x ^= x >> 27; // c
    *seed = x;
    return x * 0x2545F4914F6CDD1D;
}

void panic(const char *format, ...) {
    va_list args;
    va_start(args, format);
    Video_vprintf(format, args);
    va_end(args);
    asm volatile("cli" : : : "memory");
    asm volatile("hlt" : : : "memory");
    // Never arrives here
    while (true) { }
}

#ifndef NDEBUG
void __assert_fail(const char *condition, const char *filename, unsigned line, const char *function) {
    panic("Assertion %s failed in %s in %s:%u\n", condition, function, filename, line);
}
#endif
