#include <GLFW/glfw3.h>
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef double complex pair;
#define PI  (3.14159265358979323846264338327950288)
#define plot(pair) glVertex2dv((double*)&pair);

void rotate_triangle(double const t, pair const mouse) {
    double theta = 2*PI*t;
    pair top = cexp(I*theta);

    pair rot = cexp(I*2*PI/3);

    pair a = top*rot, b = top*rot*rot;

    glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f); glVertex2dv((double*)&(top));
        glColor3f(0.0f, 1.0f, 0.0f); glVertex2dv((double*)&(a));
        glColor3f(0.0f, 0.0f, 1.0f); glVertex2dv((double*)&(b));
    glEnd();
}

void hoberman_linkage(double const t, pair const mouse) {
    static double const bar = .7;
    static unsigned const count = 8;

    double const d = cabs(mouse); //sin(2*PI*t/8)*sin(2*PI*t/8)/2 +.25;
    double const half_height = sqrt(bar*bar - d*d/4);

    double const angle = 2*PI/count;
    double const mid_radius = half_height / tan(angle/2);

    pair fix = 1;
    pair mov = fix - d;

    pair const mid = (fix+mov)/2;
    pair const center = mid - mid_radius;

    pair const should = 0+mid_radius+d/2;
    pair const offset = should - fix;

    glBegin(GL_LINES);
    glColor3f(1.0f, 1.0f, 1.0f);

    for (unsigned k = 0; k < count; k++) {
        pair const dir = (fix-mov) / cabs(fix-mov);
        pair const nrm = -cimag(dir) +I* creal(dir);

        pair const tipA = (fix+mov)/2 + nrm*half_height;
        pair const tipB = (fix+mov)/2 - nrm*half_height;

        plot(mov); plot(tipA);
        plot(mov); plot(tipB);
        plot(fix); plot(tipA);
        plot(fix); plot(tipB);

        fix = (fix+offset)*cexp(I*angle) - offset;
        mov = (mov+offset)*cexp(I*angle) - offset;
    }

    glEnd();
}

#define render hoberman_linkage

void framebuffer_size_callback(GLFWwindow* const window, int const width, int const height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION_MATRIX);
    glLoadIdentity();
    glOrtho(-width/500., width/500., -height/500., height/500., 0, 1);
}

int main(void) {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(500, 500, "windowosdlfjawf", NULL, NULL);
    if (window) for (glfwMakeContextCurrent(window), glfwSetFramebufferSizeCallback(window, framebuffer_size_callback); !glfwWindowShouldClose(window); glfwSwapBuffers(window), glfwPollEvents()) {
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        double xpos, ypos;
        int width, height;
        glfwGetCursorPos(window, &xpos, &ypos);
        glfwGetWindowSize(window, &width, &height);
        render(glfwGetTime(), (xpos/width +I* ypos/height)*2 -1-I);
    }
    glfwDestroyWindow(window);
    glfwTerminate();
}
