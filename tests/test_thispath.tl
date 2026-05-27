// test_thispath.tl — test thispath() with includes

function println(x=0) { print(x) print("\n") }

include "thispath_sub.tl"

// thispath() in the main file — just call and verify length is > 0 by printing it
main_path = thispath()
println(#main_path)

// thispath() in the included file should return that file's path
sub_result = get_sub_path()
println(#sub_result)

// thispath() inside a function body in this file should still be this file
p = thispath()
println(#p)

// Verify paths exist and differ
println(main_path != sub_result)

println("ok")
