# unwrap - unpack starkits to a file system tree
#
# Usage: unwrap starkit
#
# A directory called "starkit.vfs" is created and filled with all
# files present in the input starkit.  The dir must not exist.
#
# 06-10-2000: files are now extracted to the current directory
#	      so "unwrap ../mystar.kit" will end up in "./mystar.vfs/"
#
# by Jean-Claude Wippler <jcw@equi4.com>

package require vfs::mk4

if {[llength $argv] != 1} {
    puts stderr "Usage: $argv0 starkit"
    exit 1
}

set skit $argv
set odir [file root [file tail $skit]].vfs ;# 06-10-2000

if {[file exists $odir] || [catch {file mkdir $odir}]} {
    puts stderr "Cannot create '$odir' directory"
    exit 1
}

vfs::mk4::Mount $skit skdb -readonly

set argv [list -verbose 0 -noerror 0 skdb $odir]
source [file join [file dirname [info script]] sync.tcl]
