#include "test.h"

int exitCode = 0;

extern void LibcTest_run();
extern void CpuNodeTest_run();

int Log_printf(const char *format, ...) { return 0; }
void panic(const char *format, ...) { }

int main() {
    RUN_SUITE(LibcTest_run);
    RUN_SUITE(CpuNodeTest_run);
    return exitCode;
}
