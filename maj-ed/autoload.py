EXT_MAP = {
    'py': "python",
    'pyw': "python",
}

current = None

def editing(buf, sao, hst, path, load, unload):
    global current

    ext = path.rsplit('.', 1)[1]
    niw = EXT_MAP.get(ext, None)

    if niw != current:
        if current: unload(current)
        if niw: load(niw)
    current = niw

export = lambda: { 'editing': editing }
