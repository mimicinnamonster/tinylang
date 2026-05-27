#!/usr/bin/env python3
# N-Body benchmark — Python version
import sys
import math

SOLAR_MASS = 4 * math.pi * math.pi
DAYS_PER_YEAR = 365.24
N = 7  # px, py, pz, vx, vy, vz, mass

def init():
    b = [0.0] * (5 * N)
    # Sun
    b[0*N+6] = SOLAR_MASS
    # Jupiter
    b[1*N+0] = 4.84143144246472090e+00
    b[1*N+1] = -1.16032004402742839e+00
    b[1*N+2] = -1.03622044471123109e-01
    b[1*N+3] = 1.66007664274403694e-03 * DAYS_PER_YEAR
    b[1*N+4] = 7.69901118419740425e-03 * DAYS_PER_YEAR
    b[1*N+5] = -6.90460016972063023e-05 * DAYS_PER_YEAR
    b[1*N+6] = 9.54791938424326609e-04 * SOLAR_MASS
    # Saturn
    b[2*N+0] = 8.34336671824457987e+00
    b[2*N+1] = 4.12479856412430479e+00
    b[2*N+2] = -4.03523417114321381e-01
    b[2*N+3] = -2.76742510726862411e-03 * DAYS_PER_YEAR
    b[2*N+4] = 4.99852801234917238e-03 * DAYS_PER_YEAR
    b[2*N+5] = 2.30417297573763929e-05 * DAYS_PER_YEAR
    b[2*N+6] = 2.85885980666130812e-04 * SOLAR_MASS
    # Uranus
    b[3*N+0] = 1.28943695621391310e+01
    b[3*N+1] = -1.51111514016986312e+01
    b[3*N+2] = -2.23307578892655734e-01
    b[3*N+3] = 2.96460137564761618e-03 * DAYS_PER_YEAR
    b[3*N+4] = 2.37847173959480950e-03 * DAYS_PER_YEAR
    b[3*N+5] = -2.96589568540237556e-05 * DAYS_PER_YEAR
    b[3*N+6] = 4.36624404335156298e-05 * SOLAR_MASS
    # Neptune
    b[4*N+0] = 1.53796971148509165e+01
    b[4*N+1] = -2.59193146099879641e+01
    b[4*N+2] = 1.79258772950371181e-01
    b[4*N+3] = 2.68067772490389322e-03 * DAYS_PER_YEAR
    b[4*N+4] = 1.62824170038242295e-03 * DAYS_PER_YEAR
    b[4*N+5] = -9.51592254519715870e-05 * DAYS_PER_YEAR
    b[4*N+6] = 5.15138902046611451e-05 * SOLAR_MASS
    return b

def offset_momentum(b):
    px = py = pz = 0.0
    for i in range(5):
        px += b[i*N+3] * b[i*N+6]
        py += b[i*N+4] * b[i*N+6]
        pz += b[i*N+5] * b[i*N+6]
    b[0*N+3] = -px / SOLAR_MASS
    b[0*N+4] = -py / SOLAR_MASS
    b[0*N+5] = -pz / SOLAR_MASS

def advance(b):
    for i in range(5):
        for j in range(i + 1, 5):
            dx = b[i*N+0] - b[j*N+0]
            dy = b[i*N+1] - b[j*N+1]
            dz = b[i*N+2] - b[j*N+2]
            d2 = dx*dx + dy*dy + dz*dz
            mag = 0.01 / (d2 * math.sqrt(d2))
            mi = b[i*N+6] * mag
            mj = b[j*N+6] * mag
            b[i*N+3] -= dx * mj
            b[i*N+4] -= dy * mj
            b[i*N+5] -= dz * mj
            b[j*N+3] += dx * mi
            b[j*N+4] += dy * mi
            b[j*N+5] += dz * mi
    for i in range(5):
        b[i*N+0] += 0.01 * b[i*N+3]
        b[i*N+1] += 0.01 * b[i*N+4]
        b[i*N+2] += 0.01 * b[i*N+5]

def energy(b):
    e = 0.0
    for i in range(5):
        vx, vy, vz = b[i*N+3], b[i*N+4], b[i*N+5]
        m = b[i*N+6]
        e += 0.5 * m * (vx*vx + vy*vy + vz*vz)
        for j in range(i + 1, 5):
            dx = b[i*N+0] - b[j*N+0]
            dy = b[i*N+1] - b[j*N+1]
            dz = b[i*N+2] - b[j*N+2]
            e -= m * b[j*N+6] / math.sqrt(dx*dx + dy*dy + dz*dz)
    return e

def run(n):
    bodies = init()
    offset_momentum(bodies)
    print(f"{energy(bodies):.9f}")
    for _ in range(n):
        advance(bodies)
    print(f"{energy(bodies):.9f}")

if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 5000000
    run(n)
