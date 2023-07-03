vim9script
import './hien.vim'

const pieces = map(split('  @ ,  @ ,  @ ,  @ ;@  ,@@@,   ;  @,@@@,   ;@@,@@; @@,@@ ,   ; @ ,@@@,   ;@@ , @@,   ', ';'), (_, w) => map(split(w, ','), (_, v) => split(v, '\zs')))

var first: bool
var dead: bool
var pend: string
var cur_piece: list<list<string>>
var next_rand: number
var x: number
var y: number

def TryPickPlace(id: number)
    const save = next_rand

    next_rand = rand() % len(pieces)
    cur_piece = pieces[next_rand]
    x = 13
    y = 2
    for j in range(len(cur_piece))
        for i in range(len(cur_piece))
            hien.Set(id, x + i, y + j, cur_piece[j][i])
        endfor
    endfor

    cur_piece = pieces[save]
    const hl = len(cur_piece) / 2
    x = 5 - hl
    y = hl - 1
    for j in range(len(cur_piece))
        for i in range(len(cur_piece))
            if ' ' != hien.Get(id, x + i, y + j)
                dead = true
                break
            endif
            hien.Set(id, x + i, y + j, cur_piece[j][i])
        endfor
    endfor
enddef

def DoMoveCur(id: number, dx: number, dy: number)
    const l = len(cur_piece)
    for j in range(l)
        for i in range(l)
            if ' ' != cur_piece[j][i]
                const [ii, jj] = [i - dx, j - dy]
                if !hien.Inr(ii, jj, 0, 0, l, l) || ' ' == cur_piece[jj][ii]
                    hien.Set(id, x + i, y + j, ' ')
                endif
                hien.Set(id, x + i + dx, y + j + dy, cur_piece[j][i])
            endif
        endfor
    endfor
    x += dx
    y += dy
enddef

def CanMoveCur(id: number, dx: number, dy: number): bool
    const l = len(cur_piece)
    for j in range(l)
        for i in range(l)
            if ' ' != cur_piece[j][i] && ' ' != hien.Get(id, x + i + dx, y + j + dy)
                const [ii, jj] = [i + dx, j + dy]
                if !hien.Inr(ii, jj, 0, 0, l, l) || ' ' == cur_piece[jj][ii]
                    return false
                endif
            endif
        endfor
    endfor
    return true
enddef

def Init(): dict<any>
    first = true
    dead = false
    next_rand = rand() % len(pieces)
    return { width: 17, height: 21, fps: 6, keys: split("q r h j k l f t") }
enddef

def Btnp(id: number, key: string)
    pend = key

    if first || dead
        return
    endif

    if 'j' == pend
        if CanMoveCur(id, 0, 1)
            DoMoveCur(id, 0, 1)
        endif
    elseif 'h' == pend
        if CanMoveCur(id, -1, 0)
            DoMoveCur(id, -1, 0)
        endif
    elseif 'l' == pend
        if CanMoveCur(id, 1, 0)
            DoMoveCur(id, 1, 0)
        endif
    endif
enddef

def Loop(id: number): bool
    if 'q' == pend
        return true
    endif

    if first
        hien.Setr(id, 0, 0, 12, 21, '#')
        hien.Setr(id, 1, 0, 11, 20, ' ')
        TryPickPlace(id)
        first = false
    elseif dead
        if 'r' == pend
            Init()
        endif
        return false
    endif

    if CanMoveCur(id, 0, 1)
        DoMoveCur(id, 0, 1)
    else
        TryPickPlace(id)
    endif

    return false
enddef

hien.Register('Tetris', Init, Btnp, Loop)
