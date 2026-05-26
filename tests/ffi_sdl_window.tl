// ffi_sdl_window.tl — OpenGL 3.3 rotating textured cube via SDL2 + FFI
// Requires: libffi (brew install libffi), SDL2 (brew install sdl2)
//   Build helper: cc -shared -fPIC -o tests/ffi_cube_helper.dylib tests/ffi_cube_helper.c \
//                        -framework OpenGL -framework Cocoa -lm

lib = dlopen("/opt/homebrew/lib/libSDL2-2.0.0.dylib")
if lib = [] { lib = dlopen("/usr/local/lib/libSDL2-2.0.0.dylib") }
if lib = [] { lib = dlopen("libSDL2-2.0.0.dylib") }
assert(lib != [])

// Init SDL video
sdl_init = dlsym(lib, "SDL_Init")
ffi_call(sdl_init, "ii", 32)   // SDL_INIT_VIDEO

// OpenGL 3.3 core profile
// SDL_GLattr: DOUBLEBUFFER=5, DEPTH_SIZE=6, CONTEXT_MAJOR=17, CONTEXT_MINOR=18, CONTEXT_PROFILE=21
sgl = dlsym(lib, "SDL_GL_SetAttribute")
ffi_call(sgl, "iii", 5, 1)     // double buffer
ffi_call(sgl, "iii", 6, 24)    // depth size
ffi_call(sgl, "iii", 17, 3)    // major
ffi_call(sgl, "iii", 18, 3)    // minor
ffi_call(sgl, "iii", 21, 1)    // core profile

// Window with OpenGL flag (SDL_WINDOW_OPENGL = 2)
scw = dlsym(lib, "SDL_CreateWindow")
window = ffi_call(scw, "psiiiii", "Rotating Cube", 200, 100, 800, 600, 2)

// OpenGL context
sgcc = dlsym(lib, "SDL_GL_CreateContext")
context = ffi_call(sgcc, "pp", window)

// Load helper
helper = dlopen("tests/ffi_cube_helper.dylib")
if helper = [] { helper = dlopen("ffi_cube_helper.dylib") }
assert(helper != [])

ci  = dlsym(helper, "cube_init")
cr  = dlsym(helper, "cube_render")
cc  = dlsym(helper, "cube_cleanup")
cge = dlsym(helper, "cube_get_error")

state = ffi_call(ci, "p")

err = ffi_call(cge, "s")
if #err > 0 {
    print("cube init failed: ")
    print(#err)
    ffi_call(cc, "vp", state)
    sgdc = dlsym(lib, "SDL_GL_DeleteContext")
    ffi_call(sgdc, "vp", context)
    sdw = dlsym(lib, "SDL_DestroyWindow")
    ffi_call(sdw, "vp", window)
    sq = dlsym(lib, "SDL_Quit")
    ffi_call(sq, "v")
    assert([] != [])
}

// Render loop
ssw = dlsym(lib, "SDL_GL_SwapWindow")
sp  = dlsym(lib, "SDL_PumpEvents")
sd  = dlsym(lib, "SDL_Delay")

angle = 0
f = 0
while f < 300 {
    ffi_call(cr, "vpdii", state, angle, 800, 600)
    ffi_call(ssw, "vp", window)
    ffi_call(sp, "v")
    ffi_call(sd, "vi", 33)
    angle = angle + 2
    f = f + 1
}

// Cleanup
ffi_call(cc, "vp", state)
sgdc = dlsym(lib, "SDL_GL_DeleteContext")
ffi_call(sgdc, "vp", context)
sdw = dlsym(lib, "SDL_DestroyWindow")
ffi_call(sdw, "vp", window)
sq = dlsym(lib, "SDL_Quit")
ffi_call(sq, "v")
print("rotating cube test ok")
