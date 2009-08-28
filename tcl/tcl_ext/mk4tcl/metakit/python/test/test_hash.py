# test_hash.py -- Test Metakit Python bindings for hashed views
# $Id: test_hash.py 1230 2007-03-09 15:58:53Z jcw $
# This is part of Metakit, see http://www.equi4.com/metakit/

from mktestsupport import *

works = 't[z:I,pizza:S]', 'pizza'
fails = 't[z:I,a:S]', 'a'

for struc, field in (works, fails):
    v = HashedViewTester(struc, 1)
    for i in range(4):
        v.insert(1, 'A')
    for i in range(4):
        v.insert(**{field: 'A', 'z': 1})
    v.finished()
