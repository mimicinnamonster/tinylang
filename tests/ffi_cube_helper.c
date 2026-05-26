/* ffi_cube_helper.c — Tiny OpenGL 3.3 wrapper for TinyLang FFI
 * Compile:
 *   cc -shared -fPIC -o tests/ffi_cube_helper.dylib tests/ffi_cube_helper.c \
 *       -framework OpenGL -framework Cocoa -lm
 *
 * Bridges parts of OpenGL that TinyLang's FFI can't express:
 *   - float parameters (FFI only has double)
 *   - output pointers (glGen*, glGet*iv)
 *   - pointer-to-pointer (glShaderSource)
 *   - float array math (MVP matrix)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <OpenGL/gl3.h>

/* Mat4 helpers (column-major) */
static void m4_id(float m[16]) { memset(m,0,64); m[0]=m[5]=m[10]=m[15]=1; }
static void m4_mul(float r[16],const float a[16],const float b[16]) {
    float t[16];
    for(int i=0;i<4;i++)for(int j=0;j<4;j++)
        t[i+j*4]=a[i]*b[j*4]+a[i+4]*b[1+j*4]+a[i+8]*b[2+j*4]+a[i+12]*b[3+j*4];
    memcpy(r,t,64);
}
static void m4_roty(float m[16],float rad){
    float c=cosf(rad),s=sinf(rad),r[16];m4_id(r);
    r[0]=c;r[2]=-s;r[8]=s;r[10]=c;
    float t[16];memcpy(t,m,64);m4_mul(m,t,r);
}

/* GLSL shaders */
static const char *vs =
    "#version 330 core\n"
    "layout(location=0)in vec3 p;layout(location=1)in vec2 u;"
    "uniform mat4 m;out vec2 v;"
    "void main(){gl_Position=m*vec4(p,1);v=u;}";
static const char *fs =
    "#version 330 core\n"
    "in vec2 v;uniform sampler2D t;out vec4 c;"
    "void main(){c=texture(t,v);}";

/* 64×64 checkerboard texture */
static GLuint mktex(void){
    unsigned char d[64*64*4];
    for(int y=0;y<64;y++)for(int x=0;x<64;x++){
        int i=(y*64+x)*4,e=((x/8)+(y/8))&1;
        d[i+0]=e?220:120;d[i+1]=e?180:80;d[i+2]=e?130:40;d[i+3]=255;
    }
    d[(32*64+32)*4+0]=255;d[(32*64+32)*4+1]=0;
    d[(32*64+32)*4+2]=0;d[(32*64+32)*4+3]=255;
    GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,d);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    return t;
}

/* 36 unindexed vertices: each face = 2 tris × 3 verts × 5 floats (3pos+2uv) = 30 floats per face */
/* All triangles verified CCW from outside via cross product. */  
static const float verts[] = {
    /* front (+z) */ -.5f,-.5f, .5f,0,0,  .5f,-.5f, .5f,1,0,  .5f, .5f, .5f,1,1,
                     -.5f,-.5f, .5f,0,0,  .5f, .5f, .5f,1,1,  -.5f, .5f, .5f,0,1,
    /* back  (-z) */  .5f,-.5f,-.5f,1,0,  -.5f,-.5f,-.5f,0,0,  -.5f, .5f,-.5f,0,1,
                      .5f,-.5f,-.5f,1,0,  -.5f, .5f,-.5f,0,1,   .5f, .5f,-.5f,1,1,
    /* left  (-x) */ -.5f,-.5f,-.5f,0,0,  -.5f,-.5f, .5f,1,0,  -.5f, .5f, .5f,1,1,
                     -.5f,-.5f,-.5f,0,0,  -.5f, .5f, .5f,1,1,  -.5f, .5f,-.5f,0,1,
    /* right (+x) */  .5f,-.5f, .5f,0,0,   .5f,-.5f,-.5f,1,0,   .5f, .5f,-.5f,1,1,
                      .5f,-.5f, .5f,0,0,   .5f, .5f,-.5f,1,1,   .5f, .5f, .5f,0,1,
    /* top   (+y) */  .5f, .5f,-.5f,1,1,  -.5f, .5f,-.5f,0,1,   .5f, .5f, .5f,1,0,
                     -.5f, .5f, .5f,0,0,   .5f, .5f, .5f,1,0,  -.5f, .5f,-.5f,0,1,
    /* bottom(-y) */ -.5f,-.5f,-.5f,0,0,   .5f,-.5f,-.5f,1,0,   .5f,-.5f, .5f,1,1,
                     -.5f,-.5f,-.5f,0,0,   .5f,-.5f, .5f,1,1,  -.5f,-.5f, .5f,0,1,
};

static char gerr[256] = "";

/* ── Public API ──────────────────────────────────────────────────── */

/* Returns opaque pointer, or NULL. Check cube_get_error() on NULL. */
void *cube_init(void) {
    gerr[0]=0;
    GLuint vao,vbo,tex,prog;

    glGenVertexArrays(1,&vao);
    glBindVertexArray(vao);

    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,20,(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,20,(void*)12);
    glEnableVertexAttribArray(1);

    const char *vss=vs,*fss=fs;

    GLuint vs_h=glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs_h,1,&vss,NULL);glCompileShader(vs_h);
    {GLint o;glGetShaderiv(vs_h,GL_COMPILE_STATUS,&o);
     if(!o){glGetShaderInfoLog(vs_h,256,NULL,gerr);glDeleteShader(vs_h);return NULL;}}

    GLuint fs_h=glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs_h,1,&fss,NULL);glCompileShader(fs_h);
    {GLint o;glGetShaderiv(fs_h,GL_COMPILE_STATUS,&o);
     if(!o){glGetShaderInfoLog(fs_h,256,NULL,gerr);glDeleteShader(vs_h);glDeleteShader(fs_h);return NULL;}}

    prog=glCreateProgram();
    glAttachShader(prog,vs_h);glAttachShader(prog,fs_h);glLinkProgram(prog);
    {GLint o;glGetProgramiv(prog,GL_LINK_STATUS,&o);
     if(!o){glGetProgramInfoLog(prog,256,NULL,gerr);glDeleteShader(vs_h);glDeleteShader(fs_h);return NULL;}}
    glDeleteShader(vs_h);glDeleteShader(fs_h);

    tex=mktex();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glClearColor(.1f,.1f,.15f,1);

    GLenum err=glGetError();
    if(err){snprintf(gerr,256,"GL err 0x%x",err);return NULL;}

    typedef struct{GLuint vao,vbo,tex,prog;GLint m;}S;
    S *st=calloc(1,sizeof(S));
    if(!st){snprintf(gerr,256,"nomem");return NULL;}
    st->vao=vao;st->vbo=vbo;st->tex=tex;st->prog=prog;
    st->m=glGetUniformLocation(prog,"m");
    return st;
}

const char *cube_get_error(void){return gerr;}

void cube_render(void *p,double ang,int w,int h){
    if(!p)return;
    typedef struct{GLuint vao,vbo,tex,prog;GLint m;}S;
    S *s=(S*)p;if(!s->prog)return;

    glViewport(0,0,w,h);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    float a=(float)w/(float)h,fov=45*(float)M_PI/180;
    float proj[16],view[16],model[16],mvp[16];

    memset(proj,0,64);float t=tanf(fov*.5f);
    proj[0]=1/(a*t);proj[5]=1/t;proj[10]=-(100.1f)/(99.9f);proj[11]=-1;proj[14]=-(20)/(99.9f);

    m4_id(view);view[14]=-5;
    m4_id(model);m4_roty(model,(float)(ang*(float)M_PI/180));
    m4_mul(mvp,proj,view);m4_mul(mvp,mvp,model);

    glUseProgram(s->prog);
    glUniformMatrix4fv(s->m,1,GL_FALSE,mvp);
    glUniform1i(glGetUniformLocation(s->prog,"t"),0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,s->tex);
    glBindVertexArray(s->vao);
    glDrawArrays(GL_TRIANGLES,0,36);
}

void cube_cleanup(void *p){
    if(!p)return;
    typedef struct{GLuint vao,vbo,tex,prog;GLint m;}S;
    S *s=(S*)p;
    glDeleteProgram(s->prog);glDeleteTextures(1,&s->tex);
    glDeleteBuffers(1,&s->vbo);glDeleteVertexArrays(1,&s->vao);free(s);
}
