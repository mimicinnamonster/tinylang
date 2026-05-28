fun _mapgen_rooms(MW=60, MH=20) {
    rooms = []
    n = 0
    max_rooms = utils_randint(6, 10)
    t = 0
    for t < max_rooms * 15 && n < max_rooms {
        rw = utils_randint(4, 10)
        rh = utils_randint(3, 7)
        rx = utils_randint(1, MW - rw - 1)
        ry = utils_randint(1, MH - rh - 1)

        overlap = 0
        i = 0
        for i < n {
            r = rooms[i]
            if rx < room_x(r) + room_w(r) + 1 &&
               rx + rw + 1 > room_x(r) &&
               ry < room_y(r) + room_h(r) + 1 &&
               ry + rh + 1 > room_y(r) {
                overlap = 1
            }
            i = i + 1
        }
        if !overlap {
            rooms = rooms + [room_new(rx, ry, rw, rh)]
            n = n + 1
        }
        t = t + 1
    }
    if n == 0 {
        rooms = [room_new(1, 1, 8, 6)]
    }
    ret rooms
}

fun _mapgen_fill(rooms=[], MW=60, MH=20) {
    map = []
    y = 0
    for y < MH {
        map = map + [[0] * MW]
        y = y + 1
    }
    i = 0
    for i < #rooms {
        r = rooms[i]
        yy = room_y(r)
        for yy < room_y(r) + room_h(r) {
            xx = room_x(r)
            for xx < room_x(r) + room_w(r) {
                map[yy][xx] = 1
                xx = xx + 1
            }
            yy = yy + 1
        }
        i = i + 1
    }
    ret map
}

fun _mapgen_carve(rooms=[], map=[], MW=60, MH=20) {
    i = 1
    for i < #rooms {
        r1 = rooms[i - 1]
        r2 = rooms[i]

        x1 = room_x(r1) + floor(room_w(r1) / 2)
        y1 = room_y(r1) + floor(room_h(r1) / 2)
        x2 = room_x(r2) + floor(room_w(r2) / 2)
        y2 = room_y(r2) + floor(room_h(r2) / 2)

        cx = x1
        if x1 <= x2 {
            for cx <= x2 {
                if y1 >= 0 && y1 < MH && cx >= 0 && cx < MW {
                    map[y1][cx] = 1
                }
                cx = cx + 1
            }
        } else {
            for cx >= x2 {
                if y1 >= 0 && y1 < MH && cx >= 0 && cx < MW {
                    map[y1][cx] = 1
                }
                cx = cx - 1
            }
        }

        cy = y1
        if y1 <= y2 {
            for cy <= y2 {
                if cy >= 0 && cy < MH && x2 >= 0 && x2 < MW {
                    map[cy][x2] = 1
                }
                cy = cy + 1
            }
        } else {
            for cy >= y2 {
                if cy >= 0 && cy < MH && x2 >= 0 && x2 < MW {
                    map[cy][x2] = 1
                }
                cy = cy - 1
            }
        }
        i = i + 1
    }
    ret map
}

fun _mapgen_stairs(map=[], rooms=[], MW=60, MH=20) {
    r0 = rooms[0]
    rl = rooms[#rooms - 1]

    sx = room_x(r0) + floor(room_w(r0) / 2)
    sy = room_y(r0) + floor(room_h(r0) / 2)
    rsx = room_x(rl) + floor(room_w(rl) / 2)
    rsy = room_y(rl) + floor(room_h(rl) / 2)

    map[rsy][rsx] = 2
    ret [sx, sy]
}

fun _mapgen_populate_monsters(rooms=[], sx=0, sy=0, lvl=1, MW=60, MH=20) {
    mon = []
    i = 1
    for i < #rooms {
        r = rooms[i]
        count = utils_randint(1, utils_min(3, lvl + 1))
        j = 0
        for j < count {
            mx = utils_randint(room_x(r), room_x(r) + room_w(r) - 1)
            my = utils_randint(room_y(r), room_y(r) + room_h(r) - 1)
            if !(mx == sx && my == sy) {
                mt = _monster_GOBLIN()
                rr = utils_randint(1, 100)
                if lvl >= 2 && rr > 70 { mt = _monster_TROLL() }
                if lvl >= 3 && rr > 90 { mt = _monster_DRAGON() }
                mon = mon + [factory_monster(mt, mx, my, lvl)]
            }
            j = j + 1
        }
        i = i + 1
    }
    ret mon
}

fun _mapgen_populate_items(rooms=[], MW=60, MH=20) {
    its = []
    i = 1
    for i < #rooms {
        r = rooms[i]
        count = utils_randint(0, 2)
        j = 0
        for j < count {
            ix = utils_randint(room_x(r), room_x(r) + room_w(r) - 1)
            iy = utils_randint(room_y(r), room_y(r) + room_h(r) - 1)
            if utils_randint(0, 1) == 0 {
                its = its + [factory_potion(ix, iy, utils_randint(5, 15))]
            } else {
                its = its + [factory_gold(ix, iy, utils_randint(3, 12))]
            }
            j = j + 1
        }
        i = i + 1
    }
    ret its
}

fun mapgen_level(lvl=1, MW=60, MH=20) {
    rooms = _mapgen_rooms(MW, MH)
    map = _mapgen_fill(rooms, MW, MH)
    map = _mapgen_carve(rooms, map, MW, MH)

    start_pos = _mapgen_stairs(map, rooms, MW, MH)
    sx = start_pos[0]
    sy = start_pos[1]

    monsters = _mapgen_populate_monsters(rooms, sx, sy, lvl, MW, MH)
    items = _mapgen_populate_items(rooms, MW, MH)

    vis = []
    y = 0
    for y < MH {
        vis = vis + [[0] * MW]
        y = y + 1
    }

    ret level_new(map, vis, monsters, items, sx, sy)
}
