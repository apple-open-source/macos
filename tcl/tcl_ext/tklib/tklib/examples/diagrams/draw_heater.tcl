package require Tk
package require Diagrams
namespace import ::Diagrams::*

proc ::Diagrams::bracket {dir dist begin end} {
    variable state

    # Ad hoc!
    set coords [lrange $begin 1 2]
    if { $dir == "west" } {
       lappend coords [expr {[lindex $begin 1]-$dist}] [lindex $begin 2]
       lappend coords [expr {[lindex $begin 1]-$dist}] [lindex $end 2]
    }
    if { $dir == "east" } {
       lappend coords [expr {[lindex $begin 1]+$dist}] [lindex $begin 2]
       lappend coords [expr {[lindex $begin 1]+$dist}] [lindex $end 2]
    }
    if { $dir == "south" } {
       lappend coords [lindex $begin 1] [expr {[lindex $begin 2]+$dist}]
       lappend coords [lindex $end 1]   [expr {[lindex $begin 2]+$dist}]
    }
    if { $dir == "north" } {
       lappend coords [lindex $begin 1] [expr {[lindex $begin 2]-$dist}]
       lappend coords [lindex $end 1]   [expr {[lindex $begin 2]-$dist}]
    }
    lappend coords [lindex $end 1] [lindex $end 2]

    $state(canvas) create line $coords -arrow last
    # TODO: nice diagram object
}
namespace eval ::Diagrams {
    namespace export bracket
}
namespace import ::Diagrams::bracket

pack [canvas .c -width 300 -height 300 -bg white]
drawin .c

currentpos [position 100 20]
circle "Start"
direction south
arrow "" 40

#
# Store the object for later reference
#
set d [diamond "T > Tset?"]
arrow "no" 40
set b [box "Heat during\n1 minute"]
#
# Note: the order of direction/currentpos is important :(
#
direction east
currentpos [getpos E $d]

arrow "yes" 40
circle "Stop"

bracket west 30 [getpos W $b] [getpos W $d]

saveps heater.ps
