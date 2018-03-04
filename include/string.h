#ifndef STRING_H_INCLUDED
#define STRING_H_INCLUDED

/** Provides the standard memcmp functionality. */
int memcmp(const void *s1, const void *s2, size_t n);
/** Provides the standard memcpy functionality. */
void *memcpy(void *s1, const void *s2, size_t n);
/** Zeroes the content of the specified memory block. */
void *memzero(void *s, size_t n);
/** Provides the standard strcmp functionality. */
int strcmp(const char *s1, const char *s2);

#endif
