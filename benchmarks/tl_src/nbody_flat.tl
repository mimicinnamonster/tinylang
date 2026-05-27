// N-body benchmark — flat array version
// Uses a single flat array: [px, py, pz, vx, vy, vz, mass] × 5 bodies
// Mutates in-place — no velocity extraction, no body rebuilding

fun init_bodies() {
    sm = 39.47841760435743

    ret [0, 0, 0, 0, 0, 0, sm,
        4.84143144246472090, -1.16032004402742839, -0.103622044471123109,
        0.6064335992583953, 2.8117979576226787, -0.025215989670027062,
        0.03769324733927477,
        8.34336671824457987, 4.12479856412430479, -0.403523417114321381,
        -1.0108885027287756, 1.8254686789260235, 0.008415581651704794,
        0.011286326131968767,
        12.8943695621391310, -15.1111514016986312, -0.223307578892655734,
        1.0827384675401304, 0.8684887231806749, -0.010830486652174911,
        0.0017237240570597112,
        15.3796971148509165, -25.9193146099879641, 0.179258772950371181,
        0.9790216096574758, 0.5947621556010826, -0.03475650920626858,
        0.0020336868699246304]
}

fun offset_momentum(bodies=[]) {
    px = 0
    py = 0
    pz = 0
    i = 0
    for i < 5 {
        px = px + bodies[i * 7 + 3] * bodies[i * 7 + 6]
        py = py + bodies[i * 7 + 4] * bodies[i * 7 + 6]
        pz = pz + bodies[i * 7 + 5] * bodies[i * 7 + 6]
        i = i + 1
    }
    bodies[3] = -(px / 39.47841760435743)
    bodies[4] = -(py / 39.47841760435743)
    bodies[5] = -(pz / 39.47841760435743)
    ret bodies
}

fun advance(bodies=[]) {
    // Gravity interactions: 10 body pairs
    i = 0
    for i < 5 {
        j = i + 1
        for j < 5 {
            bi = i * 7
            bj = j * 7

            dx = bodies[bi] - bodies[bj]
            dy = bodies[bi + 1] - bodies[bj + 1]
            dz = bodies[bi + 2] - bodies[bj + 2]

            d2 = dx * dx + dy * dy + dz * dz
            if d2 > 0 {
                mag = 0.01 / (d2 * sqrt(d2))

                mi = bodies[bi + 6] * mag
                mj = bodies[bj + 6] * mag

                bodies[bi + 3] -= dx * mj
                bodies[bi + 4] -= dy * mj
                bodies[bi + 5] -= dz * mj

                bodies[bj + 3] += dx * mi
                bodies[bj + 4] += dy * mi
                bodies[bj + 5] += dz * mi
            }
            j = j + 1
        }
        i = i + 1
    }

    // Update positions
    i = 0
    for i < 5 {
        bi = i * 7
        bodies[bi]     += 0.01 * bodies[bi + 3]
        bodies[bi + 1] += 0.01 * bodies[bi + 4]
        bodies[bi + 2] += 0.01 * bodies[bi + 5]
        i = i + 1
    }

    ret bodies
}

fun energy(bodies=[]) {
    e = 0.0
    i = 0
    for i < 5 {
        bi = i * 7
        e = e + 0.5 * bodies[bi + 6] * (bodies[bi + 3] * bodies[bi + 3] +
                                         bodies[bi + 4] * bodies[bi + 4] +
                                         bodies[bi + 5] * bodies[bi + 5])
        j = i + 1
        for j < 5 {
            bj = j * 7
            dx = bodies[bi] - bodies[bj]
            dy = bodies[bi + 1] - bodies[bj + 1]
            dz = bodies[bi + 2] - bodies[bj + 2]
            d2 = dx * dx + dy * dy + dz * dz
            if d2 > 0 {
                e = e - bodies[bi + 6] * bodies[bj + 6] / sqrt(d2)
            }
            j = j + 1
        }
        i = i + 1
    }
    ret e
}

fun run(n=0) {
    bodies = init_bodies()
    bodies = offset_momentum(bodies)
    print(energy(bodies))

    iter = 0
    for iter < n {
        bodies = advance(bodies)
        iter = iter + 1
    }

    print(energy(bodies))
}

run(500000)
