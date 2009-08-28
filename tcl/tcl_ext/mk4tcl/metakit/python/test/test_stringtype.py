# test_stringtype.py -- Test Metakit Python bindings for string type
# $Id: test_stringtype.py 1230 2007-03-09 15:58:53Z jcw $
# This is part of Metakit, see http://www.equi4.com/metakit/

from mktestsupport import *

v = ViewTester('test[a:S,b:S]')

# defaults
v.insert(a='', b='')

# ASCII strings
v.insert(a='asdasdfasdfasdfsa', b='!@*%$#@#$%^&*()')
v.reject(TypeError, a=3, b='')

# Null termination
v.reject(ValueError, a='\0', b='hi\0')
v.reject(ValueError, a='abcdabcdabcd\0hi', b='lo')
v.reject(ValueError, a='\0\0hi', b='lo')

# Unicode and UTF-8 strings
if UnicodeType:
    v.insert(a=unicode('hi there', 'utf-8'), b='hi')
    v.insert(a=unicode('\xe2\x82\xac', 'utf-8'), b='hi')
    v.insert(a=unicode('Sample\xe2\x82\xacTesting', 'utf-8'), b='')
    v.reject(ValueError, a=unicode('Sample\0blahblah', 'utf-8'), b='yo')

# Non-ASCII 8-bit strings
v.insert(a='', b='\xe2\x82\xacHi there')

v.finished()
