
" splore (file tree) {{{1
fu s:tree(dir, depth)
  let dir = '/' != a:dir[strlen(a:dir)-1] ? a:dir.'/' : a:dir
  let depth = a:depth+1
  if 9 < depth | retu | en
  let p = repeat(repeat(' ', &ts), depth)
  for e in readdir(dir, {e -> e[0] != '.'})
    if isdirectory(dir.e) && '.git' != e && '.svn' != e
      cal append('$', p.e.'/ --'.depth.' ('.dir.e.'/)')
      cal s:tree(dir.e, depth)
    el
      cal append('$', p.e.('x' == getfperm(dir.e)[2] ? '*' : '').' -- ('.dir.e.')')
    en
  endfo
endf
fu s:plore(dir)
  let d = trim(expand(a:dir), '/\', 2).'/'
  try
    exe 'b' 'splore://'.d | retu
  cat
    ene | exe 'f' 'splore://'.d
  endt
  setl bt=nowrite cole=3 et fdm=marker fdt=matchstr(getline(v:foldstart),'.*\\ze\ --').'\ ('.(v:foldend-v:foldstart).'\ items)' fmr=/\ --,}}} inex=matchstr(getline('.'),'\ (\\zs.*\\ze)$') noswf sw=0 ts=3
  "TODO: bt=acwrite then BufWriteCmd probably
  cal setline(1, d)
  cal s:tree(resolve(getcwd().'/'.d), 0)
  sy match Conceal / --.*$/ conceal
  sy match Statement "^ *[^/ ]\+/"
  sy match Structure /^ \+[^* ]\+\*/
  nn <buffer> gs :<C-U>sp <cfile><CR>
  nn <buffer> gv :<C-U>vs <cfile><CR>
endf
com! -complete=dir -nargs=1 Splore cal <SID>plore(<q-args>)
exe 'hi Folded ctermbg=NONE guibg=NONE' execute('hi Statement')[20:]

