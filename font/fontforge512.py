import fontforge as ff

font = ff.font()
font.encoding = 'Custom'
for i in list(range(1, 512+1)):
    print i
    glyph = font.createChar(0x10c000 + i)
    glyph.importOutlines('512/%d.svg'%i)
#font.generate('BE512.ttf')
font.save('BE512.sfd')
#font.close()
