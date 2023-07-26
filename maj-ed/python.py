from re import compile, finditer

STRING = 31     # Red
COMMENT = 32    # Green
NUMBER = 33     # Yellow
CONSTANT = 34   # Blue
KEYWORD = 35    # Magenta
IDENTIFIER = 36 # Cyan

TOKEN = [
    (STRING, r"([rfbu]?(?:'(?:\\'|.)*?'|\"(?:\\\"|.)*?\"))"),
    (COMMENT, r"(#.*)"),
    (NUMBER, r"\b([0-9]+)\b"),
    (CONSTANT, r"\b(False|None|True)\b"),
    (KEYWORD, r"\b(class|from|or|continue|global|pass|def|if|raise|and|del|import|return|as|elif|in|try|assert|else|is|while|async|except|lambda|with|await|finally|nonlocal|yield|break|for|not)\b"),
    (IDENTIFIER, r"([A-Z_a-z][0-9A-Z_a-z]*)"),
]

COMPILED = compile("|".join(r for _, r in TOKEN))

def grow(buf, sao, hst):
    lsa, lso = buf[1].find("\n", sao[0], sao[1]), buf[1].rfind("\n", sao[0], sao[1])
    ind = min(len(l)-len(l.lstrip) for l in buf[1][lsa:lso].splitlines())

def preval(buf, sao, hst):
    return {
        'grow': lambda: "TODO: implement grow",
        'shrink': lambda: "TODO: implement shrink",
    }

def color(buf, sao, hst):
    r = []
    sa, so = sao
    for it in finditer(COMPILED, buf[1]):
        za, zo = it.span(0)

        if za < sa: continue
        if so < zo: break
        if za < sa: za = sa
        if so < zo: zo = so

        k, gr = 0, it.groups()
        while not gr[k]: k+= 1
        r.append((za-sa, zo-sa, TOKEN[k][0]))
    return r

export = lambda: { 'preval': preval, 'color': color }

#*-*