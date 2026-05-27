// Number literal tests: hex and decimal (binary/octal removed)

// --- 1. Hex literals ---
print("--- hex ---")
print(0x0)
print(0xff)             // 255
print(0xFF)             // case-insensitive
print(0xdeadbeef)       // 3735928559

// --- 2. More hex ---
print("--- more hex ---")
print(0x10)             // 16
print(0x7fffffff)       // 2147483647
print(0x80000000)       // 2147483648
print(0xffffffff)       // 4294967295

// --- 3. Edge cases ---
print("--- edges ---")
print(0x0 + 0x1)       // 1
print(0xa * 0x2)       // 20
print(0xf - 0x5)       // 10
print(0x64 / 0xa)      // 10

// --- 4. Decimal ---
print("--- decimal ---")
print(0)
print(123)
print(999)
print(-5)
print(1.5)
print(.5)

// --- 5. Arithmetic ---
print("--- arithmetic ---")
print(5 + 3)
print(10 - 4)
print(6 * 7)
print(10 / 3)
print(10 % 3)
print(-5 + 3)
print(1.5 + 2.5)

print("done")
