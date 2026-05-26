/* Fasta benchmark — simple version
 * Based on: https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/fasta-gcc-3.html
 * Simplified version for fair comparison with tinylang.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IM 139968
#define IA 3877
#define IC 29573
#define SEED 42

static unsigned int seed = SEED;

static inline unsigned int uint32_rand(void) {
    seed = (seed * IA + IC) % IM;
    return seed;
}

static const char *alu =
    "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGG"
    "GAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGA"
    "CCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAAT"
    "ACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCA"
    "GCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGG"
    "AGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCC"
    "AGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";

static void repeat_fasta(const char *seq, int n) {
    int len = strlen(seq);
    char buf[61];
    int pos = 0;
    while (n > 0) {
        int line = n < 60 ? n : 60;
        for (int i = 0; i < line; i++) {
            buf[i] = seq[pos++];
            if (pos >= len) pos = 0;
        }
        buf[line] = '\n';
        fwrite(buf, 1, line + 1, stdout);
        n -= line;
    }
}

static void random_fasta(const char *symb, const float *probs, int n) {
    // Build cumulative probability table
    float cum[16];
    int nsymb = strlen(symb);
    float sum = 0;
    for (int i = 0; i < nsymb; i++) {
        sum += probs[i];
        cum[i] = sum;
    }

    char buf[61];
    while (n > 0) {
        int line = n < 60 ? n : 60;
        for (int i = 0; i < line; i++) {
            float r = (float)uint32_rand() / IM;
            int j;
            for (j = 0; j < nsymb; j++)
                if (r < cum[j]) break;
            buf[i] = symb[j];
        }
        buf[line] = '\n';
        fwrite(buf, 1, line + 1, stdout);
        n -= line;
    }
}

int main(int argc, char *argv[]) {
    int n = argc > 1 ? atoi(argv[1]) : 1000;

    printf(">ONE Homo sapiens alu\n");
    repeat_fasta(alu, n * 2);

    printf(">TWO IUB ambiguity codes\n");
    {
        const char *iub = "acgtBDHKMNRSVWY";
        const float iub_p[] = {0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02,
                               0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02};
        random_fasta(iub, iub_p, n * 3);
    }

    printf(">THREE Homo sapiens frequency\n");
    {
        const char *hs = "acgt";
        const float hs_p[] = {0.3029549426680, 0.1979883004921,
                              0.1975473066391, 0.3015094502008};
        random_fasta(hs, hs_p, n * 5);
    }

    return 0;
}
