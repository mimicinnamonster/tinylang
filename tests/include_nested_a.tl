// include_nested_a.tl — main file for nested include test
// nesting works even with include "..." in comments
include "include_nested_b.tl"

function main() {
    print(compute())
}
main()
