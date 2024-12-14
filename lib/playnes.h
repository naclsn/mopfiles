#if 0
if [ -f "$1" ]
  then cc -x c "$1" -o "$1.exe" -lGL -lglfw -lm && "./$1.exe"
  else echo "$0: expected name"
fi
:<<END
#endif
#define END

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef double complex pair;
#define PI  (3.14159265358979323846264338327950288)

#ifndef PLYN_W
# define PLYN_W 500
#endif
#ifndef PLYN_H
# define PLYN_H 500
#endif
#ifndef PLYN_T
# define PLYN_T "Playnes"
#endif

#include <GLFW/glfw3.h>

typedef uint32_t color;
//struct rgba { char rgba[4]; };
//#define rgba(_color) ();

#ifndef resize_callback
static void resize_callback(GLFWwindow* const window, int const width, int const height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION_MATRIX);
    glLoadIdentity();
    glOrtho(-width/(double)PLYN_W, width/(double)PLYN_W, -height/(double)PLYN_H, height/(double)PLYN_H, 0, 1);
}
#endif

#define pairx(p) (((double*)&p)[0])
#define pairy(p) (((double*)&p)[1])

#define renderloop(_st) for (                                                        \
        struct {                                                                     \
            GLFWwindow* const window;                                                \
            long long unsigned frame;                                                \
            double time;                                                             \
            int width, height;                                                       \
            pair win_pos, pos;                                                       \
        } _st = {                                                                    \
            .window= (                                                               \
                glfwInit(),                                                          \
                glfwCreateWindow(PLYN_W, PLYN_H, PLYN_T, NULL, NULL)),               \
            .frame= (_st.window                                                      \
                ? glfwSetFramebufferSizeCallback(_st.window, resize_callback),       \
                  glfwMakeContextCurrent(_st.window) : exit(1), 0),                  \
        };                                                                           \
        glfwWindowShouldClose(_st.window)                                            \
            ? glfwDestroyWindow(_st.window), glfwTerminate(), 0 : 1;                 \
        glfwSwapBuffers(_st.window), glfwPollEvents(),                               \
            _st.time = glfwGetTime(),                                                \
            glfwGetWindowSize(_st.window, &_st.width, &_st.height),                  \
            glfwGetCursorPos(_st.window, &pairx(_st.win_pos), &pairy(_st.win_pos)),  \
            _st.pos = ((creal(_st.win_pos)*2-_st.width)/PLYN_W                       \
                    -I*(cimag(_st.win_pos)*2-_st.height)/PLYN_H))

#define clearscreen(...) (glClearColor(__VA_ARGS__), glClear(GL_COLOR_BUFFER_BIT))

#define drawblock(_what) for (  \
        int _block_##_what = (glBegin(GL_##_what), 1); _block_##_what; _block_##_what = (glEnd(), 0))

#define colornow(_color) do {             \
        color const c = (_color);         \
        glColor4ubv((char unsigned*)&c);  \
    } while (0)

#define vertexat(_pair) do {       \
        pair const p = (_pair);    \
        glVertex2dv((double*)&p);  \
    } while (0)

#define colorvertexat(_color, _pair) do {  \
        color const c = (_color);          \
        pair const p = (_pair);            \
        glColor4ubv((char unsigned*)&c);   \
        glVertex2dv((double*)&p);          \
    } while (0)

END
