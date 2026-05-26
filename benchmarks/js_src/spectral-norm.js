// Spectral-Norm benchmark — Node.js (V8) version
// Simple algorithm matching C/tinylang: no SIMD

function eval_A(i, j) {
    return 1.0 / ((i + j) * (i + j + 1) / 2 + i + 1);
}

function eval_A_times_u(n, u, Au) {
    for (let i = 0; i < n; i++) {
        let sum = 0.0;
        for (let j = 0; j < n; j++) {
            sum += eval_A(i, j) * u[j];
        }
        Au[i] = sum;
    }
}

function eval_At_times_u(n, u, Au) {
    for (let i = 0; i < n; i++) {
        let sum = 0.0;
        for (let j = 0; j < n; j++) {
            sum += eval_A(j, i) * u[j];
        }
        Au[i] = sum;
    }
}

const n = parseInt(process.argv[2] || '100', 10);
const u = new Float64Array(n);
const v = new Float64Array(n);

for (let i = 0; i < n; i++) u[i] = 1.0;

for (let iter = 0; iter < 10; iter++) {
    eval_A_times_u(n, u, v);
    eval_At_times_u(n, v, u);
}

let uv = 0.0, vv = 0.0;
for (let i = 0; i < n; i++) {
    uv += u[i] * v[i];
    vv += v[i] * v[i];
}
console.log((Math.sqrt(uv / vv)).toFixed(9));
