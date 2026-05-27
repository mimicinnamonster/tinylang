// Fibonacci benchmark — Node.js (V8) version
// Computes fib(N) using iterative approach

function fib(n) {
    if (n < 2) return n;
    let a = 0, b = 1, c;
    for (let i = 2; i <= n; i++) {
        c = a + b;
        a = b;
        b = c;
    }
    return b;
}

const n = parseInt(process.argv[2] || '35', 10);
console.log(fib(n));
