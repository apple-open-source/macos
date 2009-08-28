
proc main {} {
    set fd [open dri_dispatch.defs r]
    set data [read $fd]
    close $fd

    set api [list]

    foreach {match exact} [regexp -all -inline {\((.*?)\)[^\n]*\n} $data] {
	#puts EXACT:$exact
	if {[llength $exact] != 4} {
	    continue
	}

	set rettype [lindex $exact 0]
	set fnsuffix [lindex $exact 1]
	#skip offset
	set argpat [split [lindex $exact 3] ,]
	set argpatfinal [list]

	foreach i $argpat {
	    lappend argpatfinal [string trim $i]
	}
	
	lappend api [list $rettype $fnsuffix $argpatfinal] 
    }

    foreach i $api {
	puts $i
    }
    
}
main
