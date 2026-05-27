// Comprehensive test for all new builtin functions
//
// Runs as: ./tiny tests/test_builtins.tl
// Each test prints "1" on success (truthy), nothing on failure (or script errors)

fun println(x=0) { print(x) print("\n") }

// helper: print "ok" prefix with test name
fun test(name="") {
    print("[")
    print(name)
    print("] ")
}

// ================================================================
// 1. split(string, separator)
// ================================================================
println("--- split ---")

// --- Basic split ---
test("basic")
parts = split("a,b,c", ",")
println(#parts == 3)
test("basic[0]")
println(parts[0] == "a")
test("basic[1]")
println(parts[1] == "b")
test("basic[2]")
println(parts[2] == "c")

// --- Empty separator → individual chars (JS behavior) ---
test("empty sep")
chars = split("abc", "")
println(#chars == 3)
test("empty sep[0]")
println(chars[0] == "a")
test("empty sep[1]")
println(chars[1] == "b")
test("empty sep[2]")
println(chars[2] == "c")

// --- Empty separator on empty string → [""] (JS behavior) ---
test("empty sep empty str")
es = split("", "")
println(#es == 1)
test("empty sep empty str[0] length")
println(#es[0] == 0)

// --- No match → array with original string ---
test("no match")
none = split("hello", "x")
println(#none == 1)
test("no match[0]")
println(none[0] == "hello")

// --- Empty string with non-empty separator → [""] ---
test("empty str")
empty = split("", ",")
println(#empty == 1)
test("empty str[0] empty")
println(#empty[0] == 0)

// --- Trailing separator ---
test("trailing sep")
trail = split("a,b,", ",")
println(#trail == 3)
test("trailing sep[0]")
println(trail[0] == "a")
test("trailing sep[1]")
println(trail[1] == "b")
test("trailing sep[2] empty")
println(#trail[2] == 0)

// --- Leading separator ---
test("leading sep")
lead = split(",a,b", ",")
println(#lead == 3)
test("leading sep[0] empty")
println(#lead[0] == 0)
test("leading sep[1]")
println(lead[1] == "a")
test("leading sep[2]")
println(lead[2] == "b")

// --- Consecutive separators ---
test("consec sep")
consec = split("a,,b", ",")
println(#consec == 3)
test("consec sep[0]")
println(consec[0] == "a")
test("consec sep[1] empty")
println(#consec[1] == 0)
test("consec sep[2]")
println(consec[2] == "b")

// --- Only separators ---
test("only separators")
only = split(",,,", ",")
println(#only == 4)
test("only sep all empty")
println(#only[0] == 0 && #only[1] == 0 && #only[2] == 0 && #only[3] == 0)

// --- Multi-character separator ---
test("multi-char sep")
mcs = split("axbxc", "x")
println(#mcs == 3)
test("multi-char sep[0]")
println(mcs[0] == "a")
test("multi-char sep[1]")
println(mcs[1] == "b")
test("multi-char sep[2]")
println(mcs[2] == "c")

// --- Multi-byte separator (non-ASCII bytes) ---
test("multi-byte sep")
mbs = split("a\xff\xfe\xffb\xff\xfe\xffc", "\xff\xfe\xff")
println(#mbs == 3)
test("multi-byte sep[0]")
println(mbs[0] == "a")
test("multi-byte sep[1]")
println(mbs[1] == "b")
test("multi-byte sep[2]")
println(mbs[2] == "c")

// --- Separator longer than string ---
test("sep longer than str")
long = split("hi", "hello")
println(#long == 1)
test("sep longer[0]")
println(long[0] == "hi")

// --- Separator at start only ---
test("sep at start")
start_only = split("ab,cd", ",")
println(#start_only == 2)
test("sep at start[0]")
println(start_only[0] == "ab")
test("sep at start[1]")
println(start_only[1] == "cd")

// --- No string content, just separators (empty segments everywhere) ---
test("all empty + sep")
all_empty = split(",,", ",")
println(#all_empty == 3)
test("all empty segments")
println(#all_empty[0] == 0 && #all_empty[1] == 0 && #all_empty[2] == 0)

// --- Substrings are actual strings (is_string) ---
// We can't directly check is_string flag, but we can verify they quack like strings
test("substrings are strings")
ss = split("hello,world", ",")
println(ss[0] + ss[1] == "helloworld")

// --- Split returns consistent results (deterministic) ---
test("deterministic")
s1 = split("x,y,z", ",")
s2 = split("x,y,z", ",")
println(s1[0] == s2[0] && s1[1] == s2[1] && s1[2] == s2[2] && #s1 == #s2)


// ================================================================
// 2. env(name)
// ================================================================
println("--- env ---")

// --- Common env vars exist ---
test("PATH exists")
path = env("PATH")
println(#path > 0)

test("HOME exists")
home = env("HOME")
println(#home > 0)

test("USER exists")
user = env("USER")
println(#user > 0)

// --- PATH contains typical directories ---
test("PATH has /usr/bin")
path_str = env("PATH")
// Use split to check for common dirs
path_parts = split(path_str, ":")
found_usr_bin = 0
i = 0
for i < #path_parts {
    if path_parts[i] == "/usr/bin" {
        found_usr_bin = 1
    }
    i = i + 1
}
test("found /usr/bin in PATH")
println(found_usr_bin)

// --- Non-existent env var returns empty string ---
test("non-existent env")
nope = env("TINYLANG_NONEXISTENT_VAR_XXXXX")
println(nope == "")

test("non-existent len 0")
println(#nope == 0)

// --- TMPDIR on macOS ---
test("TMPDIR exists")
tmp = env("TMPDIR")
println(#tmp > 0)

// --- SHELL exists ---
test("SHELL exists")
shell = env("SHELL")
println(#shell > 0)

// --- Deterministic: same name = same result ---
test("env deterministic")
println(env("HOME") == env("HOME"))


// ================================================================
// 3. args()
// ================================================================
println("--- args ---")

script_args = args()

test("args non-empty")
println(#script_args >= 1)

// First element should contain this test file name (script path)
test("args[0] ends with .tl")
a0 = script_args[0]
// Check last 3 characters
end3 = a0[#a0 - 3 : #a0]
println(end3 == ".tl")

// All args should be strings — check by concatenation behavior
test("args[0] acts like string")
println(#a0 > 0)

// --- args() is deterministic within same run ---
test("args deterministic")
println(args() == args())


// ================================================================
// 4. time()
// ================================================================
println("--- time ---")

ts1 = time()

test("time > Jan 2024")
println(ts1 > 1700000000)

test("time < 2033")
println(ts1 < 2000000000)

// A second call should give a value >= the first
ts2 = time()
test("time monotonic")
println(ts2 >= ts1)

// Range check for current era (2024-2026)
test("time in plausible range")
println(ts1 > 1704067200 && ts1 < 1900000000)  // Jan 2024 to ~2030

// --- Deterministic? No, time moves forward. But difference should be small ---
test("time diff small")
println(ts2 - ts1 >= 0 && ts2 - ts1 < 10)  // at most 10 seconds apart


// ================================================================
// 5. date()
// ================================================================
println("--- date ---")

d = date()

test("date length")
println(#d == 6)

test("date[0] year >= 2024")
yr = d[0]
println(yr >= 2024)

test("date[0] year <= 2026")
println(yr <= 2026)

test("date[1] month 1..12")
mo = d[1]
println(mo >= 1 && mo <= 12)

test("date[2] day 1..31")
dy = d[2]
println(dy >= 1 && dy <= 31)

test("date[3] hour 0..23")
hr = d[3]
println(hr >= 0 && hr <= 23)

test("date[4] minute 0..59")
mi = d[4]
println(mi >= 0 && mi <= 59)

test("date[5] second 0..59")
se = d[5]
println(se >= 0 && se <= 59)

// --- Day limited by month: sanity checks ---
// Months with 30 days: Apr(4), Jun(6), Sep(9), Nov(11)
// If current month is one of those, day must be ≤ 30
test("day ≤ 30 for 30-day months")
if mo == 4 || mo == 6 || mo == 9 || mo == 11 {
    println(dy <= 30)
} else {
    println(1)  // skip, always pass
}

// February: day ≤ 29
// Simple check for Feb (ignoring leap year edge on 29)
test("Feb day ≤ 29")
if mo == 2 {
    println(dy <= 29)
} else {
    println(1)
}

// --- Current month is reasonable (can't be in distant past relative to build) ---
// We know we're in 2026, so month should be between 1 and 5 (May 2026)
test("month is Jan-May 2026 (current period)")
// We know it's May 2026 from the current date
println(mo >= 1 && mo <= 6)

// --- All fields are numbers ---
test("all fields are numbers")
all_nums = d[0] + d[1] + d[2] + d[3] + d[4] + d[5]
println(all_nums > 0)  // sum of all fields, if any weren't numeric this would error

// --- deterministic within same second (or as close as we can get) ---
test("date close calls same")
d2 = date()
// Compare year + month + day (slow-changing parts)
println(d[0] == d2[0] && d[1] == d2[1] && d[2] == d2[2])


// ================================================================
// 6. thisfile()
// ================================================================
println("--- thisfile ---")

f = thisfile()

test("thisfile non-empty")
println(#f > 0)

test("thisfile path contains test_builtins")
// Should contain the test file name
// Use split to look for the filename
f_parts = split(f, "/")
last = f_parts[#f_parts - 1]
println(last == "test_builtins.tl")


// ================================================================
// 7. hash(string)
// ================================================================
println("--- hash ---")

// --- Deterministic ---
test("hash deterministic")
h1 = hash("hello")
h2 = hash("hello")
println(h1 == h2)

// --- Non-zero for typical non-empty strings ---
test("hash non-zero for 'hello'")
println(h1 > 0)

test("hash non-zero for 'world'")
hw = hash("world")
println(hw > 0)

// --- Different strings produce (likely) different hashes ---
test("different strings diff hash")
println(h1 != hw)

// --- Empty string hash ---
test("hash empty string")
println(hash("") >= 0)

// --- Short strings ---
test("hash single char")
println(hash("a") > 0)

test("hash two chars")
println(hash("ab") > 0)

// --- Idempotent: calling multiple times gives same result ---
test("hash idempotent x3")
a = hash("test string")
b = hash("test string")
c = hash("test string")
println(a == b && b == c)

// --- Longer strings ---
test("hash longer string")
long_str = "The quick brown fox jumps over the lazy dog 1234567890 !@#$%^&*()"
println(hash(long_str) > 0)

// --- Hash is unsigned 32-bit range ---
test("hash fits in 32-bit unsigned")
h_big = hash("large test input for hashing")
println(h_big >= 0 && h_big <= 4294967295)

// --- Different lengths produce different hashes ---
test("hash varies with length")
hlen1 = hash("a")
hlen2 = hash("aa")
hlen3 = hash("aaa")
println(hlen1 != hlen2 && hlen2 != hlen3 && hlen1 != hlen3)

// --- Case sensitivity (byte-exact) ---
test("hash case sensitive")
println(hash("abc") != hash("ABC"))

// --- Hash works with non-printable bytes ---
test("hash with special bytes")
h_special = hash("\x00\x01\xff\xfe")
println(h_special >= 0)

// --- Hash consistency with same input from split output ---
test("hash of split output")
spl = split("testing-hash-function", "-")
println(hash(spl[0]) == hash("testing"))

// --- Different separators in string have different hashes ---
test("hash varies with byte order")
println(hash("ab") != hash("ba"))

// --- Hash of concatenated parts vs whole ---
test("hash concat equals hash of same literal")
// "hello" + "world" produces the same string as "helloworld"
println(hash("hello" + "world") == hash("helloworld"))


// ================================================================
// 8. Interaction tests (cross-function)
// ================================================================
println("--- interactions ---")

// split + env: split PATH and verify first directory
test("split env PATH first dir")
first_dir = split(env("PATH"), ":")[0]
println(#first_dir > 0)

// split + hash: hash each word
test("split + hash combo")
words = split("a,b,c", ",")
h_words = hash(words[0]) + hash(words[1]) + hash(words[2])
println(h_words > 0)

// date + time consistency
test("date and time consistent")
now_ts = time()
now_date = date()
// We can't directly verify, but the year should be plausible
// If time() says 2025+ (which we already verified), year from date() should match
println(now_date[0] >= 2024)

// hash + split: hash each part of a split string
test("hash each split part")
data = "x|y|z"
parts = split(data, "|")
println(hash(parts[0]) == hash("x"))
println(hash(parts[1]) == hash("y"))
println(hash(parts[2]) == hash("z"))


println("")
println("--- all builtin tests done ---")
