// Operator precedence tests
// Note: bitwise operators (& | ^) removed from VM.
// Arithmetic, comparison, and logical operators only.

fun println(x=0) { print(x) print("\n") }

println("\n--- 1. Simple ---")
println(5 + 3)            // 8
println(10 - 3)           // 7
println(5 * 3)            // 15
println(10 / 3)           // 3.33333...
println(10 % 3)           // 1

println("\n--- 2. Precedence: */% before +- ---")
println(5 + 3 * 2)        // 5 + 6 = 11
println(10 - 6 / 2)       // 10 - 3 = 7
println(5 * 3 + 2)        // 15 + 2 = 17
println(5 + 3 * 2 - 1)    // 5 + 6 - 1 = 10
println(10 - 2 * 3 + 4 / 2)  // 10 - 6 + 2 = 6
println(10 % 3 * 2)       // 1 * 2 = 2
println(10 * 2 % 3)       // 20 % 3 = 2
println(5 + 10 % 3)       // 5 + 1 = 6

println("\n--- 3. Parentheses override ---")
println((5 + 3) * 2)      // 8 * 2 = 16
println(10 - (6 / 2))     // 10 - 3 = 7
println((10 - 2) * (3 + 4 / 2)) // 8 * 5 = 40

println("\n--- 4. Comparisons after arithmetic ---")
println(5 + 3 > 2 * 3)    // 8 > 6 = 1
println(10 - 4 <= 3 * 2)  // 6 <= 6 = 1
println(5 + 1 > 2 * 5)    // 6 > 10 = []
println(3 + 4 > 2 * 3)    // 7 > 6 = 1
println(12 / 3 < 2 + 2)   // 4 < 4 = []
println(12 / 3 <= 2 + 2)  // 4 <= 4 = 1

println("\n--- 5. Equality after comparison ---")
println(5 + 3 > 6 == 10 - 4 > 5)   // (8 > 6) == (6 > 5) = 1 == 1 = 1
println(1 + 1 == 2)        // 2 == 2 = 1
println(5 + 6 == 10)       // 11 == 10 = []

println("\n--- 6. && after comparisons ---")
println(1 && 1)           // 1
println(1 && [])          // []
println([] && 1)          // []
println(5 > 3 && 2 < 4)  // 1 && 1 = 1
println(5 > 3 && 2 > 4)  // 1 && [] = []
println(5 < 3 && 2 < 4)  // [] && (no eval) = []

println("\n--- 7. || after && ---")
println(1 || [])          // 1 (short-circuit)
println([] || 1)          // 1
println([] || [])         // []
println(5 > 3 || 2 > 4)  // 1 || (no eval) = 1
println(5 < 3 || 2 < 4)  // [] || 1 = 1
println(5 < 3 || 2 > 4)  // [] || [] = []

println("\n--- 8. Mixed precedence levels ---")
println(1 + 2 * 3 > 4 + 5)    // 1 + 6 > 9 = 7 > 9 = []
println(1 + 2 * 3 > 4 + 2)    // 1 + 6 > 6 = 7 > 6 = 1
println(3 + 4 * 2 == 5 + 6)    // 3 + 8 = 11 == 11 = 1
println((3 + 4) * 2 == 5 + 6)  // 7 * 2 = 14 == 14 = []
println(10 / 2 + 3 * 2 < 5 + 6 * 2) // 5 + 6 < 5 + 12 = 11 < 17 = 1

println("\n--- 9. Complex logical chains ---")
println(1 && 1 && 1)      // 1
println(1 || [] || [])    // 1 (short-circuit first ||)
println([] || [] || 1)    // 1
println(1 && [] || 1)     // 1
println(1 || [] && 1)     // 1 (|| short-circuits before && evaluates)
println([] || [] && 1)    // []

println("done")
