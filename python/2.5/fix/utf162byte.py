#!/usr/bin/python

import sys
import codecs

if len(sys.argv) != 3:
    sys.stderr.write("Usage: " + sys.argv[0] + " inutf16 outbyte\n")
    sys.exit(1)
input = codecs.open(sys.argv[1], 'r', 'utf-16');
out = open(sys.argv[2], 'w')

out.write(input.read())
out.close()
input.close()
