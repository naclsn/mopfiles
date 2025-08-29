> disregarding the `del __builtins__` loophole

    $ py -Ic 'print(len(vars(__builtins__).keys()))'
    157

there are 157-7 builtin-exposed object aorn
the most obvious escapes would be
  - `__import__`
  - `exec` (`eval` I'm not sure is enough) ((tho I'm not sure `exec` is either))
  - `compile` (again, not sure is enough)

a potential dirrection would be through (un)intentional object capture;
however it is rather unlikely to find an object that would exhibit such
if all are C-level things

## retrievable

a bunch of builtin types can be retrieved:

    str = "".__class__
    type = str.__class__
    object = str.mro()[1]
    bool = type(not 1)
    int = type(1)
    float = type(.1)
    bytes = type(b"")
    list = type([])
    dict = type({})
    tuple = type(())
    set = type({1}}
    slice = type('', (), {'__getitem__': lambda self, s: type(s)})()[1:1]
    Ellipsis = ...
    False = not 1
    True = not False
    None = [].append(1)

if `BaseException` or `Exception` can somehow be retrieved:

    try:
        [][0]
    except BaseException as e:
        IndexError = type(e)
    try:
        {}[0]
    except BaseException as e:
        KeyError = type(e)
    LookupError, Exception = KeyError.mro()[1:3]
    try:
        a
    except BaseException as e:
        NameError = type(e)
    try:
        0[]
    except BaseException as e:
        TypeError = type(e)
    try:
        ().__iter__().__next__()
    except BaseException as e:
        StopIteration = type(e)

## implementable

these can at least be manually re-implemented to some extent;
however the original object may not be retrievable (or not know yet)

    def abs(x, /): return -x if x < 0 else x # except eg errors differ
    def any(...): ...
    def repr(...): ...
    def iter(...): ...
    def next(...): ...
    ...
    def issubclass(cls, class_or_tuple, /): return (
            any(issubclass(cls, ty) for ty in class_or_tuple)
            if type(class_or_tuple) != type
            else class_or_tuple in set(cls.mro())
        )
    def isinstance(obj, class_or_tuple, /): return issubclass(type(obj), class_or_tuple)
    ...
    def getattr(...): ...
    def hasattr(...): ...
    def setattr(...): ...
    def delattr(...): ...
    def vars(...): ...

## outside of builtins

as a side note, a few extras can be obtained such as from `types`:

    FunctionType = function = type(lambda: 1)
    GeneratorType = generator = type(1 for a in ())
