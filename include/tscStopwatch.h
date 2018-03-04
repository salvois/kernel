/*
Serializing stopwatch based on the x86 timestamp counter (TSC).
Copyright 2012-2018 Salvatore ISAJA. All rights reserved.

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
#ifndef TSCSTOPWATCH_H_INCLUDED
#define TSCSTOPWATCH_H_INCLUDED

#include <stdint.h>

/** Returns the TSC value at the beginning of the event to measure. */
static inline uint64_t tscStopwatchBegin() {
    uint32_t a, d;
    asm volatile(
    "   cpuid\n"
    "   rdtsc\n"
    : "=a" (a), "=d" (d) : : "rbx", "rcx", "memory");
    return (((uint64_t)a) | (((uint64_t)d) << 32));
}

/** Returns the TSC value at the end of the event to measure. */
static inline uint64_t tscStopwatchEnd() {
    uint32_t a, d;
    asm volatile(
    "   rdtscp\n"
    "   mov %%edx, %0\n"
    "   mov %%eax, %1\n"
    "   cpuid\n"
    : "=r" (d), "=r" (a) : : "rax", "rbx", "rcx", "rdx", "memory");
    return (((uint64_t)a) | (((uint64_t)d) << 32));
}

#endif
