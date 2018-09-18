import fontforge as ff

try:
    font = ff.font()
    font.encoding = 'Custom'
    for i in list(range(1, 512+1)):
        print i
        glyph = font.createChar(0x10c000 + i)
        glyph.importOutlines('512/%d.svg'%i)
    #font.generate('BE512.ttf')
    font.save('BE512.sfd')
    #font.close()
except Exception as e:
    print "cannot generate BE512.sfd"

try:
    font = ff.font()
    font.encoding = 'Custom'
    for i in range(256):
        print i
        glyph = font.createChar(0x10af00+i)
        glyph.importOutlines('BE256/%d.svg'%i)
    font.save('BE256.sfd')
except Exception as e:
    print "cannot generate BE256.sfd"
