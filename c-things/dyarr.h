#include <stdbool.h>
#include <stdlib.h>

static bool   _dyarr_resize(void* ptr, size_t isz, size_t* cap, size_t rsz);
static size_t _dyarr_insert(void* ptr, size_t isz, size_t* cap, size_t* len, size_t k, size_t n);
static size_t _dyarr_remove(void* ptr, size_t isz, size_t* len, size_t k, size_t n);

#ifndef dyarr
#include <string.h>

#define dyarr(__elty) struct { __elty* ptr; size_t len, cap; }
#define slarr(__elty) struct { __elty* ptr; size_t len; }

/* clears it empty and free used memory */
#define dyarr_clear(__da)  ((__da)->len = (__da)->cap = 0, free((__da)->ptr), (__da)->ptr = NULL)
/* resizes exactly to given, new size should not be 0 */
#define dyarr_resize(__da, __rsz)  _dyarr_resize(&(__da)->ptr, sizeof*(__da)->ptr, &(__da)->cap, (__rsz))
/* doubles the capacity if more memory is needed */
#define dyarr_push(__da)  ((__da)->len < (__da)->cap || dyarr_resize((__da), (__da)->cap ? (__da)->cap * 2 : 16) ? &(__da)->ptr[(__da)->len++] : NULL)
/* NULL if empty */
#define dyarr_pop(__da)  ((__da)->len ? &(__da)->ptr[--(__da)->len] : NULL)
/* NULL (or rather 0) if OOM, else pointer to the new sub-array (at k, of size n) */
#define dyarr_insert(__da, __k, __n)  ((__da)->ptr + _dyarr_insert(&(__da)->ptr, sizeof*(__da)->ptr, &(__da)->cap, &(__da)->len, (__k), (__n)))
/* doesn't check bounds, pointer to where the sub-array was */
#define dyarr_remove(__da, __k, __n)  ((__da)->ptr + _dyarr_remove(&(__da)->ptr, sizeof*(__da)->ptr, &(__da)->len, (__k), (__n)))
/* pointer to begin or NULL */
#define slarr_alloc(__sl, __len) (((__sl)->ptr = calloc((__sl)->len = (__len), sizeof*(__sl)->ptr)) ? (__sl)->ptr : NULL)

static bool _dyarr_resize(void* ptr, size_t isz, size_t* cap, size_t rsz) {
    void* niw = realloc(*(void**)ptr, rsz * isz);
    return niw ? *(void**)ptr = niw, *cap = rsz, true : false;
    (void)_dyarr_insert;
    (void)_dyarr_remove;
}

static size_t _dyarr_insert(void* ptr, size_t isz, size_t* cap, size_t* len, size_t k, size_t n) {
    return *len+n < *cap || _dyarr_resize(ptr, isz, cap, *len+n) ? memmove(*(char**)ptr+(k+n)*isz, *(char**)ptr+k*isz, (*len-k)*isz), *len+= n, k : -(size_t)*(char**)ptr;
}

static size_t _dyarr_remove(void* ptr, size_t isz, size_t* len, size_t k, size_t n) {
    return memmove(*(char**)ptr+k*isz, *(char**)ptr+(k+n)*isz, ((*len-= n)-k)*isz), k;
}

#endif /* dyarr */
