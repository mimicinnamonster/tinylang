// ffi_sdl_window.tl — Create an SDL2 window via FFI
// Requires: SDL2 (libSDL2-2.0.0.dylib installed via brew)
//
// Tests: dlopen, dlsym, ffi_call with real-world SDL2 functions
// Displays a 640x480 window for 5 seconds, then cleans up.

lib = dlopen("/opt/homebrew/lib/libSDL2-2.0.0.dylib")
if lib = [] {
    lib = dlopen("/usr/local/lib/libSDL2-2.0.0.dylib")
}
if lib = [] {
    lib = dlopen("libSDL2-2.0.0.dylib")
}
assert(lib != [])

// SDL_Init with SDL_INIT_VIDEO (0x20 = 32)
sdl_init = dlsym(lib, "SDL_Init")
assert(sdl_init != [])
result = ffi_call(sdl_init, "ii", 32)
assert(result = 0)

// SDL_CreateWindow(title, x, y, w, h, flags)
// Returns SDL_Window*, args: string, int × 5
// Sig: p = return pointer, s = title, i = x, i = y, i = w, i = h, i = flags
sdl_create_window = dlsym(lib, "SDL_CreateWindow")
assert(sdl_create_window != [])

window = ffi_call(sdl_create_window, "psiiiii", "TinyLang SDL Test", 200, 200, 640, 480, 0)
print("window created")

// Keep the window visible for 5 seconds with event pumping
sdl_pump = dlsym(lib, "SDL_PumpEvents")
assert(sdl_pump != [])
sdl_delay = dlsym(lib, "SDL_Delay")
assert(sdl_delay != [])

print("showing window for 5 seconds...")
i = 0
while i < 10 {
    ffi_call(sdl_pump, "v")
    ffi_call(sdl_delay, "vi", 500)
    i = i + 1
}

// Clean up
sdl_destroy_window = dlsym(lib, "SDL_DestroyWindow")
assert(sdl_destroy_window != [])
ffi_call(sdl_destroy_window, "vp", window)

sdl_quit = dlsym(lib, "SDL_Quit")
assert(sdl_quit != [])
ffi_call(sdl_quit, "v")

print("sdl window test ok")
