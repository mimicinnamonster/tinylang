fun _room_X() { ret 0 }
fun _room_Y() { ret 1 }
fun _room_W() { ret 2 }
fun _room_H() { ret 3 }

fun room_new(x=0, y=0, w=0, h=0) {
    r = [0] * 4
    r[_room_X()] = x
    r[_room_Y()] = y
    r[_room_W()] = w
    r[_room_H()] = h
    ret r
}

fun room_x(r=[]) { ret floor(r[_room_X()]) }
fun room_y(r=[]) { ret floor(r[_room_Y()]) }
fun room_w(r=[]) { ret floor(r[_room_W()]) }
fun room_h(r=[]) { ret floor(r[_room_H()]) }
