import readline as _

w = {}
while 1:
    try:
        print(eval(input("[@] "), {"__builtins__": {}}, w))
    except EOFError:
        break
    except BaseException as e:
        print(e)
