fun println(x=0) { print(x) print("\n") }

// --- write(filepath, string, mode) ---
println("--- write ---")

// Write a test file
write("/tmp/tinylang_test.txt", "hello world", "w")

// --- read(filepath, mode) ---
println("--- read ---")

// Read it back
content = read("/tmp/tinylang_test.txt", "r")
println(content)               // hello world
println(#content)              // 11
println(content == "hello world")  // 1

// --- Append mode ---
println("--- append ---")

// Append
write("/tmp/tinylang_test.txt", " second line", "a")
content = read("/tmp/tinylang_test.txt", "r")
println(content == "hello world second line")  // 1
println(#content)              // 23

// --- Overwrite mode ---
println("--- overwrite ---")

write("/tmp/tinylang_test.txt", "overwritten", "w")
content = read("/tmp/tinylang_test.txt", "r")
println(content == "overwritten")  // 1
println(#content)              // 11

// --- Empty file ---
println("--- empty ---")

write("/tmp/tinylang_test_empty.txt", "", "w")
content = read("/tmp/tinylang_test_empty.txt", "r")
println(#content == 0)         // 1
println(content == "")         // 1

// --- Binary content ---
println("--- binary ---")

write("/tmp/tinylang_test_bin.txt", "\x00\x01\xff\xfe", "w")
content = read("/tmp/tinylang_test_bin.txt", "r")
println(#content == 4)         // 1
println(content[0] == 0)       // 1
println(content[1] == 1)       // 1
println(content[2] == 255)     // 1
println(content[3] == 254)     // 1

println("ok")
