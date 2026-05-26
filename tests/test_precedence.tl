// test_precedence.tl — operator precedence tests
// All binary operators now chain correctly with standard precedence

// ── 1. Multiplicative (* / %) before additive (+ -) ──
print(5 + 3 * 2)          // 5 + 6 = 11
print(10 - 6 / 3)         // 10 - 2 = 8
print(5 * 3 + 2)          // 15 + 2 = 17
print(12 / 4 - 1)         // 3 - 1 = 2
print(5 + 3 * 2 - 8 / 4)  // 5 + 6 - 2 = 9
print(10 % 3 * 4 + 1)     // 1 * 4 + 1 = 5
print(2 * 3 + 4 * 5)      // 6 + 20 = 26

// ── 2. Left-associativity ──
print(10 - 5 - 2)          // (10 - 5) - 2 = 3
print(24 / 4 / 2)          // (24 / 4) / 2 = 3
print(10 - 5 + 2)          // (10 - 5) + 2 = 7
print(24 / 4 * 2)          // (24 / 4) * 2 = 12
print(10 % 6 % 2)          // (10 % 6) % 2 = 0

// ── 3. Parentheses override ──
print((5 + 3) * 2)         // 8 * 2 = 16
print(5 + (3 * 2))         // 5 + 6 = 11
print((10 - 5) - 2)        // 5 - 2 = 3
print(10 - (5 - 2))        // 10 - 3 = 7
print(2 * (3 + 4) * 5)     // 2 * 7 * 5 = 70
print((2 + 3) * (4 - 10 / 2))  // 5 * (4 - 5) = 5 * -1 = -5

// ── 4. Comparison (< > <= >=) after arithmetic ──
print(5 + 3 > 2 * 3)       // 8 > 6 = 1
print(10 > 5 + 3)          // 10 > 8 = 1
print(10 > 5 + 6)          // 10 > 11 = []
print(2 * 5 <= 11 - 1)     // 10 <= 10 = 1
print(2 * 5 < 11 - 1)      // 10 < 10 = []
print(3 + 4 >= 2 * 3)      // 7 >= 6 = 1
print(3 + 3 >= 2 * 3)      // 6 >= 6 = 1
print(10 > 5 > 2)          // (10 > 5) > 2 = 1 > 2 = []

// ── 5. Equality (= !=) after comparison ──
print(5 + 5 = 10)          // (5 + 5) = 10 = 1
print(5 + 6 = 10)          // 11 = 10 = []
print(2 * 3 != 7)          // 6 != 7 = 1
print(2 * 3 != 6)          // 6 != 6 = []
print(10 - 2 = 4 + 4)      // (10 - 2) = (4 + 4) = 8 = 8 = 1
print(10 - 2 = 4 + 5)      // (10 - 2) = (4 + 5) = 8 = 9 = []

// ── 6. Shift (@) after arithmetic ──
print(16 @ 2 + 1)          // 16 @ (2 + 1) = 16 @ 3 = 128
print(8 @ 1 + 1)           // 8 @ (1 + 1) = 8 @ 2 = 32
print(1 + 2 @ 3)           // (1 + 2) @ 3 = 3 @ 3 = 24  (+ tighter than @, like C)
print(1 @ 1 + 2 @ 2)       // (1 @ 1) + (2 @ 2) = 2 + 8 = 10

// ── 7. Bitwise AND (&) after equality, before XOR (^) ──
print(1 | 2 & 4)           // 1 | (2 & 4) = 1 | 0 = 1
print(1 & 2 | 4)           // (1 & 2) | 4 = 0 | 4 = 4
print(2 ^ 1 & 3)           // 2 ^ (1 & 3) = 2 ^ 1 = 3
print(1 & 2 ^ 3)           // (1 & 2) ^ 3 = 0 ^ 3 = 3
print(1 | 2 ^ 3)           // 1 | (2 ^ 3) = 1 | 1 = 1
print(3 ^ 2 | 1)           // (3 ^ 2) | 1 = 1 | 1 = 1
print(12 & 10 | 6 & 5)     // (12 & 10) | (6 & 5) = 8 | 4 = 12
print(6 | 4 & 2)           // 6 | (4 & 2) = 6 | 0 = 6

// ── 8. Mixed all levels ──
print(1 + 2 * 3 > 4 + 5 @ 1)    // 1 + 6 > 4 + 10 = 7 > 14 = []
print(1 + 2 * 3 > 4 + 5 @ -1)   // 1 + 6 > 4 + 2 = 7 > 6 = 1
print(3 * 4 / 2 + 1)            // (3 * 4) / 2 + 1 = 12 / 2 + 1 = 6 + 1 = 7
print(3 * (4 / 2) + 1)          // 3 * 2 + 1 = 6 + 1 = 7
print(3 * (4 / (2 + 1)))        // 3 * (4 / 3) = 3 * 1 = 3

// ── 9. Unary minus / hash / negation + binary ops ──
print(-5 + 3)              // (-5) + 3 = -2
print(-5 * 3)              // (-5) * 3 = -15
print(5 + -3)              // 5 + (-3) = 2
print(5 * -3)              // 5 * (-3) = -15
print(-10 - -3)            // (-10) - (-3) = -7
print(-(5 + 2))            // -(7) = -7
print(-(5 + 2) * 2)        // -7 * 2 = -14

// hash (#) — array length
arr = [10, 20, 30, 40]
print(#arr + 1)            // 4 + 1 = 5
print(#arr * 2)            // 4 * 2 = 8
print(#arr + #arr)         // 4 + 4 = 8
print(2 * #arr - 1)        // 2 * 4 - 1 = 7

// negation (!)
print(!(5 = 5))            // !1 = []
print(!!(5 = 5))           // !!1 = 1
print(!(5 = 6))            // ![] = 1
print(!!(5 = 6))           // !![] = []

// ── 10. Expressions in function call args ──
print(5 + 3 * 2)           // 11
print(10 - 5 - 2)          // 3
print(1 | 2 & 4)           // 1

// ── 11. Expressions in array literals ──
result = [5 + 3 * 2, 10 - 5 - 2, 1 | 2 & 4]
print(result[0])           // 11
print(result[1])           // 3
print(result[2])           // 1

// ── 12. Expressions in conditions ──
if 5 + 3 > 2 * 3 {
    print(1)
} else {
    print(0)
}

if 10 - 5 - 2 = 3 {
    print(1)
} else {
    print(0)
}

// ── 13. Expressions in assignment right-hand side ──
x = 5 + 3 * 2
print(x)                   // 11

y = x + 4 / 2
print(y)                   // 11 + 2 = 13

z = y * (x - 5) - 1
print(z)                   // 13 * 6 - 1 = 77

// ── 14. Logical && and || with short-circuit ──

// Basic && (only [] is falsy, everything else is truthy)
print(1 && 1)              // both truthy → eval second → 1
print(1 && [])             // first truthy → eval second → []
print([] && 1)             // first falsy → short-circuit → []
print([] && [])            // first falsy → short-circuit → []

// Basic ||
print(1 || 1)              // first truthy → short-circuit → 1
print(1 || [])             // first truthy → short-circuit → 1
print([] || 1)             // first falsy → eval second → 1
print([] || [])            // first falsy → eval second → []

// && before || precedence
print(1 || 0 && [])        // 1 || (0 && []) = 1 || [] = 1  (|| short-circuits)
print(0 && 0 || 1)         // (0 && 0) || 1 = 0 || 1 = 0  (|| doesn't short-circuit)
print(1 && 2 || 3 && 4)   // (1 && 2) || (3 && 4) = 2 || 4 = 2
print([] && [] || 1)       // ([] && []) || 1 = [] || 1 = 1

// Combining with arithmetic and comparison
print(1 + 2 > 0 && 3 * 4 > 10)  // (1+2 > 0) && (3*4 > 10) = 1 && 1 = 1
print(10 > 5 && 5 > 8)          // (10>5) && (5>8) = 1 && [] = []
print(10 > 5 || 5 > 8)          // (10>5) || (5>8) = 1 || [] = 1 (short-circuit)
print(1 && 2 + 3 = 4)           // 1 && (2+3 = 4) = 1 && [] = []
print(1 && 2 + 3 = 5)           // 1 && (2+3 = 5) = 1 && 1 = 1

// Chained && (left-assoc)
print(1 && 1 && 1)         // ((1&&1)&&1) = 1&&1 = 1
print(1 && 1 && [])        // ((1&&1)&&[]) = 1&&[] = []
print([] && 1 && 1)        // ([]&&1)&&1 = []&&1 = [] (short-circuit at first [])

// Chained || (left-assoc)
print([] || [] || 1)       // ([]||[])||1 = []||1 = 1
print([] || [] || [])      // ([]||[])||[] = []||[] = []
print(1 || [] || 0)        // (1||[])||0 = 1||0 = 1 (short-circuit at first 1)

// Mixed && and || chains
print(1 || [] && 1 || [])  // 1 || (([] && 1) || []) = 1 || ([] || []) = 1 (short-circ)
print(0 || [] && 1)        // 0 || ([] && 1) = 0 || [] = 0

// Short-circuit safety: right side that would error is never evaluated
function maybe_err() {
    return 1 / 0            // would divide by zero
}
print([] && maybe_err())   // short-circuit ([] is falsy), never calls maybe_err → []
print(1 || maybe_err())    // short-circuit (1 is truthy), never calls maybe_err → 1

// ── 15. Nested parentheses sanity ──
print((((5 + 3) * 2)))     // 16
print(((((10)))))          // 10
print((1 + 2) * (3 + 4) / (5 - 2))  // 3 * 7 / 3 = 7

// ── 16. Hash (#) binds tighter than index ([] follows inner primary) ──
// #arr[i] means #(arr[i]), not (#arr)[i]
mat = [[10, 20], [30, 40], [50, 60]]
print(#mat[1])           // #(mat[1]) = #[30, 40] = 2
print(#mat[0])           // #(mat[0]) = #[10, 20] = 2
print(#mat[2])           // #(mat[2]) = #[50, 60] = 2

// Index expression uses full precedence inside brackets
print(mat[#mat - 1])     // mat[3-1] = mat[2] = [50, 60]
print(mat[#mat - 1][0])  // [50, 60][0] = 50
print(mat[#mat - 1][1])  // [50, 60][1] = 60

// # on array literal with index
print(#[[1, 2], [3, 4]][1])  // #([[1,2],[3,4]][1]) = #[3,4] = 2
print(#[[1, 2], [3, 4]][0])  // #([[1,2],[3,4]][0]) = #[1,2] = 2

// plain # (basic, unindexed)
print(#[10, 20, 30])     // 3
