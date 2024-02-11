#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dyarr.h"

#ifndef countof
#define countof(__a) (sizeof(__a)/sizeof*(__a))

#define ref * const
#define cref const ref
#define opref * const
#define opcref const opref

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;
typedef float     f32;
typedef double    f64;
typedef size_t    sz;
typedef dyarr(u8) buf;

//#define mkbuf(__c) (buf){.ptr= (u8*)__c, .len= strlen(__c)}
//#define mkbufa(...) (buf){.ptr= (u8[])__VA_ARGS__, .len= countof(((u8[])__VA_ARGS__))}
//#define mkbufsl(__b, __st, __ed) (buf){.ptr= (__b)->ptr+(__st), .len= (__ed)-(__st)}
#endif
