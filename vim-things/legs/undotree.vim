
fu s:pr(nodes, cur, depth)
  let d = a:depth+1
  let ind = repeat('  ', d)
  for n in a:nodes
    cal append('$', ind.(a:cur == n.seq ? '('.n.seq.')' : n.seq))
    if has_key(n, 'alt')
      cal s:pr(n.alt, a:cur, d)
    en
  endfo
endf

fu s:eundotree(width)
  let x = undotree()
  let b = bufnr()
  exe 'abo' (0 == a:width ? 20 : a:width) 'vs'
  ene
  exe 'f' '[undotree -' bufname(b).']'
  setl bt=nofile nobl noswf nonu nornu
  cal s:pr(x.entries, x.seq_cur, 0)
  1d
  setl noma
  /(
  exe 'no <buffer> <CR> :<C-U>'.b.'bufdo undo <C-R>=getline(".")<CR><CR><C-^>:setl ma<CR>I[<Esc>/(<CR>xf)x/[<CR>r(A)<Esc>^:setl noma<CR>'
endf

com! -count Eundotree cal <SID>eundotree(<count>)

