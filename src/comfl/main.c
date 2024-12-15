#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>

//#define countof(__a) (sizeof(__a)/sizeof*(__a))

#define ref * const
#define cref const* const

#define _HERE_STR(__ln) #__ln
#define _HERE_XSTR(__ln) _HERE_STR(__ln)
#define HERE(__fmt, ...) __FILE__ ":" _HERE_XSTR(__LINE__) ": (in %s) " __fmt "%c", __func__, __VA_ARGS__ '\n'

#define notif(...) (fflush(stdout), fprintf(stderr, HERE(__VA_ARGS__,)))
#define exitf(...) (notif(__VA_ARGS__), exit(EXIT_FAILURE))
#define asserf(__cdt) (__cdt ? 0 : exitf("%s", #__cdt))


#define _FIRST(__conn, ...) (__conn)
#define with_reply_to(__n, ...) for (                 \
    xcb_##__n##_reply_t* _##__n = xcb_##__n##_reply(  \
            _FIRST(__VA_ARGS__),                      \
            xcb_##__n(__VA_ARGS__),                   \
            NULL),                                    \
        *__n = (asserf(_##__n), _##__n);              \
    __n;                                              \
    free(__n), __n = NULL)


xcb_atom_t atom(xcb_connection_t ref conn, char const name[]) {
    xcb_atom_t r;
    static bool const only_if_exists = false;
    with_reply_to (intern_atom, conn, only_if_exists, strlen(name), name)
        r = intern_atom->atom;
    return r;
}

typedef struct geometry {
    uint16_t x, y, w, h;
} geometry_t;

geometry_t get_geometry(xcb_connection_t ref conn, xcb_window_t const win) {
    geometry_t r;
    with_reply_to (get_geometry, conn, win)
        r = (geometry_t){get_geometry->x, get_geometry->y, get_geometry->width, get_geometry->height};
    return r;
}

void set_geometry(xcb_connection_t ref conn, xcb_window_t const win, geometry_t cref geo) {
    uint32_t new_values[] = {geo->x, geo->y, geo->w, geo->h};
    xcb_configure_window(conn, win,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         new_values);
    //xcb_flush(conn);
}

void disconnect(int _, void* arg) {
    xcb_connection_t* conn = arg;
    xcb_disconnect(conn);
}

int main(int argc, char** argv) {
    int screen_num;
    xcb_connection_t* conn = xcb_connect(NULL, &screen_num);
    asserf(!xcb_connection_has_error(conn));
    on_exit(disconnect, conn);

    xcb_window_t focused_window;
    with_reply_to (get_input_focus, conn)
        focused_window = get_input_focus->focus - 1; // XXX: why tho?
    asserf(XCB_WINDOW_NONE != focused_window);

    printf("focused window id: %d\n", focused_window);

    geometry_t geo = get_geometry(conn, focused_window);
    printf("geometry: {x=%d, y=%d, w=%d, h=%d}\n", geo.x, geo.y, geo.w, geo.h);
    //geo.x+= 10;
    geo.x = 150;
    geo.y = 150;
    geo.w = 800;
    geo.h = 800;
    printf("will go: {x=%d, y=%d, w=%d, h=%d}\n", geo.x, geo.y, geo.w, geo.h);

    getchar();

    set_geometry(conn, focused_window, &geo);
    asserf(0 < xcb_flush(conn));

    getchar();
}
