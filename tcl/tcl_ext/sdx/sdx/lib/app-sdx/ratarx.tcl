#! /usr/bin/env tclkit

# ratar - reverse actions of tar (also "tar" spelt backwards :)
#
# This command deletes those files from a directory tree which have
# the same size and modification date as in a specified targz file.
# Empty directories are recursively deleted as final cleanup step.
#
# The logic is such that deletion is "safe": only files/dirs which
# are redundant (i.e. also present in the tar file) will be deleted.
#
# The following two commands together will usually be a no-op:
#	tar xfz mydata.tar.gz
#	ratar mydata.tar.fz
#
# Use the "-n" option to see what would be deleted without doing so.
#
# jcw, 30-3-2002

set fake [expr {[lindex $argv 0] == "-n"}]
if {$fake} { set argv [lrange $argv 1 end] }

set infile [lindex $argv 0]

if {![file isfile $infile]} {
  puts stderr "usage: $argv0 ?-n? targzfile"
  exit 1
}

set fd [open "| gzip -d < $infile"]
fconfigure $fd -translation binary -encoding binary

array set dirs {}

while {[set header [read $fd 512]] != ""} {
  binary scan $header A100A8A8A8A12A12A8A1A100A6A6A32A32A8A8A155 \
  			name mode uid gid size mtime cksum typeflag \
           		linkname ustar_p ustar_vsn uname gname devmaj \
	            	devmin prefix
		    
  if {$name != ""} {
    if {$mode & 040000} {
      set dirs($name) ""
    } elseif {$mode & 0100000 && [file isfile $name]} {
      if {[file size $name] != $size || [file mtime $name] != $mtime} {
	puts " different: $name"
      } elseif {$fake} {
        puts " rm $name"
      } else {
	file delete $name
      }
    }
  }

  if {$size != ""} {
    read $fd [expr {(($size+511)/512)*512}]
  }
}

close $fd

proc cleandir {{d ""}} {
  set f [expr {![info exists ::dirs($d/)]}]
  foreach x [glob -nocomplain [file join $d *]] {
    incr f [expr {[file isdir $x] ? [cleandir $x] : 1}]
  }
  if {$f == 0} { file delete $d }
  return $f
}

if {!$fake} cleandir
