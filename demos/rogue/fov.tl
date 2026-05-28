fun fov_reveal(vis=[], px=0, py=0, r=7, MW=60, MH=20) {
    rr = r * r
    vy = py - r
    for vy <= py + r {
        if vy >= 0 && vy < MH {
            vx = px - r
            for vx <= px + r {
                if vx >= 0 && vx < MW {
                    if utils_dist2(vx, vy, px, py) <= rr { vis[vy][vx] = 1 }
                }
                vx = vx + 1
            }
        }
        vy = vy + 1
    }
    ret vis
}
