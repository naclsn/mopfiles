"""

## Scoping:

will not handle __weakref__
will not handle __slots__ being a one-time iterator (typ. generator expression)

## Notes:

Python objects that cannot have attributes (ie neither __dict__ nor __slots__):
* instances of builtins accessible types (23):
    bool bytearray bytes complex dict enumerate filter float frozenset int list
    map memoryview object property range reversed set slice str super tuple zip
* instances of builtins hidden types (17):
    async_generator builtin_function_or_method callable_iterator cell
    classmethod_descriptor code coroutine ellipsis frame generator
    getset_descriptor mappingproxy member_descriptor method-wrapper
    method_descriptor traceback wrapper_descriptor

"""

function = type(lambda: 0)

getset_descriptor = type(function.__code__)
member_descriptor = type(function.__globals__)

safe_property_descriptor_types = (getset_descriptor, member_descriptor)


def gather(obj: object) -> set[str]:
    """smurter version of vars

    BUT! in the names it returns, getattr(obj, name) might not always work
    eg if obj is of a class with __slots__ but these have not been filled yet

    >>> sorted(gather(1))
    ['__class__', 'denominator', 'imag', 'numerator', 'real']

    >>> class c1: ...
    >>> sorted(gather(c1()))
    ['__class__', '__dict__', '__weakref__']

    >>> class c2: __slots__ = ['a', 'b']
    >>> a = c2()
    >>> sorted(gather(a))
    ['__class__', 'a', 'b']
    >>> hasattr(a, 'a')
    False
    >>> a.a = 'aa'
    >>> hasattr(a, 'a')
    True

    >>> class c3(c1, c2): __slots__ = ['a', 'c']
    >>> sorted(gather(c3()))
    ['__class__', '__dict__', '__weakref__', 'a', 'b', 'c']

    >>> a = c3()
    >>> a.some = 'thing'
    >>> sorted(gather(a))
    ['__class__', '__dict__', '__weakref__', 'a', 'b', 'c', 'some']
    >>> c3.coucou = 42
    >>> sorted(gather(a))
    ['__class__', '__dict__', '__weakref__', 'a', 'b', 'c', 'some']
    >>> sorted(gather(c3))
    ['__abstractmethods__', '__annotations__', '__base__', '__bases__', '__basicsize__', '__class__', '__dict__', '__dictoffset__', '__doc__', '__flags__', '__itemsize__', '__module__', '__mro__', '__name__', '__qualname__', '__slots__', '__text_signature__', '__weakref__', '__weakrefoffset__', 'a', 'c', 'coucou']
    """
    return {
        # NOTE: __slots__ declares descriptors in the class, so it's already caught below;
        #       we can do this because the source for the name doesn't matter at this point
        # *(
        #     name
        #     for cls in type(obj).__mro__
        #     if "__slots__" in vars(cls)
        #     for name in vars(cls)["__slots__"]
        # ),
        *getattr(obj, "__dict__", {}),
        *(
            name
            for cls in type(obj).__mro__
            # XXX: should it be gather, or just vars? (what about if metaclasses)
            for name, var in vars(cls).items()
            # XXX: or maybe type(var) in safe_property_descriptor_types?
            if isinstance(var, safe_property_descriptor_types)
        ),
    }

def bidoof(obj: object):
    return type(obj) in [bool,bytes,complex,float,int,str]

def isbasicbuiltin(obj: object):
    return type(obj) in [bool,bytearray,bytes,complex,dict,enumerate,filter,float,frozenset,int,list,map,memoryview,object,property,range,reversed,set,slice,str,super,tuple,zip]

def ishiddenbuiltin(obj: object):
    return idk
    #return type(obj) in [async_generator,builtin_function_or_method,callable_iterator,cell,classmethod_descriptor,code,coroutine,ellipsis,frame,generator,getset_descriptor,mappingproxy,member_descriptor,method-wrapper,method_descriptor,traceback,wrapper_descriptor]

def isbuiltin(obj: object):
    return isbasicbuiltin(obj) or ishiddenbuiltin(obj)

def canattr(obj: object):
    return '__slots__' in vars(type(obj)) or type(getattr(obj, '__dict__', None)) is dict


def callect(root: object, seen: set[int]):
    """depth first for now-"""
    missing = object()
    for name in gather(root):
        val = getattr(root, name, missing)
        if val is not missing and id(val) not in seen:
            seen.add(id(val))
            yield val
            yield from callect(val, seen)


if "__main__" == __name__:
    from doctest import testmod

    testmod()
