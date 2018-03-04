/*
C wrappers for assembly atomic operations on a word-sized integer.
Copyright 2014-2018 Salvatore ISAJA. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef ATOMICWORD_H_INCLUDED
#define ATOMICWORD_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

/** Plain word-sized integer. */
typedef uint32_t Word;

/** Word-sized integer to be used with atomic operations. */
typedef struct AtomicWord {
    Word value;
} AtomicWord;

/** Initializes the atomic integer with the specified value. */
static inline void AtomicWord_init(AtomicWord *a, Word value) {
    a->value = value;
}

/**
 * Atomic compare and swap.
 * Compares oldValue against the current value. If they are equal, assigns newValue.
 * @return The original value.
 */
static inline Word AtomicWord_compareAndSwap(AtomicWord *a, Word oldValue, Word newValue) {
    asm volatile("lock; cmpxchg %2, %1" : "=a" (oldValue) : "m" (a->value), "r" (newValue), "0" (oldValue) : "memory");
    return oldValue;
}

/**
 * Atomic compare and set.
 * Compares oldValue against the current value. If they are equal, assigns newValue.
 * @return True if the new value has been assigned.
 */
static inline bool AtomicWord_compareAndSet(AtomicWord *a, Word oldValue, Word newValue) {
    uint8_t result;
    asm volatile("lock; cmpxchg %2, %1; setz %0" : "=a" (result) : "m" (a->value), "r" (newValue), "0" (oldValue) : "memory");
    return result;
}

/** Atomically assigns the specified value and returns the previous value. */
static inline Word AtomicWord_getAndSet(AtomicWord *a, Word newValue) {
    asm volatile("xchg %0, %1" // implies lock
            : "=r" (newValue) : "m" (a->value), "0" (newValue) : "memory");
    return newValue;
}

/** Atomically increments by one. */
static inline void AtomicWord_increment(AtomicWord *a) {
    asm volatile("lock; incl %0" : "+m" (a->value) : : "memory");
}

/** Atomically decrements by one. */
static inline void AtomicWord_decrement(AtomicWord *a) {
    asm volatile("lock; decl %0" : "+m" (a->value) : : "memory");
}

/** Atomically decrements by one and return true if the result is 0. */
static inline bool AtomicWord_decrementAndTestZero(AtomicWord *a) {
    uint8_t res;
    asm volatile("lock; decl %0; setz %1" : "+m" (a->value), "=a" (res) : : "memory");
    return res;
}

/** Atomically gets the current value. */
static inline Word AtomicWord_get(const AtomicWord *a) {
    int result;
    asm volatile("mov %1, %0" : "=a" (result) : "m" (a->value) : "memory");
    return result;
}

/** Atomically sets the current value. */
static inline void AtomicWord_set(AtomicWord *a, Word newValue) {
    asm volatile("mov %1, %0" : "=m" (a->value) : "r" (newValue) : "memory");
}

/** Atomically performs a bitwise OR between this value and the specified bit mask. */
static inline void AtomicWord_bitwiseOr(AtomicWord *a, Word bits) {
    asm volatile("lock; orl %0, %1" : : "m" (a->value), "r" (bits) : "memory");
}

/** Atomically performs a bitwise AND between this value and the specified bit mask. */
static inline void AtomicWord_bitwiseAnd(AtomicWord *a, Word bits) {
    asm volatile("lock; andl %0, %1" : : "m" (a->value), "r" (bits) : "memory");
}

/** Atomically performs a bitwise XOR between this value and the specified bit mask. */
static inline void AtomicWord_bitwiseXor(AtomicWord *a, Word bits) {
    asm volatile("lock; xorl %0, %1" : : "m" (a->value), "r" (bits) : "memory");
}

/** Atomically adds the specified delta returns the previous value. */
static inline Word AtomicWord_getAndAdd(AtomicWord *a, int delta) {
    asm volatile("lock; xadd %0, %1" : "+r" (delta) : "m" (a->value) : "memory");
    return delta;
}

/** Atomically adds the specified delta and returns the new value. */
static inline Word AtomicWord_addAndGet(AtomicWord *a, int delta) {
    int i = delta;
    asm volatile("lock; xadd %0, %1" : "=r" (delta) : "m" (a->value), "0" (delta) : "memory");
    return delta + i;
}

#endif
