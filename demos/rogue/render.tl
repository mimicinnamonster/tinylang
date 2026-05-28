fun render_frame(level=[], player=[], messages=[], MW=60, MH=20, MSG_MAX=6) {
    map = level_map(level)
    vis = level_vis(level)
    mon = level_monsters(level)
    its = level_items(level)
    px = level_px(level)
    py = level_py(level)
    _W = 0
    _F = 1
    _S = 2

    print("\x1b[H")
    y = 0
    for y < MH {
        x = 0
        for x < MW {
            ch = " "
            if x == px && y == py {
                ch = "@"

            } elif vis[y][x] {
                mi = monster_find_at(x, y, mon)
                ii = item_find_at(x, y, its)
                t = map[y][x]

                if mi >= 0 {
                    ch = bestiary_glyph(monster_type(mon[mi]))
                } elif ii >= 0 {
                    if item_is_potion(its[ii]) { ch = "!" } else { ch = "$" }
                } elif t == _W {
                    ch = "#"
                } elif t == _F {
                    ch = "."
                } elif t == _S {
                    ch = ">"
                }
            }
            print(ch)
            x = x + 1
        }
        print("\n")
        y = y + 1
    }

    print("HP: ")
    print(player_hp(player))
    print("/")
    print(player_maxhp(player))
    print(" ATK:")
    print(player_atk(player))
    print(" DEF:")
    print(player_def(player))
    print(" G:")
    print(player_gold(player))
    print(" LV:")
    print(player_level(player))
    print(" K:")
    print(player_kills(player))
    print("\n")

    i = 0
    for i < MSG_MAX {
        idx = #messages - MSG_MAX + i
        if idx >= 0 && idx < #messages {
            print(messages[idx])
        }
        print("\x1b[K\n")
        i = i + 1
    }
}
