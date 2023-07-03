vim9script
import './hien.vim'

const [W, H] = [20, 20]
const dirmap = { h: [-1, 0], j: [0, 1], k: [0, -1], l: [1, 0] }
const diropp = { h: 'l', j: 'k', k: 'j', l: 'h' }

var first: bool
var dead: bool
var tail: list<number>
var head: number
var d_curr: string
var d_pend: string
var ax: number
var ay: number

def Init(): dict<any>
    first = true
    dead = false
    tail = [1, 1, 1, 2, 1, 3]
    head = 2
    d_curr = 'l'
    d_pend = ''
    [ax, ay] = [rand() % (W - 4) + 2, rand() % (H - 4) + 2]
    return { width: W, height: H, fps: 12, keys: split("q r h j k l \<left> \<down> \<up> \<right>") }
enddef

def Btnp(id: number, key: string)
    const foldarr = { ["\<left>"]: 'h', ["\<down>"]: 'j', ["\<up>"]: 'k', ["\<right>"]: 'l' }
    d_pend = get(foldarr, key, key)
enddef

def Loop(id: number): bool
    if 'q' == d_pend
        return true
    endif

    if first
        hien.Setr(id, 0, 0, W, H, '#')
        hien.Setr(id, 1, 1, W - 1, H - 1, ' ')
        hien.Set(id, ax, ay, '@')
        first = false

    elseif dead
        if 'r' == d_pend
            hien.Erase(id, W / 2 - 4, H / 2 - 2, 16)
            hien.Erase(id, W / 2 - 3, H / 2, 12)
            Init()
        endif
        return false
    endif

    const next = len(tail) / 2 - 1 == head ? 0 : head + 1
    const px = tail[next * 2]
    const py = tail[next * 2 + 1]
    hien.Set(id, px, py, ' ')

    var x = tail[head * 2]
    var y = tail[head * 2 + 1]
    hien.Set(id, x, y, 'O')

    if '' != d_pend
        if diropp[d_curr] != d_pend && d_curr != d_pend
            d_curr = d_pend
        endif
        d_pend = ''
    endif
    const [dx, dy] = dirmap[d_curr]
    x += dx
    y += dy

    head = next
    tail[head * 2] = x
    tail[head * 2 + 1] = y

    if x == ax && y == ay
        [ax, ay] = [rand() % (W - 2) + 1, rand() % (H - 2) + 1]
        while ' ' != hien.Get(id, ax, ay)
            [ax, ay] = [rand() % (W - 2) + 1, rand() % (H - 2) + 1]
        endwhile
        hien.Set(id, ax, ay, '@')
        insert(tail, py, head * 2 + 2)
        insert(tail, px, head * 2 + 2)

    elseif ' ' != hien.Get(id, x, y)
        dead = true
        hien.Set(id, x, y, '!')
        hien.Print(id, W / 2 - 4, H / 2 - 2, printf('Score: %9d', len(tail) * 10))
        hien.Print(id, W / 2 - 3, H / 2, "'r' to retry")
        return false
    endif

    const headmap = { h: '<', j: 'V', k: 'A', l: '>' }
    hien.Set(id, x, y, headmap[d_curr])

    return false
enddef

hien.Register('Snake', Init, Btnp, Loop)
