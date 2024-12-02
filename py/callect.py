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

""" attributes and descriptors {{{1
>>> class get:
...     def __get__(self, obj, ty=None):
...             print("get", self, obj)
...             return 42
... 
>>> class prout:
...     idk = get()
... 
>>> prout.__dict__
mappingproxy({'__module__': '__main__', 'idk': <__main__.get object at 0x7e7421e255e0>, '__dict__': <attribute '__dict__' of 'prout' objects>, '__weakref__': <attribute '__weakref__' of 'prout' objects>, '__doc__': None})
>>> prout.idk
get <__main__.get object at 0x7e7421e255e0> None
42
>>> type(prout.idk)
get <__main__.get object at 0x7e7421e255e0> None
<class 'int'>
>>> prout.__dict__['idk']
<__main__.get object at 0x7e7421e255e0>
>>> getattr(prout, 'idk')
get <__main__.get object at 0x7e7421e255e0> None
42
"""

""" dict and slots {{{1
>>> class bidoof:
...     __slots__ = ['name', 'size']
... 
>>> dir(bidoof)
['__class__', '__delattr__', '__dir__', '__doc__', '__eq__', '__format__', '__ge__', '__getattribute__', '__getstate__', '__gt__', '__hash__', '__init__', '__init_subclass__', '__le__', '__lt__', '__module__', '__ne__', '__new__', '__reduce__', '__reduce_ex__', '__repr__', '__setattr__', '__sizeof__', '__slots__', '__str__', '__subclasshook__', 'name', 'size']
>>> bidoof.__dict__
mappingproxy({'__module__': '__main__', '__slots__': ['name', 'size'], 'name': <member 'name' of 'bidoof' objects>, 'size': <member 'size' of 'bidoof' objects>, '__doc__': None})
>>> bidoof.__slots__
['name', 'size']
>>> bidoof().__dict__
Traceback (most recent call last):
  File "<stdin>", line 1, in <module>
AttributeError: 'bidoof' object has no attribute '__dict__'. Did you mean: '__dir__'?
>>> bidoof().__slots__
['name', 'size']
>>> dir(bidoof())
['__class__', '__delattr__', '__dir__', '__doc__', '__eq__', '__format__', '__ge__', '__getattribute__', '__getstate__', '__gt__', '__hash__', '__init__', '__init_subclass__', '__le__', '__lt__', '__module__', '__ne__', '__new__', '__reduce__', '__reduce_ex__', '__repr__', '__setattr__', '__sizeof__', '__slots__', '__str__', '__subclasshook__', 'name', 'size']
>>> vars(bidoof())
Traceback (most recent call last):
  File "<stdin>", line 1, in <module>
TypeError: vars() argument must have __dict__ attribute
"""

""" slots/dict for class methods {{{1
>>> class a:
...     def any(*_): ...
...     @staticmethod
...     def sta(*_): ...
...     @classmethod
...     def cla(*_): ...
... 
>>> a.sta
<function a.sta at 0x79bcbe1bf560>
>>> a.cla
<bound method a.cla of <class '__main__.a'>>
>>> a.any
<function a.any at 0x79bcbe1bf6a0>
>>> a.cla.__self__
<class '__main__.a'>
>>> a.any.__self__
Traceback (most recent call last):
  File "<stdin>", line 1, in <module>
AttributeError: 'function' object has no attribute '__self__'. Did you mean: '__call__'?
>>> vars(a.any)
{}
>>> vars(a.sta)
{}
>>> vars(a.cla)
{}
>>> vars(a)
mappingproxy({'__module__': '__main__', 'sta': <staticmethod(<function a.sta at 0x79bcbe1bf560>)>, 'cla': <classmethod(<function a.cla at 0x79bcbe1bf600>)>, 'any': <function a.any at 0x79bcbe1bf6a0>, '__dict__': <attribute '__dict__' of 'a' objects>, '__weakref__': <attribute '__weakref__' of 'a' objects>, '__doc__': None})
"""

# }}}

function = type(lambda: 0)

getset_descriptor = type(function.__code__)
member_descriptor = type(function.__globals__)


def gather(obj: object, safe_properties=(getset_descriptor, member_descriptor)):
    """smurter version of dir"""
    pass


def callect(root: object, seen: set[int], **kwargs):
    for name in getattr(root, '__slots__', ()):
        val = getattr(root, name)
        if not id(val) in seen:
            seen.add(id(val))
            yield val
            yield from callect(val, seen)

    for val in getattr(root, '__dict__', {}).values():
        if not id(val) in seen:
            seen.add(id(val))
            yield val
            yield from callect(val, seen)

    val = root.__class__
    if not id(val) in seen:
        seen.add(id(val))
        yield val
        yield from callect(val, seen)

    if isinstance(root, type):
        for val in root.__bases__:
            if not id(val) in seen:
                seen.add(id(val))
                yield val
                yield from callect(val, seen)

    # __closure__, __bidoof__, ...
    # maybe could at least fastforward all the `member_descriptor` ones,
    # but even just __class__ and __bases__ are `getset_descriptor`s!
