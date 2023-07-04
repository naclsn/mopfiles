vim9script
import './hien.vim'

const pieces_raw = '  @ ,  @ ,  @ ,  @ ;@  ,@@@,   ;  @,@@@,   ;@@,@@; @@,@@ ,   ; @ ,@@@,   ;@@ , @@,   '
const pieces = map(split(pieces_raw, ';'), (_, w) => map(split(w, ','), (_, v) => split(v, '\zs')))

var first: bool
var dead: bool
var pend: string
var cur_piece: list<list<string>>
var next_rand: number
var x: number
var y: number
var score: number

def TryPickPlace(id: number)
    const save = next_rand

    next_rand = rand() % len(pieces)
    hien.Setr(id, 13, 2, 17, 6, ' ')
    const next_piece = pieces[next_rand]
    for j in range(len(next_piece))
        for i in range(len(next_piece))
            hien.Set(id, 13 + i, 2 + j, next_piece[j][i])
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
                const [ii, jj] = [i + dx, j + dy]
                const [pii, pjj] = [i - dx, j - dy]
                if !hien.Inr(pii, pjj, 0, 0, l, l) || ' ' == cur_piece[pjj][pii]
                    hien.Set(id, x + i, y + j, ' ')
                endif
                hien.Set(id, x + ii, y + jj, cur_piece[j][i])
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
            if ' ' != cur_piece[j][i]
                const [ii, jj] = [i + dx, j + dy]
                if ' ' != hien.Get(id, x + ii, y + jj) && (!hien.Inr(ii, jj, 0, 0, l, l) || ' ' == cur_piece[jj][ii])
                    return false
                endif
            endif
        endfor
    endfor
    return true
enddef

def DoRotCur(id: number, dt: number)
    final next = deepcopy(cur_piece)
    const l = len(cur_piece)
    for j in range(l)
        for i in range(l)
            const [ii, jj] = dt < 0 ? [j, l - 1 - i] : [l - 1 - j, i]
            next[jj][ii] = cur_piece[j][i]
            if ' ' != cur_piece[j][i]
                hien.Set(id, x + i, y + j, ' ')
            endif
            hien.Set(id, x + ii, y + jj, next[jj][ii])
        endfor
    endfor
    cur_piece = next
enddef

def CanRotCur(id: number, dt: number, second_try=false): bool
    const l = len(cur_piece)
    for j in range(l)
        for i in range(l)
            if ' ' != cur_piece[j][i]
                const [ii, jj] = dt < 0 ? [j, l - 1 - i] : [l - 1 - j, i]
                if ' ' != hien.Get(id, x + ii, y + jj) && (!hien.Inr(ii, jj, 0, 0, l, l) || ' ' == cur_piece[jj][ii])
                    const away_from_wall = x < 5 ? 1 : -1
                    if second_try
                        DoMoveCur(id, -away_from_wall, 0)
                        return false
                    elseif CanMoveCur(id, away_from_wall, 0)
                        DoMoveCur(id, away_from_wall, 0)
                        return CanRotCur(id, dt, true)
                    else
                        return false
                    endif
                endif
            endif
        endfor
    endfor
    return true
enddef

def RedrawScore(id: number)
    hien.Erase(id, 13, 17, len('Score:'))
    hien.Erase(id, 13, 19, 8)
    if -1 < score
        hien.Print(id, 13, 17, 'Score:')
        hien.Print(id, 13, 19, printf('%8d', score))
    endif
enddef

def ClearLinesCur(id: number)
    const l = len(cur_piece)
    const ls = hien.Getr(id, 1, y, 11, min([y + l, 20]))
    var cleared = 0
    for k in range(len(ls))
        if join(ls[k], '') !~ ' '
            ++cleared
            hien.Move(id, 1, 1, 1, 0, 11, y + k)
        endif
    endfor
    score += [0, 40, 100, 300, 1200][cleared]
    RedrawScore(id)
enddef

def Init(): dict<any>
    first = true
    dead = false
    next_rand = rand() % len(pieces)
    score = 0
    return { width: 18, height: 21, fps: 6, keys: split("q r h j k l a e") }
enddef

def Btnp(id: number, key: string)
    pend = key

    if first || dead
        return
    endif

    if 'j' == pend
        if CanMoveCur(id, 0, 1)
            DoMoveCur(id, 0, 1)
            ++score
            RedrawScore(id)
        endif
    elseif 'k' == pend
        while CanMoveCur(id, 0, 1)
            DoMoveCur(id, 0, 1)
            ++score
        endwhile
        DoMoveCur(id, 0, -1)
        --score
        RedrawScore(id)

    elseif 'h' == pend
        if CanMoveCur(id, -1, 0)
            DoMoveCur(id, -1, 0)
        endif
    elseif 'l' == pend
        if CanMoveCur(id, 1, 0)
            DoMoveCur(id, 1, 0)
        endif

    elseif 'a' == pend
        if CanRotCur(id, -1)
            DoRotCur(id, -1)
        endif
    elseif 'e' == pend
        if CanRotCur(id, 1)
            DoRotCur(id, 1)
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
        RedrawScore(id)
        first = false
    elseif dead
        if 'r' == pend
            score = -1
            RedrawScore(id)
            Init()
        endif
        return false
    endif

    if CanMoveCur(id, 0, 1)
        DoMoveCur(id, 0, 1)
    else
        ClearLinesCur(id)
        TryPickPlace(id)
    endif

    return false
enddef

hien.Register('Tetris', Init, Btnp, Loop)
