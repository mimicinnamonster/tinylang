// Non-decimal number literal tests: binary, hex, octal

// --- 1. Binary literals ---
print("--- binary ---")
print(0b0)
print(0b1)
print(0b101)            // 5
print(0B101)            // case-insensitive prefix
print(0B11111111)       // 255 (max u8)

// --- 2. Hex literals ---
print("--- hex ---")
print(0x0)
print(0xf)
print(0xff)             // 255
print(0xFF)             // case-insensitive digits
print(0X10)             // case-insensitive prefix
print(0xdeadbeef)       // 3735928559
print(0xABCDEF)         // 11259375

// --- 3. Octal literals ---
print("--- octal ---")
print(0)                // decimal 0 (bare)
print(00)               // octal 0
print(07)               // octal 7
print(0123)             // octal 123 = 83
print(0777)             // octal 777 = 511

// --- 4. Comparison with decimal ---
print("--- comparison ---")
print(0b101 = 5)        // 1
print(0b1010 = 10)      // 1
print(0xff = 255)       // 1
print(0xFF = 255)       // 1
print(0x10 = 16)        // 1
print(0123 = 83)        // 1
print(0777 = 511)       // 1
print(00 = 0)           // 1
print(07 = 7)           // 1

// --- 5. Arithmetic ---
print("--- arithmetic ---")
print(0b101 + 0b10)     // 5 + 2 = 7
print(0xff - 0xf)       // 255 - 15 = 240
print(0xff * 0x10)      // 255 * 16 = 4080
print(0123 + 01)        // 83 + 1 = 84
print(0x10 / 0b10)      // 16 / 2 = 8

// --- 6. Array literals ---
print("--- arrays ---")
arr = [0b101, 0xff, 0123]
print(arr)
print(arr[0])           // 5
print(arr[1])           // 255
print(arr[2])           // 83

// --- 7. Type kinds ---
print("--- type kinds ---")
print(type(0b101))      // 0 (U8: 5 fits in uint8)
print(type(0xff))       // 0 (U8: 255 fits in uint8)
print(type(0123))       // 0 (U8: 83 fits in uint8)
print(type(0x100))      // 1 (U16: 256 > 255)
print(type(0xdeadbeef)) // 2 (U32)
print(type(0b1111111111111111)) // 1 (U16: 65535 < 65536)

// --- 8. Mixed with decimal in expressions ---
print("--- mixed ---")
print(0b101 + 10)       // 5 + 10 = 15
print(0xff + 0x1)       // 255 + 1 = 256
print(0123 - 23)        // 83 - 23 = 60
