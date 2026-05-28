// Test UTF-8 / emoji handling

fun println(x=0) { print(x) print("\n") }

// --- Basic UTF-8 in string literals ---
println("--- emoji ---")

s = "héllo wörld 😀🔥"
println(s)
println(#s)

// --- Indexing bytes ---
println("--- byte indexing ---")
println("hé"[0])     // 104 'h'
println("hé"[1])     // 195 (first byte of é in UTF-8: 0xC3)
println("hé"[2])     // 169 (second byte of é in UTF-8: 0xA9)

// --- String as bytes ---
println("--- len as bytes ---")
println(#"hé")       // 3 (bytes, h=1 + é=2)
println(#"😀")       // 4 (bytes for U+1F600)

// --- Concat with number ---
println("--- string + number ---")
println("héllo " + 42)
println(42 + " héllo")

// --- Array ops ---
println("--- array ops ---")
a = ["héllo", "wörld"]
println(a[0])
println(a[1])

// --- Slice ---
println("--- slice ---")
println("héllo wörld"[0:5])

// --- Print emoji directly ---
println("")
println("👉 test directional emoji")
println("🎉 party")
println("🌟 star")
println("✅ checkmark")
println("❤️ heart")
println("🔥🔥🔥 fire x3")

// --- Mix of text and emoji ---
println("--- mixed ---")
println("Hello, 🌍! How are you?")
println("Café au lait ☕")
println("κόσμε")   // Greek
println("你好")    // Chinese
println("Привет") // Russian
println("日本語")  // Japanese


// --- input ---
println("--- input ---")
