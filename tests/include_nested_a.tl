// include_nested_a.tl — main file for nested include test
// nesting works even with include "..." in comments

function println(x=0) { print(x) print("\n") }

include "include_nested_b.tl"

function main() {
    println(compute())
}
main()
