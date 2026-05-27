// Test: return type consistency check
// Function has two return statements with different types

function foo(x) {
    if x {
        return 42
    }
    return "hello"
}
