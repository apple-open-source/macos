"""
Storing 25.000.000 rows in a Metakit file.

(C) Christian Tismer, Professional Net Service
  first version from 990822
  update: improved, faster spreading.

This implementation is hereby donated to JCW, therefore
(C) Jean-Claude Wippler (Equi4 Software) 1999

Data structure:
We split the main view by some number of subviews.
This gives us one level of indirection.

First simple test:
10 fields of tiny integers.
Addressing is done only by row number.

In order to allow for deletes and inserts, we keep
a list of block sizes and do a little B-tree like
juggling.

"""

import Mk4py
mk=Mk4py

import whrandom, string, sys, bisect

class big_mk:
  def __init__(self, dbpath, rw):
    self.db = mk.storage(dbpath, rw)
    
  def getas(self, struc_str):
    parts = string.split(struc_str, "[", 1)
    if len(parts) < 2:
      self.db.getas(parts[0])
      return   # this was a delete
    self.main_name, rest = parts
    ret= big_view(self.db.getas("%s[data[%s]" % (self.main_name, rest)))
    ret.big_db = self
    return ret
    
  def commit(self): return self.db.commit()
  def rollback(self): return self.db.rollback()
  def description(self): return self.db.description()
    
class big_view:
  def __init__(self, view):
    self.view = view
    if len(self.view) == 0:
      self.view.append()
    self.calc_recnos()
    self.blocksize = blocksize
    self.lower = self.blocksize *2/3
    self.upper = self.blocksize *2 - self.lower
    self.bisect = bisect.bisect
    self.names = []
    for prop in self.view[-1].data.structure():
      self.names.append(prop.name)
      
  def __len__(self):
    rn = self.recnos
    if not rn: return 0
    return rn[-1]+len(self.view[-1].data)
    
  def __getitem__(self, idx):
    main, sub = self._seek(idx)
    return self.view[main].data[sub]
    
  def __setitem__(self, idx, rec):
    main, sub = self._seek(idx)
    self.view[main].data[sub] = rec
    
  """
  we can't do slices yet, since I have no idea
  if this is necessary, and I don't see exactly
  how this should work
  """
    
  def append(self, record = None):
    v = self.view
    if not self.recnos or len(v[-1].data) >= self.blocksize:
      v.append()
      self.calc_recnos()
    return self.recnos[-1] + v[-1].data.append(record)
    
  def insert(self, idx, rec=None):
    main, sub = self._seek(idx)
    view = self.view[main].data
    view.insert(sub, rec)
    if len(view) > self.upper:
      self._balance(main)
    self.calc_recnos()

  def delete(self, idx):
    main, sub = self._seek(idx)
    view = self.view[main].data
    view.delete(sub)
    if len(view) <= self.lower:
      self._balance(main)
    self.calc_recnos()

  def _seek(self, idx):
    rn = self.recnos
    pos = self.bisect(rn, idx)-1
    base = rn[pos]
    return pos, idx-base
  
  def calc_recnos(self):
    v = self.view
    res = [None] * len(v)
    recno = 0
    for i in range(len(res)):
      res[i] = recno
      recno = recno + len(v[i].data)
    self.recnos = res
    
  def _balance(self, spot):
    """
    very simple approach: we merge about three 
    blocks and spread them again.
    """
    v = self.view
    if spot < 0 or spot > len(v) or len(v)==1 : return
    if spot > 0:
      spot = spot-1
    self._merge(spot)
    if spot < len(v)-1:
      self._merge(spot)
    self._spread(spot)
      
  def _merge(self, spot):
    """
    merge this block and the next one.
    Delete the then empty next one
    """
    v = self.view
    v[spot].data = v[spot].data + v[spot+1].data
    v.delete(spot+1)
    
  def _spread(self, spot):
    """
    Spread this block into equally sized ones.
    """
    v = self.view
    source = v[spot]
    bs = self.blocksize
    nblocks = (len(source.data)+bs-1) / bs
    chunk = len(source.data) / nblocks +1
    if chunk >= self.upper-10 or chunk < self.lower+10: chunk = bs
    chunk, nextchunk = len(source.data) % chunk, chunk
    if chunk < self.lower:
      chunk = chunk + nextchunk
    while len(source.data) > chunk:
      self.view.insert(spot+1)
      self.view[spot+1].data=source.data[-chunk:]
      source.data = source.data[:-chunk]
      chunk = nextchunk
    return

  def _getrec(self, subview):
    res = []
    ga = getattr
    for name in self.names:
      res.append(ga(subview, name))
    return res
    
  def __del__(self):
    del self.view

view_struc = "big_test[A:I,B:I,C:I,D:I,E:I,F:I,G:I,H:I,J:I,K:I]"

dbpath = "_bigfile.mk"

n_fields = 10

rand_len = 4096 + n_fields

random_ints = map(lambda x:whrandom.randint(0,256), range(rand_len))

blocksize = 10000 # default size for append, but not mandatory

ds = None

db = big_mk(dbpath, 1)

ds = db.getas(view_struc)

def make_rec(idx):
  return random_ints[idx:idx+n_fields]
  
def add_recs(n=1000):
  for i in range(n):
    idx = len(ds) % rand_len
    ds.append(make_rec(idx))

if __name__ == '__main__':
    # expect this to take hours (1000..2000 recs/sec on modern PII)
    import sys
    for i in xrange(25000):
        add_recs()
        sys.stdout.write(".")
        sys.stdout.flush()
        if i % 50 == 49:
            db.commit()
            sys.stdout.write("\n")
