# Wrapping Python data as Metakit views
#
# Expected output (3 times):
#    a      b      c
#    -----  -----  -
#    one    un     1
#    two    deux   2
#    three  trois  3
#    -----  -----  -
#    Total: 3 rows

import metakit

class C:
    def __init__(self, a, b, c):
    	self.a, self.b, self.c = a, b, c

dc = [	C ('one', 'un', 1),
	C ('two', 'deux', 2),
	C ('three', 'trois', 3)  ]

dd = [	{'a':'one', 'b':'un', 'c':1}, 
	{'a':'two', 'b':'deux', 'c':2}, 
	{'a':'three', 'b':'trois', 'c':3}  ] 

dt = [	('one', 'un', 1), 
	('two', 'deux', 2), 
	('three', 'trois', 3)  ] 

pl = [	metakit.property('S','a'),
	metakit.property('S','b'),
	metakit.property('I','c')  ]

vc = metakit.wrap(dc, pl)
vd = metakit.wrap(dd, pl)
vt = metakit.wrap(dt, pl, 1)

metakit.dump(vc, 'class objects:')
metakit.dump(vd, 'dictionary elements:')
metakit.dump(vt, 'tuples (by position):')
