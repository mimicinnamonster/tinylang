fun _ai_try_move(mx=0, my=0, nx=0, ny=0, map=[], monsters=[], self_idx=0, px=-1, py=-1, MW=60, MH=20) {
    if nx < 0 || nx >= MW || ny < 0 || ny >= MH { ret [mx, my] }
    if map[ny][nx] == 0 { ret [mx, my] }
    i = 0
    for i < #monsters {
        if i != self_idx {
            m = monsters[i]
            if monster_alive(m) && monster_x(m) == nx && monster_y(m) == ny {
                ret [mx, my]
            }
        }
        i = i + 1
    }
    if nx == px && ny == py { ret [mx, my] }
    ret [nx, ny]
}

fun _ai_move_towards(mx=0, my=0, px=0, py=0, map=[], monsters=[], self_idx=0, MW=60, MH=20) {
    dx = 0
    dy = 0
    if mx < px { dx = 1 } elif mx > px { dx = -1 }
    if my < py { dy = 1 } elif my > py { dy = -1 }
    if dx != 0 {
        p = _ai_try_move(mx, my, mx + dx, my, map, monsters, self_idx, px, py, MW, MH)
        if p[0] != mx || p[1] != my { ret p }
    }
    if dy != 0 {
        p = _ai_try_move(mx, my, mx, my + dy, map, monsters, self_idx, px, py, MW, MH)
        if p[0] != mx || p[1] != my { ret p }
    }
    ret [mx, my]
}

fun _ai_random_walk(mx=0, my=0, map=[], monsters=[], self_idx=0, px=-1, py=-1, MW=60, MH=20) {
    dir = utils_randint(0, 3)
    dx = 0
    dy = 0
    if dir == 0 {
        dy = -1
    } elif dir == 1 {
        dy = 1
    } elif dir == 2 {
        dx = -1
    } else {
        dx = 1
    }
    ret _ai_try_move(mx, my, mx + dx, my + dy, map, monsters, self_idx, px, py, MW, MH)
}

fun ai_process_monsters(level=[], player=[], messages=[], MW=60, MH=20) {
    monsters = level_monsters(level)
    map = level_map(level)
    px = level_px(level)
    py = level_py(level)
    pdef = player_def(player)
    php = player_hp(player)

    i = 0
    for i < #monsters {
        if monster_alive(monsters[i]) {
            mx = monster_x(monsters[i])
            my = monster_y(monsters[i])
            matk = monster_atk(monsters[i])
            mtype = monster_type(monsters[i])

            if utils_dist2(mx, my, px, py) <= 2 {
                dmg = combat_damage(matk, pdef)
                php = php - dmg
                messages = messages + [bestiary_name(mtype) + " hits you for " + dmg + "!"]
                if php <= 0 { php = 0; messages = messages + ["You die!"] }

            } elif utils_dist2(mx, my, px, py) <= 100 {
                p = _ai_move_towards(mx, my, px, py, map, monsters, i, MW, MH)
                if p[0] != mx || p[1] != my {
                    monsters[i] = monster_move(monsters[i], p[0], p[1])
                }

            } elif utils_randint(0, 100) < 30 {
                p = _ai_random_walk(mx, my, map, monsters, i, px, py, MW, MH)
                if p[0] != mx || p[1] != my {
                    monsters[i] = monster_move(monsters[i], p[0], p[1])
                }
            }
        }
        i = i + 1
    }

    level = level_set_monsters(level, monsters)
    player = player_set_hp(player, php)
    ret [level, player, messages]
}
