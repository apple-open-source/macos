# mkshow.tcl -- Metakit file show utility
# $Id: mkshow.tcl 1230 2007-03-09 15:58:53Z jcw $
# This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

package require Mk4tcl

proc dumpView {db view {prop ""}} {
  array set fields {}
  set desc {}
  set work {}
  
    # a property list of the form "-file" causes reading from file
  if {[regexp {^-(.*)} $prop - file]} {
    if {$file == ""} { set fd stdin } else { set fd [open $file] }
    set prop {}
    while {[gets $fd line] >= 0} {
      lappend prop $line
    }
    if {$file != ""} { close $fd }
  }

    # set up datatypes, build property list if none given 
  foreach name [mk::view info $db.$view] {
    if {![regexp {^(.*):(.)} $name - name type]} {
      set type S
    }
    switch $type {
      M   { set format {fn:(%db)} }
      V   { set format {s:[#%d]} }
      default { set format f }
    }
    set fields($name) $format
    lappend desc $name
  }

    # expand all fields if there is an empty entry
  set n [lsearch -exact $prop ""]
  if {$prop == "" || $n >= 0} {
    set prop [eval lreplace [list $prop] $n $n $desc]
  }

    # split off commands and formatting details into a work list
  foreach name $prop {
    if {![regexp {^([^/]*)/(.*)} $name - name actions]} {
      set actions ""
    }
    if {$actions == "" && [info exists fields($name)]} {
      set actions $fields($name)
    }
    if {![regexp {^([^:]*):(.*)} $actions - actions format]} {
      set format ""
    }
    lappend work $name [split $actions ""] $format
  }

    # go through each row in the view
  mk::loop c $db.$view {
    set r {}
    foreach {s a f} $work {
      set gmt 0
      foreach x $a {
        switch -- $x {

          f { set s [mk::get $c $s] }
          n { set s [string length $s] }
          s { set s [mk::view size $c.$s] }

          g { set gmt [expr {1-$gmt}] }
          d { set s [clock format $s -format {%Y/%m/%d} -gmt $gmt] }
          t { set s [clock format $s -format {%H:%M:%S} -gmt $gmt] }

          k { set s [expr {($s+1023)/1024}] }
          x { binary scan $s H* s }
          h { set s [join [hexDump $s] \n] }

          z { package require Trf; set s [zip -mode d $s] }
          b { package require Trf; set s [bz2 -mode d $s] }

          l { puts $r; set r {} }

          i { set s [mk::cursor pos c] }
                    p { set s $desc }
                    v { set s $view }
        }
      }
      if {$f != ""} {
        set s [format $f $s]
      }
      lappend r $s
    }
    if {[llength $r] > 0} {
      puts [join $r "  "]
    }
  }
}

proc hexDump {s} {
    set r {}
    set n [string length $s]
    for {set i 0} {$i < $n} {incr i 16} {
        set t [string range $s $i [expr {$i + 15}]]
        regsub -all {[^ -~]} $t {.} u
        binary scan $t H* t
        lappend r [format {%8d:  %-32s  %-16s} $i $t $u]
    }
    return $r
}

set USAGE "  Usage: mkshow file view ?prop ...?

  file  is the name of the Metakit datafile
  view  is a view or subview description (e.g. 'view/123.subview')
  prop  lists of properties (all if omitted, from file if '-file')
  
  The properties may not contain a ':X' type specifier, but they may
  contain a list of action specifiers (i.e. 'prop/abc...').  Each is
  applied to the result so far.  An optional ':...' introduces a format.
  An empty argument is expanded to the list of all fields in the view.
  
  Examples:
  mkshow file view         -- one line per row, all properties
  mkshow file view /i:%5d: \"\"    -- prefix each line with the row #
  mkshow file view :blah \"\" /l     -- dump each row as a Tcl command
  mkshow file view name:%-12s    -- just the name, left, fixed width
  mkshow file view name/fz:%.100s  -- unzip, then show first 100 chars
  mkshow file view date/fd date/ft -- show field as date and as time

  For a description of all actions specifiers, type 'mkshow -actions'.
"

set ACTIONS "  Action codes available in mkshow:

  f fetch the property value (this is usually the first action)
  n returns size of the property value (default for :B and :M)
  s returns the number of rows in the subview
  
  g set gmt mode for following 'd' and 't' actions
  d converts int seconds to a YYYY/MM/DD value
  t converts int seconds to a HH:MM:SS value
  
  k convert int value to 'kilo' (i.e. '(N+1023)/1024')
  x convert string value to hexadecimal form
  h convert string value to a pretty-printed hex dump
  
  z uncompress string value using Trf's zip
  b uncompress string value using Trf's bz2
  
  i returns the row number (used without property, i.e. '/i:%5d:')
  p returns list of all properties (used without property: '/p')
  v returns the input view path (used without property: '/v')

  l generate output in Tcl list format (must be last arg: '/l')
  : the remainder of this argument specifies a format string
"

if {[llength $argv] < 2} {
    switch -glob -- [lindex $argv 0] {
        -a*       { puts stderr $ACTIONS }
        default { puts stderr $USAGE }
  }
  exit 1
}

set file [lindex $argv 0]
set view [lindex $argv 1]
set prop [lrange $argv 2 end]

mk::file open db $file -readonly

dumpView db $view $prop
