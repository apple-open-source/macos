#! /usr/bin/env tclkit

# adjust mod times in dir trees to match most recent files inside

if {$argv == ""} {
  puts stderr "Usage: $argv0 dirs..."
  exit 1
}

proc setdate {dir} {
  set t 0
  # do not traverse sym links, only do this for real dirs
  if {[catch { file readlink $dir }]} {
    set n [clock seconds]
    foreach x [glob -nocomplain $dir/* $dir/.*] {
      set m 0
      if {[file isdir $x]} {
	# 2007-04-07: also skip CVS and .svn dirs
	switch -- [file tail $x] . - .. - CVS - .svn continue
	set m [setdate $x]
      } elseif {[file isfile $x]} {
	set m [file mtime $x]
      }
      if {$m > $t && $m < $n + 900} { # allow some skew
	set t $m
      }
    }
    if {$t > 0 && $t != [file mtime $dir]} {
      if {[catch { file mtime $dir $t } err]} {
        puts "\n  $dir: $err"
      } else { 
	puts -nonewline +
      }
    } else {
      puts -nonewline .
    }
  }
  return $t
}

fconfigure stdout -buffering none

foreach x $argv {
  if {[catch {
    setdate $x
    puts ""
  } err]} {
    puts stderr "$x: $err"
  }
}
