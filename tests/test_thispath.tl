// test_thispath.tl — test thispath() with includes
include "thispath_sub.tl"

// thispath() in the main file — just call and verify length is > 0 by printing it
main_path = thispath()
print(#main_path)

// thispath() in the included file should return that file's path
sub_result = get_sub_path()
print(#sub_result)

// thispath() inside a function body in this file should still be this file
p = thispath()
print(#p)

// Verify paths exist and differ
print(main_path != sub_result)

print("ok")
