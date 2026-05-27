#!/usr/bin/env python3
# Mandelbrot benchmark — Python version
import sys

def run(w):
    h = w
    total = 0
    for y in range(h):
        for x in range(w):
            cr = 2.0 * x / w - 1.5
            ci = 2.0 * y / h - 1.0
            zr = zi = 0.0
            for i in range(50):
                zr2 = zr * zr
                zi2 = zi * zi
                if zr2 + zi2 > 4.0:
                    break
                zi = 2.0 * zr * zi + ci
                zr = zr2 - zi2 + cr
            else:
                total += 1
    print(total)

if __name__ == "__main__":
    w = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    run(w)
