#define RUVL_IMPLEMENTATION
#include "ruvl.h"

void rotate_triangle(float const time, let const mouse)
{
    scrclear(0x333333);

    (void)time;
    //let const theta = 2*PI*time;
    let const top = mouse; //cexp(I*theta);
    let const rot = cexp(I*2*PI/3);

    lineloop(
            pointc(top        , 0x008888),
            pointc(top*rot    , 0x880088),
            pointc(top*rot*rot, 0x888800));
}

void hoberman_linkage(float const time, let const mouse)
{
    scrclear(0x333333);

    (void)time;
    static double const bar = .7;
    static unsigned const count = 8;

    let fix = 1;
    let mov = mouse;
    float const d = cabs(fix-mov);

    float const half_height = sqrt(bar*bar - d*d/4);

    float const angle = 2*PI/count;
    let const mid_radius = (half_height / tan(angle/2)) * (fix-mov)/d;

    let const should = 0+mid_radius+d/2 * (fix-mov)/d;
    let const offset = should - fix;

    for (unsigned k = 0; k < count; k++) {
        let const dir = (fix-mov) / cabs(fix-mov);
        let const nrm = -cimag(dir) +I* creal(dir);

        let const tipA = (fix+mov)/2 + nrm*half_height;
        let const tipB = (fix+mov)/2 - nrm*half_height;

        lineloop(
                pointc(tipA, 0x770000),
                pointc(mov,  0x770077),
                pointc(tipB, 0x007700),
                pointc(fix,  0x777777));

        fix = (fix+offset)*cexp(I*angle) - offset;
        mov = (mov+offset)*cexp(I*angle) - offset;
    }
}

void pendulum(float const t, let const m)
{
    scrclear(0x333333);

    static float pt = 0;
    float const dt = t-pt;
    pt = t;
    if (0 == pt) return;

#   define countof(_a) (sizeof(_a)/sizeof*(_a))
    static float const l[] = {.5, .5};
    static float a[countof(l)] = {0};
    static float da[countof(l)] = {0};

    let o = 0 +I* .7;
    for (size_t k = 0; k < countof(l); ++k) {
        float const d = cabs(m - (o+l[k]*cexp(I*a[k])));
        float const magnet_f = + 10*sin(a[k]-carg(o-m))/(d*d);
        (void)magnet_f;

        da[k]+= dt * ( -20*cos(a[k]) + !k*magnet_f );
        a[k]+=  dt * ( da[k] - 2*a[k] );

        let const x = o+l[k]*cexp(I*a[k]);
        lineab(point(o), point(x));
        o = x;
    }
}

int main(void)
{
    if (!initContext(500, 500, hoberman_linkage)) return 1;
    while (loopContext());
    deinitContext();
}
