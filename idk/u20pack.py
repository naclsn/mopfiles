"""pack 5 bytes in 2 unicode codepoints (on 20 bits)"""

import sys

if "-d" in sys.argv:
    for large in iter(lambda: sys.stdin.read(2), ""):
        num = ord(large[0]) | ord(large[1:] or "\0") << 20
        for _ in range(5):
            sys.stdout.write(chr(num & 0xFF))
            num >>= 8

else:
    for pack in iter(lambda: sys.stdin.read(5), ""):
        ords = [ord(byte) for byte in pack] + [0] * 4
        assert all(byte < 256 for byte in ords)
        a, b, c, d, e = ords[:5]

        sys.stdout.write(chr(a | b << 8 | (c & 0x0F) << 16))
        sys.stdout.write(chr((c & 0xF0) >> 4 | d << 4 | e << 12))
