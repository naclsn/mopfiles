" Idea:
"   - open a small buffer at the bottom with items in it (eg. list of buffers)
"   - edit
"   - apply the edits on buffer close/leave/unlist (with <C-W>_q or <Enter>)
" Limitations:
"   - line-wise only
"   - does not deal with reordering
"   - there is no context (lines are handled individually)
" Example:
"   see further in the file

" Applies the changes; only guarantee is 'ok' last. When `b:cwl_yes` is true,
" 'ok' is called with the current line as argument. Otherwise it is called
" with an empty string.
fu! cmdwinlike#Apply()
  let kn = 1 " now
  let kw = 1 " was
  let len = line('$')

  for was in b:cwl_src
    let may = []
    wh kn <= len
      let may = prop_list(kn, {'types': ['cwl-line']})
      if len(may) | brea | en

      " line was added
      cal b:cwl_mk(getline(kn))
      let kn+= 1
    endw

    if len(may) && may[0].id == kw
      let now = getline(kn)
      " line was kept, see if changed
      if was != now | cal b:cwl_mv(was, now) | en
      let kn+= 1
    el
      " line was removed
      cal b:cwl_rm(was)
    en

    let kw+= 1
  endfo

  " lines where added at the end
  if kn <= len
    for now in getline(kn, '$')
      cal b:cwl_mk(now)
    endfo
  en

  cal b:cwl_ok(b:cwl_yes ? getline('.') : '')
endf

" Actions are run in the cwl buffer, odrer for mk/mv/rm is unspecified but ok
" is always called last. If not specified, actions are no-ops. When
" `options.apply` is set to false, you may want to use your own mapping /
" autocommand to call `cmdwinlike#Apply`. Use `b:cwl_yes` too. `b:p_win_id` is
" set to the id of the window the function was called from.
"   actions.mk: func(now: string)               when a line was added
"   actions.mv: func(was: string, now: string)  when a line was modified
"   actions.rm: func(was: string)               when a line was removed
"   actions.ok: func(may: string)               for the selected line, or ''
"   options.width: number?
"   options.height: number?=10
"   options.title: string?
"   options.apply: bool?=true                   call #Apply on BufLeave
fu! cmdwinlike#Make(content_lines, actions={}, options={})
  if !has('textprop')
    try
      cal function('nvim_buf_set_extmark')
    cat
      th 'No support for neither textprop nor extmarks'
    endt
  en

  let p_win_id = win_getid()
  " TODO: use options.width; don't ':bel', let caller do
  exe 'bel' get(a:options, 'height', 10) 'sp'
  ene | setl bt=nofile nobl noswf
  exe 'f' '['.get(a:options, 'title', 'Untitled Command Line').']'
  let b:p_win_id = p_win_id

  let b:cwl_src = a:content_lines
  let lul = &ul
  se ul=-1 | cal setline(1, b:cwl_src)
  let &ul = lul

  if has('textprop')
    cal prop_type_add('cwl-line', {'bufnr': bufnr()})
    for k in range(len(b:cwl_src))
      cal prop_add(k+1, 1, {'type': 'cwl-line', 'id': k+1})
    endfo
  el
    th 'TODO: for nvim use extmarks'
  en

  let b:cwl_mk = funcref(get(a:actions, 'mk', {now -> 0}))
  let b:cwl_mv = funcref(get(a:actions, 'mv', {was, now -> 0}))
  let b:cwl_rm = funcref(get(a:actions, 'rm', {was -> 0}))
  let b:cwl_ok = funcref(get(a:actions, 'ok', {may -> 0}))
  if get(a:options, 'apply', 1)
      au BufLeave <buffer> ++once cal cmdwinlike#Apply() | bw
  en
  let b:cwl_yes = 0
  map <buffer> <silent> <CR> :<C-U>let b:cwl_yes=1<Bar>bw<CR>
endf

"---

fu s:mk(now)
  echom 'mk' a:now
endf
fu s:mv(was, now)
  echom 'mv' a:was a:now
endf
fu s:rm(was)
  echom 'rm' a:was
endf
fu s:ok(may)
  echom 'ok' a:may
endf

com! -nargs=* -complete=expression CWLtest cal cmdwinlike#Make(<args>, #{mk: funcref('s:mk'), mv: funcref('s:mv'), rm: funcref('s:rm'), ok: funcref('s:ok')})
" :CWLtest execute('ls')->split('\n')
" :CWLtest readdir('.')
" :CWLtest system('git status -s')->split('\n')

fu s:cwl_buf_ls(bang)
  let com = a:bang ? 'buffers!' : 'buffers'
  cal cmdwinlike#Make(split(execute(com), '\n'), #{rm: funcref('s:cwl_buf_rm'), ok: funcref('s:cwl_buf_ok')}, #{title: com})
  cal matchadd('Comment', '^\s*\d\+u.*$')
  let b:bang = a:bang
  let b:rm_bufs = ''
endf
fu s:cwl_buf_rm(was)
  let b:rm_bufs.= ' '.matchstr(a:was, '\d\+')
endf
fu s:cwl_buf_ok(may)
  if len(b:rm_bufs) | exe 'b'.(b:bang ? 'w' : 'd') b:rm_bufs | en
  if len(a:may) | cal win_execute(b:p_win_id, 'b '.matchstr(a:may, '\d\+')) | en
endf

com! -bang CWLbuffers cal <SID>cwl_buf_ls(<bang>0)

fu s:cwl_file_ls(path)
  let path = a:path[:-1-(a:path=~'/$')]
  cal cmdwinlike#Make(map(readdir(path), {_,v -> v.(isdirectory(path.'/'.v) ? '/' : '')}), #{mk: funcref('s:cwl_file_mk'), mv: funcref('s:cwl_file_mv'), rm: funcref('s:cwl_file_rm'), ok: funcref('s:cwl_file_ok')}, #{title: path.'/'})
  cal matchadd('Directory', '^.*/$')
  let b:path = path
endf
fu s:cwl_file_mk(now)
  if now =~ '/$'
    sil cal echoraw('mkdir '.b:path.'/'.a:now)
  el
    exe 'bad' b:path.'/'.a:now
  en
endf
fu s:cwl_file_mv(was, now)
  sil cal system('mv '.b:path.'/'.a:was.' '.b:path.'/'.a:now)
endf
fu s:cwl_file_rm(was)
  if was =~ '/$'
    sil cal system('rm -rf '.b:path.'/'.a:was)
  el
    sil cal system('rm '.b:path.'/'.a:was)
  en
endf
fu s:cwl_file_ok(may)
  if '' == a:may | retu | en
  if a:may =~ '/$'
    cal win_execute(b:p_win_id, 'CWLfiles '.b:path.'/'.a:may)
  el
    cal win_execute(b:p_win_id, 'e '.b:path.'/'.a:may)
  en
endf

com! -nargs=* -complete=file CWLfiles cal <SID>cwl_file_ls(len(<q-args>) ? <q-args> : '.')
