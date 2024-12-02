vim9script
import './hien.vim'

const width = 9
const height = 9
const max_count = 13

var dead: bool
var x: number
var y: number
var field: list<list<number>>
var count: number
var unknown: number

def Init(): dict<any>
    dead = false

    x = (width - 1) / 2
    y = (height - 1) / 2

    field = []
    count = 0
    unknown = width * height
    for k in range(width)
        add(field, repeat([0], height))
    endfor
    for k in range(max_count)
        const i = rand() % width
        const j = rand() % height
        if -1 == field[i][j]
            continue
        endif
        field[i][j] = -1
        ++count
        for ii in [i - 1, i, i + 1]
            for jj in [j - 1, j, j + 1]
                if hien.Inr(ii, jj, 0, 0, width, height) && -1 < field[ii][jj]
                    ++field[ii][jj]
                endif
            endfor
        endfor
    endfor
    unknown -= count

    return {
        width: width, height: height + 1, fps: 0,
        keys: split('q r d D f F h j k l'),
        sprites: {
            '*': 'ErrorMsg',
            '#': 'Comment',
            '&': 'Cursor',
            '|': 'Search',
            '3': 'Question',
            '4': 'Question',
            '5': 'WarningMsg',
            '6': 'WarningMsg',
            '7': 'WarningMsg',
            '8': 'WarningMsg',
        },
    }
enddef

def UpdateCount(id: number)
    hien.Erase(id, 0, height, 10)
    hien.Print(id, 0, height, printf('Left: %4d', count))
enddef

def RevealAll(id: number)
    for i in range(width)
        for j in range(height)
            hien.Set(id, i, j, -1 == field[i][j] ? '*' : 0 == field[i][j] ? ' ' : string(field[i][j]))
        endfor
    endfor
enddef

# TODO:
#def Reveal(id: number, todo: list<list<number>>)
                #final todo = [[x, y]]
                #while 0 != len(todo)
                #    const [i, j] = remove(todo, 0)
                #    if !hien.Inr(i, j, 0, 0, width, height) || '#' != hien.Get(id, i, j) || -1 == field[i][j]
                #        continue
                #    endif
                #    if 0 == field[i][j]
                #        hien.Set(id, i, j, ' ')
                #        add(todo, [i - 1, j])
                #        add(todo, [i + 1, j])
                #        add(todo, [i, j - 1])
                #        add(todo, [i, j + 1])
                #    else
                #        hien.Set(id, i, j, string(field[i][j]))
                #    endif
                #endwhile
#enddef

def Btnp(id: number, key: string): bool
    if 'q' == key
        return true
    endif
    if 'r' == key
        if !dead
            hien.Erase(id, x, y, 2)
            hien.Erase(id, 0, height, 9)
        endif
        Init()
        Loop(id)
        return false
    endif
    if dead
        return false
    endif

    var tile = hien.Get(id, x, y)
    hien.Erase(id, x, y, 2)
    hien.Set(id, x, y, tile)

    if 'd' == key
        if '#' == tile
            --unknown
            if -1 == field[x][y]
                tile = '*'
                RevealAll(id)
                dead = true
                return false

            elseif 0 != field[x][y]
                tile = string(field[x][y])
            else
                tile = ' '
                #Reveal(id, [[x, y]])
            endif
        endif

    elseif 'D' == key
        var nr = str2nr(tile)
        if 0 != nr
            final todo: list<list<number>> = []
            for i in [x - 1, x, x + 1]
                for j in [y - 1, y, y + 1]
                    if hien.Inr(i, j, 0, 0, width, height) && !(x == i && y == j)
                        const at = hien.Get(id, i, j)
                        if '|' == at
                            --nr
                        elseif '#' == at
                            if -1 == field[i][j]
                                RevealAll(id)
                                dead = true
                                return false
                            endif
                            add(todo, [i, j])
                        endif
                    endif
                endfor
            endfor

            if 0 == nr
                unknown -= len(todo)
                #Reveal(id, todo)
                for [i, j] in todo
                    hien.Set(id, i, j, 0 == field[i][j] ? ' ' : string(field[i][j]))
                endfor
            endif
        endif

    elseif 'f' == key || 'F' == key
        if '#' == tile
            tile = '|'
            --count
        elseif '|' == tile
            tile = '#'
            ++count
        endif
        UpdateCount(id)

    else
        if 'h' == key
            --x
        elseif 'j' == key
            ++y
        elseif 'k' == key
            --y
        elseif 'l' == key
            ++x
        endif

        x = hien.Clamp(x, 0, width)
        y = hien.Clamp(y, 0, height)

        tile = hien.Get(id, x, y)
    endif

    echom 'unknowns:' unknown
    if 0 == unknown
        hien.Print(id, 0, height, 'All clear!')
        dead = true
    else
        hien.Print(id, x, y, tile .. '&')
    endif

    return false
enddef

def Loop(id: number): bool
    hien.Setr(id, 0, 0, width, height, '#')
    hien.Print(id, x, y, '#&')
    UpdateCount(id)
    return false
enddef

hien.Register('Minesweeper', Init, Btnp, Loop)
