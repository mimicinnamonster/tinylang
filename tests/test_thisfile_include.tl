// test_thisfile_include.tl — test thisfile() with includes
// thisfile() should always return the literal path of the file
// where it is called, even inside includes.

fun println(x=0) { print(x) print("\n") }

// Test 1: thisfile() at top level of the main file
main_f = thisfile()
println(#main_f > 0)     // non-empty path

// Test 2: thisfile() contains "test_thisfile_include.tl"
parts = split(main_f, "/")
last_part = parts[#parts - 1]
println(last_part == "test_thisfile_include.tl")

// Test 3: thisfile() exists before include, capture value
tv = main_f

// Now include the sub file
include "thisfile_included.tl"

// Test 4: After include, the included file should have reported its own path.
// The included file calls get_included_thisfile() which returns thisfile() from the included context.
inc_f = get_included_thisfile()
println(#inc_f > 0)      // non-empty

// Test 5: The included file's thisfile should contain "thisfile_included.tl"
inc_parts = split(inc_f, "/")
inc_last = inc_parts[#inc_parts - 1]
println(inc_last == "thisfile_included.tl")

// Test 6: The two paths should be different (different files)
println(main_f != inc_f)

// Test 7: After include, thisfile() in main file still points to main file
after_f = thisfile()
println(after_f == main_f)

// Test 8: thisfile() inside a function body in this file
fun get_thisfile() {
    ret thisfile()
}
fun_f = get_thisfile()
println(fun_f == main_f)

println("ok")
