from codeop import compile_command
from types import FunctionType
import readline as _


g = {"__builtins__": {}}

# TODO(wip): trying to circumvent the `del __builtins__` loophole)
# class G(dict):
#     def __getitem__(self, key):
#         return {} if "__builtins__" == key else super().__getitem__(key)
# g = G()

while 1:

    try:
        i = input("[@] ") + "\n"
        if "???" == i.strip():
            print(g)
            g["_"] = dict(g)
            continue

        c = compile_command(i)
        if c is None:
            for l in iter(lambda: input("    "), ""):
                i += l + "\n"
            c = compile_command(i)
            assert c

        r = FunctionType(c, g)()
        if r is not None:
            print(r)
            g["_"] = r

    except (EOFError, KeyboardInterrupt):
        break
    except BaseException as e:
        print("!!!", e)
