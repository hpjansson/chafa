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
        # SVG tail
        self.tail = ''' </g> </svg> '''
        # write the header to file
        self.f.write(self.head)
    def block(self, x, y, w, h):
        self.f.write(
f'<rect stroke="black" width="{w}px" height="{h}px" x="{x}px" y="{y}px" />')
    def genBE512(self, i, code):
        if 0b100000000 & i: self.block(000, 000, 166, 333)
        if 0b010000000 & i: self.block(166, 000, 166, 333)
        if 0b001000000 & i: self.block(333, 000, 166, 333)
        if 0b000100000 & i: self.block(000, 333, 166, 333)
        if 0b000010000 & i: self.block(166, 333, 166, 333)
        if 0b000001000 & i: self.block(333, 333, 166, 333)
        if 0b000000100 & i: self.block(000, 666, 166, 333)
        if 0b000000010 & i: self.block(166, 666, 166, 333)
        if 0b000000001 & i: self.block(333, 666, 166, 333)
    def genBE256(self, i, code):
        if 0b10000000 & i: self.block(000, 000, 250, 250)
        if 0b01000000 & i: self.block(250, 000, 250, 250)
        if 0b00100000 & i: self.block(000, 250, 250, 250)
        if 0b00010000 & i: self.block(250, 250, 250, 250)
        if 0b00001000 & i: self.block(000, 500, 250, 250)
        if 0b00000100 & i: self.block(250, 500, 250, 250)
        if 0b00000010 & i: self.block(000, 750, 250, 250)
        if 0b00000001 & i: self.block(250, 750, 250, 250)
    def close(self):
        self.f.write(self.tail)
        self.f.close()


class CWriter(object):
    def __init__(self, name):
        self.f = open(name, 'w')
    def genBE512(self, i, code):
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
    def genBE256(self, i, code):
        self.f.write('{\n')
        self.f.write(f'    /* Auto-Generated, BE256, Unicode: 0x{code:x} */\n')
        self.f.write(f'    CHAFA_SYMBOL_TAG_BLOCK,\n')
        self.f.write(f'    0x{code:x},\n')
        S = np.zeros((8,8), dtype=np.bool)
        if 0b10000000 & i: S[0:4, 0:2] = True
        if 0b01000000 & i: S[4:8, 0:2] = True
        if 0b00100000 & i: S[0:4, 2:4] = True
        if 0b00010000 & i: S[4:8, 2:4] = True
        if 0b00001000 & i: S[0:4, 4:6] = True
        if 0b00000100 & i: S[4:8, 4:6] = True
        if 0b00000010 & i: S[0:4, 6:8] = True
        if 0b00000001 & i: S[4:8, 6:8] = True
        mask = ''.join(('X' if x else ' ') for x in S.flatten())
        self.f.write('    "' + mask + '"\n')
        self.f.write('},\n')
    def close(self):
        self.f.close()


def EigthPowerOfTwo():
    '''
    Generate 2^8 = 256 SVG files for Block Extended 256 (BE256)
    BE256's resolution is 2*4
    '''
    if not os.path.exists('BE256'):
        os.mkdir('BE256')
    for i in range(256):
        d = SVGWriter(f'BE256/{i}.svg')
        d.genBE256(i, 0x10af00+i)
        d.close()
    c = CWriter('BE256.c')
    for i in range(256):
        c.genBE256(i, 0x10af00+i)
    c.close()


def NinthPowerOfTwo():
    '''
    Generate 2^9 = 512 SVG files for Block Extended 512 (BE512)
    BE512's resolution is 3*3
    '''
    if not os.path.exists('512'):
        os.mkdir('512')
    for i in range(1, 512+1):
        d = SVGWriter(f'512/{i}.svg')
        d.genBE512(i, 0x10c000+i)
        d.close()
    c = CWriter("BE512.c")
    for i in range(1, 512+1):
        c.genBE512(i, 0x10c000+i)
    c.close()


if __name__ == '__main__':

    EigthPowerOfTwo()
    #NinthPowerOfTwo()
