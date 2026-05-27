#!/usr/bin/env python3
# Fibonacci benchmark — Python version
# Computes fib(N) using iterative approach
import sys

def fib(n):
    if n < 2:
        return n
    a, b = 0, 1
    for _ in range(2, n + 1):
        a, b = b, a + b
    return b

if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 35
    print(fib(n))
