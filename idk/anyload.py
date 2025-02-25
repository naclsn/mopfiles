from sys import argv
from ast import Assign, Name, Store, fix_missing_locations, parse

with open(argv[1]) as f:
    t = parse(f.read())
t.body[-1] = Assign([Name("data", Store())], t.body[-1].value)
l = {}
exec(compile(fix_missing_locations(t), argv[1], "exec"), {}, l)
data = l["data"]

