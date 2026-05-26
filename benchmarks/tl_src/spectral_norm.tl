// Spectral-Norm benchmark — tinylang version
// Based on the same algorithm as the C version.

// Newton-Raphson square root
function sqrt(x) {
    if x <= 0 { return 0 }
    guess = x / 2
    i = 0
    while i < 100 {
        if guess = 0 { return x }
        new_guess = (guess + x / guess) / 2
        if new_guess = guess { return guess }
        guess = new_guess
        i = i + 1
    }
    return guess
}

function eval_A(i_val, j_val) {
    i = i_val
    j = j_val
    return 1 / ((i + j) * (i + j + 1) / 2 + i + 1)
}

function eval_A_times_u(n_in, u) {
    n = n_in
    Au = []
    i = 0
    while i < n {
        sum = 0.0
        j = 0
        while j < n {
            sum = sum + eval_A(i, j) * u[j]
            j = j + 1
        }
        Au = Au + [sum]
        i = i + 1
    }
    return Au
}

function eval_At_times_u(n_in, u) {
    n = n_in
    Au = []
    i = 0
    while i < n {
        sum = 0.0
        j = 0
        while j < n {
            sum = sum + eval_A(j, i) * u[j]
            j = j + 1
        }
        Au = Au + [sum]
        i = i + 1
    }
    return Au
}

function run(n_in) {
    n = n_in
    u = []
    i = 0
    while i < n {
        u = u + [1.0]
        i = i + 1
    }

    v = []
    iter = 0
    while iter < 10 {
        v = eval_A_times_u(n, u)
        u = eval_At_times_u(n, v)
        iter = iter + 1
    }

    uv = 0.0
    vv = 0.0
    i = 0
    while i < n {
        uv = uv + u[i] * v[i]
        vv = vv + v[i] * v[i]
        i = i + 1
    }

    if vv > 0 {
        print(sqrt(uv / vv))
    } else {
        print(0.0)
    }
}

run(100)
