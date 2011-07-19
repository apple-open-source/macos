# -*- tcl -*-

package require Tk
package require cache::async
package require struct::set

namespace eval ::city {

    proc block {n} { variable part ; return [expr {$n * $part}] }

    variable tessel 64
    variable part   [expr {$tessel/8}]
    variable cstart [block 2]
    variable cend   [block 6]
    variable rstart [block 3]
    variable rend   [block 5]

    variable parcel [image create photo -height $part -width $part]
    $parcel put black -to 0 0 $part $part

    variable tilecache [cache::async tc ::city::Gen]

    variable lego  {}
    variable neigh ; array set neigh {} ; # name,dir -> list(name)
    variable map   ; array set map   {} ; # name -> (type flags)
    variable grid  ; array set grid  {} ; # at -> name
}

proc ::city::tile {} {
    variable tessel
    return  $tessel
}

proc ::city::grid {__ at donecmd} {
    Tile get [Randomize $at] [list ::city::ToGrid $at $donecmd --]
    return
}
proc ::city::ToGrid {at donecmd -- what key args} {
    # Route the cache result retrieved by name to the grid cell the
    # original request came from.
    #puts "\tToGrid ($at) '$donecmd' $what ($key) <$args>"
    if {[catch {
	uplevel #0 [eval [linsert $args 0 linsert $donecmd end $what $at]]
    }]} { puts $::errorInfo }
}

proc ::city::Randomize {at} {
    variable grid
    set p [Possibilities $at]
    if {[llength $p] == 1} {
	set res [lindex $p 0]
    } else {
	set res [lindex $p [Rand [llength $p]]]
    }
    #puts "($at) = $p"
    set grid($at) $res
    return $res
}

proc ::city::Rand {n} {
    # 0...n-1
    # (0,1) -> (0,n)
    expr {int(rand()*$n)}
}

proc ::city::Possibilities {at} {
    variable lego
    variable grid
    foreach {y x} $at break

    set l [list [expr {$x - 1}] $y]
    set r [list [expr {$x + 1}] $y]
    set u [list $x [expr {$y - 1}]]
    set d [list $x [expr {$y - 1}]]

    set allowed $lego
    Cut $l r allowed
    Cut $r l allowed
    Cut $u d allowed
    Cut $d u allowed

    return $allowed
}

proc ::city::Cut {at dir v} {
    variable grid
    variable neigh
    upvar 1 $v allowed
    foreach {y x} $at break
    if {![info exists grid($at)]} return
    set allowed [struct::set intersect $allowed $neigh($grid($at),$dir)]
    return
}

proc ::city::Tile {__ name donecmd} {
    variable tilecache
    #puts "__ $name ($donecmd)"
    $tilecache get $name $donecmd
    return
}

proc ::city::Gen {__ name donecmd} {
    variable tessel
    variable cstart
    variable cend
    variable rstart
    variable rend
    variable parcel
    variable map

    #puts "\tGENERATE $name ($donecmd)"

    foreach {olx orx oux odx ilx irx iux idx cx} $map($name) break
    set tile [image create photo -height $tessel -width $tessel]
    $tile put white -to 0 0 $tessel $tessel
    #puts ([join $map($name) {)(}])|$olx|$orx|$oux|$odx|$ilx|$irx|$iux|$idx|$cx|
    if {$cx}  { $tile copy $parcel -to $rstart $rstart $rend   $rend   } ; # center

    if {$olx} { $tile copy $parcel -to 0       $rstart $cstart $rend   } ; # ou left
    if {$orx} { $tile copy $parcel -to $cend   $rstart $tessel $rend   } ; # ou right
    if {$oux} { $tile copy $parcel -to $rstart 0       $rend   $cstart } ; # ou up
    if {$odx} { $tile copy $parcel -to $rstart $cend   $rend   $tessel } ; # ou down

    if {$ilx} { $tile copy $parcel -to $cstart $rstart $rstart $rend   } ; # in left
    if {$irx} { $tile copy $parcel -to $rend   $rstart $cend   $rend   } ; # in right
    if {$iux} { $tile copy $parcel -to $rstart $cstart $rend   $rstart } ; # in up
    if {$idx} { $tile copy $parcel -to $rstart $cend   $rend   $cend   } ; # in down

    if 0 {
	set label $olx$orx$oux$odx/$ilx$irx$iux$idx/$cx
	#set label [string range $name 0 3]/[string range $name 4 7]/[string index $name 8]
	label .l$name -image $tile -bd 2 -relief sunken
	pack .l$name -side left
	tooltip::tooltip .l$name $label
    }

    #puts "run ([linsert $donecmd end set $name $tile])"
    uplevel #0 [linsert $donecmd end set $name $tile]
    return
}

proc ::city::Name {olx orx oux odx ilx irx iux idx cx} {
    #set name "$olx$orx$oux$odx$ilx$irx$iux$idx$cx"
    set name ""
    if {$cx}  { append name c } ; # center
    if {$olx} { append name l } ; # left
    if {$ilx} { append name - } ; # left
    if {$orx} { append name r } ; # right
    if {$irx} { append name _ } ; # right
    if {$oux} { append name u } ; # up
    if {$iux} { append name = } ; # up
    if {$odx} { append name d } ; # down
    if {$idx} { append name % } ; # down
    if {$name eq ""} { set name empty }
    #puts $name\ ...
    return $name
}

proc ::city::Init {} {
    variable lego
    variable neigh
    variable map

    foreach olx {0 1} {
	foreach orx {0 1} {
	    foreach oux {0 1} {
		foreach odx {0 1} {
		    foreach ilx {0 1} {
			foreach irx {0 1} {
			    foreach iux {0 1} {
				foreach idx {0 1} {
				    foreach cx {0 1} {
					# inner not allowed without center
					if {!$cx && $ilx} continue
					if {!$cx && $irx} continue
					if {!$cx && $iux} continue
					if {!$cx && $idx} continue

					#if {!$olx && $ilx} continue
					#if {!$orx && $irx} continue
					#if {!$oux && $iux} continue
					#if {!$odx && $idx} continue

					set n [Name $olx $orx $oux $odx $ilx $irx $iux $idx $cx]
					set map($n) [list $olx $orx $oux $odx $ilx $irx $iux $idx $cx]
					lappend bins(l$olx) $n
					lappend bins(r$orx) $n
					lappend bins(u$oux) $n
					lappend bins(d$odx) $n
					lappend lego $n
				    }
				}
			    }
			}
		    }
		}
	    }
	}
    }

    #puts /[llength $lego]
    
    # Now compute which tiles can be neighbours of what others, for
    # all four sides.

    foreach t $bins(d0) { foreach n $bins(u0) { lappend neigh($t,d) $n } }
    foreach t $bins(d1) { foreach n $bins(u1) { lappend neigh($t,d) $n } }
    foreach t $bins(l0) { foreach n $bins(r0) { lappend neigh($t,l) $n } }
    foreach t $bins(l1) { foreach n $bins(r1) { lappend neigh($t,l) $n } }
    foreach t $bins(u0) { foreach n $bins(d0) { lappend neigh($t,u) $n } }
    foreach t $bins(u1) { foreach n $bins(d1) { lappend neigh($t,u) $n } }
    foreach t $bins(r0) { foreach n $bins(l0) { lappend neigh($t,r) $n } }
    foreach t $bins(r1) { foreach n $bins(l1) { lappend neigh($t,r) $n } }

    foreach k [array names neigh] { set neigh($k) [lsort -unique $neigh($k)] }
    return
}

::city::Init
#exit
