// ── Field index functions (private) ──

fun _item_X()    { ret 0 }
fun _item_Y()    { ret 1 }
fun _item_KIND() { ret 2 }
fun _item_VAL()  { ret 3 }

// ── Kind constants ──

fun _item_POTION() { ret 1 }
fun _item_GOLD()   { ret 2 }

// ── Constructor ──

fun item_new(x=0, y=0, kind=1, val=0) {
    it = [0] * 4
    it[_item_X()] = x
    it[_item_Y()] = y
    it[_item_KIND()] = kind
    it[_item_VAL()] = val
    ret it
}

// ── Accessors ──

fun item_x(it=[])    { ret floor(it[_item_X()]) }
fun item_y(it=[])    { ret floor(it[_item_Y()]) }
fun item_kind(it=[]) { ret floor(it[_item_KIND()]) }
fun item_val(it=[])  { ret floor(it[_item_VAL()]) }

fun item_is_potion(it=[]) { ret it[_item_KIND()] == _item_POTION() }
fun item_is_gold(it=[])   { ret it[_item_KIND()] == _item_GOLD() }

// ── Queries ──

fun item_find_at(x=0, y=0, items=[]) {
    i = 0
    for i < #items {
        it = items[i]
        if item_x(it) == x && item_y(it) == y { ret i }
        i = i + 1
    }
    ret -1
}
