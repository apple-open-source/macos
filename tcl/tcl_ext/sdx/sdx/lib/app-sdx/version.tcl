# Determine "version" of a starkit, i.e. a unique signature
# Jan 2003, jcw@equi4.com

package require Tcl 8.5-
package require Trf

proc traverse {args} {
  # the following call will throw an error for non-mk files
  #set sig [mk::file end [lindex $args 0]]
  # cannot use the above, it depends on file size, i.e. it sees commit gaps
  # the real problem is that we need start i.s.o. end, which is not available
  set sig 0
  set mod 0
  while {[llength $args] > 0} {
    set d [lindex $args 0]
    set args [lrange $args 1 end]
    foreach path [lsort [glob -nocomplain [file join $d *]]] {
      set t [file tail $path]
      switch -- $t CVS - RCS - core - a.out continue
      lappend sig $t
      if {[file isdir $path]} {
	lappend args $path
      } else {
	set m [file mtime $path]
	if {$m > $mod} { set mod $m }
	lappend sig $m [file size $path]
      }
    }
  }
  binary scan [::crc-zlib [join $sig " "]] n c
  list $c $mod
}

proc showvers {fn {name ""}} {
  lassign [traverse $fn] sig mod
  set time [clock format $mod -format {%Y/%m/%d %H:%M:%S} -gmt 1]
  set v [format {%s  %d-%d  %s} $time [expr {(($sig>>16) & 0xFFFF) + 10000}] \
				      [expr {($sig & 0xFFFF) + 10000}] $name]
  puts $v
  return $mod
}

if {[llength $argv] == 0} {
  showvers $starkit::topdir
} else {
  if {[llength $argv] > 1 && [lindex $argv 0] eq "-fixtime"} {
    set fixtime 1
    set argv [lrange $argv 1 end]
  }

  if {[llength $argv] < 1} {
    puts stderr "usage: $argv0 ?-fixtime? file ..."
    exit 1
  }

  foreach fn $argv {
    if {[file isdir [file join $fn .]]} {
      set mod [showvers $fn $fn]
      if {[info exists fixtime] && $mod} {
	file mtime $fn $mod
      }
    } elseif {[file exists $fn]} {
      # the following call will throw an error for non-mk files
      if {[catch { mk::file end $fn }]} {
	puts stderr "$fn: not a starkit"
	return 0
      }
      # symlinks don't seem to work as mount point, so expand them
      set nf $fn
      catch { set nf [file normalize [file join \
      			[file dirname $fn] [file readlink $fn]]] }
      vfs::mk4::Mount $nf $nf -readonly
      set mod [showvers $nf $fn]
      vfs::unmount $nf
      if {[info exists fixtime] && $mod} {
	file mtime $fn $mod
      }
    } else {
      puts stderr "$fn: not found"
    }
  }
}
