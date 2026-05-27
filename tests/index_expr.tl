// Test: all expressions that evaluate to an array can be indexed with []

fun println(x=0) { print(x) print("\n") }

// --- 1. Array literal indexing ---
println("--- array literal indexing ---")
println([1, 2, 3][0])       // 1
println([1, 2, 3][1])       // 2
println([10, 20, 30][2])    // 30

// --- 2. Nested array literal indexing ---
println("--- nested literal indexing ---")
println([[1, 2], [3, 4]][0][1])  // 2
println([[1, 2], [3, 4]][1][0])  // 3

// --- 3. Multi-index on array literal ---
println("--- multi-index on literal ---")
println([[1, 2], [3, 4]][0, 1])   // 2
println([[1, 2], [3, 4]][1, 0])   // 3

// --- 4. Function call returning array, then index ---
println("--- fun call indexing ---")
fun get_arr() { ret [10, 20, 30] }
println(get_arr()[0])       // 10
println(get_arr()[1])       // 20
println(get_arr()[2])       // 30

// --- 5. Function call with multi-index ---
println("--- fn call multi-index ---")
fun get_mat() { ret [[1, 2], [3, 4]] }
println(get_mat()[0, 1])    // 2
println(get_mat()[1, 0])    // 3

// --- 6. Parenthesized expression indexing ---
println("--- parenthesized indexing ---")
arr = [5, 10, 15]
println((arr)[0])           // 5
println((arr)[1])           // 10
println((arr)[2])           // 15

// --- 7. String indexing (strings are arrays) ---
println("--- string indexing on literal ---")
println("hello"[0])         // 104
println("abc"[2])           // 99

// --- 8. Array concat result indexing ---
println("--- concat result indexing ---")
a = [1, 2]
b = [3, 4]
println((a + b)[0])         // 1
println((a + b)[2])         // 3
println((a + b)[3])         // 4

// --- 9. Array repetition result indexing ---
println("--- repetition result indexing ---")
println(([0] * 10)[0])      // 0
println(([0] * 10)[5])      // 0
println(([0] * 10)[9])      // 0
println(([1, 2] * 3)[0])   // 1
println(([1, 2] * 3)[1])   // 2
println(([1, 2] * 3)[4])   // 2
println(([1, 2] * 3)[5])   // 1

// --- 10. Nested indexing on binary op result ---
println("--- nested on binop ---")
println(([1, 2] + [3, 4])[0])     // 1
println(([1, 2] + [3, 4])[2])     // 3
println(([[1],[2]]+[[3],[4]])[1][0]) // 2
println(([[1],[2]]+[[3],[4]])[2][0]) // 3

// --- 11. Slice on non-identifier expression ---
println("--- slice on expression ---")
println([1, 2, 3, 4, 5][1:3])    // [2, 3]
println([1, 2, 3, 4, 5][0:2])    // [1, 2]
println([1, 2, 3, 4, 5][3:])     // [4, 5]
println([1, 2, 3, 4, 5][:2])     // [1, 2]
println(([0] * 10)[2:5])         // [0, 0, 0]
println(([0] * 10)[:3])          // [0, 0, 0]

// --- 13. Slice on fun call result ---
println("--- slice on fn call ---")
fun get_vals() { ret [10, 20, 30, 40, 50] }
println(get_vals()[1:4])         // [20, 30, 40]

// --- 14. Chained: literal index then slice ---
println("--- chained ---")
println([1, 2, 3, 4][1:3][0])    // 2
println([1, 2, 3, 4][0:2][1])    // 2

// --- 15. Slice on parenthesized expression ---
println("--- parenthesized slice ---")
arr2 = [1, 2, 3, 4, 5]
println((arr2)[1:3])             // [2, 3]

// --- 16. nil expression indexing (should error - tested separately) ---
// println(nil[0])  -- runtime error: cannot index into non-array

// --- 17. Nested fun call with indexing ---
println("--- nested fn indexing ---")
fun wrap(x=0) { ret [x] }
fun get_val() { ret 42 }
println(wrap(get_val())[0])       // 42

// --- 18. Slice with step on expression ---
println("--- slice with step ---")
println([1, 2, 3, 4, 5][::2])    // [1, 3, 5]
println([1, 2, 3, 4, 5][1:4:2])  // [2, 4]

// --- 19. Empty array literal with index (OOB) ---
// error test: [][0] would halt, tested in e_oob_literal.tl

// --- 20. Variable slice bound with expression indexing ---
println("--- var slice bound ---")
x = [10, 20, 30, 40]
i = 1
j = 3
println(x[i:j])                  // [20, 30]

// --- 21. Double indexing on fun call ---
println("--- double indexing on fn ---")
fun make_grid() { ret [[1, 2], [3, 4], [5, 6]] }
println(make_grid()[2][0])       // 5
println(make_grid()[1, 1])       // 4
