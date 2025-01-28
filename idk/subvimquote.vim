

if has('nvim')
  aug subvimquote
    au!
    au TermRequest * cal Tapi_Subvimquote(v:termrequest[5:]->json_decode()[2][0])
  aug END
en

fu Tapi_Subvimquote(lines)
  let @" = a:lines->join('')
endf

svq() {
  printf '\e]51;["call","Tapi_Subvimquote",'
  cat "$@" |`command -v python3 || echo python` -c 'import json, sys; print(json.dumps(sys.stdin.readlines()))'
  printf ']\e\' # or ^G
}

fu Svq(lines)
  exe "set t_ts=\<Esc>]51; t_fs=\<Esc>\\"
  let &titlestring = '["call","Tapi_Subvimquote",'..a:lines->json_encode()..']'
  redraw
  set t_ts& t_fs&
endf
