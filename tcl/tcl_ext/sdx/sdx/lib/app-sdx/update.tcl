# Update starkit using the starsync mechanism
# Jan 2003, jcw@equi4.com

package require starsync

# support automatic proxying
package require autoproxy
autoproxy::init

set server http://localhost/sync.cgi
#set server http://www.equi4.com/sync.cgi

set n [lsearch -exact $argv -from]
if {$n >= 0} {
  set m $n
  set server [lindex $argv [incr n]]
  set argv [lreplace $argv $m $n]
}

set fake 0
if {[string match -n* [lindex $argv 0]]} {
  set fake 1
  set argv [lrange $argv 1 end]
}

if {[llength $argv] != 1} {
  puts stderr "usage: $argv0 ?-from server? ?-n? starkit"
  exit 1
}

set path [lindex $argv 0]
set before [file exists $path]
set name [string toupper [file root [file tail $path]]]

if {$fake} {
  if {$before} {
    puts " $name: comparing with $server ..."
  } else {
    puts " $name: looking up on $server ..."
  }
} else {
  if {$before} {
    puts " $name: updating from $server ..."
  } else {
    puts " $name: fetching from $server ..."
  }
}

set catalog [starsync::request $server $path $fake]
set listing [starsync::summary $catalog]

set after [file exists $path]
set nothing [expr {[llength $listing] == 0}]

if {$fake} {
  if {$nothing} {
    if {$before} {
      puts "  Up to date."
    } else {
      puts stderr "* File not found."
    }
  } else {
    puts "  [expr {[llength $listing]/2}] differences:"
    foreach {x y} $listing {
      puts [format {%10s  %s} $y $x]
    }
  }
} else {
  if {$after} {
    if {$nothing} {
      puts "  No change."
    } elseif {$before} {
      puts "  [expr {[llength $listing]/2}] changes applied."
    } else {
      puts "  File created."
    }
  } else {
    puts stderr "* File not found."
  }
}
