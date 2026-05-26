// Mandelbrot benchmark — Node.js (V8) version
// Generates PBM image of the Mandelbrot set to stdout

const w = parseInt(process.argv[2] || '200', 10);
const h = w;

// Build row data: array of bytes
const bytesPerRow = (w + 7) >>> 3;
const row = Buffer.alloc(bytesPerRow);

process.stdout.write(`P4\n${w} ${h}\n`);

for (let y = 0; y < h; y++) {
    row.fill(0);
    for (let x = 0; x < w; x++) {
        const cr = 2.0 * x / w - 1.5;
        const ci = 2.0 * y / h - 1.0;
        let zr = 0.0, zi = 0.0;
        let i;
        for (i = 0; i < 50; i++) {
            const zr2 = zr * zr, zi2 = zi * zi;
            if (zr2 + zi2 > 4.0) break;
            zi = 2.0 * zr * zi + ci;
            zr = zr2 - zi2 + cr;
        }
        // Set bit if point is in the set
        if (i === 50) {
            const byteIdx = x >>> 3;
            const bitIdx = 7 - (x & 7);
            row[byteIdx] |= (1 << bitIdx);
        }
    }
    process.stdout.write(row);
}
