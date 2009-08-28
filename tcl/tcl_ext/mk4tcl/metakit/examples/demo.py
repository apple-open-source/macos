# Simple demo, can be used to verify proper Mk4py installation

import Mk4py, sys
mk = Mk4py
print sys.argv[0], '- Mk4py', mk.version, '-', sys.platform

# On first run, output should consist of 5 lines:
#   John Lennon 44
#   Flash Gordon 42
#   Flash Gordon 42
#   John Lennon 44
#   John Lennon 44
# Each following run will generate 5 more lines of output

  # create a file called "demo.db"
db = mk.storage("demo.db",1)
  # define a view in it called "people", containing three fields
vw = db.getas("people[first:S,last:S,shoesize:I]")

  # let's append two rows to the end of the view
vw.append(first='John',last='Lennon',shoesize=44)
vw.append(first='Flash',last='Gordon',shoesize=42)

  # commit the structure and data to file
db.commit()

  # a simple loop to print out all rows
for r in vw:
  print r.first, r.last, r.shoesize

  # another way to loop, in sorted order
for r in vw.sort(vw.last):
  print r.first, r.last, r.shoesize

  # this loop iterates over a selection 
for r in vw.select(first='John'):
  print r.first, r.last, r.shoesize
