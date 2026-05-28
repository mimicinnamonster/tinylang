fun bestiary_name(mt=0) {
    if mt == _monster_GOBLIN() { ret "goblin" }
    if mt == _monster_TROLL()  { ret "troll" }
    if mt == _monster_DRAGON() { ret "dragon" }
    ret "monster"
}

fun bestiary_glyph(mt=0) {
    if mt == _monster_GOBLIN() { ret "g" }
    if mt == _monster_TROLL()  { ret "T" }
    if mt == _monster_DRAGON() { ret "D" }
    ret "?"
}

fun bestiary_gold(mt=0) {
    if mt == _monster_GOBLIN() { ret utils_randint(1, 5) }
    if mt == _monster_TROLL()  { ret utils_randint(3, 10) }
    ret utils_randint(10, 30)
}
