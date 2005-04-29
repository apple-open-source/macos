# The new MK 2.3 commit-aside feature, in Python
#
# Expected output:
# 1 2 3 ok

import os
import Mk4py
mk = Mk4py

def t1(): # add several rows using commit-aside
  try: os.remove("_f.mk")
  except: pass
  try: os.remove("_f.mka")
  except: pass

  db = mk.storage("_f.mk", 1)
  vw = db.getas("a[i:I]")
  vw.append(i=111)
  vw.append(i=222)
  vw.append(i=333)
  db.commit()
  del db

  db = mk.storage("_f.mk", 0)
  vw = db.view("a")
  assert len(vw) == 3

  dba = mk.storage("_f.mka", 1)
  db.aside(dba)
  vw = db.view("a")
  assert len(vw) == 3

  vw.append(i=444)
  vw.append(i=555)
  assert len(vw) == 5

  db.commit()
  dba.commit()
  del db
  del dba

  db = mk.storage("_f.mk", 0)
  vw = db.view("a")
  assert len(vw) == 3 # now it has three rows
  assert vw[0].i == 111
  assert vw[1].i == 222
  assert vw[2].i == 333

  dba = mk.storage("_f.mka", 0)
  db.aside(dba)
  vw = db.view("a")
  assert len(vw) == 5 # now it has five :)
  assert vw[0].i == 111
  assert vw[1].i == 222
  assert vw[2].i == 333
  assert vw[3].i == 444
  assert vw[4].i == 555

def t2(): # add second property using commit-aside
  try: os.remove("_f.mk")
  except: pass
  try: os.remove("_f.mka")
  except: pass

  db = mk.storage("_f.mk", 1)
  vw = db.getas("a[i:I]")
  vw.append(i=111)
  db.commit()
  del db

  db = mk.storage("_f.mk", 0)
  vw = db.view("a")
  assert len(vw) == 1

  dba = mk.storage("_f.mka", 1)
  db.aside(dba)
  vw = db.getas("a[i:I,j:I]")
  assert len(vw) == 1

  vw.append(i=222,j=333)
  assert len(vw) == 2

  db.commit()
  dba.commit()
  del db
  del dba

  db = mk.storage("_f.mk", 0)
  vw = db.view("a")
  assert len(vw) == 1 # now it has one property and row
  assert len(vw.structure()) == 1
  assert vw[0].i == 111

  dba = mk.storage("_f.mka", 0)
  db.aside(dba)
  vw = db.view("a")
  assert len(vw) == 2 # now it has two of both :)
  assert len(vw.structure()) == 2
  assert vw[0].i == 111
  assert vw[0].j == 0
  assert vw[1].i == 222
  assert vw[1].j == 333

def t3(): # remove second property using commit-aside
  try: os.remove("_f.mk")
  except: pass
  try: os.remove("_f.mka")
  except: pass

  db = mk.storage("_f.mk", 1)
  vw = db.getas("a[i:I,j:I]")
  vw.append(i=111,j=222)
  db.commit()
  del db

  db = mk.storage("_f.mk", 0)
  vw = db.view("a")
  assert len(vw) == 1

  dba = mk.storage("_f.mka", 1)
  db.aside(dba)
  vw = db.getas("a[i:I]")
  assert len(vw) == 1

  db.commit()
  dba.commit()
  del db
  del dba

  db = mk.storage("_f.mk", 0)
  vw = db.view("a")
  assert len(vw) == 1
  assert len(vw.structure()) == 2 # now you see j
  assert vw[0].i == 111
  assert vw[0].j == 222

  dba = mk.storage("_f.mka", 0)
  db.aside(dba)
  vw = db.view("a")
  assert len(vw) == 1
  assert len(vw.structure()) == 1 # now you don't :)
  assert vw[0].i == 111

print 1,
t1()
print 2,
t2()
print 3,
t3()
print "ok"

os.remove("_f.mk")
os.remove("_f.mka")
