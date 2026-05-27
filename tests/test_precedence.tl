// Operator precedence tests
// Note: bitwise operators (& | ^) removed from VM.
// Arithmetic, comparison, and logical operators only.

function eq(a, b) {
    if a = b {
        print(1)
    } else {
        print(0)
    }
}

print("--- 1. Simple ---")
print(5 + 3)            // 8
print(10 - 3)           // 7
print(5 * 3)            // 15
print(10 / 3)           // 3.33333...
print(10 % 3)           // 1

print("--- 2. Precedence: */% before +- ---")
print(5 + 3 * 2)        // 5 + 6 = 11
print(10 - 6 / 2)       // 10 - 3 = 7
print(5 * 3 + 2)        // 15 + 2 = 17
print(5 + 3 * 2 - 1)    // 5 + 6 - 1 = 10
print(10 - 2 * 3 + 4 / 2)  // 10 - 6 + 2 = 6
print(10 % 3 * 2)       // 1 * 2 = 2
print(10 * 2 % 3)       // 20 % 3 = 2
print(5 + 10 % 3)       // 5 + 1 = 6

print("--- 3. Parentheses override ---")
print((5 + 3) * 2)      // 8 * 2 = 16
print(10 - (6 / 2))     // 10 - 3 = 7
print((10 - 2) * (3 + 4 / 2)) // 8 * 5 = 40

print("--- 4. Comparisons after arithmetic ---")
print(5 + 3 > 2 * 3)    // 8 > 6 = 1
print(10 - 4 <= 3 * 2)  // 6 <= 6 = 1
print(5 + 1 > 2 * 5)    // 6 > 10 = []
print(3 + 4 > 2 * 3)    // 7 > 6 = 1
print(12 / 3 < 2 + 2)   // 4 < 4 = []
print(12 / 3 <= 2 + 2)  // 4 <= 4 = 1

print("--- 5. Equality after comparison ---")
print(5 + 3 > 6 = 10 - 4 > 5)   // (8 > 6) = (6 > 5) = 1 = 1 = 1
print(1 + 1 = 2)        // 2 = 2 = 1
print(5 + 6 = 10)       // 11 = 10 = []

print("--- 6. && after comparisons ---")
print(1 && 1)           // 1
print(1 && [])          // []
print([] && 1)          // []
print(5 > 3 && 2 < 4)  // 1 && 1 = 1
print(5 > 3 && 2 > 4)  // 1 && [] = []
print(5 < 3 && 2 < 4)  // [] && (no eval) = []

print("--- 7. || after && ---")
print(1 || [])          // 1 (short-circuit)
print([] || 1)          // 1
print([] || [])         // []
print(5 > 3 || 2 > 4)  // 1 || (no eval) = 1
print(5 < 3 || 2 < 4)  // [] || 1 = 1
print(5 < 3 || 2 > 4)  // [] || [] = []

print("--- 8. Mixed precedence levels ---")
print(1 + 2 * 3 > 4 + 5)    // 1 + 6 > 9 = 7 > 9 = []
print(1 + 2 * 3 > 4 + 2)    // 1 + 6 > 6 = 7 > 6 = 1
print(3 + 4 * 2 = 5 + 6)    // 3 + 8 = 11 = 11 = 11 = 1
print((3 + 4) * 2 = 5 + 6)  // 7 * 2 = 14 = 14 = 11 = []
print(10 / 2 + 3 * 2 < 5 + 6 * 2) // 5 + 6 < 5 + 12 = 11 < 17 = 1

print("--- 9. Complex logical chains ---")
print(1 && 1 && 1)      // 1
print(1 || [] || [])    // 1 (short-circuit first ||)
print([] || [] || 1)    // 1
print(1 && [] || 1)     // 1
print(1 || [] && 1)     // 1 (|| short-circuits before && evaluates)
print([] || [] && 1)    // []

print("done")
