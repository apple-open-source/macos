#!/usr/bin/python

# $Id: test.py,v 1.1 2003/02/20 23:36:41 jas Exp $

locale = "Latin-1"

import idn
import sys

if len(sys.argv) <= 1:
    print "Usage: %s name" % sys.argv[0]
    sys.exit(1)
    
name = sys.argv[1]

ustring = unicode(name, locale)
print idn.idn2ace(ustring.encode("UTF-8"))
