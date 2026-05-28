fun _level_MAP()       { ret 0 }
fun _level_VIS()       { ret 1 }
fun _level_MONSTERS()  { ret 2 }
fun _level_ITEMS()     { ret 3 }
fun _level_PX()        { ret 4 }
fun _level_PY()        { ret 5 }

fun level_new(map=[], vis=[], monsters=[], items=[], px=0, py=0) {
    l = [[]] * 6
    l[_level_MAP()] = map
    l[_level_VIS()] = vis
    l[_level_MONSTERS()] = monsters
    l[_level_ITEMS()] = items
    l[_level_PX()] = px
    l[_level_PY()] = py
    ret l
}

fun level_map(l=[])         { ret l[_level_MAP()] }
fun level_vis(l=[])         { ret l[_level_VIS()] }
fun level_monsters(l=[])    { ret l[_level_MONSTERS()] }
fun level_items(l=[])       { ret l[_level_ITEMS()] }
fun level_px(l=[])          { ret floor(l[_level_PX()]) }
fun level_py(l=[])          { ret floor(l[_level_PY()]) }

fun level_set_vis(l=[], v=[])      { l[_level_VIS()] = v; ret l }
fun level_set_monsters(l=[], v=[]) { l[_level_MONSTERS()] = v; ret l }
fun level_set_items(l=[], v=[])    { l[_level_ITEMS()] = v; ret l }
fun level_set_pos(l=[], x=0, y=0)  { l[_level_PX()] = x; l[_level_PY()] = y; ret l }
