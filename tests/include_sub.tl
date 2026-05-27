// include_sub.tl — defines utilities for include_test.tl
fun greet(name="") {
    print("hello, ")
    print(name)
    print("!\n")
}

fun double(n=0) {
    ret n * 2
}

message = "included!\n"
