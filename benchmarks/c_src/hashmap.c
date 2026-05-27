/* hashmap.c — C hashmap benchmark (FNV-1a over array, matching TinyLang)
 * Compile: cc -O2 -lm -o hashmap_c hashmap.c && ./hashmap_c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define FNV32_BASE  0x811c9dc5u
#define FNV32_PRIME 0x01000193u

static unsigned int fnv1a(const unsigned char *s, int len) {
    unsigned int hash = FNV32_BASE;
    for (int i = 0; i < len; i++)
        hash = (hash * FNV32_PRIME) ^ s[i];
    return hash;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* ============================================================
 * Benchmark 1: Same-key read (1M)
 * ============================================================ */
static void bench_samekey_read(void) {
    int size = 1000;
    long *map = calloc(size, sizeof(long));
    map[fnv1a((const unsigned char*)"target", 6) % size] = 42;

    double t0 = now_sec();
    long sum = 0;
    for (int i = 0; i < 1000000; i++) {
        int idx = fnv1a((const unsigned char*)"target", 6) % size;
        sum += map[idx];
    }
    double t1 = now_sec();
    printf("  C same-key read (1M):   %.3fs  (hash recomputed each iter, no caching)\n", t1 - t0);
    (void)sum;
    free(map);
}

/* ============================================================
 * Benchmark 2: Different-key write (50k unique keys)
 * ============================================================ */
static void bench_diffkey_write(void) {
    int size = 100000;
    long *map = calloc(size, sizeof(long));

    double t0 = now_sec();
    for (int i = 0; i < 50000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        int len = strlen(key);
        int idx = fnv1a((const unsigned char*)key, len) % size;
        map[idx] = i;
    }
    double t1 = now_sec();
    printf("  C diff-key write (50k):  %.3fs\n", t1 - t0);
    free(map);
}

/* ============================================================
 * Benchmark 3: Multi-key read (1000 keys x 100 reps)
 * ============================================================ */
static void bench_multikey_read(void) {
    int size = 100000;
    long *map = calloc(size, sizeof(long));
    int nkeys = 1000;
    /* Pre-compute indices for fair comparison */
    int *indices = malloc(nkeys * sizeof(int));
    for (int i = 0; i < nkeys; i++) {
        char key[32];
        snprintf(key, sizeof(key), "k%d", i);
        indices[i] = fnv1a((const unsigned char*)key, strlen(key)) % size;
        map[indices[i]] = i;
    }

    double t0 = now_sec();
    long sum = 0;
    for (int r = 0; r < 100; r++) {
        for (int j = 0; j < nkeys; j++) {
            sum += map[indices[j]];
        }
    }
    double t1 = now_sec();
    printf("  C multi-key read (100k): %.3fs  (pre-computed indices)\n", t1 - t0);
    (void)sum;
    free(indices);
    free(map);
}

/* ============================================================
 * Benchmark 4: Collision-safe append (50k appends)
 * Using linked list per bucket to match TinyLang's array-of-arrays
 * ============================================================ */
typedef struct Node { long val; struct Node *next; } Node;

static void bench_append(void) {
    int size = 100;
    Node **buckets = calloc(size, sizeof(Node*));

    double t0 = now_sec();
    unsigned int h = fnv1a((const unsigned char*)"batch", 5) % size;
    for (int i = 0; i < 50000; i++) {
        Node *n = malloc(sizeof(Node));
        n->val = i;
        n->next = buckets[h];
        buckets[h] = n;
    }
    double t1 = now_sec();
    printf("  C append (50k):          %.3fs  (linked list per bucket)\n", t1 - t0);
    /* cleanup */
    for (int i = 0; i < size; i++) {
        Node *n = buckets[i];
        while (n) { Node *next = n->next; free(n); n = next; }
    }
    free(buckets);
}

int main(void) {
    printf("=== C Hashmap Benchmarks ===\n\n");
    bench_samekey_read();
    bench_diffkey_write();
    bench_multikey_read();
    bench_append();
    printf("\n");
    return 0;
}
