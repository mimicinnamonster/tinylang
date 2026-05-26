/* ffi_lib.c — shared library for FFI testing */
#include <string.h>

int tl_abs(int x) { return x < 0 ? -x : x; }
double tl_pi(void) { return 3.14159; }
double tl_cube(double x) { return x * x * x; }
double tl_square(double x) { return x * x; }
int tl_strlen(const char *s) { int n = 0; while (s[n]) n++; return n; }
const char *tl_greet(void) { return "hello from c"; }
void tl_nop(void) {}
int tl_add(int a, int b) { return a + b; }
double tl_mul3(double a, double b, double c) { return a * b * c; }
