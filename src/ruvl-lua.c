#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <pthread.h>
#include <readline/readline.h>

#define RUVL_IMPLEMENTATION
#define RUVL_KEEP_ARRY
#include "ruvl.h"

struct {
    arry(int) x;
} shared;

void render(float const time, float complex const mouse)
{
    (void)time;
    (void)mouse;
    // TODO: apply from lua ruvl lib
}

void* read_eval_loop(void* const usr)
{
    lua_State* const L = usr;
    for (char* line; (line = readline("+| ")); free(line)) {
        char* as_callexpr = malloc(sizeof line + sizeof "_P()");
        if (!as_callexpr) printf("OOM"), exit(1);
        *as_callexpr = '\0';
        strcat(strcat(strcat(as_callexpr, "_P("), line), ")");

        if (luaL_loadstring(L, as_callexpr)) {
            lua_pop(L, 1);
            if (luaL_loadstring(L, line)) goto failed;
        }
        if (lua_pcall(L, 0, 0, 0)) {
        failed:
            printf("%s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }

        free(as_callexpr);
    }
    return NULL;
}

int main(void)
{
    (void)render;
    //if (!initContext(500, 500, render)) return 1;

    lua_State* const L = luaL_newstate();
    luaL_openlibs(L);

    // TODO: add udata to the lua env: time, mousex, mousey (function-like, ()->number)

#   define _LUA_SOURCE(...) #__VA_ARGS__
    if (luaL_dostring(L, _LUA_SOURCE(
            _ = nil
            function _P(o, ...)
                if o then _ = o print(o) end
            end

            local Value, Point

            local function val(o)
                if 'number' == type(o)
                  then return o
                elseif 'table' == type(o) and getmetatable(o) == Value
                  then return val(o[1])
                end
                return o()
            end
            local function isp(o)
                local s, r = pcall(function() return o[1] and o[2] end)
                return s and r
            end

            Value = {
                __add= function(l, r) return setmetatable({function() return val(l) + val(r) end}, Value) end,
                __sub= function(l, r) return setmetatable({function() return val(l) + val(r) end}, Value) end,
                __mul= function(l, r)
                    local pl, pr = isp(l), isp(r)
                    if pl then return setmetatable({function() return val(l[1]) * val(r) end, function() return val(l[2]) * val(r) end}, Point) end
                    if pr then return setmetatable({function() return val(l) * val(r[1]) end, function() return val(l) * val(r[2]) end}, Point) end
                    return setmetatable({function() return val(l) * val(r) end}, Value)
                end,
                __div= function(l, r)
                    local pl, pr = isp(l), isp(r)
                    if pl then return setmetatable({function() return val(l[1]) / val(r) end, function() return val(l[2]) / val(r) end}, Point) end
                    if pr then return setmetatable({function() return val(l) / val(r[1]) end, function() return val(l) / val(r[2]) end}, Point) end
                    return setmetatable({function() return val(l) / val(r) end}, Value)
                end,
                __unm= function(o) return setmetatable({function() return -val(o) end}, Value) end,
                __mod= function(l, r) return setmetatable({function() return val(l) % val(r) end}, Value) end,
                __pow= function(l, r) return setmetatable({function() return val(l) ^ val(r) end}, Value) end,
                // __idiv.. oops

                __tostring= function(self) return tostring(val(self)) end
            }

            Point = {
                __add= function(l, r)
                    assert(isp(l), 'left hand side is not a point')
                    assert(isp(r), 'right hand side is not a point')
                    return setmetatable({function() return val(l[1]) + val(r[1]) end, function() return val(l[2]) + val(r[2]) end}, Point)
                end,

                __sub= function(l, r)
                    assert(isp(l), 'left hand side is not a point')
                    assert(isp(r), 'right hand side is not a point')
                    return setmetatable({function() return val(l[1]) - val(r[1]) end, function() return val(l[2]) - val(r[2]) end}, Point)
                end,

                __mul= function(l, r)
                    local pl, pr = isp(l), isp(r)
                    if pl and pr then return setmetatable({function() return val(l[1]) * val(r[1]) end, function() return val(l[2]) * val(r[2]) end}, Point) end
                    if pl then return setmetatable({function() return val(l[1]) * val(r) end, function() return val(l[2]) * val(r) end}, Point) end
                    if pr then return setmetatable({function() return val(l) * val(r[1]) end, function() return val(l) * val(r[2]) end}, Point) end
                end,

                __div= function(l, r)
                    local pl, pr = isp(l), isp(r)
                    if pl and pr then return setmetatable({function() return val(l[1]) / val(r[1]) end, function() return val(l[2]) / val(r[2]) end}, Point) end
                    if pl then return setmetatable({function() return val(l[1]) / val(r) end, function() return val(l[2]) / val(r) end}, Point) end
                    if pr then return setmetatable({function() return val(l) / val(r[1]) end, function() return val(l) / val(r[2]) end}, Point) end
                end,

                __tostring= function(self)
                    return self.name..'{'..val(self[1])..', '..val(self[2])..'}'
                end,

                __call= function(self, xy)
                    if 'string' == type(xy)
                      then
                        // TODO: parse and set color
                      else
                        if not xy[2] then xy = xy[1] end
                        self[1], self[2] = xy[1], xy[2]
                    end
                    return self
                end,
            }

            mouse = setmetatable({mousex, mousey}, Point)

            function dot(u, v)
                assert(isp(v), 'left hand side is not a vector')
                assert(isp(v), 'right hand side is not a vector')
                return setmetatable({function() return val(u[1])*val(u[1]) + val(u[2])*val(u[2]) end}, Value)
            end

            function mag(v)
                assert(isp(v), 'operand is not a vector')
                return setmetatable({function() return ( val(v[1])^2 + val(v[2])^2 )^.5 end}, Value)
            end

            function dist(a, b)
                assert(isp(a), 'start is not a point')
                assert(isp(b), 'end is not a point')
                return mag(b-a)
            end

            setmetatable(_G, {__index= function(_, name)
                return function(xy)
                    if not name then return end
                    if not xy[1]
                      then xy = {0, 0}
                      else if not xy[2] then xy = xy[1] end
                    end
                    local r = setmetatable({xy[1], xy[2], name= name}, Point)
                    // TODO: r.id = _register(r)
                    _G[name] = r
                    name = nil
                    return r
                end
            end})))) {
        printf("%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 1;
    }
#   undef _LUA_SOURCE

    pthread_t thread;
    pthread_create(&thread, NULL, read_eval_loop, L);

    //while (loopContext());
    //deinitContext();

    pthread_join(thread, NULL);
    lua_close(L);
    return 0;
}
