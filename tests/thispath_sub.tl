// thispath_sub.tl — included file for thispath test
function get_sub_path() {
    return thispath()
}

function check_sub_path() {
    p = thispath()
    assert(#p > 0)
}
