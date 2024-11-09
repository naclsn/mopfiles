#include "utils/playnes.h"

void rotate_triangle(void) {
    renderloop (st) {
        clearscreen(0.2f, 0.2f, 0.2f, 1.0f);

        pair const theta = 2*PI*st.time;
        pair const top = st.pos; //cexp(I*theta);
        pair const rot = cexp(I*2*PI/3);

        drawblock (TRIANGLES) {
            colorvertexat(0xff0000, top);
            colorvertexat(0x00ff00, top*rot);
            colorvertexat(0x0000ff, top*rot*rot);
        }
    }
}

void hoberman_linkage(void) {
    static double const bar = .7;
    static unsigned const count = 8;

    renderloop (st) {
        clearscreen(0.2f, 0.2f, 0.2f, 1.0f);

        pair fix = 1;
        pair mov = st.pos; //fix - d;
        double const d = cabs(fix-mov);

        //double const d = cabs(st.pos); //sin(2*PI*t/8)*sin(2*PI*t/8)/2 +.25;
        double const half_height = sqrt(bar*bar - d*d/4);

        double const angle = 2*PI/count;
        pair const mid_radius = (half_height / tan(angle/2)) * (fix-mov)/d;

        pair const mid = (fix+mov)/2;
        colornow(0xff00ff);
        glRectd(
                creal(mid)-.1, cimag(mid)-.1,
                creal(mid)+.1, cimag(mid)+.1);
        pair const center = mid - mid_radius;

        pair const should = 0+mid_radius+d/2;
        pair const offset = should - fix;

        drawblock (LINES) {
            colornow(0xffffff);

            for (unsigned k = 0; k < count; k++) {
                pair const dir = (fix-mov) / cabs(fix-mov);
                pair const nrm = -cimag(dir) +I* creal(dir);

                pair const tipA = (fix+mov)/2 + nrm*half_height;
                pair const tipB = (fix+mov)/2 - nrm*half_height;

                vertexat(mov); vertexat(tipA);
                vertexat(mov); vertexat(tipB);
                vertexat(fix); vertexat(tipA);
                vertexat(fix); vertexat(tipB);

                fix = (fix+offset)*cexp(I*angle) - offset;
                mov = (mov+offset)*cexp(I*angle) - offset;
            }

        }
    }
}

int main(void) { hoberman_linkage(); }
