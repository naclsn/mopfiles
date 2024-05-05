/** vectors, as in (x,y) ones, with eg `typedef vec(2, f) vec2f` */

#ifndef vec

#define __make_t_f float
#define __make_t_d double
#define __make_t_i int
#define __make_t_l long
#define __make_c_2 x, y
#define __make_c_3 x, y, z
#define __make_c_4 x, y, z, w
#define vec(__n, __t)                               \
    union {                                         \
        __make_t_##__t _c[__n];                     \
        struct { __make_t_##__t __make_c_##__n; };  \
    }                    // XXX: this is c11 :/ ^^

#define __vfor_1(__n, __do, ...) __do((__n-1), __n, __VA_ARGS__)
#define __vfor_2(__n, __do, ...) __do((__n-2), __n, __VA_ARGS__) __vfor_1(__n, __do, __VA_ARGS__)
#define __vfor_3(__n, __do, ...) __do((__n-3), __n, __VA_ARGS__) __vfor_2(__n, __do, __VA_ARGS__)
#define __vfor_4(__n, __do, ...) __do((__n-4), __n, __VA_ARGS__) __vfor_3(__n, __do, __VA_ARGS__)
#define __vfor_5(__n, __do, ...) __do((__n-5), __n, __VA_ARGS__) __vfor_4(__n, __do, __VA_ARGS__)
#define __vfor_6(__n, __do, ...) __do((__n-6), __n, __VA_ARGS__) __vfor_5(__n, __do, __VA_ARGS__)
#define __vfor_7(__n, __do, ...) __do((__n-7), __n, __VA_ARGS__) __vfor_6(__n, __do, __VA_ARGS__)
#define __vfor_8(__n, __do, ...) __do((__n-8), __n, __VA_ARGS__) __vfor_7(__n, __do, __VA_ARGS__)
#define __vfor_9(__n, __do, ...) __do((__n-9), __n, __VA_ARGS__) __vfor_8(__n, __do, __VA_ARGS__)
#define __vforx(__vforn, __n, __do, ...) __vforn(__n, __do, __VA_ARGS__)
#define __vfor(__n, __do, ...) __vforx(__vfor_##__n, __n, __do, __VA_ARGS__)

#define __vones_one(__k, __n, _) 1,
#define vones(__n) {__vfor(__n, __vones_one, )}

#define __vadd_one(__k, __n, __l, __r) __l._c[__k] + __r._c[__k],
#define vadd(__n, __l, __r) {__vfor(__n, __vadd_one, (__l), (__r))}
#define __vmul_one(__k, __n, __l, __r) __l*__r._c[__k],
#define vmul(__n, __l, __r) {__vfor(__n, __vmul_one, (__l), (__r))}

#define __vcvt_one(__k, __n, __i) __i._c[__k],
#define vcvt(__n, __i) {__vfor(__n, __vcvt_one, (__i))}

#endif // vec

// ---

typedef vec(2, f) vec2f;
typedef vec(2, i) vec2i;
typedef vec(3, f) vec3f;
typedef vec(3, i) vec3i;

#define vones2() vones(2)
#define vadd2(...) vadd(2, __VA_ARGS__)
#define vmul2(...) vmul(2, __VA_ARGS__)

void bidoof(void) {
    //vec2f a = vmul(2, 3, ((vec(2, f)){1, 1}).c), b = {0};
    vec2f a = vmul2(3, ((vec2f)vones2())), b = {0};
    vec2f r = vadd2(a, b);

    //vec2i bla = vcvt(2, r.c);
}

