# Small demo of v1.remapwith(v2)
#
# Expected output:
#  s    
#  -----
#  one  
#  three
#  five 
#  two  
#  four 
#  one  
#  three
#  five 
#  -----
#  Total: 8 rows

import metakit

db = metakit.storage()
v1 = db.getas("counts[s:S]")
v2 = db.getas("map[i:I]")

for v in ['zero','one','two','three','four','five']:
  v1.append(s=v)
for v in [1,3,5,2,4,1,3,5]:
  v2.append(i=v)

metakit.dump(v1.remapwith(v2))
