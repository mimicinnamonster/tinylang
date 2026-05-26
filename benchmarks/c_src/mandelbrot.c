/* Mandelbrot benchmark — simple version, no SIMD
 * Based on: https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/mandelbrot-gcc-6.html
 * Naive version for fair comparison with tinylang.
 */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int w = argc > 1 ? atoi(argv[1]) : 200;
    int h = w;
    
    printf("P4\n%d %d\n", w, h);
    
    for (int y = 0; y < h; y++) {
        unsigned char row[8192];
        int bit = 0;
        unsigned char byte = 0;
        for (int x = 0; x < w; x++) {
            double cr = 2.0 * x / w - 1.5;
            double ci = 2.0 * y / h - 1.0;
            double zr = 0.0, zi = 0.0;
            int i;
            for (i = 0; i < 50; i++) {
                double zr2 = zr * zr, zi2 = zi * zi;
                if (zr2 + zi2 > 4.0) break;
                zi = 2.0 * zr * zi + ci;
                zr = zr2 - zi2 + cr;
            }
            byte = (byte << 1) | (i == 50 ? 1 : 0);
            bit++;
            if (bit == 8) {
                row[bit / 8 - 1] = byte;
                byte = 0;
                bit = 0;
            }
        }
        if (bit) {
            byte <<= (8 - bit);
            row[w / 8] = byte;
        }
        fwrite(row, 1, (w + 7) / 8, stdout);
    }
    return 0;
}
