// Mandelbrot benchmark — tinylang version
// Computes Mandelbrot set and reports pixel count
// (Binary image output not feasible with print's newline behavior)

function mandelbrot(size) {
    w = size
    h = size
    total = 0

    y = 0
    while y < h {
        x = 0
        while x < w {
            cr = (2 * x) / w - 1.5
            ci = (2 * y) / h - 1.0
            zr = 0.0
            zi = 0.0

            i = 0
            escaped = 0
            while i < 50 {
                zr2 = zr * zr
                zi2 = zi * zi
                if (zr2 + zi2) > 4 {
                    escaped = 1
                    i = 50
                }
                if i < 50 {
                    zi = 2 * zr * zi + ci
                    zr = zr2 - zi2 + cr
                }
                i = i + 1
            }

            if escaped = 0 {
                total = total + 1
            }

            x = x + 1
        }

        y = y + 1
    }

    print(total)
}

mandelbrot(200)
