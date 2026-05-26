// Test assert built-in function

// --- 1. Truthy expressions return nil ---
print("--- truthy ---")
r = assert(1)
print(#r)          // 0 (nil)

r = assert(42)
print(#r)          // 0

r = assert([1, 2])
print(#r)          // 0

r = assert("x")
print(#r)          // 0

// --- 2. Falsy expressions return "assertion failed" ---
print("--- falsy ---")
e = assert([])
print(#e)          // 16 ("assertion failed")

e = assert(nil)
print(#e)          // 16

// --- 3. Caught runtime errors return the actual error message ---
print("--- errors ---")
e = assert(5 / 0)
print(e)           // "division by zero"

e = assert(1 / 0)
print(e)           // "division by zero"

e = assert(5 + [])
print(e)           // "'+' type mismatch"

e = assert(5 - [])
print(e)           // "'-' requires numbers"

e = assert(5 * [])
print(e)           // "'*' type mismatch"

e = assert(5 / [])
print(e)           // "'/' requires numbers"

// --- 4. No arguments ---
print("--- no args ---")
e = assert()
print(#e)          // 16 ("assertion failed")

// --- 5. Nested assert calls ---
print("--- nested ---")
// inner assert returns "assertion failed" (truthy), outer returns nil
r = assert(assert([]))
print(#r)          // 0

// inner assert of truthy returns nil (falsy), outer catches it
e = assert(assert(1))
print(#e)          // 16

// --- 6. In conditions ---
print("--- in conditions ---")
if assert(1) = [] {
    print("ok")
}

if assert([]) != [] {
    print("caught falsy")
}

// --- 7. Error inside a called function ---
print("--- function error ---")
function div(a, b) {
    return a / b
}
e = assert(div(10, 0))
print(e)           // "division by zero"

function crash() {
    return 1 / 0
}
e = assert(crash())
print(e)           // "division by zero"

// --- 8. Multiple args (only first is checked) ---
print("--- multiple args ---")
r = assert(1, 2, 3)
print(#r)          // 0

e = assert([], "ignored")
print(#e)          // 16 ("assertion failed")
