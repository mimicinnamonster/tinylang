// Test: all expressions that evaluate to an array can be indexed with []

// --- 1. Array literal indexing ---
print("--- array literal indexing ---")
print([1, 2, 3][0])       // 1
print([1, 2, 3][1])       // 2
print([10, 20, 30][2])    // 30

// --- 2. Nested array literal indexing ---
print("--- nested literal indexing ---")
print([[1, 2], [3, 4]][0][1])  // 2
print([[1, 2], [3, 4]][1][0])  // 3

// --- 3. Multi-index on array literal ---
print("--- multi-index on literal ---")
print([[1, 2], [3, 4]][0, 1])   // 2
print([[1, 2], [3, 4]][1, 0])   // 3

// --- 4. Function call returning array, then index ---
print("--- function call indexing ---")
function get_arr() { return [10, 20, 30] }
print(get_arr()[0])       // 10
print(get_arr()[1])       // 20
print(get_arr()[2])       // 30

// --- 5. Function call with multi-index ---
print("--- fn call multi-index ---")
function get_mat() { return [[1, 2], [3, 4]] }
print(get_mat()[0, 1])    // 2
print(get_mat()[1, 0])    // 3

// --- 6. Parenthesized expression indexing ---
print("--- parenthesized indexing ---")
arr = [5, 10, 15]
print((arr)[0])           // 5
print((arr)[1])           // 10
print((arr)[2])           // 15

// --- 7. String indexing (strings are arrays) ---
print("--- string indexing on literal ---")
print("hello"[0])         // 104
print("abc"[2])           // 99

// --- 8. Array concat result indexing ---
print("--- concat result indexing ---")
a = [1, 2]
b = [3, 4]
print((a + b)[0])         // 1
print((a + b)[2])         // 3
print((a + b)[3])         // 4

// --- 9. Array repetition result indexing ---
print("--- repetition result indexing ---")
print(([0] * 10)[0])      // 0
print(([0] * 10)[5])      // 0
print(([0] * 10)[9])      // 0
print(([1, 2] * 3)[0])   // 1
print(([1, 2] * 3)[1])   // 2
print(([1, 2] * 3)[4])   // 2
print(([1, 2] * 3)[5])   // 1

// --- 10. Nested indexing on binary op result ---
print("--- nested on binop ---")
print(([1, 2] + [3, 4])[0])     // 1
print(([1, 2] + [3, 4])[2])     // 3
print(([[1],[2]]+[[3],[4]])[1][0]) // 2
print(([[1],[2]]+[[3],[4]])[2][0]) // 3

// --- 11. Slice on non-identifier expression ---
print("--- slice on expression ---")
print([1, 2, 3, 4, 5][1:3])    // [2, 3]
print([1, 2, 3, 4, 5][0:2])    // [1, 2]
print([1, 2, 3, 4, 5][3:])     // [4, 5]
print([1, 2, 3, 4, 5][:2])     // [1, 2]
print(([0] * 10)[2:5])         // [0, 0, 0]
print(([0] * 10)[:3])          // [0, 0, 0]

// --- 13. Slice on function call result ---
print("--- slice on fn call ---")
function get_vals() { return [10, 20, 30, 40, 50] }
print(get_vals()[1:4])         // [20, 30, 40]

// --- 14. Chained: literal index then slice ---
print("--- chained ---")
print([1, 2, 3, 4][1:3][0])    // 2
print([1, 2, 3, 4][0:2][1])    // 2

// --- 15. Slice on parenthesized expression ---
print("--- parenthesized slice ---")
arr2 = [1, 2, 3, 4, 5]
print((arr2)[1:3])             // [2, 3]

// --- 16. nil expression indexing (should error - tested separately) ---
// print(nil[0])  -- runtime error: cannot index into non-array

// --- 17. Nested function call with indexing ---
print("--- nested fn indexing ---")
function wrap(x) { return [x] }
function get_val() { return 42 }
print(wrap(get_val())[0])       // 42

// --- 18. Slice with step on expression ---
print("--- slice with step ---")
print([1, 2, 3, 4, 5][::2])    // [1, 3, 5]
print([1, 2, 3, 4, 5][1:4:2])  // [2, 4]

// --- 19. Empty array literal with index (OOB) ---
// error test: [][0] would halt, tested in e_oob_literal.tl

// --- 20. Variable slice bound with expression indexing ---
print("--- var slice bound ---")
x = [10, 20, 30, 40]
i = 1
j = 3
print(x[i:j])                  // [20, 30]

// --- 21. Double indexing on function call ---
print("--- double indexing on fn ---")
function make_grid() { return [[1, 2], [3, 4], [5, 6]] }
print(make_grid()[2][0])       // 5
print(make_grid()[1, 1])       // 4
