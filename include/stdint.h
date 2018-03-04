#ifndef STDINT_H_INCLUDED
#define STDINT_H_INCLUDED

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;
typedef uint32_t           uintptr_t;
typedef int32_t            intptr_t;
typedef int32_t            ptrdiff_t;
#define INT32_MAX          0x7FFFFFFF
#define UINT32_MAX         0xFFFFFFFFU
#define UINT64_MAX         0xFFFFFFFFFFFFFFFFULL

#endif
