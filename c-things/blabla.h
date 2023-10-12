#ifndef __BLABLA_H__
#define __BLABLA_H__

#include <stdlib.h>
#include <stdbool.h>

#define subarr(__elty)  struct { size_t len; __elty* ptr; }
#define dynarr(__elty)  struct { size_t len; __elty* ptr; size_t cap; }

static inline bool _dynarr_resize(void* ptr, size_t* cap, size_t rsz, size_t isz) {
    void* niw = realloc(*(void**)ptr, rsz * isz);
    return niw ? *(void**)ptr = niw, *cap = rsz, true : false;
}

/* clears it empty and free used memory */
#define dynarr_clear(__da)  ((__da)->len = (__da)->cap = 0, free((__da)->ptr), (__da)->ptr = NULL)
/* resize exactly to given, new size should not be 0 */
#define dynarr_resize(__da, __res)  _dynarr_resize(&(__da)->ptr, &(__da)->cap, (__res), sizeof*(__da)->ptr)
/* double the capacity if more memory is needed */
#define dynarr_push(__da)  ((__da)->len < (__da)->cap || dynarr_resize(__da, (__da)->cap ? (__da)->cap * 2 : 16) ? &(__da)->ptr[(__da)->len++] : NULL)
/* NULL if empty */
#define dynarr_pop(__da)  ((__da)->len ? &(__da)->ptr[(__da)->len--] : NULL)
/* NULL if out of range */
#define dynarr_at(__da, __k)  (0 <= (__k) && (__k) < (__da)->len ? &(__da)->ptr[__k] : NULL)
/* NULL if OOM, else pointer to the new sub-array (at k, of size n) */
#define dynarr_insert(__da, __k, __n)  (__n+(__da)->len < (__da)->cap || dynarr_resize(__da, (__n)+(__da)->len) ? memmove( (__k)+(__n)+(__da)->ptr, (__k)+(__da)->ptr, (__k)+(__n)-(__da)->len), (__da)->len+= (__n), (__k)+(__da)->ptr : NULL)
/* (you have to declare your iterator, and you don't get enumerator either) `some* it; dynarr_for(&some_dynarr, it) { .. }` */
#define dynarr_for(__da, __it)  for (__it = (__da)->ptr; (size_t)(__it-(__da)->ptr) < (__da)->len; ++__it)

/* */
#define dynarr_sub(__da, __k, __n)  {.len= (__n), .ptr= (__n)+(__da)->ptr}
/* */
#define subarr_cpy(__sa)

#endif /* __BLABLA_H__ */
