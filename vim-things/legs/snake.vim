fu s:w(h)
    retu h
    if 'nono' == a:h
        retu nr2char(0x3003)
    elsei 'maru' == a:h
        retu nr2char(0x3007)
    en
    retu nr2char(' ' == a:h ? 0x3000 : char2nr(a:h)+0xfee0)
endf

fu s:start()
    enew
    setl bt=nofile noswf

    let b:empty = s:w(' ')
    let b:walls = map(['|', '+', '-', '+', '|', '+', '-', '+'], 's:w(v:val)')
    let b:width = 20
    let b:height = 20
    let b:xoff = 3
    let b:yoff = 3

    let m = repeat(' ', b:xoff-2)
    cal setline(1, repeat([''], b:yoff-2))
    cal append('$',         m..b:walls[3]..repeat(b:walls[2], b:width)..b:walls[1])
    cal append('$', repeat([m..b:walls[4]..repeat(b:empty,    b:width)..b:walls[0]], b:height))
    cal append('$',         m..b:walls[5]..repeat(b:walls[6], b:width)..b:walls[7])

    let b:rev_dir = {'h':'l', 'j':'k', 'k':'j', 'l':'h'}

    let b:dir = 'l'
    map <buffer> h :let b:dir = 'l' == b:dir ? b:dir : 'h'<CR>
    map <buffer> j :let b:dir = 'k' == b:dir ? b:dir : 'j'<CR>
    map <buffer> k :let b:dir = 'j' == b:dir ? b:dir : 'k'<CR>
    map <buffer> l :let b:dir = 'h' == b:dir ? b:dir : 'l'<CR>
    map <buffer> <Left> h
    map <buffer> <Down> j
    map <buffer> <Up> k
    map <buffer> <Right> l

    let b:tail = s:w('#')
    let b:head = s:w('e')
    let b:apple = s:w('@')

    let b:snake = ['l', 'j', 'j']

    cal s:goto([rand()%b:width, rand()%b:height])
    exe 'norm! r'..b:apple

    cal s:goto([0,0])
    for d in b:snake
        exe 'norm! r'..b:tail..d
    endfo
    exe 'norm! r'..b:head

    cal timer_start(120, "\<SID>step", {'repeat':-1})
endf

fu s:goto(xy)
    cal setcharpos('.', [0, b:yoff+a:xy[1], b:xoff+a:xy[0], 0])
endf

fu s:step(timer)
    if !exists('b:dir')
        cal timer_stop(a:timer)
        retu
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
            cal s:goto([rand()%b:width, rand()%b:height])
            exe 'norm! vy'
        endw
        exe 'norm! r'..b:apple
        cal setcharpos('.', p)
        exe 'norm! r'..b:tail..b:dir..'r'..b:head
        cal add(b:snake, b:dir)

    el
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
