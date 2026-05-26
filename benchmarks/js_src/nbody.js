// N-Body benchmark — Node.js (V8) version
// Solar system simulation using flat Float64Array for body data
// Each body: 7 values [px, py, pz, vx, vy, vz, mass]

const SOLAR_MASS = 4 * Math.PI * Math.PI;
const DAYS_PER_YEAR = 365.24;
const NBODIES = 5;
const N = 7;

function init() {
    const b = new Float64Array(NBODIES * N);
    // Sun
    b[0*7+6] = SOLAR_MASS;
    // Jupiter
    b[1*7+0] = 4.84143144246472090e+00;
    b[1*7+1] = -1.16032004402742839e+00;
    b[1*7+2] = -1.03622044471123109e-01;
    b[1*7+3] = 1.66007664274403694e-03 * DAYS_PER_YEAR;
    b[1*7+4] = 7.69901118419740425e-03 * DAYS_PER_YEAR;
    b[1*7+5] = -6.90460016972063023e-05 * DAYS_PER_YEAR;
    b[1*7+6] = 9.54791938424326609e-04 * SOLAR_MASS;
    // Saturn
    b[2*7+0] = 8.34336671824457987e+00;
    b[2*7+1] = 4.12479856412430479e+00;
    b[2*7+2] = -4.03523417114321381e-01;
    b[2*7+3] = -2.76742510726862411e-03 * DAYS_PER_YEAR;
    b[2*7+4] = 4.99852801234917238e-03 * DAYS_PER_YEAR;
    b[2*7+5] = 2.30417297573763929e-05 * DAYS_PER_YEAR;
    b[2*7+6] = 2.85885980666130812e-04 * SOLAR_MASS;
    // Uranus
    b[3*7+0] = 1.28943695621391310e+01;
    b[3*7+1] = -1.51111514016986312e+01;
    b[3*7+2] = -2.23307578892655734e-01;
    b[3*7+3] = 2.96460137564761618e-03 * DAYS_PER_YEAR;
    b[3*7+4] = 2.37847173959480950e-03 * DAYS_PER_YEAR;
    b[3*7+5] = -2.96589568540237556e-05 * DAYS_PER_YEAR;
    b[3*7+6] = 4.36624404335156298e-05 * SOLAR_MASS;
    // Neptune
    b[4*7+0] = 1.53796971148509165e+01;
    b[4*7+1] = -2.59193146099879641e+01;
    b[4*7+2] = 1.79258772950371181e-01;
    b[4*7+3] = 2.68067772490389322e-03 * DAYS_PER_YEAR;
    b[4*7+4] = 1.62824170038242295e-03 * DAYS_PER_YEAR;
    b[4*7+5] = -9.51592254519715870e-05 * DAYS_PER_YEAR;
    b[4*7+6] = 5.15138902046611451e-05 * SOLAR_MASS;
    return b;
}

function offset_momentum(b) {
    let px = 0, py = 0, pz = 0;
    for (let i = 0; i < NBODIES; i++) {
        px += b[i*7+3] * b[i*7+6];
        py += b[i*7+4] * b[i*7+6];
        pz += b[i*7+5] * b[i*7+6];
    }
    b[0*7+3] = -px / SOLAR_MASS;
    b[0*7+4] = -py / SOLAR_MASS;
    b[0*7+5] = -pz / SOLAR_MASS;
}

function advance(b) {
    for (let i = 0; i < NBODIES; i++) {
        for (let j = i + 1; j < NBODIES; j++) {
            const dx = b[i*7+0] - b[j*7+0];
            const dy = b[i*7+1] - b[j*7+1];
            const dz = b[i*7+2] - b[j*7+2];
            const d2 = dx*dx + dy*dy + dz*dz;
            const mag = 0.01 / (d2 * Math.sqrt(d2));
            const mi = b[i*7+6] * mag;
            const mj = b[j*7+6] * mag;
            b[i*7+3] -= dx * mj;
            b[i*7+4] -= dy * mj;
            b[i*7+5] -= dz * mj;
            b[j*7+3] += dx * mi;
            b[j*7+4] += dy * mi;
            b[j*7+5] += dz * mi;
        }
    }
    for (let i = 0; i < NBODIES; i++) {
        b[i*7+0] += 0.01 * b[i*7+3];
        b[i*7+1] += 0.01 * b[i*7+4];
        b[i*7+2] += 0.01 * b[i*7+5];
    }
}

function energy(b) {
    let e = 0.0;
    for (let i = 0; i < NBODIES; i++) {
        const vx = b[i*7+3], vy = b[i*7+4], vz = b[i*7+5];
        const m = b[i*7+6];
        e += 0.5 * m * (vx*vx + vy*vy + vz*vz);
        for (let j = i + 1; j < NBODIES; j++) {
            const dx = b[i*7+0] - b[j*7+0];
            const dy = b[i*7+1] - b[j*7+1];
            const dz = b[i*7+2] - b[j*7+2];
            e -= m * b[j*7+6] / Math.sqrt(dx*dx + dy*dy + dz*dz);
        }
    }
    return e;
}

const steps = parseInt(process.argv[2] || '5000000', 10);
const bodies = init();
offset_momentum(bodies);
console.log(energy(bodies).toFixed(9));

for (let i = 0; i < steps; i++) advance(bodies);

console.log(energy(bodies).toFixed(9));
