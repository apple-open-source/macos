# A small demonstration: drawing chemical structures
#
# NOTE: needs fixing!
#
package require Tk
package require Diagrams
namespace import ::Diagrams::*

pack [canvas .c -width 400 -height 130 -bg white]

#console show
drawin .c

proc ring {} {
   set side 20
   line $side 60 $side 0 $side -60 $side -120 $side 180 $side 120
}
proc benzene {} {
   set item [ring]

   foreach {x1 y1 x2 y2} [lindex $item 2] {break}

   $::Diagrams::state(canvas) create oval \
      [expr {($x1+$x2)/2-12}] [expr {($y1+$y2)/2+12}] \
      [expr {($x1+$x2)/2+12}] [expr {($y1+$y2)/2-12}]

   return $item
}

proc bond { {angle 0} {item {}} } {
   set side 20

   set anchor E
   switch -- $angle {
   "0"   { direction E  ; set anchor E  }
   "60"  { direction NE ; set anchor NE }
   "90"  { direction N  ; set anchor N  }
   "120" { direction NW ; set anchor NW }
   "180" { direction W  ; set anchor W  }
   "240" { direction SW ; set anchor SW }
   "-90" -
   "270" { direction S  ; set anchor S  }
   "-60" -
   "300" { direction SE ; set anchor SE }
   }

   if { $item != {} } {
      currentpos [getpos $anchor $item]
   }

   line $side $angle
}

#
# Very primitive chemical formula
# -- order of direction/currentpos important!
#
direction east
currentpos [position 40 60]
benzene; bond;

set Catom [plaintext C]

bond 90 $Catom
direction north
plaintext C
direction east
plaintext OOH

bond -90 $Catom
direction south
plaintext H

bond 0 $Catom
direction east
benzene
bond; direction east
plaintext NH2  ;# UNICODE \u2082 is subscript 2, but that is not
                # supported in PostScript

saveps chemical.eps
