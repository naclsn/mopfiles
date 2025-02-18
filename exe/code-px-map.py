#!/usr/bin/env python3
import argparse

p = argparse.ArgumentParser(description="""
    make colored pixel map of code using pygments""")
p.add_argument("type", nargs='?')
p.add_argument("file", type=argparse.FileType('r'))
p.add_argument("result", type=argparse.FileType('wb'))
p.add_argument("--transparent", "-t", action='store_true')
args = p.parse_args()

# honorable mention:
# pygmentize -f bmp -O font_size=1,line_numbers=False,image_pad=0,line_pad=0 <file> -o <result>

# custom formatter {{{1
try:
    from PIL import Image
    from pygments import highlight
    from pygments.formatter import Formatter
    from pygments.lexers import find_lexer_class_by_name, find_lexer_class_for_filename
    from sys import argv
    from typing import TypedDict
except ModuleNotFoundError:
    print("need pygments and PIL")
    exit(1)


class Formatter(Formatter):
    def __init__(self, transparent=False, **options):
        super().__init__(**options)
        self.transparent = transparent

    def format(self, tokensource: 'list[tuple[int, str]]', outfile):
        pixels = [[]]
        bg = (0, 0, 0, 0) if self.transparent else (*self._parse_hex(self.style.background_color), 0xff)

        for ttype, value in tokensource:
            r, g, b = self._parse_hex(self.style.style_for_token(ttype)['color'] or '#fff')
            for char in value.expandtabs():
                if '\n' == char:
                    pixels.append([])
                elif char.isspace():
                    pixels[-1].append(bg)
                else:
                    pixels[-1].append((r, g, b, 0xff))

        if not pixels[-1]:
            pixels.pop()

        w, h = max(map(len, pixels)), len(pixels)
        for line in pixels:
            pad = w - len(line)
            if 0 < pad:
                line.extend([bg]*pad)

        im = Image.new('RGBA', (w, h))
        im.putdata([px for row in pixels for px in row])
        im.show() if '<stdout>' == outfile.name else im.save(outfile, 'PNG')

    def _parse_hex(self, hex: str):
        r, g, b = hex[1:] if 4 == len(hex) else (hex[1:3], hex[3:5], hex[5:7])
        return int(r, 16), int(g, 16), int(b, 16)

    def get_style_defs(self, arg: str = ''):
        return str(self._parse_hex(hex))


# dobidoothething {{{1
Lexer = find_lexer_class_for_filename(args.file.name) or find_lexer_class_by_name(args.type)
highlight(args.file.read(), Lexer(), Formatter(args.transparent), args.result)
args.file.close()
args.result.close()
