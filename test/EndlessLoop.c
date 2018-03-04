char c1[] = "Lorem ipsum dolor sit amet";
int i = 42;
char ca[128];

__attribute__((noreturn)) int _start() {
    int a = 1;
    int b = 1;
    while (1) {
        int c = a + b;
        a = b;
        b = c;
    }
}
