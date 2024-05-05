/** idea was somewhat based on Lua (and other) coroutine,

    so you have functions with an associated state type like:
    ```c
    int myco(struct myco_state* state) {
        ...
    }
    ```

    then you initialise such a state and call the function; each times you call it,
    it advances through its internal logic, updating its state as it goes in a way
    it can continue when called again:
    ```c
    struct myco_state st = {0};
    myco(&st);
    myco(&st);
    myco(&st);
    ...
    ```

    once the coroutine is done it returns non-zero, that is a return of zero is
    like yield:
    ```c
    while (!myco(&st))
        ; // can do something between each resume...
    ```
*/

#ifndef co_define

/// vaargs should be the state structure, after should be the function body
#define co_define(__name, ...)              \
    struct __name##_state { __VA_ARGS__ };  \
    int __name(struct __name##_state* const state)
#define co_declare(__name)  \
    struct __name##_state;  \
    int __name(struct __name##_state* const)

/// vaargs should be some `co_seq_item`s
#define co_sequence(__seqst, ...) do {  \
        int* const _seqst = (__seqst);  \
        switch (*_seqst) {              \
            char const _pk;             \
            __VA_ARGS__;                \
            return 0;                   \
        }                               \
    } while (0)
/// vaargs should be an expression that is non-zero if moving to the next state
#define co_seq_item(...)                                     \
        case sizeof(struct _k { char _[sizeof _pk+1]; })-2:  \
            for (struct _k const _pk = {{*_seqst+= !!(__VA_ARGS__)}} ;0; (void)_pk)

/// vaargs should be an expression that is non-zero if resetting
#define co_repeat(__repst, __count, __itst, ...) do {  \
        int* const _repst = (__repst);                 \
        if (*_repst < (__count)) {                     \
            if (__VA_ARGS__) {                         \
                ++*_repst;                             \
                memset((__itst), 0, sizeof*(__itst));  \
                __VA_ARGS__;                           \
            }                                          \
            return 0;                                  \
        }                                              \
    } while (0)

/// vaargs should be the initial state
#define co_consume(__name, ...)  \
    for (struct __name##_state state = {__VA_ARGS__}; !__name(&state); )

#endif // co_define

// ---

co_declare(uno);
co_declare(dos);
co_declare(tres);

#include <stdio.h>
#include <string.h>

co_define(uno,  int n;) { if (state->n < 1) { ++state->n; puts("uno!");  } return 1 == state->n; }
co_define(dos,  int n;) { if (state->n < 2) { ++state->n; puts("dos!");  } return 2 == state->n; }
co_define(tres, int n;) { if (state->n < 3) { ++state->n; puts("tres!"); } return 3 == state->n; }

co_define(seq_example,
    struct uno_state uno_st;
    struct dos_state dos_st;
    struct tres_state tres_st;
    struct tres_state tres_st_2;
    int seqst;
) {
    co_sequence (&state->seqst,
        co_seq_item(tres(&state->tres_st))
        co_seq_item(uno(&state->uno_st))
        co_seq_item(dos(&state->dos_st))
        co_seq_item(tres(&state->tres_st_2))
    );
    return 1;
}

co_define(rep_example,
    int k;
    int const n;
    struct seq_example_state bdf;
) {
    co_repeat(&state->k, state->n, &state->bdf, seq_example(&state->bdf));
    return 1;
}

int main(void) {
    co_consume(rep_example, .n= 3) puts("---");
}
