fu s:fwch(h)
    retu nr2char(' ' == a:h ? 0x3000 : char2nr(a:h)+0xfee0)
endf

fu s:goto(x, y)
    cal setcharpos('.', [0, b:yoff+a:y, b:xoff+a:x, 0])
endf

fu s:start()
    ene
    setl bh=wipe bt=nofile nobl nocuc nocul noswf

    let b:empty = s:fwch(' ')
    let b:walls = map(['|', '+', '-', '+', '|', '+', '-', '+'], 's:fwch(v:val)')
    let b:width = 20
    let b:height = 20
    let b:xoff = 3
    let b:yoff = 3
    let b:frames = 0

    let m = repeat(' ', b:xoff-2)
    cal setline(1, repeat([''], b:yoff-2))
    cal append('$',         m..b:walls[3]..repeat(b:walls[2], b:width)..b:walls[1])
    cal append('$', repeat([m..b:walls[4]..repeat(b:empty,    b:width)..b:walls[0]], b:height))
    cal append('$',         m..b:walls[5]..repeat(b:walls[6], b:width)..b:walls[7])

    let b:rev_dir = {'h':'l', 'j':'k', 'k':'j', 'l':'h'}

    let b:dir = 'l'
    let b:pend = []
    map <buffer> <silent> h :cal add(b:pend, 'h')<CR>
    map <buffer> <silent> j :cal add(b:pend, 'j')<CR>
    map <buffer> <silent> k :cal add(b:pend, 'k')<CR>
    map <buffer> <silent> l :cal add(b:pend, 'l')<CR>
    map <buffer> <silent> <Left> h
    map <buffer> <silent> <Down> j
    map <buffer> <silent> <Up> k
    map <buffer> <silent> <Right> l

    let b:tail = s:fwch('o')
    let b:head = s:fwch('e')
    let b:apple = s:fwch('@')

    let b:snake = ['l', 'j']

    cal s:goto(rand()%b:width, rand()%b:height)
    exe 'norm! r'..b:apple

    cal s:goto(0, 0)
    for d in b:snake
        exe 'norm! r'..b:tail..d
    endfo
    exe 'norm! r'..b:head

    cal timer_start(100, function("\<SID>step"), {'repeat':-1})
endf

fu s:step(timer)
    if !exists('b:dir')
        cal timer_stop(a:timer)
        retu
    en

    wh len(b:pend) && b:pend[0] == b:rev_dir[b:dir]
        cal remove(b:pend, 0)
    endw
    if len(b:pend)
        let b:dir = remove(b:pend, 0)
        wh len(b:pend) && b:pend[0] == b:dir
            cal remove(b:pend, 0)
        endw
    en

    exe 'norm! '..b:dir..'vy'
    if b:empty == @"
        cal add(b:snake, b:dir)
        for it in reverse(copy(b:snake))
            let ti = b:rev_dir[it]
            exe 'norm! '..ti..'vy'..it..'vp'..ti
        endfo
        exe 'norm! r'..b:empty..join(b:snake, '')
        cal remove(b:snake, 0)

    elsei b:apple == @"
        exe 'norm! '..b:rev_dir[b:dir]
        let p = getcharpos('.')
        wh b:empty != @"
            cal s:goto(rand()%b:width, rand()%b:height)
            exe 'norm! vy'
        endw
        exe 'norm! r'..b:apple
        cal setcharpos('.', p)
        exe 'norm! r'..b:tail..b:dir..'r'..b:head
        cal add(b:snake, b:dir)

    el
        echom 'Score:' len(b:snake)*17
        cal timer_stop(a:timer)
        unm <buffer> h
        unm <buffer> j
        unm <buffer> k
        unm <buffer> l
        unm <buffer> <Left>
        unm <buffer> <Down>
        unm <buffer> <Up>
        unm <buffer> <Right>
    en
endf

com! Snake cal s:start()
