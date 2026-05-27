// include_nested_b.tl — included by a, includes c
include "include_nested_c.tl"

fun compute() {
    ret add(32, 10)
}
