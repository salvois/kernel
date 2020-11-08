#include "test.h"
#include "string.h"
#include "stdlib.h"

static void LibcTest_memcmpEqual() {
    const void *s1 = "lorem ipsum dolor sit";
    const void *s2 = "lorem ipsum dolor sit amet";
    int result = memcmp(s1, s2, 21);
    ASSERT(result == 0);
}

static void LibcTest_memcmpLower() {
    const void *s1 = "lorem ipsum dolor sit";
    const void *s2 = "lorem ipsum dolox sit amet";
    int result = memcmp(s1, s2, 21);
    ASSERT(result < 0);
}

static void LibcTest_memcmpGreater() {
    const void *s1 = "lorem ipsum dolox sit";
    const void *s2 = "lorem ipsum dolor sit amet";
    int result = memcmp(s1, s2, 21);
    ASSERT(result > 0);
}

static void LibcTest_memcpy() {
    const char src[] = "lorem ipsum dolor sit";
    char dest[sizeof(src)];
    void *result = memcpy(dest, src, sizeof(src));
    ASSERT(result == dest);
    ASSERT(memcmp(src, dest, sizeof(src)) == 0);
}

static void LibcTest_memzero() {
    char buf[] = "lorem ipsum dolor sit";
    const char expected[] = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0it";
    void *result = memzero(buf, 19);
    ASSERT(result == buf);
    ASSERT(memcmp(buf, expected, sizeof(buf)) == 0);
}

static void LibcTest_strcmpEqual() {
    const char *s1 = "lorem ipsum dolor sit\0dummy1";
    const char *s2 = "lorem ipsum dolor sit\0dummy2";
    int result = strcmp(s1, s2);
    ASSERT(s1 != s2);
    ASSERT(result == 0);
}

static void LibcTest_strcmpLower() {
    const char *s1 = "lorem ipsum dolor sit";
    const char *s2 = "lorem ipsum dolox sit amet";
    int result = strcmp(s1, s2);
    ASSERT(result < 0);
}

static void LibcTest_strcmpGreater() {
    const char *s1 = "lorem ipsum dolox sit";
    const char *s2 = "lorem ipsum dolor sit amet";
    int result = strcmp(s1, s2);
    ASSERT(result > 0);
}

static void LibcTest_atou() {
    ASSERT(atou("00713", 5) == 713U);
    ASSERT(atou("1234", 4) == 1234U);
    ASSERT(atou("2147483647", 10) == 2147483647U);
}

static void LibcTest_xorshift64star() {
    uint64_t seed = 32760;
    ASSERT(xorshift64star(&seed) == 9652552169801048064ULL);
    ASSERT(xorshift64star(&seed) == 12866352477998707765ULL);
    ASSERT(xorshift64star(&seed) == 17222211839501022150ULL);
    ASSERT(xorshift64star(&seed) == 13233326533029222062ULL);
    ASSERT(xorshift64star(&seed) == 1287712557618408771ULL);
    ASSERT(xorshift64star(&seed) == 2917674389800011162ULL);
    ASSERT(xorshift64star(&seed) == 278365409644224204ULL);
}

void LibcTest_run() {
    RUN_TEST(LibcTest_memcmpEqual);
    RUN_TEST(LibcTest_memcmpLower);
    RUN_TEST(LibcTest_memcmpGreater);
    RUN_TEST(LibcTest_memcpy);
    RUN_TEST(LibcTest_memzero);
    RUN_TEST(LibcTest_strcmpEqual);
    RUN_TEST(LibcTest_strcmpLower);
    RUN_TEST(LibcTest_strcmpGreater);
    RUN_TEST(LibcTest_atou);
    RUN_TEST(LibcTest_xorshift64star);
}
