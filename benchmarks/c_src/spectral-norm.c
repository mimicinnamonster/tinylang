/* Spectral-Norm benchmark — simple version, no SIMD
 * Based on: https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/spectralnorm-gcc-6.html
 * Naive algorithm (no 4x4 kernel) for fair comparison with tinylang.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static double eval_A(int i, int j) {
    return 1.0 / ((i + j) * (i + j + 1) / 2 + i + 1);
}

static void eval_A_times_u(int n, double *u, double *Au) {
    for (int i = 0; i < n; i++) {
        double sum = 0.0;
        for (int j = 0; j < n; j++)
            sum += eval_A(i, j) * u[j];
        Au[i] = sum;
    }
}

static void eval_At_times_u(int n, double *u, double *Au) {
    for (int i = 0; i < n; i++) {
        double sum = 0.0;
        for (int j = 0; j < n; j++)
            sum += eval_A(j, i) * u[j];
        Au[i] = sum;
    }
}

static void eval_AtA_times_u(int n, double *u, double *v, double *AtAu) {
    eval_A_times_u(n, u, v);
    eval_At_times_u(n, v, AtAu);
}

int main(int argc, char *argv[]) {
    int n = argc > 1 ? atoi(argv[1]) : 100;
    double *u = calloc(n, sizeof(double));
    double *v = calloc(n, sizeof(double));
    for (int i = 0; i < n; i++) u[i] = 1.0;

    for (int i = 0; i < 10; i++) {
        eval_AtA_times_u(n, u, v, v);
        eval_AtA_times_u(n, v, v, u);
    }

    double uv = 0.0, vv = 0.0;
    for (int i = 0; i < n; i++) {
        uv += u[i] * v[i];
        vv += v[i] * v[i];
    }
    printf("%.9f\n", sqrt(uv / vv));

    free(u);
    free(v);
    return 0;
}
