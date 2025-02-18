#ifndef __RUVL_H__
#define __RUVL_H__

#include <complex.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define PI 3.14159265358979323846264338327950288
#define PPI (2*PI)
#define IPPI (I*PPI)
#define cpri(_c) creal((_c)), cimag((_c))
#define randn() ((double)rand() / RAND_MAX)
#define randnf() ((float)rand() / RAND_MAX)
#define randr(_min_inc, _max_exc) (rand() % ((_max_exc)-(_min_inc)) + (_min_inc))

typedef float complex let;
typedef uint32_t colo;
union colo {
    colo c;
    struct {
        // note: for now alpha channel is unused
        char b, g, r, a; // (yyy: little endian)
    } cc;
};

typedef void render_callback(float const time, let const mouse);

bool initContext(int const init_w, int const init_h, render_callback* const render);
bool loopContext(void);
void deinitContext(void);

typedef short unsigned ptid;

#define arry(...) struct { __VA_ARGS__* ptr; size_t len, cap; }
#define frry(__a) (free((__a)->cap ? (__a)->ptr : NULL), (__a)->ptr = NULL, (__a)->len = (__a)->cap = 0)
#define push(__a) ((__a)->len < (__a)->cap || (((__a)->cap || !(__a)->ptr) && ((__a)->ptr = realloc((__a)->ptr, ((__a)->cap+= (__a)->cap+8)*sizeof*(__a)->ptr))) ? (__a)->ptr+(__a)->len++ : (exit(EXIT_FAILURE), (__a)->ptr))
#define grow(__a, __k, __n) _ruvl_arrygrow((char**)&(__a)->ptr, &(__a)->len, &(__a)->cap, sizeof*(__a)->ptr, (__k), (__n))
#define last(__a) ((__a)->ptr+(__a)->len-1)
#define each(__a) for (void* const _ptr = (__a)->ptr,* const _end = (__a)->ptr+(__a)->len; _end == (__a)->ptr ? (__a)->ptr = _ptr, 0 : 1; ++(__a)->ptr)
static char* _ruvl_arrygrow(char** const ptr, size_t* const len, size_t* const cap, size_t const s, size_t const k, size_t const n) {
    if (!n) return *ptr+k*s;
    size_t const nlen = *len+n;
    if (*cap < nlen && !(cap && (*ptr = realloc(*ptr, (*cap = nlen)*s)))) exit(EXIT_FAILURE);
    if (k < *len) memmove(*ptr+(k+n)*s, *ptr+k*s, (*len-k)*s);
    return *len = nlen, *ptr+k*s;
}

void scrclear(colo const c);
ptid point(let const p);
ptid pointc(let const p, colo const c);
ptid plot(ptid const p);
void lineab(ptid const a, ptid const b);
void lineloopn(size_t const n, ptid const l[const n]);
#define lineloop(...) lineloopn(sizeof(ptid[]){__VA_ARGS__}/sizeof(ptid), (ptid[]){__VA_ARGS__})

#ifdef RUVL_IMPLEMENTATION

// includes {{{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#define static_assert(_name, _cdt) static char const _name[(_cdt) ? 1 : -1]; (void)_name

// }}}

static struct {
    render_callback* render;
    GLFWwindow* window;
    int init_w, init_h;
    int width, height;
    double xzoom_m1, yzoom_m1;
    bool is_size_up_to_date;

    struct _elem_obj {
        int prog, uniproj_loc, unireso_loc, elem_buffer;
        arry(ptid) indices;
    } lines, points;

    int vert_buffer, vert_attrib;
    arry(struct vertex {
        let p;
        float w;
        colo c;
    }) vertices;
    //arry(char const*) names;
} _context = {0};

// drawing functions {{{

void scrclear(colo const c)
{
    glClearColor(
            (float)(c>>16 &0xff)/0xff,
            (float)(c>> 8 &0xff)/0xff,
            (float)(c>> 0 &0xff)/0xff,
            1);
    glClear(GL_COLOR_BUFFER_BIT);
}

ptid point(let const p)
{
    size_t const r = _context.vertices.len;
    *push(&_context.vertices) = (struct vertex){(p), .02, 0xffffff};
    return r;
}

ptid pointc(let const p, colo const c)
{
    size_t const r = _context.vertices.len;
    *push(&_context.vertices) = (struct vertex){(p), .02, c};
    return r;
}

ptid plot(ptid const p)
{
    *push(&_context.points.indices) = p;
    return p;
}

void lineab(ptid const a, ptid const b)
{
    *push(&_context.lines.indices) = a;
    *push(&_context.lines.indices) = b;
}

void lineloopn(size_t const n, ptid const l[const n])
{
    for (size_t k = 1; k < n; ++k) lineab(l[k-1], l[k]);
    lineab(l[n-1], l[0]);
}

// }}}

// private functions {{{

static void _resizeCallback(GLFWwindow* const window, int const width, int const height)
{
    (void)window;
    glViewport(0, 0, width, height);
    _context.width = width;
    _context.height = height;
    _context.is_size_up_to_date = false;
}

static void _scrollCallback(GLFWwindow* const window, double const xoff, double const yoff)
{
    (void)window;
    (void)xoff;
    _context.xzoom_m1+= yoff*.1;
    _context.yzoom_m1+= yoff*.1;
    _context.is_size_up_to_date = false;
}

static GLFWwindow* _makeWindow(int const w, int const h, char const* const t)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#   ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#   endif

    GLFWwindow* const window = glfwCreateWindow(w, h, t, NULL, NULL);
    if (!window) {
        printf("Could not create GLFW window\n");
        glfwTerminate();
        return NULL;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, _resizeCallback);
    glfwSetScrollCallback(window, _scrollCallback);
    return window;
}

static int _attachShader(int const kind, char const* const text, int const prog)
{
    int shader = glCreateShader(kind);
    glShaderSource(shader, 1, &text, NULL);
    glCompileShader(shader);
    int yes;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &yes);
    if (!yes) {
        char info[1024];
        glGetShaderInfoLog(shader, sizeof info, NULL, info);
        printf("Could not compile shader type %d\n%s", kind, info);
        glDeleteShader(shader);
        return -1;
    }
    glAttachShader(prog, shader);
    return shader;
}

#define SHADER_SOURCE(...) "#version 330 core\n" #__VA_ARGS__
#define with(_initk, _deinitk) for (              \
        _initk, _once_##__LINE__ = 0;                \
        _once_##__LINE__ ? (void)(_deinitk), 0 : 1;  \
        _once_##__LINE__ = 1)

static int _progForLines(void)
{
    int prog = glCreateProgram(), r = -1, delet = 0;

    with (int geom_sh = _attachShader(GL_GEOMETRY_SHADER, SHADER_SOURCE(
            layout(lines) in;
            layout(triangle_strip, max_vertices=4) out;
            uniform mat3 uniproj;
            in vec4 v_col[];
            out vec4 g_col;
            out float across;
            void main() {
                vec2 a = gl_in[0].gl_Position.xy, b = gl_in[1].gl_Position.xy, u = normalize(b-a), n = vec2(-u.y, u.x);
                float aw = gl_in[0].gl_Position.z, bw = gl_in[1].gl_Position.z;
                g_col = v_col[0]; across = 0; gl_Position = vec4(uniproj*vec3(a + n*aw, 1.0), 1.0); EmitVertex();
                                  across = 1; gl_Position = vec4(uniproj*vec3(a - n*aw, 1.0), 1.0); EmitVertex();
                g_col = v_col[1]; across = 0; gl_Position = vec4(uniproj*vec3(b + n*bw, 1.0), 1.0); EmitVertex();
                                  across = 1; gl_Position = vec4(uniproj*vec3(b - n*bw, 1.0), 1.0); EmitVertex();
                EndPrimitive();
            }), prog), (glDetachShader(prog, geom_sh), glDeleteShader(geom_sh))) if (-1 != geom_sh)

    with (int vert_sh = _attachShader(GL_VERTEX_SHADER, SHADER_SOURCE(
            layout(location=0) in vec3 inpos;
            layout(location=1) in uint incol;
            out vec4 v_col;
            void main() {
                v_col = vec4(
                        (incol>>16 & uint(0xff)) /256.0,
                        (incol>> 8 & uint(0xff)) /256.0,
                        (incol>> 0 & uint(0xff)) /256.0,
                        1.0); //(incol>>24 & uint(0xff)) /256.0);
                gl_Position = vec4(inpos, 1.0);
            }), prog), (glDetachShader(prog, vert_sh), glDeleteShader(vert_sh))) if (-1 != vert_sh)

    with (int frag_sh = _attachShader(GL_FRAGMENT_SHADER, SHADER_SOURCE(
            layout(location=0) out vec4 outcol;
            in vec4 g_col;
            in float across;
            void main() {
                const float ridge = 0.4*2.0;
                float within = 1.0-abs(across*2.0-1.0);
                if (ridge < within)
                    outcol = g_col;
                else {
                    float t = within*2.0/ridge;
                    outcol = vec4(
                            mix(pow(g_col.rgb, vec3(2.0)), g_col.rgb, t-1.0),
                            smoothstep(0.0, 1.0, t));
                }
            }), prog), (glDetachShader(prog, frag_sh), glDeleteShader(frag_sh))) if (-1 != frag_sh)

    {
        glLinkProgram(prog);
        int yes;
        glGetProgramiv(prog, GL_LINK_STATUS, &yes);
        if (!yes) {
            char info[1024];
            glGetProgramInfoLog(prog, sizeof info, NULL, info);
            printf("Could not link shader program for lines\n%s\n", info);
            delet = 1;
        } else r = prog;
    }

    if (delet) glDeleteProgram(prog);
    return r;
}

static int _progForPoints(void)
{
    int prog = glCreateProgram(), r = -1, delet = 0;

    with (int geom_sh = _attachShader(GL_GEOMETRY_SHADER, SHADER_SOURCE(
            layout(points) in;
            layout(triangle_strip, max_vertices=3) out;
            uniform mat3 uniproj;
            in vec4 v_col[];
            out vec4 g_col;
            out vec3 g_pos;
            const float pi = acos(-1);
            void main() {
                vec2 p = gl_in[0].gl_Position.xy;
                float w = gl_in[0].gl_Position.z * 2.0/-cos(2.0*pi/3.0);
                g_pos = vec3(p, w);
                g_col = v_col[0]; gl_Position = vec4(uniproj*vec3(p + vec2(cos(0.0       ), sin(0.0       ))*w, 1.0), 1.0); EmitVertex();
                g_col = v_col[0]; gl_Position = vec4(uniproj*vec3(p + vec2(cos(2.0*pi/3.0), sin(2.0*pi/3.0))*w, 1.0), 1.0); EmitVertex();
                g_col = v_col[0]; gl_Position = vec4(uniproj*vec3(p + vec2(cos(4.0*pi/3.0), sin(4.0*pi/3.0))*w, 1.0), 1.0); EmitVertex();
                EndPrimitive();
            }), prog), (glDetachShader(prog, geom_sh), glDeleteShader(geom_sh))) if (-1 != geom_sh)

    with (int vert_sh = _attachShader(GL_VERTEX_SHADER, SHADER_SOURCE(
            layout(location=0) in vec3 inpos;
            layout(location=1) in uint incol;
            out vec4 v_col;
            void main() {
                v_col = vec4(
                        (incol>>16 & uint(0xff)) /256.0,
                        (incol>> 8 & uint(0xff)) /256.0,
                        (incol>> 0 & uint(0xff)) /256.0,
                        1.0); //(incol>>24 & uint(0xff)) /256.0);
                gl_Position = vec4(inpos, 1.0);
            }), prog), (glDetachShader(prog, vert_sh), glDeleteShader(vert_sh))) if (-1 != vert_sh)

    with (int frag_sh = _attachShader(GL_FRAGMENT_SHADER, SHADER_SOURCE(
            layout(location=0) out vec4 outcol;
            uniform mat3 uniproj;
            uniform vec2 unireso;
            in vec4 g_col;
            in vec3 g_pos; // zero center, -1..1 (current if uniproj, initial otherwise) window
            void main() {
                const vec2 init = vec2(500.0, 500.0); // XXX: %d.0, %d.0);
                vec2 uv = (gl_FragCoord.xy-unireso/2.0) /(init/2.0);
                float within = 1 - distance(uv, g_pos.xy) / g_pos.z*2.0;
                if (within < 0.0) discard;
                const float ridge = 0.4;
                if (ridge < within)
                    outcol = g_col;
                else {
                    float t = within*2.0/ridge;
                    outcol = vec4(
                            mix(pow(g_col.rgb, vec3(2.0)), g_col.rgb, t-1.0),
                            smoothstep(0.0, 1.0, t));
                }
            }), prog), (glDetachShader(prog, frag_sh), glDeleteShader(frag_sh))) if (-1 != frag_sh)

    {
        glLinkProgram(prog);
        int yes;
        glGetProgramiv(prog, GL_LINK_STATUS, &yes);
        if (!yes) {
            char info[1024];
            glGetProgramInfoLog(prog, sizeof info, NULL, info);
            printf("Could not link shader program for points\n%s\n", info);
            delet = 1;
        } else r = prog;
    }

    if (delet) glDeleteProgram(prog);
    return r;
}

#undef SHADER_SOURCE
#undef with

static bool _makeElems(struct _elem_obj* const eo, int const prog)
{
    if (-1 == (eo->prog = prog)) return false;
    eo->uniproj_loc = glGetUniformLocation(eo->prog, "uniproj");
    eo->unireso_loc = glGetUniformLocation(eo->prog, "unireso");

    glGenBuffers(1, (unsigned*)&eo->elem_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eo->elem_buffer);
    return true;
}

static void _renderElems(struct _elem_obj const* const eo, int const kind, size_t const plen, float const proj[3*3])
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eo->elem_buffer);
        if (plen != eo->indices.len) glBufferData(GL_ELEMENT_ARRAY_BUFFER, eo->indices.len*sizeof*eo->indices.ptr, eo->indices.ptr, GL_DYNAMIC_DRAW);
        else glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, eo->indices.len*sizeof*eo->indices.ptr, eo->indices.ptr);

        glUseProgram(eo->prog);
            if (proj) {
                glUniformMatrix3fv(eo->uniproj_loc, 1, GL_TRUE, proj);
                glUniform2f(eo->unireso_loc, _context.width, _context.height);
            }
            glDrawElements(kind, eo->indices.len, GL_UNSIGNED_SHORT, 0);
        glUseProgram(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// }}}

// context functions {{{

bool initContext(int const init_w, int const init_h, render_callback* const render)
{
    (void)_ruvl_arrygrow; // yyy: discard unused warning

    _context.render = render;
    _context.window = _makeWindow(_context.init_w = init_w, _context.init_h = init_h, "hi :3");

    _makeElems(&_context.lines, _progForLines());
    _makeElems(&_context.points, _progForPoints());

    glGenBuffers(1, (unsigned*)&_context.vert_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, _context.vert_buffer);

    glGenVertexArrays(1, (unsigned*)&_context.vert_attrib);
    glBindVertexArray(_context.vert_attrib);
        static_assert(assert_vertex_layout_sizeof, 16 == sizeof(struct vertex));
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)0);
        glEnableVertexAttribArray(0);
        static_assert(assert_vertex_layout_offsetof, 12 == offsetof(struct vertex, c));
        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(struct vertex), (void*)offsetof(struct vertex, c));
        glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    glfwGetWindowSize(_context.window, &_context.width, &_context.height);
    return true;
}

bool loopContext(void)
{
    size_t const vert_plen = _context.vertices.len, lines_plen = _context.lines.indices.len, points_plen = _context.points.indices.len;
    _context.vertices.len = _context.lines.indices.len = _context.points.indices.len = 0;

    double const zoomx = exp(_context.xzoom_m1), zoomy = exp(_context.yzoom_m1);

    double xpos, ypos;
    glfwGetCursorPos(_context.window, &xpos, &ypos);
    _context.render(
            glfwGetTime(),
            // FIXME: broken
            zoomx*(xpos*2-_context.width)/_context.init_w -I* zoomy*(ypos*2-_context.height)/_context.init_h);

    glBindBuffer(GL_ARRAY_BUFFER, _context.vert_buffer);
        if (vert_plen != _context.vertices.len) glBufferData(GL_ARRAY_BUFFER, _context.vertices.len*sizeof*_context.vertices.ptr, _context.vertices.ptr, GL_DYNAMIC_DRAW);
        else glBufferSubData(GL_ARRAY_BUFFER, 0, _context.vertices.len*sizeof*_context.vertices.ptr, _context.vertices.ptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    float const proj[3*3] = {zoomx*_context.init_w/_context.width, 0, 0, 0, zoomy*_context.init_h/_context.height, 0, 0, 0, 1};
    bool const resized = !_context.is_size_up_to_date;
    _context.is_size_up_to_date = true;

    glBindVertexArray(_context.vert_attrib);
    _renderElems(&_context.lines, GL_LINES, lines_plen, resized ? proj : NULL);
    _renderElems(&_context.points, GL_POINTS, points_plen, resized ? proj : NULL);
    glBindVertexArray(0);

    glfwSwapBuffers(_context.window);
    glfwPollEvents();

    return !glfwWindowShouldClose(_context.window) && GLFW_PRESS != glfwGetKey(_context.window, GLFW_KEY_ESCAPE);
}

void deinitContext(void)
{
    static_assert(check_that_everything_is_deinit, 168 == sizeof _context);

    glDeleteProgram(_context.lines.prog);
    glDeleteProgram(_context.points.prog);
    glDeleteBuffers(3, (unsigned[3]){
            _context.lines.elem_buffer,
            _context.points.elem_buffer,
            _context.vert_buffer});
    glDeleteVertexArrays(1, (unsigned*)&_context.vert_attrib);

    frry(&_context.lines.indices);
    frry(&_context.points.indices);
    frry(&_context.vertices);
    //frry(&_context.names);

    glfwDestroyWindow(_context.window);
    glfwTerminate();
}

// }}}

#undef static_assert

#endif // RUVL_IMPLEMENTATION

#ifndef RUVL_KEEP_ARRY
#undef arry
#undef frry
#undef push
#undef grow
#undef last
#undef each
#endif // RUVL_KEEP_ARRY

#endif // __RUVL_H__
