# -*- tcl -*-
# (C) 2009 Andreas Kupries <andreas_kupries@users.sourceforge.net>
##
# ###

namespace eval ::sak::note {}

# ###

proc ::sak::note::usage {} {
    package require sak::help
    puts stdout \n[sak::help::on note]
    exit 1
}

proc ::sak::note::run {m p tags} {
    global distribution
    variable notes
    LoadNotes

    set k [list $m $p]
    set notes($k) $tags

    set f [file join $distribution .NOTE]
    set f [open $f w]
    foreach k [array names notes] {
	puts $f [list $k $notes($k)]
    }
    close $f
    return
}

proc ::sak::note::show {} {
    variable notes
    LoadNotes

    getpackage struct::matrix   struct/matrix.tcl

    struct::matrix M ; M add columns 3
    foreach k [lsort -dict [array names notes]] {
	M add row [linsert $k end $notes($k)]
    }
    puts "    [join [split [M format 2string] \n] "\n    "]\n"
    return
}

proc ::sak::note::LoadNotes {} {
    global distribution
    variable  notes
    array set notes {}

    catch {
	set f [file join $distribution .NOTE]
	set f [open $f r]
	while {![eof $f]} {
	    if {[gets $f line] < 0} continue
	    set line [string trim $line]
	    if {$line == {}} continue
	    foreach {k t} $line break
	    set notes($k) $t
	}
	close $f
    }

    return
}


##
# ###

package provide sak::note 1.0
