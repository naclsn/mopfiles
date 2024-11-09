import pprint
import typing


class SnapshotCreatedException(Exception):
    def __init__(self, path: str, size: int):
        super().__init__()
        self.path = path
        self.size = size

    @typing.override
    def __str__(self):
        return f"""NEW SNAPSHOT CREATED!
    {self.path} ({self.size} bytes)
    please review the file for correctness, then re-run the test
"""


def snap(path: str, obj: object, /, *, prettyprint: bool = True):
    try:
        with open(path) as f:
            return f.read()

    except FileNotFoundError:
        with open(path, "w") as f:
            if prettyprint:
                pprint.pprint(obj, stream=f)
            else:
                print(obj, file=f)
            size = f.tell()

    def _hide_locals():
        raise SnapshotCreatedException(path, size)

    _hide_locals()
    return ""


class AssertSnapMixin:
    def assertSnap(self: ..., path: str, obj: object, /, *, prettyprint: bool = True):
        hey = object()
        pmax = getattr(self, "maxDiff", hey)
        self.maxDiff = None
        self.assertEqual(
            pprint.pformat(obj) + "\n",
            snap(path, obj, prettyprint=prettyprint),
        )
        if pmax is hey:
            del self.maxDiff
        else:
            self.maxDiff = pmax
