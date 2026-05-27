fun println(x=0) { print(x) print("\n") }

println("--- math ---")

// sin, cos
println(sin(0) < 0.001)   // sin(0) ≈ 0
println(cos(0) > 0.999)   // cos(0) ≈ 1
println(sin(3.14159) < 0.001)  // sin(pi) ≈ 0

// sqrt
println(sqrt(0) == 0)
println(sqrt(4) == 2)
println(sqrt(2) > 1.414 && sqrt(2) < 1.415)

// exp, log
println(exp(0) == 1)
println(exp(1) > 2.718 && exp(1) < 2.719)
println(log(1) == 0)
println(log(exp(1)) > 0.999 && log(exp(1)) < 1.001)

// floor, ceil
println(floor(3.14) == 3)
println(floor(-3.14) == -4)
println(ceil(3.14) == 4)
println(ceil(-3.14) == -3)

// round
println(round(3.14) == 3)
println(round(3.5) == 4)
println(round(-3.14) == -3)
println(round(-3.5) == -4)

println("--- rand ---")

// rand(min, max)
r = rand(0, 10)
println(r >= 0 && r <= 10)
r = rand(-5, 5)
println(r >= -5 && r <= 5)

println("--- sort ---")

arr = sort([3, 1, 4, 1, 5, 9, 2, 6])
println(arr[0] == 1)
println(arr[1] == 1)
println(arr[2] == 2)
println(arr[3] == 3)
println(arr[4] == 4)
println(arr[5] == 5)
println(arr[6] == 6)
println(arr[7] == 9)
println(#arr == 8)

// Sort strings
words = sort(["banana", "apple", "cherry"])
println(words[0] == "apple")
println(words[1] == "banana")
println(words[2] == "cherry")

println("--- set ---")

uniq = set([1, 2, 2, 3, 1, 3, 4])
println(#uniq == 4)
println(uniq[0] == 1 || uniq[1] == 1 || uniq[2] == 1 || uniq[3] == 1)
println(uniq[0] == 2 || uniq[1] == 2 || uniq[2] == 2 || uniq[3] == 2)
println(uniq[0] == 3 || uniq[1] == 3 || uniq[2] == 3 || uniq[3] == 3)
println(uniq[0] == 4 || uniq[1] == 4 || uniq[2] == 4 || uniq[3] == 4)

// Set preserves order of first occurrence
println(set([3, 1, 2, 1, 3]) == [3, 1, 2])

println("--- flat ---")

f = flat([1, [2, [3, 4], 5], 6])
println(#f == 6)
println(f[0] == 1)
println(f[1] == 2)
println(f[2] == 3)
println(f[3] == 4)
println(f[4] == 5)
println(f[5] == 6)

// Flat with strings (shouldn't flatten strings)
fs = flat(["hello", ["world", "!"], "foo"])
println(#fs == 4)
println(fs[0] == "hello")
println(fs[1] == "world")
println(fs[2] == "!")
println(fs[3] == "foo")

// Flat with empty array
println(#flat([]) == 0)

// Flat already flat
println(flat([1, 2, 3]) == [1, 2, 3])

println("--- glob ---")

// Should find at least itself
g = glob("tests/test_math.tl")
println(#g >= 1)
println(g[0] == "tests/test_math.tl")

// Wildcard
g = glob("tests/*.tl")
println(#g > 5)

println("so far so good")
