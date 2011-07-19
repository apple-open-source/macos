
proc ls {args} {
    set short 1
    while {[llength $args] > 0} {
	set arg [lindex $args 0]
	switch -glob -- $arg {
	-*l*	{set short 0}
	-*	{}
	default	{break}
	}
	set args [lrange $args 1 end]
    }
    if {[llength $args] == 0} {set args .}

    set ret {}
    foreach path $args {
	catch {vfs::auto $path -readonly}
	if {![file isdirectory $path]} {
	    lappend ret [_ls1 $path $short]
	}
    }
    foreach path $args {
	if {[file isdirectory $path]} {
	    if {[llength $args] > 1} {
		if {[llength $ret] > 0} {lappend ret {}}
		lappend ret "$path:"
	    }
	    set list [glob -nocomplain [file join $path *] [file join $path .*]] 
	    foreach file [lsort -dictionary $list] {
		set tail [file tail $file]
		if {$tail == "." || $tail == ".."} {continue}
		lappend ret [_ls1 $file $short]
	    }
	}
    }
    return [join $ret \n]
}
proc _ls1 {path {short 0}} {
    if {$short} {
	return [file tail $path]
    }
    if {[file type $path] eq "link"} {
      return [format {%54s %s} "(broken symlink)" [file tail $path]]
    }
    file stat $path sb
    #drwxr-xr-x    3 888      999           21 May 13 19:46 vjscdk
    return [format {%s %4d %-8s %-8s %7d %s %s} \
	[fmode sb] $sb(nlink) $sb(uid) $sb(gid) $sb(size) \
	[clock format $sb(mtime) -format {%b %d %H:%M} -gmt 1] \
	[file tail $path]]
}

proc fmode arr { # From Richard Suchenwirth, bag of algorithms, file mode
    upvar 1 $arr sb

    if {$sb(type) == "directory"} { set pfx "d" } else { set pfx "-" }

    set s [format %03o [expr $sb(mode)%512]]
    foreach i {  0   1   2   3   4   5   6   7} \
	    j {--- --x -w- -wx r-- r-x rw- rwx} {
	regsub -all $i $s $j s
    }
    return $pfx$s
}

if {[catch [concat ls $argv] ret]} {
    puts stderr $ret
    exit 1
}

puts stdout $ret
