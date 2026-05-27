/* Fibonacci benchmark — fib(N) using iterative computation
 * Compile: cc -O2 -lm -o fib_c fib.c && ./fib_c 35
 */
#include <stdio.h>
#include <stdlib.h>

static double fib(int n) {
    if (n < 2) return n;
    double a = 0, b = 1, c;
    for (int i = 2; i <= n; i++) {
        c = a + b;
        a = b;
        b = c;
    }
    return b;
}

int main(int argc, char *argv[]) {
    int n = argc > 1 ? atoi(argv[1]) : 35;
    double result = fib(n);
    printf("%.0f\n", result);
    return 0;
}
