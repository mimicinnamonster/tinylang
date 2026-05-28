// ═══════════════════════════════════════════
//  Rotating 3D Cube in ASCII — 60fps demo
//  A rotating wireframe cube rendered in the
//  terminal using TinyLang.
//  Press 'q', ESC, or Ctrl+C to quit.
// ═══════════════════════════════════════════

fun println(x="") { print(x) print("\n") }
fun abs_(x=0) { if x < 0 { ret -x } ret x }

// Convert a string of digits (e.g. from stty output) to a number
fun tonum(s="") {
    n = 0
    i = 0
    for i < #s {
        b = s[i]
        if b >= 48 && b <= 57 {
            n = n * 10 + (b - 48)
        }
        i = i + 1
    }
    ret n
}

// ── Cube geometry ──
// 8 vertices of a unit cube centred at the origin
vx = [-1,  1,  1, -1, -1,  1,  1, -1]
vy = [-1, -1,  1,  1, -1, -1,  1,  1]
vz = [-1, -1, -1, -1,  1,  1,  1,  1]

// 12 edges as pairs of vertex indices
e0 = [0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3]
e1 = [1, 2, 3, 0, 5, 6, 7, 4, 4, 5, 6, 7]

// ── Terminal characters are ~2:1 height:width, so we correct for it
ASPECT = 0.45

// ── Detect terminal size from environment (COLUMNS/LINES), fall back to 80x24
col_str = env("COLUMNS")
lin_str = env("LINES")
if col_str != "" { W = tonum(col_str) } else { W = 80 }
if lin_str != "" { H = tonum(lin_str) } else { H = 24 }

CX = W / 2
CY = H / 2
// Scale to fill ~70% of the smaller screen dimension without clipping
SCALE = floor(W / 6.5)
if SCALE * H / W > 3 { SCALE = floor(H / 2.7) }

// ── Animation state ──
angle = 0
start = time()
running = 1
key_check = 0

// Hide cursor, clear screen
print("\x1b[?25l")
print("\x1b[2J")

for running {
    // Non-blocking key check every 15 frames (~4 times/sec)
    key_check = key_check + 1
    if key_check >= 15 {
        key_check = 0
        k = exec("read -t 0.001 -n 1 key 2>/dev/null; echo \"$key\"")
        if k != "" && k[0] != 10 {
            // q=113, ESC=27, Ctrl+C=3
            if k[0] == 113 || k[0] == 27 || k[0] == 3 { running = 0 }
        }
    }

    // ── Build empty screen buffer ──
    screen = []
    row_idx = 0
    for row_idx < H {
        row = []
        col_idx = 0
        for col_idx < W {
            row = row + [" "]
            col_idx = col_idx + 1
        }
        screen = screen + [row]
        row_idx = row_idx + 1
    }

    // ── Rotate and project vertices ──
    sa = sin(angle)
    ca = cos(angle)

    sx = []; sy = []
    i = 0
    for i < 8 {
        x = vx[i]; y = vy[i]; z = vz[i]

        // Rotate around Y axis
        x1 = x * ca - z * sa
        z1 = x * sa + z * ca

        // Rotate around X axis
        y2 = y * ca - z1 * sa
        z2 = y * sa + z1 * ca

        // Simple perspective projection
        // Viewer at distance d, divide by depth
        d = 4
        f = d / (d + z2)
        px = floor(x1 * f * SCALE + CX)
        py = floor(y2 * f * SCALE * ASPECT + CY)

        sx = sx + [px]
        sy = sy + [py]
        i = i + 1
    }

    // ── Draw edges (Bresenham) ──
    j = 0
    for j < 12 {
        v0 = e0[j]
        v1 = e1[j]

        x1 = sx[v0]; y1 = sy[v0]
        x2 = sx[v1]; y2 = sy[v1]

        dx = abs_(x2 - x1)
        dy = -abs_(y2 - y1)
        step_x = 1; if x1 > x2 { step_x = -1 }
        step_y = 1; if y1 > y2 { step_y = -1 }
        err = dx + dy

        drawing = 1
        for drawing {
            if x1 >= 0 && x1 < W && y1 >= 0 && y1 < H {
                screen[y1][x1] = "*"
            }
            if x1 == x2 && y1 == y2 { drawing = 0 }
            e2 = 2 * err
            if e2 >= dy { err = err + dy; x1 = x1 + step_x }
            if e2 <= dx { err = err + dx; y1 = y1 + step_y }
        }

        j = j + 1
    }

    // ── Render frame ──
    print("\x1b[H")
    row_idx = 0
    for row_idx < H {
        col_idx = 0
        for col_idx < W {
            print(screen[row_idx][col_idx])
            col_idx = col_idx + 1
        }
        print("\n")
        row_idx = row_idx + 1
    }

    // Advance rotation
    angle = angle + 0.04

    // ~60fps timing
    sleep(0.016)
}

// ── Cleanup ──
print("\x1b[?25h")
println("")
println("3D cube demo finished! ✨")
