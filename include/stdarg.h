#ifndef STDARG_H_INCLUDED
#define STDARG_H_INCLUDED

typedef __builtin_va_list va_list;
#define va_start(l, a) __builtin_va_start(l, a)
#define va_end(l)      __builtin_va_end(l)
#define va_arg(l, t)   __builtin_va_arg(l, t)

#endif
