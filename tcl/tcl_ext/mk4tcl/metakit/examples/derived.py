# Demo of derived view dynamics
#
# Output:
# [1, 2, 3, 4, 5, 6]
# [2, 3, 4, 5]
# [2, 4, 5]
# [2, 5]
# [1, 2, 5, 6]

import metakit
db = metakit.storage()
vw = db.getas('data[value:I]')

def fill(l):
  vw[:] = []
  for i in l:
    vw.append(value=i)

def show(v):
  print map((lambda x: x.value), v)

fill([1,2,3,4,5,6])
show(vw)

  # select values in range [2..5]
v2 = vw.select({'value':2},{'value':5})
show(v2)

  # a deletion in original "vw" affects derived "v2"
vw.delete(2)
show(v2)

  # deletion in derived "v2" affects both (new in 2.3)
v2.delete(1)
show(v2)
show(vw)
