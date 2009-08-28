# Demo of the new 2.3 self-referential view structures.
#
# Output:
# 0
# 00000 1
# 11111 1
# 22222 0
# [Property('S', 's'), Property('V', 'sub')]
# 00000 1
# 11111 1
# 22222 0
# [Property('S', 's'), Property('I', 'i'), Property('V', 'sub')]
# ok

import os
try: os.remove("_selfref.mk")
except: pass

import metakit
db = metakit.storage('_selfref.mk',1)
vw = db.getas('data[s:S,sub[^]]')

print len(vw)

v = vw
for i in range(3):
  v.append(s=`i`*5)
  v = v[0].sub

def show(vw):
  for v in [vw[0], vw[0].sub[0], vw[0].sub[0].sub[0]]:
    print v.s, len(v.sub)
  print vw[0].sub[0].sub[0].sub.structure()
  assert len(vw[0].sub[0].sub[0].sub) == 0
  try:
    metakit.dump(vw[0].sub[0].sub[0].sub[0].sub)
  except IndexError:
    pass

show(vw)

# note that (recursive!) on-the-fly restructuring works
show(db.getas('data[s:S,i:I,sub[^]]'))

db.commit()
print 'ok'

os.remove("_selfref.mk")
