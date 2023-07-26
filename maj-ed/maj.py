#!/usr/bin/env python
# -*- coding: utf-8 -*-

from sys import argv, platform
from shlex import split
from subprocess import PIPE, Popen

def usage(prog):
    output("Usage:", prog, "[-m ...modes] [-f <file>]")
    output("      ", prog, "[<file>]")
    output("      ", prog, "[-h]")
    output("")
    output("    -h: show this help")
    output("    <file>: file to edit")
    output("    modes: modes to load")
    output("")
    output("    in editor help is `?'")
    output("")
#end usage

def load(mode):
    r = __import__(mode)
    if hasattr(r, 'export'): niw = r.export()
    else: niw = { 'preval': lambda *_: { mode: r } }
    modes.append((mode, niw))
def unload(mode):
    for k in range(len(modes)-1, -1, -1):
        if modes[k][0] == mode: break
def repload(mode, *now):
    if now[0]:
        unload(mode)
        load(now[0])
    else:
        unload(modes[-1][0])
        load(mode)
modes = [('default_mode', { 'preval': lambda buf, sao, _: {
    '_': buf[1][sao[0]:sao[1]],
    'load': load,
    'unload': unload,
    'repload': repload,
} })]
#end modes

def display(buf, sao, hst):
    bla, out, cts, pit = buf[1][sao[0]:sao[1]], "", [], 0

    for _, it in modes:
        if 'color' in it:
            for a, b, c in it['color'](buf, sao, hst) or []:
                if int == type(c):
                    cts.append((a, str(c)))
                    cts.append((b, "0"))
                    cts.sort()

    for it, co in cts + [(len(bla), "0")]:
        out+= bla[pit:it] + "\x1b["+co+"m"
        pit = it
    for a, b in [(" ","·"),("\t"," »» "),("\n","¬\n")]:
        try: out = out.replace(a, b)
        except: pass

    output(' ' * (sao[0] - buf[1].rfind("\n", 0, sao[0]) - 1) + out)
#end display

def edit(path):
    buf, sao, hst = ["", ""], [0, 0], []

    def read(path):
        with open(path, 'r') as file:
            return file.read().replace("\r", "")
    def shell(com):
        w = "win32" in platform # XXX: awkwardly does not work without
        r = Popen(split(com), shell=w, stdout=PIPE, stderr=PIPE)
        try:
            stdout, stderr = r.communicate()
            return decode(stderr if r.poll() else stdout)
        except: r.kill()
    def script(src):
        glo = {}
        for _, it in modes:
            if 'preval' in it: glo.update(it['preval'](buf, sao, hst) or {})
        r = eval(src, glo)
        try: return "".join(str(it) for it in r)
        except: return str(r or "")
    def user(until):
        r = [input()]
        if until:
            while not r[-1].endswith(until): r.append(input())
        return '\n'.join(r)
    def save(path):
        with open(path, 'w') as file:
            file.write(buf[1])
    def insert(what):
        r = what.replace("\r", "")
        buf[1] = buf[1][:sao[0]] + r + buf[1][sao[1]:]
        sao[1] = sao[0] + len(r)
    def exact(arg):
        if arg:
            sao[0] = buf[1].find(arg, sao[0], sao[1] or None)
            sao[1] = sao[0] + len(arg)
        else: sao[0], sao[1] = 0, len(buf[1])
    def start(niw):
        sao[0] = 0 if niw < 0 else niw
        if sao[1] < sao[0]: sao[1] = sao[0]
    def stop(niw):
        sao[1] = 0 if niw < 0 else niw
        if sao[1] < sao[0]: sao[0] = sao[1]
    def store():
        buf[0] = buf[1][sao[0]:sao[1]]
    #end utils

    try: buf[1] = read(path)
    except: pass
    for _, it in modes:
        if 'editing' in it: it['editing'](buf, sao, hst, path, load, unload)

    while True:
        try:
            com = input("[%d:%d]%d|" % (sao[0], sao[1], sao[1]-sao[0]))

            if '?' == com[:1]:
                output("List of commands:")
                output("")
                output("    q  -  quit")
                output("    w  -  write [<file>]")
                output("    e  -  edit [<file>]")
                output("    r  -  read [<file>]")
                output("    !  -  system <command>")
                output("    ;  -  python <script>")
                output("    i  -  insert [<until>]")
                output("    x  -  cut selection")
                output("    c  -  copy selection")
                output("    v  -  past override selection")
                output("    =  -  selection to exact match or whole file")
                output("    (  -  selection begin forward to")
                output("    )  -  selection end forward to")
                output("    [  -  selection begin backward to")
                output("    ]  -  selection end backward to")
                output("    {  -  selection begin move by")
                output("    }  -  selection end move by")
                output("    *  -  repeat [<n> default 1] last command(s)")
                output("")
                output("    no command: displays the selection")
                output("")
                continue

            do = {
                '': lambda _: display(buf, sao, hst)
                , 'q': lambda arg: exit(arg or 0)
                , 'w': lambda arg: save(arg or path)
                , 'e': lambda arg: edit(arg or path)
                , 'r': lambda arg: insert(read(arg or path))
                , '!': lambda arg: insert(shell(arg) or arg)
                , ';': lambda arg: insert(script(arg))
                , 'i': lambda arg: insert(user(arg))
                , 'x': lambda _: (store(),insert(""))
                , 'c': lambda _: store()
                , 'v': lambda _: insert(buf[0])
                , '=': exact
                , '(': lambda arg: start(buf[1].find(arg or "\n",sao[0]+len(arg or "\n")))
                , ')': lambda arg: stop(buf[1].find(arg or "\n",sao[1]+len(arg or "\n"))+len(arg or "\n"))
                , '[': lambda arg: start(buf[1].rfind(arg or "\n",0,sao[0]-len(arg or "\n")))
                , ']': lambda arg: stop(buf[1].rfind(arg or "\n",0,sao[1]-len(arg or "\n"))+len(arg or "\n"))
                , '{': lambda arg: start(sao[0]+int(arg) if arg else 0)
                , '}': lambda arg: stop(sao[1]+int(arg) if arg else len(buf[1]))
            }
            def repeat(arg, skip=0):
                for com in hst[-int(arg or 1)-skip:-skip or len(hst)]:
                    skip+= 1
                    if '*' != com[:1]: do[com[:1].lower()](com[1:])
                    else: do[com[:1].lower()](com[1:], skip)
            do['*'] = repeat

            do[com[:1].lower()](com[1:])
            hst.append(com)

        except KeyboardInterrupt: break
        except Exception as err: output('?', err)
    #end while True
#end edit

if '__main__' == __name__:
    try:
        output = eval("print")
        decode = lambda b: b.decode()
    except:
        def output(*va): print(" ").join(str(v) for v in va)
        input = raw_input
        decode = lambda b: ""+b
    if 1 < len(argv) and "-h" == argv[1].lower():
        usage(argv[0])
        exit(1)
    else:
        if 1 < len(argv) and "-m" == argv[1]:
            try:
                for it in argv[2:]:
                    if "-f" == it: break
                    load(it)
            except Exception as err: output('?', err)
        edit(1 < len(argv) and argv[-1] or "idk.txt")
#end if main

#*-*