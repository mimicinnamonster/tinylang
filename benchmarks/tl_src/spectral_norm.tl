// Spectral-Norm benchmark — tinylang version
// Based on the same algorithm as the C version.

// Newton-Raphson square root
fun sqrt(x=0) {
    if x <= 0 { ret 0 }
    guess = x / 2
    i = 0
    for i < 100 {
        if guess == 0 { ret x }
        new_guess = (guess + x / guess) / 2
        if new_guess == guess { ret guess }
        guess = new_guess
        i = i + 1
    }
    ret guess
}

fun eval_A(i_val=0, j_val=0) {
    i = i_val
    j = j_val
    ret 1 / ((i + j) * (i + j + 1) / 2 + i + 1)
}

fun eval_A_times_u(n_in=0, u=[]) {
    n = n_in
    Au = []
    i = 0
    for i < n {
        sum = 0.0
        j = 0
        for j < n {
            sum = sum + eval_A(i, j) * u[j]
            j = j + 1
        }
        Au = Au + [sum]
        i = i + 1
    }
    ret Au
}

fun eval_At_times_u(n_in=0, u=[]) {
    n = n_in
    Au = []
    i = 0
    for i < n {
        sum = 0.0
        j = 0
        for j < n {
            sum = sum + eval_A(j, i) * u[j]
            j = j + 1
        }
        Au = Au + [sum]
        i = i + 1
    }
    ret Au
}

fun run(n_in=0) {
    n = n_in
    u = []
    i = 0
    for i < n {
        u = u + [1.0]
        i = i + 1
    }

    v = []
    iter = 0
    for iter < 10 {
        v = eval_A_times_u(n, u)
        u = eval_At_times_u(n, v)
        iter = iter + 1
    }

    uv = 0.0
    vv = 0.0
    i = 0
    for i < n {
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

run(5500)
