# mktestsupport.py -- Support code used by multiple test modules
# $Id: mktestsupport.py 1230 2007-03-09 15:58:53Z jcw $
# This is part of Metakit, see http://www.equi4.com/metakit/

from test.test_support import TestFailed, verbose
import metakit
import sys
import string

# for overflow testing
MAXINT = sys.maxint
MININT = -MAXINT - 1
MAXLONGLONG = 2**63 - 1
MINLONGLONG = -2**63
MAXULONGLONG = 2**64 - 1

# Check for int/long integration (should fail in Python 2.2, pass in 2.3)
try:
    MAXLONGLONG = int(MAXLONGLONG)
    int_long_integrated = True
    int_long_error = OverflowError
except OverflowError: # long int too large to convert to int
    int_long_integrated = False
    int_long_error = TypeError

# Check for Unicode support (should fail in Python 1.5.2 or --disable-unicode, pass in 1.6)
try:
    UnicodeType = type(unicode(''))
except NameError: # no Unicode support
    UnicodeType = None
    unicode = None
    
class Failure:
    """Keeps track of failures as they happen, but doesn't die on the first
    one unless it's unrecoverable.  If failure_count > 0 when script
    finishes, raise TestFailed."""

    def __init__(self):
        self.failure_count = 0
    
    def fail(self, op, args, err=None, expected=None, actual=None):
        print 'FAIL:', op, args
        print '     ',
        if err is not None: print err,
        if actual is not None: print 'got', actual, actual.__class__,
        if expected is not None: print 'expected', expected,
        print
        self.failure_count = self.failure_count + 1

    def assess(self):
        if self.failure_count > 0:
            raise TestFailed(
                '%d failures; run in verbose mode for details' % self.failure_count)

class ViewTester:
    """Inserts rows into view and Python array"""
    
    def __init__(self, description):
        self.storage = metakit.storage()
        self.v = self.storage.getas(description)
        self.arr = []
        self.failure = Failure()
        self.fail = self.failure.fail
        self.columns = map(lambda c: string.split(c, ':')[0], string.split(description[string.index(description, '[') + 1:-1], ','))

    def dump_view(self):
        metakit.dump(self.v, 'VIEW CONTENTS:')

    def checklen(self, args):
        alen = len(self.arr)
        vlen = len(self.v)
        if alen != vlen:
            self.fail('append', args, 'view length mismatch',
                      actual=vlen, expected=alen)
            try:
                print 'ARRAY CONTENTS:'
                for arow in self.arr: print arow
                self.dump_view()
            except: pass
            raise TestFailed('unexpected number of rows in view, aborting; run in verbose mode for details')

    def _append(self, args):
        self.arr.append(args)

    def insert(self, *args, **kw):
        if kw:
            if args:
                raise TestFailed("can't have both positional and keyword arguments")
            args = kw
        try:
            self.v.append(args)
            self._append(args)
        except Exception, e:
            self.fail('append', args, actual=e)
        try:
            self.checklen(args)
        except TestFailed:
            raise
        except Exception, e:
            self.fail('append', args, 'spurious', actual=e)

    def reject(self, exception_class=Exception, **args):
        try:
            ix = self.v.append(args)
            self.fail('append', args, 'succeeded', expected=exception_class)
            self.v.delete(ix)
        except Exception, e:
            if isinstance(e, exception_class):
                if verbose:
                    print 'PASS: rejected', args
                    print '      as expected <%s> %s' % (e.__class__, e)
            else:
                self.fail('append', args, expected=exception_class, actual=e)
        try:
            self.checklen(args)
        except TestFailed:
            raise
        except Exception, e:
            self.fail('append', args, 'spurious', actual=e)

    def finished(self):
        if verbose:
            self.dump_view()

        # compare view with array
        for arow, vrow in zip(self.arr, self.v):
            failed = False
            for f in arow.keys():
                try:
                    vf = getattr(vrow, f)
                    af = arow[f]
                    # Fix up Unicode
                    if type(af) == UnicodeType:
                        vf = unicode(vf, 'utf-8')
                    if af == vf:
                        continue
                    # Perform the same implicit coercion as Mk4py should
                    if type(af) != type(vf):
                        try:
                            af = type(vf)(af)
                            if af == vf:
                                continue
                        except:
                            pass
                    # If we get here, we got an exception or the values didn't match
                    # even with coercion
                    failed = True
                    self.fail('%s access' % f, arow, expected=af, actual=vf)
                except Exception, e:
                    failed = True
                    self.fail('%s access' % f, arow, expected=arow[f], actual=e)
            if not failed:
                if verbose:
                    print 'PASS: retrieved', arow

        self.failure.assess()

class HashedViewTester(ViewTester):
    """Inserts rows into hashed view and Python array (where appropriate)"""

    def __init__(self, description, numkeys):
        ViewTester.__init__(self, description)
        hv = self.storage.getas('hv[_H:I,_R:I]')
        self.v = self.v.hash(hv, numkeys)

    def _append(self, args):
        if not hasattr(args, 'keys'): # operator module is broken in Python 2.3
            argdict = {}
            for i in range(len(args)):
                argdict[self.columns[i]] = args[i]
            args = argdict
        if args not in self.arr:
            self.arr.append(args)
