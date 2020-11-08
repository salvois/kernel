#include "test.h"

int exitCode = 0;

extern void LibcTest_run();

int main() {
    RUN_SUITE(LibcTest_run);
    return exitCode;
}
