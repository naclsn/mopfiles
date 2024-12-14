function = type(lambda: 0)
getset_descriptor = type(function.__code__)
member_descriptor = type(function.__globals__)
safe_property_descriptor_types = (getset_descriptor, member_descriptor)

del function, getset_descriptor, member_descriptor


#def should_process_attr(obj: object, name: str):
#    return "builtins" != type(getattr(obj, name, None)).__module__


def props(obj: object):
    return {
        name
        for cls in type(obj).__mro__
        for name, prop in vars(cls).items()
        if isinstance(prop, safe_property_descriptor_types)
    }
