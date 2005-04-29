# Copyright (C) 1996-2000 Jean-Claude Wippler <jcw@equi4.com>
#
# common definitions needed by all tests

if {[info procs S] != ""} return

# All commands and globals in this test harness use 1-char uppercase letters:
#
# S pre post        define scripts to run before and after each test
# M msg             message to stdout
# E msg             flag test as bad, throw exception
# A cond            assertion check, exception if condition is not met
# C s1 s2           check that string results are the same
# F                 return a unique id, can be used as filename root
# R                 remove lingering files from last run of this test
# N script          set script name
# T num desc body   the main test proc 
# Q ?num?           end of tests, optional arg is number after final test
#
# array V           contains various test variable settings
# array F           contains lists of failures, per test
# array N           contains descriptive test names, per test

proc S {args} {
  global V F N
  
  set V(prereq)   $args
  set V(seqnum)   0
  set V(fileid)   0
  set V(keepit)   [info exists N]
  
  set V(os)     $::tcl_platform(os)
  set V(platform)   $::tcl_platform(platform)
  
  set V(unix)     [expr {$V(platform) == "unix"}]
  set V(windows)    [expr {$V(platform) == "windows"}]
  set V(macintosh)  [expr {$V(platform) == "macintosh"}]

  if {![info exists V(script)]} {
    N [info script]
  }
  
  array set F {}
  array set N {}
}

proc M {msg} {
  puts stdout $msg
}

proc E {msg} {
  global V F
  lappend F($V(script).$V(seqnum)) $msg
  error $msg
}

proc A {cond} {
  if {[catch {uplevel [list expr $cond]} err] || !$err} {
    E [list A $cond]
  }
}

proc C {s1 s2} {
  if {[catch {uplevel [list string compare $s1 $s2]} err] || $err != 0} {
    E [list C $s1 $s2]
  }
}

proc F {} {
  global V
  return _$V(script)_$V(seqnum)_[incr V(fileid)]
}

proc R {} {
  global V
  foreach f [glob -nocomplain _$V(script)_$V(seqnum)_*] {
    file delete $f
  }
}

proc N {script} {
  global V
  set V(script) [file root [file tail $script]]
}

proc T {num desc body} {
  global V F N
  
  set N($V(script).$num) $desc
  incr V(seqnum)
  A {$num == $V(seqnum)}
  
  uplevel [lindex $V(prereq) 0]
  
  R
  
  set code [catch {uplevel $body} result]
  
  if {$code != 0} {
    M "    FAILED: $V(script).$V(seqnum) - $desc"
    if {![info exists F($V(script).$num)]} {
      lappend F($V(script).$num) $::errorInfo
    }
  }

  uplevel [lindex $V(prereq) 1]
  
  if {$code == 0} R ;# leave files if the test was not successful
}

proc Q {{num ""}} {
  global V F N
  
  if {$num != ""} {
    incr num -1
    A {$num == $V(seqnum)}
    if {$V(keepit)} return
  }
  
  set f [array size F]
  set n [array size N]
  
  if {$f == 0} {
    M "Passed $n tests"
  } else {
    M "\nFailed $f of $n tests:\n"
    foreach t [lsort -dictionary [array names F]] {
      M "*** $t - $N($t) ***"
      foreach f $F($t) {
        M $f
      }
    }
  }
}
