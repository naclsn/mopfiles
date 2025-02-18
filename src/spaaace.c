#define RUVL_IMPLEMENTATION
#define RUVL_KEEP_ARRY
#include "ruvl.h"

static arry(struct body {
    let pos, vel;
    float mass;
    colo col;
}) bodies;

void system_1(void)
{
    push(&bodies)->pos = 0;
    last(&bodies)->vel = 0;
    last(&bodies)->mass = 1.9891e6;
    last(&bodies)->col = 0xfe8a00;

    push(&bodies)->pos = 1;
    last(&bodies)->vel = I;
    last(&bodies)->mass = 5.972;
    last(&bodies)->col = 0x1787fa;

    push(&bodies)->pos = -1 -I* .003;
    last(&bodies)->vel = -.4 +I;
    last(&bodies)->mass = 7.34767309e-2;
    last(&bodies)->col = 0x788a8c;
}

void system_rand(void)
{
    *push(&bodies) = (struct body){
        .pos= 0 +I* 0,
        .vel= 0 +I* 0,
        .mass= 1e6,
        .col= 0x000000,
    };

    size_t const count = randr(7, 17);
    for (size_t k = 0; k < count; ++k) {
        *push(&bodies) = (struct body) {
            .pos= sqrt(randn()) * cexp(IPPI*randn()),
            .vel= (randn()+.5) * cexp(IPPI*randn()),
            .mass= randr(25, 500),
            .col= randr(0x111111, 0xffffff),
        };
    }
}

void spaaace(float const t, let const m)
{
    (void)m;
    scrclear(0x333333);

    static float const G = 6.6743e-7;

    static float pt = 0;

    if (!pt) {
        //system_1();
        system_rand();
    }

    float const dt = t-pt;
    pt = t;

#   define b(_k) bodies.ptr[_k]
    for (size_t i = 0; i < bodies.len; ++i) {
        for (size_t j = 0; j < bodies.len; ++j) if (i != j) {
            let const d = b(i).pos - b(j).pos;
            float const r = cabsf(d);
            let const u = d/r;

            // Fji = -G mi*mj/r2 .uji
            // ai = Fji/mi
            let acc = -G * b(j).mass/(r*r) *u;
            b(i).vel+= acc * dt;
        }

        b(i).pos+= b(i).vel * dt;
        plot(pointc(b(i).pos, b(i).col));
    }
}

int main(void)
{
    if (!initContext(500, 500, spaaace)) return 1;
    while (loopContext());
    deinitContext();
}
