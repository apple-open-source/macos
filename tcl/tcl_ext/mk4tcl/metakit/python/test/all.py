# all.py -- Run all tests for the Metakit Python bindings
# $Id: all.py 1230 2007-03-09 15:58:53Z jcw $
# This is part of Metakit, see http://www.equi4.com/metakit/

import sys
import os
import test.regrtest

def canonicalPath(path):
    """Do everything but resolve symbolic links to create an absolute path."""
    return os.path.abspath(os.path.expanduser(os.path.expandvars(path)))

# from Python 2.2's test.regrtest module
def findtestdir():
    if __name__ == '__main__':
        file = sys.argv[0]
    else:
        file = __file__
    testdir = os.path.dirname(file) or os.curdir
    return testdir

testdir = canonicalPath(findtestdir())

# Make sure 'import metakit' works, assuming metakit modules are
# in the directory above that containing this script.
sys.path.insert(0, os.path.dirname(testdir))

# Make sure we're using modules from the builds directory, assuming
# that's the current directory at the time we're run.  (While this
# directory is probably named 'test', it isn't a module, so it
# shouldn't interfere with references to the Python 'test' module).
sys.path.insert(0, os.getcwd())

# Don't run the standard Python tests, just run Metakit tests
test.regrtest.STDTESTS = []
test.regrtest.NOTTESTS = []

# Run all tests
test.regrtest.main(testdir=testdir)
