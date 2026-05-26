// Fasta benchmark — Node.js (V8) version
// Random DNA sequence generation

const IM = 139968;
const IA = 3877;
const IC = 29573;

let seed = 42;
function uint32_rand() {
    return (seed = (seed * IA + IC) % IM);
}

const alu = "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGG" +
    "GAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGA" +
    "CCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAAT" +
    "ACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCA" +
    "GCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGG" +
    "AGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCC" +
    "AGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";

function repeat_fasta(seq, n) {
    const len = seq.length;
    let pos = 0;
    while (n > 0) {
        const line = Math.min(n, 60);
        let buf = '';
        for (let i = 0; i < line; i++) {
            buf += seq[pos++];
            if (pos >= len) pos = 0;
        }
        console.log(buf);
        n -= line;
    }
}

function random_fasta(symb, probs, n) {
    const cum = [];
    let sum = 0;
    for (let i = 0; i < symb.length; i++) {
        sum += probs[i];
        cum[i] = sum;
    }
    const imF = IM;

    while (n > 0) {
        const line = Math.min(n, 60);
        let buf = '';
        for (let i = 0; i < line; i++) {
            const r = uint32_rand();
            let j = 0;
            while (j < symb.length - 1 && r >= cum[j] * imF) j++;
            buf += symb[j];
        }
        console.log(buf);
        n -= line;
    }
}

const n = parseInt(process.argv[2] || '25000', 10);

console.log('>ONE Homo sapiens alu');
repeat_fasta(alu, n * 2);

console.log('>TWO IUB ambiguity codes');
const iub = "acgtBDHKMNRSVWY";
const iub_p = [0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02];
random_fasta(iub, iub_p, n * 3);

console.log('>THREE Homo sapiens frequency');
const hs = "acgt";
const hs_p = [0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008];
random_fasta(hs, hs_p, n * 5);
