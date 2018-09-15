#!/usr/bin/python3.6
# Copyright (C) 2018 Mo Zhou <cdluminate@gmail.com>
# License: LGPL-3
import os
import numpy as np

# Generate block glyphs that can be imported by FontForge

class SVGWriter(object):
    def __init__(self, name):
        self.f = open(name, 'w')
        # SVG header
        self.head = '''\
<?xml version="1.0" encoding="utf-8" ?>
<svg version="1.1" height="1000px" width="1000px"><defs />
<g id="shapes" fill="black">'''
        # SVG block element
        self.block_template = '''
<rect stroke="black" width="250px" height="500px" x="0px" y="0px" /> '''
        # SVG tail
        self.tail = '''
</g>
</svg>
'''
        # write the header to file
        self.f.write(self.head)
    def block(self, x, y, w, h):
        self.f.write(f'<rect stroke="black" width="{w}px" height="{h}px" x="{x}px" y="{y}px" />')
    def close(self):
        self.f.write(self.tail)
        self.f.close()


class CWriter(object):
    def __init__(self, name):
        self.f = open(name, 'w')
    def block(self, i, code):
        self.f.write('{\n')
        self.f.write(f'    /* Auto-Gen, Unicode: 0x{code:x} */\n')
        self.f.write(f'    CHAFA_SYMBOL_TAG_BLOCK,\n')
        self.f.write(f'    0x{code:x},\n')
        S = np.zeros((8,8), dtype=np.bool)
        if 0b100000000 & i: S[0:3, 0:3] = True
        if 0b010000000 & i: S[3:5, 0:3] = True
        if 0b001000000 & i: S[5:8, 0:3] = True
        if 0b000100000 & i: S[0:3, 3:5] = True
        if 0b000010000 & i: S[3:5, 3:5] = True
        if 0b000001000 & i: S[5:8, 3:5] = True
        if 0b000000100 & i: S[0:3, 5:8] = True
        if 0b000000010 & i: S[3:5, 5:8] = True
        if 0b000000001 & i: S[5:8, 5:8] = True
        mask = ''.join(('X' if x else ' ') for x in S.flatten())
        self.f.write('    "' + mask + '"\n')
        self.f.write('},\n')
    def close(self):
        self.f.close()


def NinthPowerOfTwo():
    '''
    Generate 2^9 = 512 SVG files
    '''
    if not os.path.exists('512'):
        os.mkdir('512')
    for i in range(1, 512+1):
        d = SVGWriter(f'512/{i}.svg')
        if 0b100000000 & i: d.block(000, 000, 166, 333)
        if 0b010000000 & i: d.block(166, 000, 166, 333)
        if 0b001000000 & i: d.block(333, 000, 166, 333)
        if 0b000100000 & i: d.block(000, 333, 166, 333)
        if 0b000010000 & i: d.block(166, 333, 166, 333)
        if 0b000001000 & i: d.block(333, 333, 166, 333)
        if 0b000000100 & i: d.block(000, 666, 166, 333)
        if 0b000000010 & i: d.block(166, 666, 166, 333)
        if 0b000000001 & i: d.block(333, 666, 166, 333)
        d.close()
    c = CWriter("BE512.c")
    for i in range(1, 512+1):
        c.block(i, 0x10c000+i)
    c.close()


if __name__ == '__main__':

    NinthPowerOfTwo()
    #d = SVGWriter('x.svg')
    #d.block(0, 0, 250, 500)
    #d.close()
