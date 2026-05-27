fun println(x=0) { print(x) print("\n") }

// time() uses CLOCK_REALTIME with nanosecond precision
// At epoch-scale (~1.78e9 as of 2026), double precision gives
// ~238ns per ULP, so very close calls may round to same value

// Two close calls should differ by < 1s and >= 0
t1 = time()
t2 = time()
diff = t2 - t1
println(diff >= 0)           // 1
println(diff < 1)            // 1 (should be < 1 second apart)

// With a small delay, times should differ
i = 0
for i < 10000 {
    // busy-wait to let time advance measurably
    i = i + 1
}
t3 = time()
println(t3 > t1)             // 1 (time advanced)

// time() in a loop - sequential pairs should have tiny diffs
max_diff = 0
i = 0
for i < 10 {
    a = time()
    b = time()
    if b - a > max_diff {
        max_diff = b - a
    }
    i = i + 1
}
println(max_diff < 0.1)      // 1 (each pair should be << 0.1s)
println(time() > 0)          // 1

// Verify it returns a double with sub-second precision
ts = time()
found_increase = 0
j = 0
for j < 100 {
    n = time()
    if n > ts {
        found_increase = 1
        j = 100  // break
    }
    ts = n
    j = j + 1
}
println(found_increase)      // 1 (should see a change within 100 iterations)

println("ok")
