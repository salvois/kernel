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

#if LOG == LOG_PORTE9LOG

static int PortE9Log_appendChar(void *appendable, char c) {
    outp(0xE9, c);
    return 1;
}

static int PortE9Log_appendCStr(void *appendable, const char *data) {
    size_t size = 0;
    while (*data) {
        outp(0xE9, *data++);
        size++;
    }
    return size;
}

static int PortE9Log_appendCharArray(void *appendable, const char *data, size_t size) {
    for (size_t i = 0; i < size; i++) outp(0xE9, *data++);
    return size;
}

static AppendableVtbl PortE9Log_appendableVtbl = {
    .appendChar = PortE9Log_appendChar,
    .appendCStr = PortE9Log_appendCStr,
    .appendCharArray = PortE9Log_appendCharArray
};

static Appendable PortE9Log_appendable = {
    .vtbl = &PortE9Log_appendableVtbl,
    .obj = NULL
};

void Log_initialize() {
}

int Log_vprintf(const char *fmt, va_list args) {
    return Formatter_vprintf(&PortE9Log_appendable, fmt, args);
}

int Log_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = Log_vprintf(fmt, args);
    va_end(args);
    return res;
}

#endif // #if LOG == LOG_PORTE9LOG
