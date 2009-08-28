import unittest, Mk4py, sys
from test import test_support
from mktestsupport import *

class Dummy:
    def __init__(self, **kws):
        self.__dict__.update(kws)

try:
    # new-style class declaration; fails on Python 2.1 and earlier
    new_dummy = """class NewDummy(object):
    def __init__(self, **kws):
        self.__dict__.update(kws)"""
    eval(compile(new_dummy, '', 'single'))
except:
    NewDummy = None

class SequenceCounter:
    def __init__(self, initial_value):
        self.beginning = self.count = initial_value

    def begin(self):
        self.beginning = self.count

    # ideally, this wants to be a generator...
    # but then we'd have a more complex interface, and lose compatibility with
    # older Python versions
    def __call__(self):
        self.count += 1
        return self.count - 1
        
##class StorageTestCase(unittest.TestCase):
    #storge(), storage(file), storage(fnm, rw=[0,1,2])
    #getas
    #view
    #rollback
    #commit
    #aside
    #description
    #contents
    #autocommit
    #load
    #save
##    def testX(self):
##        pass

#class WrapTestCase(unittest.TestCase):
    #wrap(seq, props, usetuples=0) -> RO view
    #pass

class ViewerTestCase(unittest.TestCase):
    def setUpViews(self):
        self.s = s = Mk4py.storage()
        self.v0 = s.getas('v0[s:S,i:I]')
        self.v1 = s.getas('v1[s:S,f:D]')
        self.v2 = s.getas('v2[i:I,b:M]')
        for vals in [('a',1),('b',2),('c',3)]:
            self.v0.append(vals)
        for vals in [('c',1.0),('b',2.0),('a',3.0)]:
            self.v1.append(vals)
        for vals in [(2,'\2'),(3,'\3'),(4,'\4')]:
            self.v2.append(vals)
        t = self.v1.rename('s', 't')
        self.p0 = t.product(self.v0)
        self.g0 = self.p0.groupby(self.p0.t, self.p0.f, 'details')
        self.f0 = self.g0.flatten(self.g0.details)
        self.f1 = self.f0[3:]
        self.m0 = self.f1.copy()
        def f(row):
            row.i = row.i + 1
        self.m0.map(f)
            
    def setUp(self):
        self.setUpViews()
        self.v0 = self.v0.unique()
        # unique sorts, so v1 needs to be put back in reverse order
        self.v1 = self.v1.unique().sortrev([self.v1.s],[self.v1.s])
        self.v2 = self.v2.unique()
        self.p0 = self.p0.unique()
        
    def testStructure(self):
        proplist = self.v0.structure()
        self.assertEqual(type(proplist), type([]))
        self.assertEqual(len(proplist), 2)
        propdict = self.v0.properties()
        self.assertEqual(type(propdict), type({}))
        self.assertEqual(len(propdict), 2)
        for prop in proplist:
            prop2 = propdict[prop.name]
            self.assertEqual(prop2.name, prop.name)
            self.assertEqual(prop2.type, prop.type)
            self.assertEqual(prop2.id, prop.id)

    def testSimple(self):
        self.assertEqual(len(self.v0),3)
        self.assertEqual(self.v0[0].s, 'a')
        self.assertEqual(self.v0[0].i, 1)
        v1 = self.v0[1:]
        self.assertEqual(len(v1), 2)
        self.assertEqual(v1[0].s, 'b')
        self.assertEqual(v1[0].i, 2)
        
    def testFind(self):
        self.assertEqual(self.v0.find(s='c'), 2)
        self.assertEqual(self.v0.find(i=2), 1)
        self.assertEqual(self.v0.find(s='z'), -1)
        
    def testSearch(self):
        #search - v0 is ordered on  s 
        self.assertEqual(self.v0.search(s='c'), 2)
        self.assertEqual(self.v0.search(s='b'), 1)
        self.assertEqual(self.v0.search(s='a'), 0)
        self.assertEqual(self.v0.search(s=' '), 0)
        self.assertEqual(self.v0.search(s='z'), 3)
        
    def testLocate(self):
        #locate - v0 is ordered on  s
        self.assertEqual(self.v0.locate(s='c'), (2,1))
        self.assertEqual(self.v0.locate(s='b'), (1,1))
        self.assertEqual(self.v0.locate(s='a'), (0,1))
        self.assertEqual(self.v0.locate(s=' '), (0,0))
        self.assertEqual(self.v0.locate(s='z'), (3,0))
        
        #itemsize
        #self.assertEqual(v0.itemsize(v0.i), -2)  
        #self.assertEqual(v0.itemsize(v0.s, 0), 2) 
        
    def testCopy(self):
        #copy
        w0 = self.v0.copy()
        self.assertEqual(len(w0), len(self.v0))
        self.assertEqual(w0.structure(), self.v0.structure())
        self.assertEqual(type(w0), Mk4py.ViewType)
        
    def testConcat(self):
        #v1 + v2
        w0 = self.v0.copy()
        x0 = w0 + self.v0
        x1 = self.v0 + w0
        self.assertEqual(len(x0), len(x1))
        self.assertEqual(len(x0), 2*len(self.v0))
        self.assertEqual(x0.structure(), self.v0.structure())
        self.assertEqual(x1.structure(), self.v0.structure())
        
    def testRepeat(self):
        #v1 * 2
        x1 = self.v0 * 2
        self.assertEqual(len(x1), 2*len(self.v0))
        self.assertEqual(x1.structure(), self.v0.structure())
        
    def testRename(self):
        #rename
        self.assertEqual(self.v0.s, Mk4py.property('S','s'))
        x2 = self.v0.rename('s', 'stringprop')
        self.assertEqual(x2.stringprop, Mk4py.property('S','stringprop'))
        self.assertNotEqual(x2.structure(), self.v0.structure())
        self.assertEqual(len(x2.structure()), 2)
        self.assertEqual(x2.i, self.v0.i)

    def testSelect(self):
        #select
        t0 = self.v0.select(s='b')
        self.assertEqual(len(t0), 1)
        self.assertEqual(t0[0].s, 'b')
        t0 = self.v0.select({'s':'a'},{'s':'b'})
        self.assertEqual(len(t0), 2)
        self.assertEqual(t0[1].s, 'b')
        t0 = self.v0.select({'s':'a','i':2},{'s':'b','i':4})
        self.assertEqual(len(t0), 1)
        self.assertEqual(t0[0].s, 'b')
        
    def testSort(self):
        #sort
        t0 = self.v0.sort()  # already in order
        self.assertEqual(len(t0), len(self.v0))
        for i in range(1, len(t0)):
            self.assertEqual( cmp(t0[i-1].s,t0[i].s), -1)
        t0 = self.v1.sort(self.v1.s)  # reverses native order
        self.assertEqual(len(t0), len(self.v1))
        for i in range(1, len(t0)):
            self.assertEqual( cmp(t0[i-1].s,t0[i].s), -1)
            
    def testSortrev(self):
        #sortrev
        t0 = self.v0.sortrev([self.v0.s],[self.v0.s])  # reverses order
        self.assertEqual(len(t0), len(self.v0))
        for i in range(1, len(t0)):
            self.assertEqual( cmp(t0[i-1].s,t0[i].s), 1)
        t0 = self.v1.sortrev([self.v1.s],[self.v1.s])  # native order
        self.assertEqual(len(t0), len(self.v1))
        for i in range(1, len(t0)):
            self.assertEqual( cmp(t0[i-1].s,t0[i].s), 1)
            
    def testProject(self):
        #project
        t0 = self.v0.project(self.v0.s)
        s = t0.structure()
        self.assertEqual(len(s), 1)
        self.assertEqual(s[0], self.v0.s)
        
    def testProduct(self):
        #product
        self.assertEqual(len(self.p0), 9)
        self.assertEqual(len(self.p0.select(s='a')), 3)
        self.assertEqual(len(self.p0.select(t='a')), 3)
        
    def testCounts(self):
        #counts
        t1 = self.p0.counts(self.p0.t, self.p0.f, 'details')
        self.assertEqual(len(t1), 3)
        self.assertEqual(len(t1.select(t='a')), 1)
        for i in range(len(t1)):
            self.assertEqual(t1[i].details, len(self.v0))
            
    def testGroupby(self):
        #groupby
        self.assertEqual(len(self.g0), 3)
        self.assertEqual(len(self.g0.select(t='a')), 1)
        for i in range(len(self.g0)):
            self.assertEqual(len(self.g0[i].details), len(self.v0))
            self.assertEqual(len(self.g0[i].details.select(s='a')), 1)
               
    def testFlatten(self): 
        #flatten
        self.assertEqual(len(self.f0), 9)
        self.assertEqual(len(self.f0.select(s='a')), 3)
        self.assertEqual(len(self.f0.select(t='a')), 3)
        
    def testUnion(self):
        #union
        u0 = self.p0.union(self.f0)   # these 2 are the same
        self.assertEqual(len(u0), 9)
        self.assertEqual(len(u0.select(s='a')), 3)
        self.assertEqual(len(u0.select(t='a')), 3)
        u1 = self.p0.union(self.f1)   # strict subset
        self.assertEqual(len(u1), 9)
        u2 = self.p0.union(self.m0)
        self.assertEqual(len(u2), len(self.p0)+len(self.m0))
              
    def testIntersect(self):  
        #intersect
        i0 = self.p0.intersect(self.f0)   # these 2 are the same
        self.assertEqual(len(i0), 9)
        self.assertEqual(len(i0.select(s='a')), 3)
        self.assertEqual(len(i0.select(t='a')), 3)
        i1 = self.p0.intersect(self.f1)   # strict subset  
        self.assertEqual(len(i1), len(self.f1))
        i2 = self.p0.intersect(self.m0)
        self.assertEqual(len(i2), 0)
        
    def testDifferent(self):
        #different
        d0 = self.p0.different(self.f0)   # these 2 are the same
        self.assertEqual(len(d0),0)
        d1 = self.p0.different(self.f1)   # strict subset  
        self.assertEqual(len(d1), len(self.p0)-len(self.f1))
        d2 = self.f1.different(self.p0)
        self.assertEqual(len(d2), len(self.p0)-len(self.f1))
        d3 = self.p0.different(self.m0)
        self.assertEqual(len(d3), len(self.p0)+len(self.m0))
        
    def testMinus(self):
        #minus
        m0 = self.p0.minus(self.f0)   # these 2 are the same
        self.assertEqual(len(m0),0)
        m1 = self.p0.minus(self.f1)   # strict subset
        self.assertEqual(len(m1), len(self.p0)-len(self.f1))
        m2 = self.f1.minus(self.p0)
        self.assertEqual(len(m2), 0)
        m3 = self.p0.minus(self.m0)
        self.assertEqual(len(m3), len(self.p0))
        
    def testJoin(self):
        #join
        j1 = self.v0.join(self.v1, self.v0.s)
        self.assertEqual(len(j1),3)
        self.assertEqual(j1[0].s, 'a')
        self.assertEqual(j1[0].i, 1)
        self.assertEqual(j1[0].f, 3.0)
        j2 = self.v0.join(self.v2, self.v0.i)
        self.assertEqual(len(j2),2)
        self.assertEqual(j2[0].s, 'b')
        self.assertEqual(j2[0].i, 2)
        self.assertEqual(j2[0].b, '\2')
        # outerjoin
        j3 = self.v0.join(self.v2, self.v0.i, 1)
        self.assertEqual(len(j3),3)
        self.assertEqual(j3[0].s, 'a')
        self.assertEqual(j3[0].i, 1)
        self.assertEqual(j3[0].b, '')
        
    def testIndices(self):
        #indices
        subset = self.p0.select(s='a')
        iv = self.p0.indices(subset)
        self.assertEqual(len(iv), len(subset))
        self.assertEqual(len(iv.structure()), 1)
        for i in range(len(iv)):
            self.assertEqual(self.p0[iv[i].index].s, 'a')
            
    def testRemapwith(self):
        #remapwith
        subset = self.p0.select(s='a')
        iv = self.p0.indices(subset)
        r0 = self.p0.remapwith(iv)
        self.assertEqual(len(r0), len(iv))
        for row in r0:
            self.assertEqual(row.s, 'a')
           
    def testPair(self): 
        #pair
        p0 = self.v1.pair(self.v2)
        self.assertEqual(len(p0), len(self.v1))
        self.assertEqual(p0[0].s, 'c')
        self.assertEqual(p0[0].f, 1.0)
        self.assertEqual(p0[0].i, 2)
        self.assertEqual(p0[0].b, '\2')
        p1 = self.v1.pair(self.v2 * 2)
        self.assertEqual(len(p1), len(self.v1))
        
    def testUnique(self):
        #unique
        t = self.v0 * 2
        u = t.unique()
        self.assertEqual(len(u), len(self.v0))
        self.assertEqual(len(u.minus(self.v0)), 0)

    def testFilter(self): 
        #filter
        t = self.v0.filter(lambda row: row.s < 'b' or row.s > 'b')
        self.assertEqual(len(t), 2)
        for row in t:
            self.assertNotEqual(self.v0[row.index], 'b')

    def testReduce(self):            
        #reduce
        self.assertEqual(self.v0.reduce(lambda row, last: last+row.i), 6)
        
class ViewTestCase(ViewerTestCase):
    def setUp(self):
        self.setUpViews()
        
    def testAddProperty(self):
        x0 = self.v0.copy()
        x0.addproperty(Mk4py.property('I','shoesize'))
        self.assertEqual(len(x0.structure()), 3)
        self.assertEqual(x0.shoesize, x0.structure()[-1]) 

    def testSetsize(self):
        #setsize
        x = self.v0.copy()
        self.assertEqual(x.setsize(6), 6)
        self.assertRaises(TypeError, x.setsize, 1, 2)
        self.assertRaises(TypeError, x.setsize, 'a')
        self.assertEqual(len(x), 6)
        for row in x[3:]:
            self.assertEqual(row.s, '')
            self.assertEqual(row.i, 0)
        self.assertEqual(x.setsize(0), 0)
        self.assertEqual(len(x), 0)

    def testInsert(self):
        #insert
        x = self.v0.copy()

        # Most of insert()'s attribute handling is tested by append(),
        # so just test the features unique to insert() here.
        def insert(index, i):
            a = [r.i for r in x]
            x.insert(index, i=i)
            if index < 0:
                index += len(a) # default behavior in Python 2.3 and later
            a.insert(index, i)
            self.assertEqual(a, [r.i for r in x])

        insert(0, 7)
        insert(1, 4)
        insert(2, 8)
        insert(-1, 6)
        insert(-2, 9)
        insert(500, 48)
        insert(MAXINT, 300)
        insert(MININT, 21)
        self.assertRaises(TypeError, x.insert, 'hi', i=2)
        self.assertRaises(TypeError, x.insert, None, i=2)
        self.assertRaises(TypeError, x.insert)
        self.assertRaises(int_long_error, x.insert, MAXINT + 1, i=2)
        self.assertRaises(int_long_error, x.insert, MININT - 1, i=2)

    def testAppend(self):
        #append
        x = self.v0.copy()
        c = SequenceCounter(3)

        self.assertEqual(x.append(['hi', 2]), c())
        self.assertRaises(TypeError, x.append, 1, 2)
        self.assertRaises(IndexError, x.append, [1, 2, 3]) # could also be TypeError
        self.assertRaises(IndexError, x.append, 'abc')
        self.assertRaises(IndexError, x.append, ['hi', 2, 3])
        self.assertEqual(x.append(s='hi',i=2), c())
        self.assertEqual(x.append(i=2,s='hi'), c())
        self.assertRaises(TypeError, x.append, [1, 's'])
        self.assertRaises(TypeError, x.append, ['s', 't'])
        self.assertRaises(TypeError, x.append, 'hi')
        self.assertEqual(x.append(('hi', 2)), c())
        self.assertEqual(x.append(Dummy(s='hi', i=2)), c())
        self.assertEqual(x.append(Dummy(s='hi', i=2, j=4)), c())
        self.assertRaises(TypeError, x.append, Dummy(s=1))
        self.assertRaises(TypeError, x.append, Dummy(s=Dummy()))
        if NewDummy:
            self.assertEqual(x.append(NewDummy(s='hi', i=2)), c())
            self.assertEqual(x.append(NewDummy(s='hi', i=2, j=4)), c())
            self.assertRaises(TypeError, x.append, NewDummy(s=1))
            self.assertRaises(TypeError, x.append, NewDummy(s=NewDummy()))
        for row in x[c.beginning:]:
            self.assertEqual(row.s, 'hi')
            self.assertEqual(row.i, 2)
            
        c.begin()
        self.assertEqual(x.append(s='hi'), c())
        self.assertEqual(x.append(s='hi',j=2), c())
        self.assertEqual(x.append(['hi']), c())
        self.assertRaises(TypeError, x.append, [1])
        self.assertRaises(TypeError, x.append, 1)
        self.assertEqual(x.append(Dummy(s='hi')), c())
        self.assertEqual(x.append(Dummy(s='hi', j=2)), c())
        if NewDummy:
            self.assertEqual(x.append(NewDummy(s='hi')), c())
            self.assertEqual(x.append(NewDummy(s='hi', j=2)), c())
        for row in x[c.beginning:]:
            self.assertEqual(row.s, 'hi')
            self.assertEqual(row.i, 0)
            
        c.begin()
        self.assertEqual(x.append(), c())
        self.assertEqual(x.append(()), c())
        self.assertEqual(x.append(Dummy()), c())
        self.assertEqual(x.append(Dummy(k=Dummy())), c())
        if NewDummy:
            self.assertEqual(x.append(NewDummy()), c())
            self.assertEqual(x.append(NewDummy(k=NewDummy())), c())
        for row in x[c.beginning:]:
            self.assertEqual(row.s, '')
            self.assertEqual(row.i, 0)

        # XXX test 'L', 'D', 'M'/'B' types
        # XXX test other view types (necessary?)
        
        #delete
        #remove
        #map
        #v[n] = x
        #v[m:n] = x       
        #hash
        #blocked
        #ordered
        #indexed
        
        #access
        #modify
        
   
class RORowRefTestCase(unittest.TestCase):
    def setUp(self):
        self.setUpView()
        self.v = self.v.unique()
    def setUpView(self):
        self.s = s = Mk4py.storage()
        self.v = v = s.getas('test[i:I,l:L,f:F,d:D,s:S,v[s:S],b:B,m:M]')
        v.append()
    def testProperties(self):
        plist = self.v.structure()
        pdict = self.v.properties()
        for prop in plist:
            self.assertEqual(prop, pdict[prop.name])
            self.assertEqual(prop, getattr(self.v, prop.name))
    def testType(self):
        self.assertEqual(type(self.v[0]), Mk4py.RORowRefType)
    def testGetAttr(self):
        r = self.v[0]
        attrs = r.__attrs__
        self.assertEqual(len(attrs), 8)
        self.assertEqual(attrs[0].name, 'i')
        self.assertEqual(attrs[0].type, 'I')
        self.assertEqual(attrs[0].id, Mk4py.property('I','i').id)
        self.assertEqual(attrs[7].name, 'm')
        self.assertEqual(attrs[7].type, 'B')
        self.assertEqual(attrs[7].id, Mk4py.property('M','m').id)
        #self.assertEqual(r.__view__, v)    what's r.__view__ good for??
        self.assertEqual(r.__index__, 0)
        self.assertEqual(r.i, 0)
        self.assertEqual(r.l, 0)
        self.assertEqual(r.f, 0.0)
        self.assertEqual(r.d, 0.0)
        self.assertEqual(r.s, '')
        self.assertEqual(len(r.v, ), 0)
        self.assertEqual(type(r.v), Mk4py.ViewType)
        self.assertEqual(r.b, '')
        self.assertEqual(r.m, '')
    def testSetAttr(self):
        v = self.v.unique()
        r = v[0]
        self.assertRaises(TypeError, setattr, (r, 'i', 1))

class RowRefTestCase(RORowRefTestCase):
    def setUp(self):
        self.setUpView()
    def testType(self):
        self.assertEqual(type(self.v[0]), Mk4py.RowRefType)
    def testSetAttr(self):
        r = self.v[0]
        
        #setattr I - int, castable to int
        r.i = 3
        self.assertEqual(r.i, 3)
        self.assertEqual(type(r.i), int)
        try:
            r.i = True
        except NameError:
            pass
        else:
            self.assertEqual(r.i, 1)
            self.assertEqual(type(r.i), int)
        r.i = 8.0
        self.assertEqual(r.i, 8)
        self.assertEqual(type(r.i), int)
        r.i = 8.9
        self.assertEqual(r.i, 8)
        self.assertEqual(type(r.i), int)
        self.assertRaises(TypeError, setattr, (r, 'i', '1'))
            
        #        L - int, long, castable to long
        r.l = 3L
        self.assertEqual(r.l, 3)
        self.assertEqual(type(r.l), long)
        try:
            r.l = True
        except NameError:
            pass
        else:
            self.assertEqual(r.l, 1)
            self.assertEqual(type(r.l), long)
        r.l = 8.0
        self.assertEqual(r.l, 8)
        self.assertEqual(type(r.l), long)
        r.l = 8.9
        self.assertEqual(r.l, 8)
        self.assertEqual(type(r.l), long)
        try:
            bignum = sys.maxint + 1
        except OverflowError:
            pass
        else:
            r.l = bignum
            self.assertEqual(r.l, bignum)
        self.assertRaises(TypeError, setattr, (r, 'l', '1'))
            
        #        F - float, castable to double
        r.f = 1.0
        self.assertEqual(r.f, 1.0)
        r.f = 1
        self.assertEqual(r.f, 1.0)
        self.assertEqual(type(r.f), float)
        self.assertRaises(TypeError, setattr, (r, 'f', '1.0'))
        
        #        D - float, castable to double
        r.d = 1.0
        self.assertEqual(r.d, 1.0)
        r.d = 1
        self.assertEqual(r.d, 1.0)
        self.assertEqual(type(r.d), float)
        self.assertRaises(TypeError, setattr, (r, 'd', '1.0'))
        
        #        S - string
        s = 'a string'
        r.s = s
        self.assertEqual(r.s, s)
        r.s = s*50
        self.assertEqual(r.s, s*50)
        self.assertRaises(TypeError, setattr, (r, 's', 1.0))
        
        #        V - view, sequence
        r.v = []
        self.assertEqual(len(r.v), 0)
        r.v = [('a',),('b',)]
        self.assertEqual(len(r.v), 2)
        self.assertEqual(r.v[0].s, 'a')
        self.assertEqual(r.v[1].s, 'b')
        #special case where subview has only one property
        r.v = ['a','b']
        self.assertEqual(len(r.v), 2)
        self.assertEqual(r.v[0].s, 'a')
        self.assertEqual(r.v[1].s, 'b')
        r.v = [{'s':'a'},{'s':'b'}]
        self.assertEqual(len(r.v), 2)
        self.assertEqual(r.v[0].s, 'a')
        self.assertEqual(r.v[1].s, 'b')
        r.v = [Dummy(s='a'),Dummy(s='b')]
        self.assertEqual(len(r.v), 2)
        self.assertEqual(r.v[0].s, 'a')
        self.assertEqual(r.v[1].s, 'b')
        
        #        B,M - string
        s = '\0a\0binary\1string'
        r.b = s
        self.assertEqual(r.b, s)
        r.b = s*50
        self.assertEqual(r.b, s*50)
        self.assertRaises(TypeError, setattr, (r, 'b', 1.0))
        r.m = s
        self.assertEqual(r.m, s)
        r.m = s*50
        self.assertEqual(r.m, s*50)
        self.assertRaises(TypeError, setattr, (r, 'm', 1.0))

def test_main():
    l = [ unittest.makeSuite(RORowRefTestCase),
          unittest.makeSuite(RowRefTestCase),
          unittest.makeSuite(ViewerTestCase),
          unittest.makeSuite(ViewTestCase), ]
    suite = unittest.TestSuite(l)
    test_support.run_suite(suite)

if __name__ == '__main__':
    test_main()
