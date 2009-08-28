package require Tk
package require Diagrams
namespace import ::Diagrams::*

pack [canvas .c -width 200 -height 120 -bg white]
drawin .c

direction southeast
currentpos [position 20 20]
arrow "" 40
box " A "

direction southeast
currentpos [position 120 20]
arrow "" 40
attach northwest
box " B "

saveps anchor.ps
