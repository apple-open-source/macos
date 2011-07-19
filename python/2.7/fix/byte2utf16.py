#!/usr/bin/python

import sys
import codecs

if len(sys.argv) != 3:
    sys.stderr.write("Usage: " + sys.argv[0] + " inascii oututf16\n")
    sys.exit(1)
input = open(sys.argv[1], 'r')
out = codecs.open(sys.argv[2], 'w', 'utf-16');

out.write(input.read())
out.close()
input.close()
