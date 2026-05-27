#!/usr/bin/env python3
# Fasta benchmark — Python version
import sys

IM = 139968
IA = 3877
IC = 29573

seed = 42
def uint32_rand():
    global seed
    seed = (seed * IA + IC) % IM
    return seed

def repeat_fasta(seq, n):
    length = len(seq)
    pos = 0
    out_lines = []
    while n > 0:
        line = min(n, 60)
        buf = []
        for _ in range(line):
            buf.append(seq[pos])
            pos += 1
            if pos >= length:
                pos = 0
        out_lines.append(''.join(buf))
        n -= line
    return out_lines

def random_fasta(symb, probs, n):
    # Cumulative probabilities
    cum = []
    s = 0.0
    for p in probs:
        s += p
        cum.append(s)
    ns = len(symb)
    out_lines = []
    while n > 0:
        line = min(n, 60)
        buf = []
        for _ in range(line):
            r = uint32_rand()
            for j in range(ns):
                if r < cum[j] * IM:
                    buf.append(symb[j])
                    break
        out_lines.append(''.join(buf))
        n -= line
    return out_lines

def run(n):
    lines = []
    
    alu = ("GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGG"
           "GAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGA"
           "CCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAAT"
           "ACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCA"
           "GCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGG"
           "AGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCC"
           "AGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA")
    
    lines.append(">ONE Homo sapiens alu")
    lines.extend(repeat_fasta(alu, n * 2))
    
    iub = "acgtBDHKMNRSVWY"
    iub_p = [0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02,
             0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02]
    
    lines.append(">TWO IUB ambiguity codes")
    lines.extend(random_fasta(iub, iub_p, n * 3))
    
    hs = "acgt"
    hs_p = [0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008]
    
    lines.append(">THREE Homo sapiens frequency")
    lines.extend(random_fasta(hs, hs_p, n * 5))
    
    sys.stdout.write("\n".join(lines) + "\n")

if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 25000
    run(n)
