fun combat_damage(atk=0, ddef=0) {
    base = atk - ddef
    if base < 1 { base = 1 }
    ret base + utils_randint(0, 3)
}
