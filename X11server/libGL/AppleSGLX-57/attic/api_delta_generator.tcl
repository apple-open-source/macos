
proc main {argc argv} {
    if {2 != $argc} {
	puts stderr "syntax is: [info nameofexecutable] $::argv0 api.list gliDispatch.list"
	exit 1
    }
    
    set fd [open [lindex $argv 0] r]
    set data [read $fd]
    close $fd

    foreach line [split $data \n] {
	set gliname [lindex $line 3]
	if {"" eq $gliname} continue
	set a($gliname) 1
    }

    set fd [open [lindex $argv 1] r]
    set data [read $fd]
    close $fd

    set blist [split $data \n]

    foreach name $blist {
	if {"" eq $name} continue
	set b($name) 1
    }

    #Now find the differences between the 2 arrays.
    array set deltas {}

    foreach name [array names a] {
	if {[info exists b($name)]} {
	    set deltas($name) BOTH
	} else {
	    set deltas($name) [lindex $argv 0]
	}
    }
    
    foreach name [array names b] {
	if {![info exists a($name)]} {
	    set deltas($name) [lindex $argv 1]
	}
    }

    parray deltas
    
}
main $::argc $::argv
