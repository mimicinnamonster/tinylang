#!/usr/bin/env python3
# Spectral-Norm benchmark — Python version
import sys
import math

def eval_A(i, j):
    return 1.0 / ((i + j) * (i + j + 1) / 2 + i + 1)

def eval_A_times_u(n, u):
    return [sum(eval_A(i, j) * u[j] for j in range(n)) for i in range(n)]

def eval_At_times_u(n, u):
    return [sum(eval_A(j, i) * u[j] for j in range(n)) for i in range(n)]

def run(n):
    u = [1.0] * n
    v = [0.0] * n
    for _ in range(10):
        v = eval_A_times_u(n, u)
        u = eval_At_times_u(n, v)

    uv = sum(u[i] * v[i] for i in range(n))
    vv = sum(v[i] * v[i] for i in range(n))
    print(f"{math.sqrt(uv / vv):.9f}")

if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 100
    run(n)
