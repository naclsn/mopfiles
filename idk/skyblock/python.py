from codeop import compile_command
from types import FunctionType
import readline as _

g = {"__builtins__": {}}
while 1:

    try:
        i = input("[@] ") + "\n"
        c = compile_command(i)
        if c is None:
            for l in iter(lambda: input("    "), ""):
                i += l + "\n"
            c = compile_command(i)
            assert c

        r = FunctionType(c, g)()
        r is not None and print(r)

    except EOFError:
        break
    except BaseException as e:
        print("!!!", e)
