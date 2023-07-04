vim9script

# :h strdisplaywidth()

def W(h: string): string
    const n = char2nr(h)
    return n < 127 ? nr2char(' ' == h ? 0x3000 : n + 0xfee0) : h
enddef

def H(w: string): string
    const n = char2nr(w)
    return 0x3000 == n ? ' ' : n < 0xfee0 ? w : nr2char(n - 0xfee0)
enddef

# entries added have a `:Play[.name]`, key is the `.name`
#   name: string
#   Init: func(): dict<any>  see Register for its return type
#   Btnp: func(number, string)
#   Loop: func(number): bool
var registred = {}
# meta for a running instance, key is the bufnr (`id` in functions below)
#   id: number
#   lnwtext: dict<list<number>>  lines with `Print`ed text,
#                                key is y values are ..x,w..
#var running_meta = {}

def Play(name: string)
    const it = registred[name]
    const pp = it.Init()
    const keys: list<string> = get(pp, 'keys', [])

    var winid = 0
    var bufid = 0
    var timid = 0

    winid = popup_create(repeat([repeat(W(' '), pp.width)], pp.height), {
        minwidth: pp.width,
        minheight: pp.height,
        drag: true,
        dragall: true,
        #highlight: ..,
        scrollbar: false,
        #callback: (res) => it.Done,
        filter: (id, key) => {
            if "\<Esc>" == key
                timer_stop(timid)
                popup_close(id)
            elseif -1 == index(keys, key)
                return false
            else
                it.Btnp(bufid, key)
            endif
            return true
        },
        mapping: false,
    })

    bufid = winbufnr(winid)
    #running_meta[bufid] = { id: bufid, lnwtext: {} }

    if pp.fps <= 0
        if it.Loop(bufid)
            popup_close(winid)
        endif
    else
        timid = timer_start(1000 / pp.fps, (id) => {
            if it.Loop(bufid)
                timer_stop(id)
                popup_close(winid)
            endif
        }, { repeat: -1 })
    endif
enddef

# note on Init's return type:
# it should be a dictionary with the following keys:
#   width: number
#   height: number
#   fps: number          if <1, runs Loop once then does not close (except if
#                        Loop returned `true`)
#   keys?: list<string>  keys that Btnp can receive (null means every keys)
#                        non-captured keys will actually do editor stuff!
export def Register(name: string, Init: func(): dict<any>, Btnp: func(number, string), Loop: func(number): bool)
    registred[name] = { name: name, Init: Init, Btnp: Btnp, Loop: Loop }
    exe 'com! Play' .. name .. ' call Play("' .. name .. '")'
enddef

export def Set(id: number, x: number, y: number, char: string)
    const ln = getbufline(id, y + 1)[0]
    setbufline(id, y + 1, (0 == x ? '' : ln[: x - 1]) .. W(char) .. ln[x + 1 :])
enddef

export def Get(id: number, x: number, y: number): string
    return H(getbufline(id, y + 1)[0][x])
enddef

export def In(v: number, v0: number, v1: number): bool
    return v0 <= v && v < v1
enddef

export def Setr(id: number, x0: number, y0: number, x1: number, y1: number, char: string)
    const repl = repeat(W(char), x1 - x0)
    for y in range(y0, y1 - 1)
        const ln = getbufline(id, y + 1)[0]
        setbufline(id, y + 1, (0 == x0 ? '' : ln[: x0 - 1]) .. repl .. ln[x1 :])
    endfor
enddef

export def Getr(id: number, x0: number, y0: number, x1: number, y1: number): list<list<string>>
    final ret: list<list<string>> = []
    for y in range(y0, y1 - 1)
        add(ret, map(split(getbufline(id, y + 1)[0][x0 : x1 - 1], '\zs'), (_, c) => H(c)))
    endfor
    return ret
enddef

export def Inr(x: number, y: number, x0: number, y0: number, x1: number, y1: number): bool
    return x0 <= x && x < x1 && y0 <= y && y < y1
enddef

export def Copy(id: number, dx: number, dy: number, sx0: number, sy0: number, sx1: number, sy1: number)
    final ls: list<string> = []
    for sy in range(sy0, sy1 - 1)
        add(ls, getbufline(id, sy + 1)[0])
    endfor
    for k in range(len(ls))
        const ln = getbufline(id, dy + k + 1)[0]
        setbufline(id, dy + k + 1, (0 == dx ? '' : ln[: dx - 1]) .. ls[k][sx0 : sx1 - 1] .. ln[dx + sx1 - sx0 :])
    endfor
enddef

export def Move(id: number, dx: number, dy: number, sx0: number, sy0: number, sx1: number, sy1: number)
    const repl = repeat(W(' '), sx1 - sx0)
    final ls: list<string> = []
    for sy in range(sy0, sy1 - 1)
        const ln = getbufline(id, sy + 1)[0]
        add(ls, ln)
        setbufline(id, sy + 1, (0 == sx0 ? '' : ln[: sx0 - 1]) .. repl .. ln[sx1 :])
    endfor
    for k in range(len(ls))
        const ln = getbufline(id, dy + k + 1)[0]
        setbufline(id, dy + k + 1, (0 == dx ? '' : ln[: dx - 1]) .. ls[k][sx0 : sx1 - 1] .. ln[dx + sx1 - sx0 :])
    endfor
enddef

export def Print(id: number, x: number, y: number, text: string)
    const len = strchars(text)
    const cells = float2nr(len / 2.0 + 0.5)
    const ln = getbufline(id, y + 1)[0]
    setbufline(id, y + 1, (0 == x ? '' : ln[: x - 1]) .. text .. (and(len, 1) ? ' ' : '') .. ln[x + cells :])
enddef

export def Erase(id: number, x: number, y: number, len: number): string
    const repl = repeat(W(' '), float2nr(len / 2.0 + 0.5))
    const ln = getbufline(id, y + 1)[0]
    setbufline(id, y + 1, (0 == x ? '' : ln[: x - 1]) .. repl .. ln[x + len :])
    return ln[x : x + len - 1]
enddef
