# qwrap - wrap a stand-alone tclkit script into a Starkit
#
# Equivalent to wrap, but this creates a temporary .vfs directory
# structure.  It takes a single argument, either the name of the
# tcl script (with or without the .tcl) or a URL referring to it (uses
# the VFS layer to retrieve if necessary
#
# by Steve Landers <steve@digital-smarties.com>

package require vfs


proc usage {{msg ""}} {
   puts stderr "usage: qwrap file ?name? ?-runtime file? ?-asdir?"
   puts $msg
   exit 1
}

unset -nocomplain file asfile
set asdir 0

for {set i 0} {$i < [llength $argv]} {incr i} {
   switch -- [lindex $argv $i] {
      -runtime {
	 set runtime [::vfs::filesystem fullynormalize [lindex $argv [incr i]]]
      }
      -asdir {
	set asdir 1
      }
      default {
	 if {![info exist file]} {
	    set file [lindex $argv $i]
	 } elseif {![info exist asfile]} {
	    set asfile [lindex $argv $i]
	 } else {
	    usage "too many files specified"
	 }
      }
   }
}

#set file [lindex $argv 0]
set type [lindex [split $file :] 0]
switch -- $type {
    ftp -
    http {
        vfs::urltype::Mount $type
    }
    zip -
    mk4 -
    ns {
        puts stderr "file type of $type not supported"
        exit 1
    }
}

# read program
if {$file == "-"} {
    set prog [read -nonewline stdin]
} else {
    if {![file exists $file] } {
        puts stderr "$file doesn't exist"
        exit 1
    }
    set fd [open $file]
    set prog [read -nonewline $fd]
    close $fd
}

# derive Starkit name from input file, or 2nd arg if specified
if {[info exists asfile]} {
    set file $asfile
} 
set sd [file rootname [file tail $file]]

# create temp .vfs structure - use the .critcl for want of something better
set tmpdir [file normalize [file join ~ .critcl $sd.vfs]]
set lib [file join $tmpdir lib]

file delete -force $tmpdir
file mkdir [file join $lib app-$sd]

# $sd.vfs/main.tcl
set out [open [file join $tmpdir main.tcl] w]
puts $out "
  package require starkit
  starkit::startup
  package require app-$sd
"
close $out

# $sd.vfs/lib/app-...
if {![regexp "package\\s+provide\\s+app-$sd" $prog]} {
    set prog "package provide app-$sd 1.0\n\n$prog"
}
set out [open [file join $lib app-$sd $sd.tcl] w]
puts $out $prog
close $out

# find package version
set re [string map [list % $sd] {package\s+provide\s+app-%\s+([0-9\.]+)}]
if {![regexp $re $prog - version]} {
    set version 1.0
}

# create pkgIndex.tcl
set out [open [file join $lib app-$sd pkgIndex.tcl] w]
puts $out [string map [list % $sd # $version] {
  package ifneeded app-% # [list source [file join $dir %.tcl]]
}]
close $out

if {$asdir} {
  if {[catch { file rename $tmpdir $sd.vfs } err]} {
    file delete -force $tmpdir
    puts error $err
    exit 1
  }
} else {
  set here [pwd]
  # now pass to wrap 
  cd [file dirname $tmpdir]

  if {[info exists runtime]} {
     set argv [list $sd -runtime $runtime]
     source [file join [file dirname [info script]] wrap.tcl]
     # copy back to location where sdx invoked
     file rename -force $sd [file join $here $sd]
  } else {
     set argv [list $sd]
     source [file join [file dirname [info script]] wrap.tcl]
     # copy back to where sdx was invoked but make it have the kit extension
     file rename -force $sd [file join $here $sd.kit]
  }

  # clean up
  file delete -force $tmpdir
  cd $here
}
