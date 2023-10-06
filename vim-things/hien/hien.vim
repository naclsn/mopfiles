vim9script

# ```vim9
# vim9script
# import './hien.vim'
#
# def Init(): dict<any>
#   return {
#       width: 64, height: 48,
#       fps: 0, # call Loop only once
#       keys: ['q'],
#       sprites: {'w': 'Search'},
#   }
# enddef
#
# def Btnp(id: number, key: string): bool
#   return 'q' == key # quit if 'q'
# enddef
#
# def Loop(id: number): bool
#   hien.Setr(id, 2, 3, 4, 5, 'w')
#   return false # do not quit (ie. still call Btnp on key pressed)
# enddef
#
# hien.Register('Test', Init, Btnp, Loop) # adds `:PlayTest`
# ```

def W(h: string): string
    const n = char2nr(h)
    return n < 127 ? nr2char(' ' == h ? 0x3000 : n + 0xfee0) : h
enddef

def H(w: string): string
    const n = char2nr(w)
    return 0x3000 == n ? ' ' : n < 0xfee0 ? w : nr2char(n - 0xfee0)
enddef

var registred = {}

def Play(name: string)
    const it = registred[name]
    const pp = it.Init()
    const keys: list<string> = get(pp, 'keys', [])
    const keylock: bool = get(pp, 'keylock', false)
    const sprites: dict<string> = get(pp, 'sprites', {})

    var winid = 0
    var bufid = 0
    var timid = 0

    final opts = {
        minwidth: pp.width,
        minheight: pp.height,
        drag: true,
        dragall: true,
        scrollbar: false,
        filter: (id, key) => {
            if "\<Esc>" == key
                timer_stop(timid)
                popup_close(id)
            elseif !keylock && -1 == index(keys, key)
                return false
            elseif it.Btnp(bufid, key)
                timer_stop(timid)
                popup_close(id)
            endif
            return true
        },
        mapping: false,
    }
    if has_key(pp, 'background')
        opts.highlight = pp.background
    endif

    winid = popup_create(repeat([repeat(W(' '), pp.width)], pp.height), opts)
    bufid = winbufnr(winid)

    var lastHienHi = 0
    for [k, v] in items(sprites)
        var w = v
        if '[' == v[0]
            const [fg, bg] = matchlist(v, '\v[(\w+);(\w+)')[1 : 2]
            w = 'hien' .. lastHienHi
            exe 'hi' w join([' ctermfg=', fg, ' guibg=', bg, ' guifg=', fg, ' guibg=', bg], '')
            ++lastHienHi
        endif

        matchadd(w, '\V' .. W(k), 10, -1, { window: winid })
    endfor

    if it.Loop(bufid)
        popup_close(winid)
    elseif 0 < pp.fps
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
#   fps: number               if <1, runs Loop once then does not close
#                           (except if Loop returned `true`)
#   keys?: list<string>       keys that Btnp can receive (null means every
#                           keys) non-captured keys will actually do editor
#                           stuff! or use `keylock`
#   keylock?: bool            with it, no key will pass through to the editor
#                           (this is not the default behavior!)
#   sprites?: dict<string>    sprites (single-characters) to associate to a
#                           highlight group, for example: `'#': 'Search'`, every
#                           '#' character will be using the 'Search' group;
#                           if the string starts with a '[' then it must be of
#                           the form `[fg;bg` where bg and fg should be names
#                           taken from |cterm-colors|, ex.: `'#': '[Red;Blue'`
#   background?: string     highlight group to apply to the whole window
export def Register(name: string, Init: func(): dict<any>, Btnp: func(number, string): bool, Loop: func(number): bool)
    registred[name] = { Init: Init, Btnp: Btnp, Loop: Loop }
    exe 'com! Play' .. name .. ' call Play("' .. name .. '")'
enddef

### API

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

export def Clamp(v: number, v0: number, v1: number): number
    return v < v0 ? v0 : v1 <= v ? v1 - 1 : v
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
