# -*- tcl -*-
# sak::doc - Documentation facilities

package require sak::util

namespace eval ::sak::doc {
    set here [file dirname [file normalize [info script]]]
}

# ###
# API commands

proc ::sak::doc::validate {modules} {Gen null  null $modules}
proc ::sak::doc::html     {modules} {Gen html  html $modules}
proc ::sak::doc::nroff    {modules} {Gen nroff n    $modules}
proc ::sak::doc::tmml     {modules} {Gen tmml  tmml $modules}
proc ::sak::doc::text     {modules} {Gen text  txt  $modules}
proc ::sak::doc::wiki     {modules} {Gen wiki  wiki $modules}
proc ::sak::doc::latex    {modules} {Gen latex tex  $modules}

proc ::sak::doc::imake {modules} {
    global base ; # SAK environment, set up for each cmd.
    set idxfile [IDX]

    set top [file normalize $base]

    set manpages {}
    foreach page [fileutil::findByPattern $top *.man] {
	lappend manpages [fileutil::stripPath $top $page]
    }
    fileutil::writeFile $idxfile [join $manpages \n]\n
    return
}

proc ::sak::doc::ishow {modules} {
    set idxfile [IDX]

    if {[catch {
	set manpages [fileutil::cat $idxfile]
    } msg]} {
	puts stderr "Unable to use manpage listing '$idxfile'\n$msg"
    } else {
	puts -nonewline $manpages
    }
    return
}

proc ::sak::doc::IDX {} {
    variable here
    getpackage fileutil fileutil/fileutil.tcl
    return [file join $here manpages.txt]
}

proc ::sak::doc::dvi {modules} {
    latex $modules
    file mkdir [file join doc dvi]
    cd         [file join doc dvi]

    foreach f [lsort -dict [glob -nocomplain ../latex/*.tex]] {

	set target [file rootname [file tail $f]].dvi
	if {[file exists $target] 
	    && [file mtime $target] > [file mtime $f]} {
	    continue
	}

	puts "Gen (dvi): $f"
	exec latex $f 1>@ stdout 2>@ stderr
    }
    cd ../..
    return
}

proc ::sak::doc::ps {modules} {
    dvi $modules
    file mkdir [file join doc ps]
    cd         [file join doc ps]
    foreach f [lsort -dict [glob -nocomplain ../dvi/*.dvi]] {

	set target [file rootname [file tail $f]].ps
	if {[file exists $target] 
	    && [file mtime $target] > [file mtime $f]} {
	    continue
	}

	puts "Gen (ps): $f"
	exec dvips -o $target $f >@ stdout 2>@ stderr
    }
    cd ../..
    return
}

proc ::sak::doc::pdf {modules} {
    dvi $modules
    file mkdir [file join doc pdf]
    cd         [file join doc pdf]
    foreach f [lsort -dict [glob -nocomplain ../ps/*.ps]] {

	set target [file rootname [file tail $f]].pdf
	if {[file exists $target] 
	    && [file mtime $target] > [file mtime $f]} {
	    continue
	}

	puts "Gen (pdf): $f"
	exec ps2pdf $f $target >@ stdout 2>@ stderr
    }
    cd ../..
    return
}

proc ::sak::doc::list {modules} {
    Gen list l $modules
    
    set FILES [glob -nocomplain doc/list/*.l]
    set LIST  [open [file join doc list manpages.tcl] w]

    foreach file $FILES {
        set f [open $file r]
        puts $LIST [read $f]
        close $f
    }
    close $LIST

    eval file delete -force $FILES
    return
}

# ### ### ### ######### ######### #########
## Implementation

proc ::sak::doc::Gen {fmt ext modules} {
    global distribution
    global tcl_platform

    getpackage doctools doctools/doctools.tcl

    set null   0 ; if {![string compare $fmt null]} {set null   1}
    set hidden 0 ; if {![string compare $fmt desc]} {set hidden 1}

    if {!$null} {
	file mkdir [file join doc $fmt]
	set prefix "Gen ($fmt)"
    } else {
	set prefix "Validate  "
    }

    foreach m $modules {
	set mpath [sak::util::module2path $m]

	::doctools::new dt \
		-format $fmt \
		-module $m

	set fl [glob -nocomplain [file join $mpath *.man]]

	if {[llength $fl] == 0} {
	    dt destroy
	    continue
	}

	foreach f $fl {
	    if {!$null} {
                set target [file join doc $fmt \
                                [file rootname [file tail $f]].$ext]
                if {[file exists $target] 
                    && [file mtime $target] > [file mtime $f]} {
                    continue
                }
	    }
	    if {!$hidden} {puts "$prefix: $f"}

	    dt configure -file $f
	    if {$null} {
		dt configure -deprecated 1
	    }

	    set fail [catch {
		set data [dt format [get_input $f]]
	    } msg]

	    set warnings [dt warnings]
	    if {[llength $warnings] > 0} {
		puts stderr [join $warnings \n]
	    }

	    if {$fail} {
		puts stderr $msg
		continue
	    }

	    if {!$null} {
		write_out $target $data
	    }
	}
	dt destroy
    }
}

# ### ### ### ######### ######### #########

package provide sak::doc 1.0

##
# ###
