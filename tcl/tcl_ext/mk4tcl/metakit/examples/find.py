"""
Some demo code to show find performance and some of the new indexing
features in Metakit 2.3.  Sample output (SuSE Linux 6.4, PIII/650):

  find.py - Mk4py 2.3.2 - linux2
  filldb 100 rows: 0.015092 sec
   find_raw 10000 times, key 50 -> 50: 1.20376 sec
   find_raw 10000 times, key 101 -> -1: 1.82736 sec
   find_ord 10000 times, key 50 -> 50: 0.722506 sec
   find_ord 10000 times, key 101 -> -1: 0.670945 sec
   hash_ini 1000 times, size 129: 0.487246 sec
   hash_key 10000 times, key 50 -> 50: 0.658689 sec
   hash_key 10000 times, key 101 -> -1: 0.592888 sec
  filldb 1000 rows: 0.059973 sec
   find_raw 1000 times, key 500 -> 500: 0.715116 sec
   find_raw 1000 times, key 1001 -> -1: 1.35979 sec
   find_ord 1000 times, key 500 -> 500: 0.0832601 sec
   find_ord 1000 times, key 1001 -> -1: 0.0784379 sec
   hash_ini 100 times, size 1025: 0.565817 sec
   hash_key 10000 times, key 500 -> 500: 0.666233 sec
   hash_key 10000 times, key 1001 -> -1: 0.714768 sec
  filldb 10000 rows: 0.508873 sec
   find_raw 100 times, key 5000 -> 5000: 0.669386 sec
   find_raw 100 times, key 10001 -> -1: 1.30918 sec
   find_ord 100 times, key 5000 -> 5000: 0.00834703 sec
   find_ord 100 times, key 10001 -> -1: 0.00776601 sec
   hash_ini 10 times, size 16385: 0.424207 sec
   hash_key 10000 times, key 5000 -> 5000: 0.665882 sec
   hash_key 10000 times, key 10001 -> -1: 0.640642 sec
  filldb 100000 rows: 4.96599 sec
   find_raw 10 times, key 50000 -> 50000: 0.625583 sec
   find_raw 10 times, key 100001 -> -1: 1.23957 sec
   find_ord 10 times, key 50000 -> 50000: 0.000901937 sec
   find_ord 10 times, key 100001 -> -1: 0.00124598 sec
   hash_ini 1 times, size 131073: 0.517645 sec
   hash_key 10000 times, key 50000 -> 50000: 0.657581 sec
   hash_key 10000 times, key 100001 -> -1: 0.628431 sec

In a nutshell: unordered views use linear scan, which is extremely
efficient but O(N), ordered views will switch to binary search, and
if a hash mapping is used then you get the usual O(1) of hashing.

The statement "vwo = vw.ordered(1)" sets up a view layer around vw,
which allows Metakit to take advantage of sort order.  The numeric
argument specifies how many of the first property define the key.
The vwo view is modifiable, it will maintains order on insertions.

The statement "vwh = vw.hash(map,1)" sets up a view layer around vw,
using a secondary 'map' view to manage hashing.  As with ordered,
the last argument specifies the number or properties to use as key.
When vwh is modified, both vw and map will be adjusted.  The hash
view can be set up after vw has been filled, this is probably a bit
faster than setting up vwh first, and inserting into new data in vwh.
The underlying vw and map views may *not* be altered directly once
a hash is in use, unless you clear map and redefine the hash layer.

Metakit 2.01 only supports find_raw (linear scanning).
"""

import sys; sys.path.append('../builds'); import Mk4py; mk = Mk4py
print sys.argv[0], '-', 'Mk4py', mk.version, '-', sys.platform

from time import time

db = mk.storage()
vw = db.getas('vw[p1:I,p2:I]')
map = db.getas('map[_H:I,_R:I]')

def filldb(n=1000):
  del vw[:]
  t0 = time();
  for i in xrange(n):
    vw.append(p1=i, p2=i+i)
  print 'filldb %d rows: %g sec' % (n, time() - t0)

# this find will do a linear scan
def find_raw(k,n=1000):
  t0 = time();
  for i in xrange(n):
    r = vw.find(p1=k)
  print ' find_raw %d times, key %d -> %d: %g sec' % (n, k, r, time() - t0)

# this find switches to binary search, because it knows the view is ordered
def find_ord(k,n=1000):
  t0 = time();
  vwo = vw.ordered()
  for i in xrange(n):
    r = vwo.find(p1=k)
  print ' find_ord %d times, key %d -> %d: %g sec' % (n, k, r, time() - t0)

# setting up a hash view initializes it, since the map size is still zero
def hash_ini(n=1000):
  t0 = time();
  for i in xrange(n):
    del map[:]
    vwh = vw.hash(map)
  print ' hash_ini %d times, size %d: %g sec' % (n, len(map), time() - t0)
 
# this lookup no longer initializes, find will now do a hash lookup
def hash_key(k,n=1000):
  t0 = time();
  vwh = vw.hash(map)
  for i in xrange(n):
    r = vwh.find(p1=k)
  print ' hash_key %d times, key %d -> %d: %g sec' % (n, k, r, time() - t0)

for n in (100, 1000, 10000, 100000):
  filldb(n)
  find_raw(n/2, 1000000/n)
  find_raw(n+1, 1000000/n)
  find_ord(n/2, 1000000/n)
  find_ord(n+1, 1000000/n)
  hash_ini(100000/n)
  hash_key(n/2, 10000)
  hash_key(n+1, 10000)
