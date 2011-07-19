
# recursive directory listing, a bit like "ls -lR"
  proc listall {dirs} {
    while {[llength $dirs] > 0} {
      set dir [lindex $dirs 0]
      set dirs [lrange $dirs 1 end]
      puts "\n$dir:"
      set entries [glob -nocomplain [file join $dir *]]
      #if {[llength $entries] > 0} { puts "" }
      foreach path [lsort $entries] {
	if {[file isdir $path]} {
	  set len "         "
	  set tim "                 dir"
	  set suf "/"
	  lappend dirs $path
	} else {
	  set len [format %10d [file size $path]]
	  set tim [clock format [file mtime $path] -format {%Y/%m/%d %H:%M:%S}]
	  set suf ""
	}
	puts "$len  $tim  [file tail $path]$suf"
      }
    }
    puts ""
  }

set fname [lindex $argv 0]
if {[llength $argv] != 1 || ![file exists $fname]} {
    puts stderr "Usage: $argv0 file"
    exit 1
}

vfs::mk4::Mount $fname $fname -readonly
listall $fname
vfs::unmount $fname
