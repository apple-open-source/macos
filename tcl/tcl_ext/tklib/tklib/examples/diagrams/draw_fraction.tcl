package require Diagrams
namespace import ::Diagrams::*

pack [canvas .c -width 300 -height 80 -bg white]
drawin .c

direction east
currentpos [position 20 40]
box "A"
plaintext "divided by"
box "A+B"
plaintext " "
arrow "" 20
plaintext "      "
set pos [currentpos]
lset pos 2 15

direction south
currentpos $pos
plaintext "A"
usegap 0
set xc [lindex [currentpos] 1]
set xb [expr {$xc-15}]
set xe [expr {$xc+15}]
.c create line $xb [lindex [currentpos] 2] $xe [lindex [currentpos] 2]
plaintext "A+B"

saveps fraction.eps
