// N-body benchmark — tinylang version
// Solar system simulation with value semantics
// Arrays must end on same line as last element (parser limitation)

// Newton-Raphson square root
function sqrt(x) {
    if x <= 0 { return 0 }
    guess = x / 2
    i = 0
    while i < 100 {
        if guess == 0 { return x }
        new_guess = (guess + x / guess) / 2
        if new_guess == guess { return guess }
        guess = new_guess
        i = i + 1
    }
    return guess
}

function init_bodies() {
    // Pre-computed values (no scientific notation - tinylang doesn't support it)
    jup_vx = 0.6064335992583953
    jup_vy = 2.8117979576226787
    jup_vz = -0.025215989670027062
    jup_mass = 0.03769324733927477
    sat_vx = -1.0108885027287756
    sat_vy = 1.8254686789260235
    sat_vz = 0.008415581651704794
    sat_mass = 0.011286326131968767
    ura_vx = 1.0827384675401304
    ura_vy = 0.8684887231806749
    ura_vz = -0.010830486652174911
    ura_mass = 0.0017237240570597112
    nep_vx = 0.9790216096574758
    nep_vy = 0.5947621556010826
    nep_vz = -0.03475650920626858
    nep_mass = 0.0020336868699246304
    sm = 39.47841760435743

    // Arrays on single lines to avoid parser issue with newlines before ]
    return [[0, 0, 0, 0, 0, 0, sm],
        [4.84143144246472090, -1.16032004402742839, -0.103622044471123109, jup_vx, jup_vy, jup_vz, jup_mass],
        [8.34336671824457987, 4.12479856412430479, -0.403523417114321381, sat_vx, sat_vy, sat_vz, sat_mass],
        [12.8943695621391310, -15.1111514016986312, -0.223307578892655734, ura_vx, ura_vy, ura_vz, ura_mass],
        [15.3796971148509165, -25.9193146099879641, 0.179258772950371181, nep_vx, nep_vy, nep_vz, nep_mass]]
}

function offset_momentum(bodies) {
    px = 0
    py = 0
    pz = 0
    i = 0
    while i < 5 {
        px = px + bodies[i][3] * bodies[i][6]
        py = py + bodies[i][4] * bodies[i][6]
        pz = pz + bodies[i][5] * bodies[i][6]
        i = i + 1
    }
    sun = bodies[0]
    sun[3] = -(px / 39.47841760435743)
    sun[4] = -(py / 39.47841760435743)
    sun[5] = -(pz / 39.47841760435743)
    bodies[0] = sun
    return bodies
}

function advance(bodies) {
    vels = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
    i = 0
    while i < 5 {
        vels[i * 3] = bodies[i][3]
        vels[i * 3 + 1] = bodies[i][4]
        vels[i * 3 + 2] = bodies[i][5]
        i = i + 1
    }

    i = 0
    while i < 5 {
        j = i + 1
        while j < 5 {
            dx = bodies[i][0] - bodies[j][0]
            dy = bodies[i][1] - bodies[j][1]
            dz = bodies[i][2] - bodies[j][2]
            d2 = dx * dx + dy * dy + dz * dz
            if d2 > 0 {
                mag = 0.01 / (d2 * sqrt(d2))
                mi = bodies[i][6] * mag
                mj = bodies[j][6] * mag
                vels[i * 3] = vels[i * 3] - (dx * mj)
                vels[i * 3 + 1] = vels[i * 3 + 1] - (dy * mj)
                vels[i * 3 + 2] = vels[i * 3 + 2] - (dz * mj)
                vels[j * 3] = vels[j * 3] + (dx * mi)
                vels[j * 3 + 1] = vels[j * 3 + 1] + (dy * mi)
                vels[j * 3 + 2] = vels[j * 3 + 2] + (dz * mi)
            }
            j = j + 1
        }
        i = i + 1
    }

    result = []
    i = 0
    while i < 5 {
        body = [0,0,0,0,0,0,bodies[i][6]]
        body[0] = bodies[i][0] + 0.01 * vels[i * 3]
        body[1] = bodies[i][1] + 0.01 * vels[i * 3 + 1]
        body[2] = bodies[i][2] + 0.01 * vels[i * 3 + 2]
        body[3] = vels[i * 3]
        body[4] = vels[i * 3 + 1]
        body[5] = vels[i * 3 + 2]
        result = result + [body]
        i = i + 1
    }
    return result
}

function energy(bodies) {
    e = 0.0
    i = 0
    while i < 5 {
        vx = bodies[i][3]
        vy = bodies[i][4]
        vz = bodies[i][5]
        m = bodies[i][6]
        e = e + 0.5 * m * (vx * vx + vy * vy + vz * vz)
        j = i + 1
        while j < 5 {
            dx = bodies[i][0] - bodies[j][0]
            dy = bodies[i][1] - bodies[j][1]
            dz = bodies[i][2] - bodies[j][2]
            d2 = dx * dx + dy * dy + dz * dz
            if d2 > 0 {
                e = e - m * bodies[j][6] / sqrt(d2)
            }
            j = j + 1
        }
        i = i + 1
    }
    return e
}

function run(n) {
    bodies = init_bodies()
    bodies = offset_momentum(bodies)
    print(energy(bodies))

    iter = 0
    while iter < n {
        bodies = advance(bodies)
        iter = iter + 1
    }

    print(energy(bodies))
}

run(5000000)
