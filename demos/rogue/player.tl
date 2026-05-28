// ── Field index functions (private) ──

fun _player_HP()    { ret 0 }
fun _player_MAXHP() { ret 1 }
fun _player_ATK()   { ret 2 }
fun _player_DEF()   { ret 3 }
fun _player_GOLD()  { ret 4 }
fun _player_LEVEL() { ret 5 }
fun _player_KILLS() { ret 6 }

// ── Constructor ──

fun player_new(hp=0, maxhp=0, atk=0, def=0, gold=0, lvl=1, kills=0) {
    p = [0] * 7
    p[_player_HP()] = hp
    p[_player_MAXHP()] = maxhp
    p[_player_ATK()] = atk
    p[_player_DEF()] = def
    p[_player_GOLD()] = gold
    p[_player_LEVEL()] = lvl
    p[_player_KILLS()] = kills
    ret p
}

// ── Accessors ──

fun player_hp(p=[])    { ret floor(p[_player_HP()]) }
fun player_maxhp(p=[]) { ret floor(p[_player_MAXHP()]) }
fun player_atk(p=[])   { ret floor(p[_player_ATK()]) }
fun player_def(p=[])   { ret floor(p[_player_DEF()]) }
fun player_gold(p=[])  { ret floor(p[_player_GOLD()]) }
fun player_level(p=[]) { ret floor(p[_player_LEVEL()]) }
fun player_kills(p=[]) { ret floor(p[_player_KILLS()]) }

// ── Mutators ──

fun player_set_hp(p=[], v=0)    { p[_player_HP()] = v; ret p }
fun player_set_maxhp(p=[], v=0) { p[_player_MAXHP()] = v; ret p }
fun player_set_gold(p=[], v=0)  { p[_player_GOLD()] = v; ret p }
fun player_inc_kills(p=[])      { p[_player_KILLS()] = p[_player_KILLS()] + 1; ret p }

fun player_add_gold(p=[], v=0)  { p[_player_GOLD()] = p[_player_GOLD()] + v; ret p }

fun player_heal(p=[], v=0) {
    p[_player_HP()] = p[_player_HP()] + v
    if p[_player_HP()] > p[_player_MAXHP()] { p[_player_HP()] = p[_player_MAXHP()] }
    ret p
}

fun player_damage(p=[], v=0) {
    p[_player_HP()] = p[_player_HP()] - v
    if p[_player_HP()] < 0 { p[_player_HP()] = 0 }
    ret p
}

fun player_descend(p=[]) {
    p[_player_LEVEL()] = p[_player_LEVEL()] + 1
    p[_player_MAXHP()] = p[_player_MAXHP()] + 5
    p[_player_HP()] = p[_player_MAXHP()]
    ret p
}
