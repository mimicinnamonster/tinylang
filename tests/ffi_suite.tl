// test_ffi.tl — test generic FFI mechanism
// Sig format: first char = return type, rest = arg types
// No spaces. E.g. "ii" = return int, args: [int]
//          "ddd" = return double, args: [double, double]

// Load the test library (try dylib first, then so)
lib = dlopen("tests/ffi_lib.dylib")
if lib = [] {
    lib = dlopen("tests/ffi_lib.so")
}
assert(lib != [])

// --- void → double ---
pi_fn = dlsym(lib, "tl_pi")
assert(pi_fn != [])
result = ffi_call(pi_fn, "d")
assert(result = 3.14159)

// --- int → int ---
abs_fn = dlsym(lib, "tl_abs")
assert(abs_fn != [])
result = ffi_call(abs_fn, "ii", -5)
assert(result = 5)

// --- double → double ---
cube_fn = dlsym(lib, "tl_cube")
assert(cube_fn != [])
result = ffi_call(cube_fn, "dd", 3.0)
assert(result = 27.0)

// --- int, int → int ---
add_fn = dlsym(lib, "tl_add")
assert(add_fn != [])
result = ffi_call(add_fn, "iii", 10, 20)
assert(result = 30)

// --- double, double, double → double ---
mul3_fn = dlsym(lib, "tl_mul3")
assert(mul3_fn != [])
result = ffi_call(mul3_fn, "dddd", 2.0, 3.0, 4.0)
assert(result = 24.0)

// --- string arg ---
strlen_fn = dlsym(lib, "tl_strlen")
assert(strlen_fn != [])
result = ffi_call(strlen_fn, "is", "hello")
assert(result = 5)

// --- string return ---
greet_fn = dlsym(lib, "tl_greet")
assert(greet_fn != [])
result = ffi_call(greet_fn, "s")
assert(result = "hello from c")

// --- void return ---
nop_fn = dlsym(lib, "tl_nop")
assert(nop_fn != [])
result = ffi_call(nop_fn, "v")
assert(result = [])

// --- bad dlopen returns [] ---
result = dlopen("nonexistent_lib_XYZ.so.99")
assert(result = [])     // dlopen of nonexistent file returns NULL

// --- bad dlsym returns [] ---
result = dlsym(lib, "nonexistent_function_xyz")
assert(result = [])     // dlsym of nonexistent symbol returns NULL

print("ffi all ok")
