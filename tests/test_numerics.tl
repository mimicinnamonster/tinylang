// Number literal tests: hex and decimal (binary/octal removed)

fun println(x=0) { print(x) print("\n") }

// --- 1. Hex literals ---
println("--- hex ---")
println(0x0)
println(0xff)             // 255
println(0xFF)             // case-insensitive
println(0xdeadbeef)       // 3735928559

// --- 2. More hex ---
println("--- more hex ---")
println(0x10)             // 16
println(0x7fffffff)       // 2147483647
println(0x80000000)       // 2147483648
println(0xffffffff)       // 4294967295

// --- 3. Edge cases ---
println("--- edges ---")
println(0x0 + 0x1)       // 1
println(0xa * 0x2)       // 20
println(0xf - 0x5)       // 10
println(0x64 / 0xa)      // 10

// --- 4. Decimal ---
println("--- decimal ---")
println(0)
println(123)
println(999)
println(-5)
println(1.5)
println(.5)

// --- 5. Arithmetic ---
println("--- arithmetic ---")
println(5 + 3)
println(10 - 4)
println(6 * 7)
println(10 / 3)
println(10 % 3)
println(-5 + 3)
println(1.5 + 2.5)

println("done")
