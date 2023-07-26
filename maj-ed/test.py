CONST = 42

def some():
    print("Hello word!")
    return True

def other():
    return "Hello other!"

# entry point of this file
# (if it might need to be `import`ed for some obscure reasons)
if '__main__' == __name__:
    input(other())
    some()

#*-*