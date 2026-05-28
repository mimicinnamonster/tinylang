fun factory_monster(mt=1, mx=0, my=0, lvl=1) {
    _g = [8, 3, 1]
    _t = [20, 7, 3]
    _d = [40, 12, 5]
    _s = [[], _g, _t, _d]

    hp  = _s[mt][0] + (lvl - 1) * 3
    atk = _s[mt][1] + lvl - 1
    def = _s[mt][2] + floor((lvl - 1) / 2)

    ret monster_new(mx, my, hp, hp, atk, def, mt, 1)
}

fun factory_potion(ix=0, iy=0, hval=10) {
    ret item_new(ix, iy, _item_POTION(), hval)
}

fun factory_gold(ix=0, iy=0, gval=5) {
    ret item_new(ix, iy, _item_GOLD(), gval)
}
