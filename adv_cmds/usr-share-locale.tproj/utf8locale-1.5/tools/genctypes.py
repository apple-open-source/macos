#!/usr/bin/env python
# ex: ts=8 sts=4 et
#
# Copyright (c) 2002 FreeBSD Project. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# Written by Hye-Shik Chang <perky@FreeBSD.org>, 21 June 2002
#
# $Id: genctypes.py,v 1.2 2003/05/20 23:05:10 emoy Exp $
#
# ftp://ftp.unicode.org/Public/UNIDATA/{Blocks,EastAsianWidth,UnicodeData}.txt

import re, sys
stderr = sys.stderr

ALPHA       = 0x00001
CONTROL     = 0x00002
DIGIT       = 0x00004
GRAPH       = 0x00008
LOWER       = 0x00010
PUNCT       = 0x00020
SPACE       = 0x00040
UPPER       = 0x00080
XDIGIT      = 0x00100
BLANK       = 0x00200
PRINT       = 0x00400
IDEOGRAM    = 0x00800
SPECIAL     = 0x01000
PHONOGRAM   = 0x02000

SWIDTH0     = 0x04000
SWIDTH1     = 0x08000
SWIDTH2     = 0x10000
SWIDTH3     = 0x20000

BSD_CTYPES = (
    (ALPHA,     'ALPHA    '),
    (CONTROL,   'CONTROL  '),
    (DIGIT,     'DIGIT    '),
    (GRAPH,     'GRAPH    '),
    (LOWER,     'LOWER    '),
    (PUNCT,     'PUNCT    '),
    (SPACE,     'SPACE    '),
    (UPPER,     'UPPER    '),
    (XDIGIT,    'XDIGIT   '),
    (BLANK,     'BLANK    '),
    (PRINT,     'PRINT    '),
    (IDEOGRAM,  'IDEOGRAM '),
    (SPECIAL,   'SPECIAL  '),
    (PHONOGRAM, 'PHONOGRAM'),
    (SWIDTH0,   'SWIDTH0  '),
    (SWIDTH1,   'SWIDTH1  '),
    (SWIDTH2,   'SWIDTH2  '),
    (SWIDTH3,   'SWIDTH3  '),
)

CLASSES = {
    '':   0,
    'C':  CONTROL,                          # Other
    'Cc': CONTROL,                          # Control
    'Cf': CONTROL,                          # Format
    'Cn': 0,                                # Unassigned
    'Co': PRINT | GRAPH,                    # Private_Use
    'Cs': PRINT,                            # Surrogate
    'L':  PRINT | GRAPH,                    # Letter
    'LC': PRINT | GRAPH | ALPHA,            # Cased_Letter
    'Ll': PRINT | GRAPH | ALPHA | LOWER,    # Lowercase_Letter
    'Lm': PRINT | GRAPH,                    # Modifier_Letter
    'Lo': PRINT | GRAPH,                    # Other_Letter
    'Lt': PRINT | GRAPH | ALPHA,            # Titlecase_Letter
    'Lu': PRINT | GRAPH | ALPHA | UPPER,    # Uppercase_Letter
    'M':  PRINT | GRAPH,                    # Mark
    'Mc': PRINT | GRAPH,                    # Spacing_Mark
    'Me': PRINT | GRAPH,                    # Enclosing_Mark
    'Mn': PRINT | GRAPH,                    # Nonspacing_Mark
    'N':  PRINT | GRAPH | DIGIT,            # Number
    'Nd': PRINT | GRAPH | DIGIT,            # Decimal_Number
    'Nl': PRINT | GRAPH | SPECIAL,          # Letter_Number
    'No': PRINT | GRAPH | SPECIAL,          # Other_Number
    'P':  PRINT | GRAPH | PUNCT,            # Punctuation
    'Pc': PRINT | GRAPH | PUNCT,            # Connector_Punctuation
    'Pd': PRINT | GRAPH | PUNCT,            # Dash_Punctuation
    'Pe': PRINT | GRAPH | PUNCT,            # Close_Punctuation
    'Pf': PRINT | GRAPH | PUNCT,            # Final_Punctuation
    'Pi': PRINT | GRAPH | PUNCT,            # Initial_Punctuation
    'Po': PRINT | GRAPH | PUNCT,            # Other_Punctuation
    'Ps': PRINT | GRAPH | PUNCT,            # Open_Punctuation
    'S':  PRINT | GRAPH | PUNCT,            # Symbol
    'Sc': PRINT | GRAPH | PUNCT,            # Currency_Symbol
    'Sk': PRINT | GRAPH | PUNCT,            # Modifier_Symbol
    'Sm': PRINT | GRAPH | PUNCT,            # Math_Symbol
    'So': PRINT | GRAPH | PUNCT,            # Other_Symbol
    'Z':  PRINT | SPACE,                    # Separator
    'Zl': PRINT | SPACE,                    # Line_Separator
    'Zp': PRINT | SPACE,                    # Paragraph_Separator
    'Zs': PRINT | SPACE | BLANK,            # Space_Separator
}

BIDIRECT_PROPS = {
    '':    0,
    'AL':  0,                               # Arabic_Letter
    'AN':  0,                               # Arabic_Number
    'B':   SPACE,                           # Paragraph_Separator
    'BN':  0,                               # Boundary_Neutral
    'CS':  0,                               # Common_Separator
    'EN':  0,                               # European_Number
    'ES':  0,                               # European_Separator
    'ET':  0,                               # European_Terminator
    'L':   0,                               # Left_To_Right
    'LRE': 0,                               # Left_To_Right_Embedding
    'LRO': 0,                               # Left_To_Right_Override
    'NSM': 0,                               # Nonspacing_Mark
    'ON':  0,                               # Other_Neutral
    'PDF': 0,                               # Pop_Directional_Format
    'R':   0,                               # Right_To_Left
    'RLE': 0,                               # Right_To_Left_Embedding
    'RLO': 0,                               # Right_To_Left_Override
    'S':   BLANK | SPACE,                   # Segment_Separator
    'WS':  SPACE,                           # White_Space
}

EXCEPTIONS = {
    0x001c: CONTROL,
    0x001d: CONTROL,
    0x001e: CONTROL,
    0x001f: CONTROL,
}

WIDTH_MAP = {
    'A':    SWIDTH1,    # ambiguous (XXX: how about modifiers and comining?)
    'N':    SWIDTH1,
    'Na':   SWIDTH1,
    'H':    SWIDTH1,
    'F':    SWIDTH2,
    'W':    SWIDTH2,
}
WIDTH_DEFAULT = SWIDTH1

XDIGITMASK = re.compile('LETTER [A-F]$')
IDEOGRAMMASK = re.compile('[Ii][Dd][Ee][Oo][Gg][Rr]')
PHONOGRAMMASK = re.compile('[Ss][Yy][Ll][Ll][Aa][Bb]|[Hh][Aa][Nn][Gg][Uu][Ll][ ][Ll][Ee][Tt][Tt]|[Hh][Ii][Rr][Aa][Gg][Aa][Nn][Aa]|[Kk][Aa][Tt][Aa][Kk][Aa][Nn][Aa]')

FILEHEADER = """\
/*
 * Unicode 3.2 ctypes table
 *
 * Generated by Hye-Shik Chang <perky@FreeBSD.org>
 */

/*
 * UCD(Unicode Character Database) Terms of Use
 *
 * Disclaimer
 *
 * The Unicode Character Database is provided as is by Unicode, Inc. No claims
 * are made as to fitness for any particular purpose. No warranties of any kind
 * are expressed or implied. The recipient agrees to determine applicability of
 * information provided. If this file has been purchased on magnetic or optical
 * media from Unicode, Inc., the sole remedy for any claim will be exchange of
 * defective media within 90 days of receipt.
 *
 * This disclaimer is applicable for all other data files accompanying the
 * Unicode Character Database, some of which have been compiled by the Unicode
 * Consortium, and some of which have been supplied by other sources.
 *
 * Limitations on Rights to Redistribute This Data
 *
 * Recipient is granted the right to make copies in any form for internal
 * distribution and to freely use the information supplied in the creation of
 * products supporting the UnicodeTM Standard. The files in the Unicode
 * Character Database can be redistributed to third parties or other
 * organizations (whether for profit or not) as long as this notice and the
 * disclaimer notice are retained. Information can be extracted from these
 * files and used in documentation or programs, as long as there is an
 * accompanying notice indicating the source.
 */
"""

FILEHEADER += """
ENCODING	"UTF-8"
VARIABLE	Unicode 3.2 Character Types"""

BLOCKHEADER = """

/*
 * %(area)s : %(name)s
 */
"""

hex2int   = lambda x: eval('0x'+x)
repr_code = lambda c: (c < 0x7f and chr(c).isalnum()) and ("'%s'" % chr(c)) or ('0x%04x' % c)

def repr_codearea(st, en):
    if st == en:
        return repr_code(st)
    if abs(st - en) <= 1:
        return "%s  %s" % (repr_code(st), repr_code(en))
    else:
        return "%s - %s" % (repr_code(st), repr_code(en))

class CodeArea:

    def __init__(self, st, en=None):
        self.st = st
        self.en = en

    def isincode(self, c):
        if self.en is None:
            return c == self.st
        else:
            return self.st <= c <= self.en

    def __hash__(self):
        return hash((self.st, self.en))

    def __cmp__(self, o):
        return cmp(self.st, o.st)

    def __repr__(self):
        if self.en is None:
            return 'U+%04X' % self.st
        else:
            return 'U+%04X - U+%04X' % (self.st, self.en)


class MapStack:
    
    def __init__(self, tag):
        self.tag = tag
        self.data = []

    def __repr__(self):
        r = []
        for d in self.data:
            if d[0] == d[1]:
                r.append('%-9s < %s %s >' % (self.tag, repr_code(d[0]), repr_code(d[2])))
            else:
                r.append('%-9s < %s - %s : %s >' % (
                    self.tag, repr_code(d[0]), repr_code(d[1]), repr_code(d[2]) ) )
        return '\n'.join(r)

    def takemaps(self, st_code, en_code):
        m = MapStack(self.tag)
        for ist, ien, idst, iden in self.data:
            if st_code <= ist and ien <= en_code:
                m.data.append([ist, ien, idst, iden])
            #elif: .. no splitting needed on UCS2
        return m

    def add(self, idx, val):
        if (self.data and idx - self.data[-1][1] == 1 and val - self.data[-1][3] == 1):
            self.data[-1][1] = idx
            self.data[-1][3] = val
        else:
            self.data.append([idx, idx, val, val])

    def __nonzero__(self):
        return self.data and 1 or 0


class UnicodeData:

    def __init__(self, filepath, widths):
        self.data = []
        self.load(open(filepath))
        self.tag_bsdctype(widths)

    def load(self, fo):
        pdata = []
        dcontent = fo.readlines()
        self.codefinal = eval('0x' + dcontent[-1].split(';')[0])
        self.data = [None] * (self.codefinal + 2)

        for l in dcontent:
            l = l.split('#', 1)[0]
            if l.strip():
                code, value = l.split(';', 1)
                pdata.append([eval('0x'+code)] + value.strip().split(';'))
                if pdata[-1][1].endswith('Last>'):
                    en = pdata.pop()
                    st = pdata.pop()
                    extname = st[1].replace('First', 'Element')
                    for c in range(st[0], en[0]+1):
                        pdata.append([c,extname] + st[2:])

        for p in pdata:
            self.data[p[0]] = p[1:]

    def tag_bsdctype(self, widths):
        print >>stderr, "=>> Tagging BSD ctypes..."

        self.uppermap = MapStack('MAPUPPER')
        self.lowermap = MapStack('MAPLOWER')
        self.digitmap = MapStack('TODIGIT')

        for i in range(self.codefinal + 1):
            w = widths.get(i, WIDTH_DEFAULT)

            if EXCEPTIONS.has_key(i):
                self[i].append(EXCEPTIONS[i] | (EXCEPTIONS[i] & PRINT and w or 0))
            elif self[i]:
                self[i].append(CLASSES[self[i][1]] | BIDIRECT_PROPS[self[i][3]])
                if self[i][-1] & PRINT:
                    self[i][-1] |= w
                if i < 128 and (self[i][-1] & DIGIT or XDIGITMASK.search(self[i][0])):
                    self[i][-1] |= XDIGIT
                elif self[i][1] == 'Lo':
                    if IDEOGRAMMASK.search(self[i][0]):
                        self[i][-1] |= IDEOGRAM
                    elif PHONOGRAMMASK.search(self[i][0]):
                        self[i][-1] |= PHONOGRAM

                if i >= 128:
                    self[i][-1] &= ~(DIGIT | XDIGIT)

            if self[i]:
                if self[i][11]:
                    self.uppermap.add(i, hex2int(self[i][11]))
                if self[i][12]:
                    self.lowermap.add(i, hex2int(self[i][12]))
                if self[i][5] and i < 128:
                    self.digitmap.add(i, hex2int(self[i][5]))


    def __getitem__(self, key):
        try:
            return self.data[key]
        except IndexError:
            return None

    def __setitem__(self, key, value):
        self.data[key] = value

class BlockData(list):

    def __init__(self, filepath):
        list.__init__(self)
        self.load(open(filepath))

    def load(self, fo):
        del self[:]

        for l in fo.readlines():
            l = l.split('#', 1)[0]
            if l.strip():
                code, value = l.split(';', 1)
                code = [eval('0x'+m) for m in code.strip().split('..')]
                self.append((CodeArea(*code), value.strip()))

def WidthData(filepath):
    r = {}
    for l in open(filepath).readlines():
        l = l.split('#', 1)[0]
        if l.strip():
            code, value = l.split(';', 1)
            code = [eval('0x'+m) for m in code.strip().split('..')]
            if len(code) > 1:
                code = range(code[0], code[1]+1)
            for c in code:
                r[c] = WIDTH_MAP[value.strip()]

    return r

def generate_ctype(blocks, codes):

    print FILEHEADER

    for area, name in blocks:

        print BLOCKHEADER % locals()
        blockcodes = range(area.st, area.en+1)

        print >> stderr, "   + block <%s>" % name
        for mask, fname in BSD_CTYPES:
            cont = []
            for c in blockcodes:
                if codes[c] is not None and mask & codes[c][-1]:
                    if cont and c - cont[-1][1] == 1:
                        cont[-1][1] = c
                    else:
                        cont.append([c, c])

            ob = ''
            while cont:
                if ob:
                    ob += '  ' + repr_codearea(*cont.pop(0))
                else:
                    ob += repr_codearea(*cont.pop(0))

                if len(ob) > 50:
                    print fname, ob
                    ob = ''
            if ob:
                print fname, ob

        mapprinted = False
        for map in [codes.uppermap, codes.lowermap, codes.digitmap]:
            mapsinarea = map.takemaps(area.st, area.en)
            if mapsinarea:
                if not mapprinted:
                    print
                    mapprinted = True
                print mapsinarea

        if area.st == 0:
            print "TODIGIT   < 'A' - 'F' : 10 > < 'a' - 'f' : 10 >"

if __name__ == '__main__':
    print >> stderr, "=>> Loading Blocks data..."
    BLOCKSFILE = BlockData("Blocks.txt")

    print >> stderr, "=>> Loading Width data..."
    WIDTHFILE = WidthData("EastAsianWidth.txt")

    print >> stderr, "=>> Loading Unicode data..."
    UNICODEFILE = UnicodeData("UnicodeData.txt", WIDTHFILE)

    print >> stderr, "=>> Generating Rune Locale..."
    generate_ctype(BLOCKSFILE, UNICODEFILE)

# ex: ts=8 sts=4 et
