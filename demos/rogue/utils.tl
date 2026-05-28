fun utils_println(x="")   { print(x); print("\n") }
fun utils_randint(lo=0, hi=1) { ret floor(rand(lo, hi)) }
fun utils_min(a=0, b=0) { if a < b { ret a } ret b }
fun utils_dist2(x1=0, y1=0, x2=0, y2=0) { dx = x2 - x1; dy = y2 - y1; ret dx * dx + dy * dy }
