#ifndef STDDEF_H_INCLUDED
#define STDDEF_H_INCLUDED

#define NULL ((void *) 0)

/** Returns the offset of the specified member in the specified type. */
#define offsetof(type, member)  __builtin_offsetof(type, member)

typedef unsigned size_t;
typedef int      ssize_t;

#endif
