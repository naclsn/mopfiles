#!/usr/bin/python3

MH_STANDBY  = "STANDBY" # initial state
MH_RUNNING  = "RUNNING" # found begin, started
MH_LOCKED   = "LOCKED" # found end, locked on result
MH_DISABLED = "DISABLED" # idk

class Machine:
    def __init__(self, begin, end):
        self.begin = begin
        self.begin1 = 0
        self.begin2 = 0
        self.end = end
        self.end1 = 0
        self.end2 = 0
        self.status = MH_STANDBY

    def __repr__(self):
        return "Machine(%r, %r) [%s]" % (
            self.begin, self.end,
            self.status
        )

LVL_CHAR   = "CHAR"
LVL_WORD   = "WORD"
LVL_OBJECT = "OBJECT"

def forward(text, at, level, machines):
    if LVL_CHAR == level: return at+1, at+2

    if LVL_WORD == level:
        begin = at
        prev = text[begin]
        while True:
            begin+= 1
            if len(text) == begin: return at, begin
            curr = text[begin]
            if not prev.isalnum() and curr.isalnum():
                # found begin
                end = begin
                prev = curr
                while True:
                    end+= 1
                    if len(text) == end: return begin, end
                    curr = text[end]
                    if prev.isalnum() and not curr.isalnum():
                        # found end
                        return begin, end
                    prev = curr
            prev = curr
        # unreachable
    #end if LVL_WORD

    # reset machines
    for it in machines:
        # start every machines
        it.statue = MH_STANDBY
        # from the earliest (a)
        it.begin1 = it.begin2 = 0
        it.end1 = it.end2 = 0

    # done when a machine is locked and no
    # machine with higher priority is running
    # (or end of text)
    while at < len(text):
        char = text[at]
        at+= 1
        most = len(machines)

        for k, it in enumerate(machines):

            if MH_STANDBY == it.status:
                if char == it.begin[it.begin2]:
                    it.begin2+= 1
                    if len(it.begin) == it.begin2:
                        # begin mached, now running
                        it.status = MH_RUNNING
                        # save where begin's open and close are
                        it.begin1 = at - it.begin2
                        it.begin2 = at
                else:
                    # not what expected, back to open
                    it.begin2 = 0
                continue

            if MH_RUNNING == it.status:
                if char == it.end[it.end2]:
                    it.end2+= 1
                    if len(it.end) == it.end2:
                        # locked onto a result
                        it.status = MH_LOCKED
                        # save where end's open and close are
                        it.end1 = at - it.end2
                        it.end2 = at
                else:
                    # not what expected, back to open
                    it.end2 = 0
                # indicate still running to least prio
                most = min(most, k)
                #fallthrough

            if MH_LOCKED == it.status:
                most = min(most, k)
                # if this is the most prio to have locked
                if most == k: return it.begin1, it.end2

        #end for machines
    #end while True
    return at, at+1

if '__main__' == __name__:
    # from sys import argv
    # self = argv.pop(0)
    # with open(argv and argv[0] or self, 'r') as file:
    #     text = file.read()
    text = """local argparse = require('utils".argparse')
-- bla = { "ye's", "\e[36mno" }
debug("argparse["..something.."] = "..argparse[something])
argparse.configure("../cli_args.txt")

--[[
    user should provide only the
    necessary arguments,
    extraneous are considered erroneous
]]
if not argparse.parse(args) then
    print(argparse.help())
    exit(2)
end

for k,v in pairs(argparse)
    argactions[k](v)
end
"""
    at = 13

    machines = [
        Machine("--[[", "]]"),
        Machine("--", "\n"),
        # Machine("\"", "\""),
        # Machine("'", "'"),
        Machine("[", "]"),
        Machine("{", "}"),
        Machine("(", ")"),
        Machine("if", "then"),
        Machine("then", "end"),
        Machine("for", "end"),
    ]

    a, b = forward(text, at, LVL_OBJECT, machines)

    print(
        text[:a],
        "\x1b[7m",
        text[a:b],
        "\x1b[27m",
        text[b:],
        sep=""
    )
