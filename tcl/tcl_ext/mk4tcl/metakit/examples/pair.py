# Small demo of v1.pair(v2)
#
# Expected output:
#  s    i
#  -----  -
#  zero 0
#  one  1
#  two  2
#  three  3
#  four 4
#  five 5
#  -----  -
#  Total: 6 rows

import metakit

db = metakit.storage()
v1 = db.getas("one[s:S]")
v2 = db.getas("two[i:I]")

for v in ['zero','one','two','three','four','five']:
  v1.append(s=v)
for v in range(6):
  v2.append(i=v)

metakit.dump(v1.pair(v2))
