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
