// Fasta benchmark — tinylang version
// Generates random DNA/RNA sequences
// All constants inlined since functions can't see globals

function make_rng() {
    return [42.0]
}

function next_rand(rng) {
    seed = rng[0]
    seed = (seed * 3877 + 29573) % 139968
    rng[0] = seed
    return seed
}

function repeat_fasta(seq, n) {
    len = #seq
    pos = 0
    while n > 0 {
        line = n
        if line > 60 { line = 60 }
        buf = []
        i = 0
        while i < line {
            buf = buf + [seq[pos]]
            pos = pos + 1
            if pos >= len { pos = 0 }
            i = i + 1
        }
        buf = buf + [10]
        print(buf)
        n = n - line
    }
}

function random_fasta(rng, symb, probs, n) {
    // Build cumulative probabilities
    cum = []
    sum = 0.0
    ns = #symb
    i = 0
    while i < ns {
        sum = sum + probs[i]
        cum = cum + [sum]
        i = i + 1
    }

    while n > 0 {
        line = n
        if line > 60 { line = 60 }
        buf = []
        i = 0
        while i < line {
            r = next_rand(rng)
            j = 0
            found = 0
            while j < ns {
                if r < cum[j] * 139968.0 {
                    buf = buf + [symb[j]]
                    found = 1
                    j = ns
                }
                j = j + 1
            }
            if found = 0 {
                buf = buf + [symb[ns - 1]]
            }
            i = i + 1
        }
        buf = buf + [10]
        print(buf)
        n = n - line
    }
}

function run(n) {
    rng = make_rng()

    alu = "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA"

    print(">ONE Homo sapiens alu")
    repeat_fasta(alu, n * 2)

    iub = "acgtBDHKMNRSVWY"
    iub_p = [0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02]

    print(">TWO IUB ambiguity codes")
    random_fasta(rng, iub, iub_p, n * 3)

    hs = "acgt"
    hs_p = [0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008]

    print(">THREE Homo sapiens frequency")
    random_fasta(rng, hs, hs_p, n * 5)
}

run(25000)
