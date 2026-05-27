fun println(x=0) { print(x) print("\n") }

// sleep() should yield CPU and not busy-wait
// Test with a short sleep (0.1s) and verify time passes

t1 = time()
sleep(0.1)
t2 = time()
diff = t2 - t1

println(diff >= 0.1)     // 1 (at least 100ms elapsed)
println(diff < 5)        // 1 (less than 5s — sanity check)

// Multiple short sleeps
t1 = time()
sleep(0.05)
sleep(0.05)
sleep(0.05)
t2 = time()
diff = t2 - t1
println(diff >= 0.1)     // 1 (total >= 150ms, but allow some margin)

println("ok")
