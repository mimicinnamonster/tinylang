// test_thispath.tl — test thispath() with includes
include "thispath_sub.tl"

// thispath() in the main file should be truthy
main_path = thispath()
assert(#main_path > 0)

// thispath() in the included file should return that file's path
sub_result = get_sub_path()
assert(#sub_result > 0)

// The paths should differ since they're from different files
assert(main_path != sub_result)

// thispath() inside a function body in this file should still be this file
p = thispath()
assert(p = main_path)

print("ok")
