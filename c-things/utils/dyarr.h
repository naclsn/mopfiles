/** dynamic arrays, with eg `typedef dyarr(char) buf`

    and here's a simplified one I use too, but it still has a pesky inline function:
    ```c

// exit on failure
#define OOM  (fputs("OOM", stderr), exit(EXIT_FAILURE), NULL)
// null on failure
#define OOM  NULL

#ifndef arry
#define arry(...) struct { __VA_ARGS__* ptr; size_t len, cap; }
#define frry(__a) ((__a)->cap ? free((__a)->ptr) : (void)0, (__a)->ptr = NULL, (__a)->len = (__a)->cap = 0)
#define push(__a) ((__a)->len < (__a)->cap || ((__a)->cap && ((__a)->ptr = realloc((__a)->ptr, ((__a)->cap+= (__a)->cap+8)*sizeof*(__a)->ptr))) ? (__a)->ptr+(__a)->len++ : (OOM))
#define grow(__a, __k, __n) _grow((char**)&(__a)->ptr, &(__a)->len, &(__a)->cap, sizeof*(__a)->ptr, (__k), (__n))
#define last(__a) ((__a)->ptr+(__a)->len-1)
#define each(__a) for (void* const _ptr = (__a)->ptr,* const _end = _ptr+(__a)->len; _end == (__a)->ptr ? (__a)->ptr = _ptr, 0 : 1; ++(__a)->ptr)
static inline void* _grow(char** const ptr, size_t* const len, size_t* const cap, size_t const s, size_t const k, size_t const n) {
    size_t const nlen = *len+n;
    if (*cap < nlen && !(cap && (*ptr = realloc(*ptr, (*cap = nlen)*s)))) return (OOM);
    if (k < *len) memmove(*ptr+(k+n)*s, *ptr+k*s, (*len-k)*s);
    return *len = nlen, *ptr+k*s;
}
#endif

    ```

*/

#include <stdbool.h>
#include <stdlib.h>

#ifndef _dyarr_emptyresize
#define _dyarr_emptyresize 8
#endif

#if __STDC_VERSION__ < 199901L
#define inline
#endif

static inline bool  _dyarr_resize(void** ptr, size_t isz, size_t* cap, size_t rsz);
static inline void* _dyarr_insert(void** ptr, size_t isz, size_t* cap, size_t* len, size_t k, size_t n);
static inline void  _dyarr_remove(void** ptr, size_t isz, size_t* len, size_t k, size_t n);
static inline void* _dyarr_replace(void** ptr, size_t isz, size_t* cap, size_t* len, size_t k, size_t n, void* spt, size_t sln);
static inline int   _dyarr_cmp(void** s1, size_t z1, void** s2, size_t z2);

#ifdef inline
#undef inline
#endif

#ifndef dyarr
#include <string.h>

#define dyarr(...) struct { __VA_ARGS__* ptr; size_t len, cap; }

/* clears it empty and free used memory */
#define dyarr_clear(__da)  ((__da)->len = (__da)->cap = (__da)->cap ? free((__da)->ptr), 0 : 0, (__da)->ptr = NULL)
/* resizes exactly to given, new size should not be 0 */
#define dyarr_resize(__da, __rsz)  _dyarr_resize((void**)&(__da)->ptr, sizeof*(__da)->ptr, &(__da)->cap, (__rsz))

/* doubles the capacity if more memory is needed */
#define dyarr_push(__da)  ((__da)->len < (__da)->cap || dyarr_resize((__da), (__da)->cap ? (__da)->cap*2 : (_dyarr_emptyresize)) ? &(__da)->ptr[(__da)->len++] : NULL)
/* NULL if empty */
#define dyarr_pop(__da)  ((__da)->len ? &(__da)->ptr[--(__da)->len] : NULL)
/* NULL if empty */
#define dyarr_top(__da)  ((__da)->len ? &(__da)->ptr[(__da)->len-1] : NULL)
/* NULL if empty */
#define dyarr_bot(__da)  ((__da)->len ? &(__da)->ptr[0] : NULL)

/* insert spaces at [k : k+n], NULL if OOM else (void*)(da->ptr+k) */
#define dyarr_insert(__da, __k, __n)  _dyarr_insert((void**)&(__da)->ptr, sizeof*(__da)->ptr, &(__da)->cap, &(__da)->len, (__k), (__n))
/* removes [k : k+n], doesn't check bounds */
#define dyarr_remove(__da, __k, __n)  _dyarr_remove((void**)&(__da)->ptr, sizeof*(__da)->ptr, &(__da)->len, (__k), (__n))
/* replace [k : k+n] with a copy of src (src->len can be different from n), NULL if OOM else (void*)(da->ptr+k) */
#define dyarr_replace(__da, __k, __n, __src)  _dyarr_replace((void**)&(__da)->ptr, sizeof*(__da)->ptr, &(__da)->cap, &(__da)->len, (__k), (__n), (void*)(__src)->ptr, (__src)->len)

/* same as memcmp as for value sign, also compares lengths first in same way */
#define dyarr_cmp(__s1, __s2)  _dyarr_cmp((void**)(__s1)->ptr, (__s1)->len*sizeof*(__s1)->ptr, (void**)(__s2)->ptr, (__s2)->len*sizeof*(__s2)->ptr)

bool _dyarr_resize(void** ptr, size_t isz, size_t* cap, size_t rsz) {
    void* niw = realloc(*ptr, rsz * isz);
    return niw ? *ptr = niw, *cap = rsz, true : false;
}

void* _dyarr_insert(void** ptr, size_t isz, size_t* cap, size_t* len, size_t k, size_t n) {
    size_t nln = *len+n;
    if (*cap < nln && !_dyarr_resize(ptr, isz, cap, nln)) return NULL;
    memmove(*(char**)ptr+(k+n)*isz, *(char**)ptr+k*isz, (*len-k)*isz);
    *len = nln;
    return *(char**)ptr+k*isz;
}

void _dyarr_remove(void** ptr, size_t isz, size_t* len, size_t k, size_t n) {
    memmove(*(char**)ptr+k*isz, *(char**)ptr+(k+n)*isz, ((*len-= n)-k)*isz);
}

void* _dyarr_replace(void** ptr, size_t isz, size_t* cap, size_t* len, size_t k, size_t n, void* spt, size_t sln) {
    size_t nln = *len+sln-n;
    if (n < sln && *cap < nln && !_dyarr_resize(ptr, isz, cap, nln)) return NULL;
    memmove(*(char**)ptr+(k+sln)*isz, *(char**)ptr+(k+n)*isz, (*len-n-k)*isz);
    memcpy(*(char**)ptr+k*isz, spt, sln*isz);
    *len = nln;
    return *(char**)ptr+k*isz;
}

int _dyarr_cmp(void** s1, size_t z1, void** s2, size_t z2) {
    int d = z1-z2;
    return d ? d : memcmp(s1, s2, z1);
}

#endif /* dyarr */
