// ── Field index functions (private) ──

fun _monster_X()     { ret 0 }
fun _monster_Y()     { ret 1 }
fun _monster_HP()    { ret 2 }
fun _monster_MAXHP() { ret 3 }
fun _monster_ATK()   { ret 4 }
fun _monster_DEF()   { ret 5 }
fun _monster_TYPE()  { ret 6 }
fun _monster_ALIVE() { ret 7 }

// ── Type constants ──

fun _monster_GOBLIN() { ret 1 }
fun _monster_TROLL()  { ret 2 }
fun _monster_DRAGON() { ret 3 }

// ── Constructor ──

fun monster_new(x=0, y=0, hp=0, maxhp=0, atk=0, def=0, typ=1, alive=1) {
    m = [0] * 8
    m[_monster_X()] = x
    m[_monster_Y()] = y
    m[_monster_HP()] = hp
    m[_monster_MAXHP()] = maxhp
    m[_monster_ATK()] = atk
    m[_monster_DEF()] = def
    m[_monster_TYPE()] = typ
    m[_monster_ALIVE()] = alive
    ret m
}

// ── Accessors ──

fun monster_x(m=[])     { ret floor(m[_monster_X()]) }
fun monster_y(m=[])     { ret floor(m[_monster_Y()]) }
fun monster_hp(m=[])    { ret floor(m[_monster_HP()]) }
fun monster_maxhp(m=[]) { ret floor(m[_monster_MAXHP()]) }
fun monster_atk(m=[])   { ret floor(m[_monster_ATK()]) }
fun monster_def(m=[])   { ret floor(m[_monster_DEF()]) }
fun monster_type(m=[])  { ret floor(m[_monster_TYPE()]) }
fun monster_alive(m=[]) { ret floor(m[_monster_ALIVE()]) }

// ── Mutators ──

fun monster_set_x(m=[], v=0)     { m[_monster_X()] = v; ret m }
fun monster_set_y(m=[], v=0)     { m[_monster_Y()] = v; ret m }
fun monster_set_hp(m=[], v=0)    { m[_monster_HP()] = v; ret m }
fun monster_set_alive(m=[], v=0) { m[_monster_ALIVE()] = v; ret m }

fun monster_move(m=[], nx=0, ny=0) {
    m[_monster_X()] = nx
    m[_monster_Y()] = ny
    ret m
}

fun monster_damage(m=[], dmg=0) {
    m[_monster_HP()] = m[_monster_HP()] - dmg
    ret m
}

// ── Queries ──

fun monster_find_at(x=0, y=0, monsters=[]) {
    i = 0
    for i < #monsters {
        m = monsters[i]
        if monster_alive(m) && monster_x(m) == x && monster_y(m) == y { ret i }
        i = i + 1
    }
    ret -1
}
