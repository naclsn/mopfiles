fu s:khor_ft()
  setl com=b:; def=define\|rules lisp

  sy cluster khorAtom contains=khorNumber,khorString,khorRuleToken,khorSymbol
  sy match   khorComment /;.*/

  sy match   khorRuleToken /$[^ \t\r\n"();]\+/
  sy match   khorSymbol /[^ \t\r\n"();]\+/
  sy match   khorNumber /-\?\d\+/
  sy region  khorString start=/"/ skip=/\\./ end=/"/

  sy cluster khorFirst contains=khorKeyword,khorOperator,khorRuleToken,khorSymbol
  sy keyword khorKeyword define halt if lambda list progn rules contained
  sy match   khorOperator "\V<\|>\|<=\|>=\|=\|+\|-\|*\|/\|and\|or\|.." contained
  sy match   khorOpen /(/ nextgroup=@khorFirst

  hi def link khorComment   Comment
  hi def link khorNumber    Number
  hi def link khorString    String
  hi def link khorList      Special
  hi def link khorKeyword   Keyword
  hi def link khorOperator  Operator
  hi def link khorRuleToken Identifier
  hi def link khorUnmatched Error
endf

aug filetypedetect
  au BufRead,BufNewFile *.khor se ft=khor
aug END
au FileType khor cal <SID>khor_ft()
