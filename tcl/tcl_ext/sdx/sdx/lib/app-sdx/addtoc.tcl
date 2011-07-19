package require md5

proc calcDigest {file} {
    set fd [open $file]
    fconfigure $fd -trans binary
    set sum [md5::md5 [read $fd]]
    close $fd
    return $sum
}

proc walk {fd {dir .} {pre ""}} {
  global count size
  set g [lsort -dictionary [glob -nocomplain [file join $dir *]]]
  foreach x $g {
    set t [file tail $x]
    if {[file isfile $x] && "$pre$t" != "TOC"} {
      set s [file size $x]
      incr count
      incr size [expr {($s+1023)/1024}]
      if {[file executable $x]} { append s " x" }
      puts $fd "$pre[file mtime $x] [calcDigest $x] $t $s"
    }
  }
  foreach x $g {
    if {[file isdir $x]} {
      puts $fd "$pre/[file tail $x]"
      walk $fd $x "$pre "
    }
  }
}

set dir [lindex $argv 0]

if {$dir == ""} {
  puts stderr "usage: addtoc dir"
  exit 1
}

set fn [file join $dir TOC]

set fd [open $fn w]
fconfigure $fd -trans binary

set count 0
set size 0

walk $fd $dir

close $fd

puts " File '$fn' created ($count files, total $size Kb)"
