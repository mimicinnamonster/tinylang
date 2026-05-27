// Test: ret type consistency check
// Function has two ret statements with different types

fun foo(x=0) {
    if x {
        ret 42
    }
    ret "hello"
}
