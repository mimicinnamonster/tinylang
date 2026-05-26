/* N-body benchmark — simple version, no SIMD
 * Based on: https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/nbody-gcc-4.html
 * Flat array version matching tinylang approach.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define SOLAR_MASS (4 * M_PI * M_PI)
#define DAYS_PER_YEAR 365.24
#define NBODIES 5
#define N 7  // px,py,pz, vx,vy,vz, mass

static double bodies[5 * 7];

static void init(void) {
    /* Sun */
    bodies[0*7+0] = 0; bodies[0*7+1] = 0; bodies[0*7+2] = 0;
    bodies[0*7+3] = 0; bodies[0*7+4] = 0; bodies[0*7+5] = 0;
    bodies[0*7+6] = SOLAR_MASS;

    /* Jupiter */
    bodies[1*7+0] = 4.84143144246472090e+00;
    bodies[1*7+1] = -1.16032004402742839e+00;
    bodies[1*7+2] = -1.03622044471123109e-01;
    bodies[1*7+3] = 1.66007664274403694e-03 * DAYS_PER_YEAR;
    bodies[1*7+4] = 7.69901118419740425e-03 * DAYS_PER_YEAR;
    bodies[1*7+5] = -6.90460016972063023e-05 * DAYS_PER_YEAR;
    bodies[1*7+6] = 9.54791938424326609e-04 * SOLAR_MASS;

    /* Saturn */
    bodies[2*7+0] = 8.34336671824457987e+00;
    bodies[2*7+1] = 4.12479856412430479e+00;
    bodies[2*7+2] = -4.03523417114321381e-01;
    bodies[2*7+3] = -2.76742510726862411e-03 * DAYS_PER_YEAR;
    bodies[2*7+4] = 4.99852801234917238e-03 * DAYS_PER_YEAR;
    bodies[2*7+5] = 2.30417297573763929e-05 * DAYS_PER_YEAR;
    bodies[2*7+6] = 2.85885980666130812e-04 * SOLAR_MASS;

    /* Uranus */
    bodies[3*7+0] = 1.28943695621391310e+01;
    bodies[3*7+1] = -1.51111514016986312e+01;
    bodies[3*7+2] = -2.23307578892655734e-01;
    bodies[3*7+3] = 2.96460137564761618e-03 * DAYS_PER_YEAR;
    bodies[3*7+4] = 2.37847173959480950e-03 * DAYS_PER_YEAR;
    bodies[3*7+5] = -2.96589568540237556e-05 * DAYS_PER_YEAR;
    bodies[3*7+6] = 4.36624404335156298e-05 * SOLAR_MASS;

    /* Neptune */
    bodies[4*7+0] = 1.53796971148509165e+01;
    bodies[4*7+1] = -2.59193146099879641e+01;
    bodies[4*7+2] = 1.79258772950371181e-01;
    bodies[4*7+3] = 2.68067772490389322e-03 * DAYS_PER_YEAR;
    bodies[4*7+4] = 1.62824170038242295e-03 * DAYS_PER_YEAR;
    bodies[4*7+5] = -9.51592254519715870e-05 * DAYS_PER_YEAR;
    bodies[4*7+6] = 5.15138902046611451e-05 * SOLAR_MASS;
}

static void offset_momentum(void) {
    double px = 0, py = 0, pz = 0;
    for (int i = 0; i < NBODIES; i++) {
        px += bodies[i*7+3] * bodies[i*7+6];
        py += bodies[i*7+4] * bodies[i*7+6];
        pz += bodies[i*7+5] * bodies[i*7+6];
    }
    bodies[0*7+3] = -px / SOLAR_MASS;
    bodies[0*7+4] = -py / SOLAR_MASS;
    bodies[0*7+5] = -pz / SOLAR_MASS;
}

static void advance(void) {
    for (int i = 0; i < NBODIES; i++) {
        for (int j = i + 1; j < NBODIES; j++) {
            double dx = bodies[i*7+0] - bodies[j*7+0];
            double dy = bodies[i*7+1] - bodies[j*7+1];
            double dz = bodies[i*7+2] - bodies[j*7+2];
            double d2 = dx*dx + dy*dy + dz*dz;
            double mag = 0.01 / (d2 * sqrt(d2));
            double mi = bodies[i*7+6] * mag;
            double mj = bodies[j*7+6] * mag;
            bodies[i*7+3] -= dx * mj;
            bodies[i*7+4] -= dy * mj;
            bodies[i*7+5] -= dz * mj;
            bodies[j*7+3] += dx * mi;
            bodies[j*7+4] += dy * mi;
            bodies[j*7+5] += dz * mi;
        }
    }
    for (int i = 0; i < NBODIES; i++) {
        bodies[i*7+0] += 0.01 * bodies[i*7+3];
        bodies[i*7+1] += 0.01 * bodies[i*7+4];
        bodies[i*7+2] += 0.01 * bodies[i*7+5];
    }
}

static double energy(void) {
    double e = 0.0;
    for (int i = 0; i < NBODIES; i++) {
        double vx = bodies[i*7+3], vy = bodies[i*7+4], vz = bodies[i*7+5];
        double m = bodies[i*7+6];
        e += 0.5 * m * (vx*vx + vy*vy + vz*vz);
        for (int j = i + 1; j < NBODIES; j++) {
            double dx = bodies[i*7+0] - bodies[j*7+0];
            double dy = bodies[i*7+1] - bodies[j*7+1];
            double dz = bodies[i*7+2] - bodies[j*7+2];
            e -= m * bodies[j*7+6] / sqrt(dx*dx + dy*dy + dz*dz);
        }
    }
    return e;
}

int main(int argc, char *argv[]) {
    int n = argc > 1 ? atoi(argv[1]) : 50000000;
    init();
    offset_momentum();
    printf("%.9f\n", energy());

    for (int i = 0; i < n; i++)
        advance();

    printf("%.9f\n", energy());
    return 0;
}
