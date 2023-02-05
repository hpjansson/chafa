#!/usr/bin/env python3
# Copyright (C) 2018, Mo Zhou <lumin@debian.org>
# LGPLv3+
import fontforge as ff
import json

N = len(json.load(open('chafa8x8.json', 'r')))

try:
    font = ff.font()
    font.clear()
    font.copyright = "(C) 2018 Mo Zhou <lumin@debian.org>, LGPLv3+"
    font.fontname = "Chafa8x8"
    font.familyname = "monospace"
    font.fullname = "Chafa8x8 block glyphs by K-Means for character art"
    font.version = "0a"
    font.encoding = 'Custom'
    print('Number of glyphs:', N)
    for i in range(N):
        glyph = font.createChar(0x100000 + i)
        glyph.importOutlines('chafa8x8_svg/%d.svg'%i)
        glyph.left_side_bearing = 0
        glyph.right_side_bearing = 0
        glyph.width = 0
        glyph.vwidth = 0
    #font.save('chafa8x8.sfd')
    font.generate('chafa8x8.ttf')
    font.close()
except Exception as e:
    print(e)
