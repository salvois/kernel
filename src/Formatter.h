/*
FreeDOS-32 kernel
Copyright (C) 2008-2020  Salvatore ISAJA

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
#ifndef FORMATTABLE_H_INCLUDED
#define FORMATTABLE_H_INCLUDED

#include "Types.h"

/** Virtual function table for objects to which characters can be appended. */
typedef struct AppendableVtbl {
    /** Appends a char and returns the number of chars actually appended. */
    int (*appendChar)(void *appendable, char c);
    /** Appends a nul-terminated string and returns the number of chars actually appended. */
    int (*appendCStr)(void *appendable, const char *data);
    /** Appends an array of chars and returns the number of chars actually appended. */
    int (*appendCharArray)(void *appendable, const char *data, size_t size);
} AppendableVtbl;

/** Fat pointer to an object supporting AppendableVtbl operations. */
typedef struct Appendable {
    const AppendableVtbl *vtbl;
    void *obj;
} Appendable;

int  Formatter_vprintf(Appendable *appendable, const char *fmt, va_list args);
int  Formatter_printf(Appendable *appendable, const char *fmt, ...);
void Video_initialize();
int  Video_printf(const char *format, ...);
int  Video_vprintf(const char *fmt, va_list args);
#if LOG == LOG_NONE
static inline void Log_initialize() { }
static inline int Log_printf(const char *format, ...) { return 0; }
static inline int Log_vprintf(const char *fmt, va_list args) { return 0; }
#else
void Log_initialize();
int  Log_printf(const char *format, ...);
int  Log_vprintf(const char *fmt, va_list args);
#endif

#endif
