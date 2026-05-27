// include_sub.tl — defines utilities for include_test.tl
function greet(name="") {
    print("hello, ")
    print(name)
    print("!\n")
}

function double(n=0) {
    return n * 2
}

message = "included!\n"
