# test_inttypes.py -- Test Metakit Python bindings for integral types
# $Id: test_inttypes.py 1230 2007-03-09 15:58:53Z jcw $
# This is part of Metakit, see http://www.equi4.com/metakit/

from mktestsupport import *

v = ViewTester('test[intf:I,longf:L]')

# defaults
v.insert(intf=0, longf=0)

# int field
v.insert(intf=1, longf=0)
v.insert(intf=-5, longf=0)
v.insert(intf=MAXINT, longf=0)
v.insert(intf=MININT, longf=0)
v.reject(int_long_error, intf=MAXINT + 1, longf=0)
v.reject(int_long_error, intf=MININT - 1, longf=0)
if int_long_integrated:
    v.reject(OverflowError, intf=MAXLONGLONG, longf=0)
    v.reject(OverflowError, intf=MINLONGLONG, longf=0)

# long field
v.insert(intf=0, longf=-1L)
v.insert(intf=0, longf=5L)
v.insert(intf=0, longf=MAXLONGLONG)
v.insert(intf=0, longf=MINLONGLONG)
v.reject(ValueError, intf=0, longf=MAXULONGLONG)
v.reject(ValueError, intf=0, longf=MAXLONGLONG + 1)
v.reject(ValueError, intf=0, longf=MAXULONGLONG)
v.reject(ValueError, intf=0, longf=MINLONGLONG - 1)

# mixed valid int/long
v.insert(intf=1, longf=2)
v.insert(intf=-5, longf=-2**30)

# implicit conversion to int
v.insert(intf=14L, longf=0)
v.insert(intf=-30L, longf=0)
v.insert(intf=45.0, longf=0)
v.insert(intf=21.4, longf=0)
v.reject(int_long_error, intf=float(MAXINT + 1), longf=0)
v.reject(int_long_error, intf=float(MININT - 1), longf=0)
v.reject(TypeError, intf='215', longf=0)
v.reject(TypeError, intf='-318.19', longf=0)
v.reject(TypeError, intf=str(MAXINT + 1), longf=0)

# implicit conversion to long
v.insert(intf=0, longf=278)
v.insert(intf=0, longf=-213)
v.insert(intf=0, longf=95.0)
v.insert(intf=0, longf=27.3)
v.reject(ValueError, intf=0, longf=float(2 * MAXLONGLONG))
v.reject(ValueError, intf=0, longf=float(2 * MINLONGLONG))
v.reject(TypeError, intf=0, longf=str(MAXLONGLONG))
v.reject(TypeError, intf=0, longf=str(MINLONGLONG))
v.reject(TypeError, intf=0, longf='-21.39')
v.reject(TypeError, intf=0, longf=str(MAXULONGLONG))

# XXX should repeat with assignment instead of appending
# XXX test v.select()

v.finished()
