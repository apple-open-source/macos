# Properties are case-insensitive, but this can lead to some
# surprising behavior: the first way a property is used will
# determine how it ends up in the global symbol table.
#
# Sample output:
#   Property('S', 'HeLLo') Property('S', 'HeLLo')
#   2
#   2
#   135099576
#   135033272
#   0

import metakit
db = metakit.storage()
v1 = db.getas('lo[HeLLo:S]')
v2 = db.getas('hi[hello:S]')

# surprise: this prints two mixed-case names
print v1.HeLLo, v2.hello

# this shows that the Metakit property is the same for both
# reason: there is a single global case-insensitive symbol table
print metakit.property('S','HeLLo').id
print metakit.property('S','hello').id

# this shows that the Python objects differ
# reason: these are two wrapper objects around the same thing
print id(metakit.property('S','HeLLo'))
print id(metakit.property('S','hello'))

# this causes a mismatch, it will have to be fixed one day
print metakit.property('S','HeLLo') == metakit.property('S','hello')
