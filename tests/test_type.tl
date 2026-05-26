// Test type() built-in — exposes internal NumKind / ArrKind
// Numbers → nkind (0-9), Arrays → 100+ArrKind (100-110), nil → -1

print("=== scalar: all bit widths ===")
// Unsigned: U8=0, U16=1, U32=2, U64=3
// Signed:   I8=4, I16=5, I32=6, I64=7
// Float: F32=8, F64=9

print(type(0))              // 0  U8 (1 byte)
print(type(255))            // 0  U8
print(type(256))            // 1  U16 (2 bytes)
print(type(65535))          // 1  U16
print(type(65536))          // 2  U32 (4 bytes)
print(type(4294967295))     // 2  U32
print(type(4294967296))     // 3  U64 (8 bytes)
print(type(5000000000))     // 3  U64

print(type(-128))           // 4  I8 (1 byte)
print(type(127))            // 0  U8 (positive → unsigned preferred)
print(type(-129))           // 5  I16 (2 bytes)
print(type(-32768))         // 5  I16
print(type(32767))          // 1  U16 (positive)
print(type(-32769))         // 6  I32 (4 bytes)
print(type(-2147483648))    // 6  I32
print(type(2147483647))     // 2  U32 (positive)
print(type(-2147483649))    // 7  I64 (8 bytes)
print(type(-3000000000))    // 7  I64
print(type(3000000000))     // 3  U64 (positive)

print(type(1.5))            // 8  F32 (4 bytes)
print(type(0.125))          // 8  F32
print(type(3.14159265358979)) // 9  F64 (8 bytes)

print("=== arrays: all bit widths ===")
// type(arr) = 100 + ArrKind
// U8=0, U16=1, U32=2, U64=3
// I8=4, I16=5, I32=6, I64=7
// F32=8, F64=9, VAL=10

print(type([0, 255]))           // 100  U8  (1 byte/elem)
print(type([256, 65535]))       // 101  U16 (2 bytes/elem)
print(type([65536, 4294967295])) // 102  U32 (4 bytes/elem)
print(type([4294967296]))       // 103  U64 (8 bytes/elem)

print(type([-128, 127]))        // 104  I8  (1 byte/elem)
print(type([-32768, 32767]))    // 105  I16 (2 bytes/elem)
print(type([-2147483648, 2147483647])) // 106  I32 (4 bytes/elem)
print(type([-3000000000, 0]))   // 107  I64 (8 bytes/elem)

print(type([1.5, 2.5]))         // 108  F32 (4 bytes/elem)
print(type([3.14159265358979])) // 109  F64 (8 bytes/elem)
print(type([1, "hello"]))       // 110  VAL (16+ bytes/elem)

print("=== nil / empty ===")
print(type([]))             // -1
print(type(nil))            // -1

print("=== strings ===")
print(type("hello"))        // 104  I8 (byte array)

print("=== arithmetic: result types ===")
print(type(200 + 100))      // 1   U16 (300 > 255)
print(type(3 - 5))          // 4   I8  (-2)
print(type(-100 - 100))     // 5   I16 (-200, < -128)
print(type(200 + (-50)))    // 0   U8  (150, positive, fits U8)
print(type(100 + 0.5))      // 8   F32

print("=== array promotion ===")
// Same kind concat: kind preserved
print(type([1, 2] + [3, 4]))             // 100 U8+U8 → U8
print(type([256] + [257]))               // 101 U16+U16 → U16

// Cross-kind concat: promotes to fit all elements
print(type([100, 200] + [-50, -100]))    // 105 I16 (U8 can't hold -50)
print(type([50000] + [-100]))            // 106 I32 (U16 can't hold both)
print(type([100, 200] + [1.5, 2.5]))     // 108 F32 (U8+F32 → F32)

// String concat
print(type("hello" + " world"))          // 104 I8+I8 → I8

print("=== mutation promotes to VAL ===")
x = [1, 2, 3]
print(type(x))              // 100 ARR_U8
x[0] = 99
print(type(x))              // 110 ARR_VAL (promoted)

y = [256, 257]
print(type(y))              // 101 ARR_U16
y[0] = -1
print(type(y))              // 110 ARR_VAL

print("done")
