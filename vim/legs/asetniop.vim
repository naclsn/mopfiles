aug asetniop
  au!
  au InsertEnter * se to tm=100
  au InsertLeave * se noto
aug END
lmapc
let km =<< trim KEYMAP
a a
b fj
c sf
d ds
e d
f fa
g fl
h jk
i k
j js
k ks
l lk
m j;
n j
o l
p ;
q aj
r fd
s s
t f
u jl
v fk
w sa
x da
y jd
z ak
! ;k
@ :K
' ;d
" :D
<BS> ;f
; ;l
: :L
, kd
< KD
. ls
> LS
/ ;a
? :A
( al
[ AL
) ;s
] :S
- ld
_ LD
KEYMAP
"<Tab> asdf
"<CR> ;lkj
for kv in km
  let [k, v] = split(kv)
  exe 'lm' v k
  exe 'lm' v[1].v[0] k
  let u = toupper(k)
  if u != k
    let w = (';'==v[0]?':':toupper(v[0])).(';'==v[1]?':':toupper(v[1]))
    exe 'lm' w u
    exe 'lm' w[1].w[0] u
  en
endfo
unl km k v w
