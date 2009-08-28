## 
## This is the file `docstrip_util.tcl',
## generated with the SAK utility
## (sak docstrip/regen).
## 
## The original source files were:
## 
## tcldocstrip.dtx  (with options: `utilpkg')
## 
## In other words:
## **************************************
## * This Source is not the True Source *
## **************************************
## the true source is the file from which this one was generated.
##
package require Tcl 8.4
package require docstrip 1.2
package provide docstrip::util 1.2
namespace eval docstrip::util {
   namespace import [namespace parent]::extract
   namespace export ddt2man guard patch thefile
}
proc docstrip::util::ddt2man {text} {
   set wascode 0
   set verbatim 0
   set res ""
   foreach line [split $text \n] {
      if {$verbatim} then {
         if {$line eq $endverbline} then {
            set verbatim 0
         } else {
            append res [string map {[ [lb] ] [rb]} $line] \n
         }
      } else {
         switch -glob -- $line %%* {
            if {$wacode} then {
               append res {[example_end]} \n
               set wascode 0
            }
            append res [string range $line 2 end] \n
         } %<<* {
            if {!$wascode} then {
               append res {[example_begin]} \n
               set wascode 1
            }
            set endverbline "%[string range $line 3 end]"
            set verbatim 1
         } %<* {
            if {!$wascode} then {
               append res {[example_begin]} \n
               set wascode 1
            }
            set guard ""
            regexp -- {(^%<[^>]*>)(.*)$} $line "" guard line
            append res \[ [list emph $guard] \]\
              [string map {[ [lb] ] [rb]} $line] \n
         } %* {
            if {$wascode} then {
               append res {[example_end]} \n
               set wascode 0
            }
            append res [string range $line 1 end] \n
         } {\\endinput} {
           break
         } "" {
            append res \n
         } default {
            if {!$wascode} then {
               append res {[example_begin]} \n
               set wascode 1
            }
            append res [string map {[ [lb] ] [rb]} $line] \n
         }
      }
   }
   if {$wascode} then {append res {[example_end]} \n}
   return $res
}
proc docstrip::util::guards {subcmd text} {
   set verbatim 0
   set lineno 1
   set badL {}
   foreach line [split $text \n] {
      if {$verbatim} then {
         if {$line eq $endverbline} then {set verbatim 0}
      } else {
         switch -glob -- $line %<<* {
            set endverbline "%[string range $line 3 end]"
            set verbatim 1
         } %<* {
            if {![
               regexp -- {^%<([*/+-]?)([^>]*)>(.*)$} $line ""\
                 modifier expression line
            ]} then {
               lappend badL $lineno $line
            } else {
               if {$modifier eq ""} then {set modifier " "}
               append E($expression) $modifier
            }
         }
      }
      incr lineno
   }
   if {$subcmd eq "rotten"} then {return $badL}
   switch -- $subcmd "exprmods" {
      return [array get E]
   } "expressions" {
      return [array names E]
   } "exprerr" {
      set res {}
      foreach expr [array names E] {
         regsub -all {[^()!,|&]+} $expr 0 e
         regsub -all {,} $e {|} e
         if {[catch {expr $e}]} then {lappend res $expr}
      }
      return $res
   }
   foreach name [array names E] {
      set E($name) [string length $E($name)]
   }
   if {$subcmd eq "exprcounts"} then {return [array get E]}
   foreach expr [array names E] {
      foreach term [split $expr "()!,|&"] {
         if {$term eq ""} then {continue}
         if {![info exists T($term)]} then {set T($term) 0}
         incr T($term) $E($expr)
      }
   }
   switch -- $subcmd "counts" {
      return [array get T]
   } "names" {
      return [array names T]
   } default {
      error "Unknown subcommand '$subcmd', must be one of:\
        counts, exprcounts, expressions, exprmods, names, rotten"
   }
}
proc docstrip::util::patch {sourcevar termL fromtext diff args} {
   upvar 1 $sourcevar SL
   array set O {-trimlines 1 -matching exact}
   array set O $args
   set cmd [list extract [join $SL \n]  $termL -annotate 2]
   foreach opt {-metaprefix -trimlines} {
      if {[info exists O($opt)]} then {lappend cmd $opt $O($opt)}
   }
   set EL [split [eval $cmd] \n]
   lset EL end \n
   set ptr 0
   set lineno 1
   set FL [list {}]
   foreach line [split $fromtext \n] {
      lappend FL $line
      if {$O(-trimlines)} then {set line [string trimright $line " "]}
      if {$line eq [lindex $EL $ptr]} then {
         set lift($lineno) [lindex $EL [incr ptr]]
         lset lift($lineno) 0 [expr { [lindex $EL [incr ptr]] - 1 }]
         incr ptr
      }
      incr lineno
   }
   if {![array size lift]} then {
      return -code error "The extract did not match any part of the\
        fromtext. Check the list of terminals and the options"
   }
   set RL [list]
   set log [list]
   foreach hunk [lsort -decreasing -integer -index 0 $diff] {
      set replL [list]
      set l1 [lindex $hunk 0]
      set repl {0 -1}
      set matches 1
      foreach {type line} [lindex $hunk 4] {
         switch -glob -- $type {[0-]} {
            switch -- $O(-matching) "exact" {
               if {[lindex $FL $l1] ne $line} then {set matches 0}
            } "nonspace" {
               if {[regsub -all -- {\s} $line {}] ne\
                 [regsub -all -- {\s} [lindex $FL $l1] {}]} then {
                  set matches 0
               }
            } "anyspace" {
               if {[regsub -all -- {\s+} $line { }] ne\
                 [regsub -all -- {\s+} [lindex $FL $l1] { }]} then {
                  set matches 0
               }
            }
         }
         switch -- $type synch {
            if {[llength $repl]>2 ||\
              [lindex $repl 1]-[lindex $repl 0]>=0} then {
               lappend replL $repl
            }
            set repl [list $l1 [expr {$l1-1}]]
         } + {
            lappend repl $line
         } - {
            lset repl 1 $l1
            incr l1
         } 0 {
            if {[llength $repl]>2 ||\
              [lindex $repl 1]-[lindex $repl 0]>=0} then {
               lappend replL $repl
               set repl {0 -1}
            }
            lset repl 1 $l1
            incr l1
            lset repl 0 $l1
         }
      }
      if {[llength $repl]>2 || [lindex $repl 1]-[lindex $repl 0]>=0}\
      then {lappend replL $repl}
      if {$matches} then {
         lappend hunk [lsort -decreasing -integer -index 0 $replL]
         lappend RL $hunk
      } else {
         lappend hunk "(-- did not match fromtext --)"
         lappend log $hunk
      }
   }
   foreach hunk $RL {
      set applied 0
      set misapplied 0
      foreach repl [lindex $hunk 5] {
         unset -nocomplain from to
         for {set n [lindex $repl 1]} {$n>=[lindex $repl 0]}\
           {incr n -1} {
            if {![info exists lift($n)]} then {
               incr misapplied
               continue
            } elseif {![info exists from]} then {
               set to [lindex $lift($n) 0]
               set from $to
            } elseif {[lindex $lift($n) 0] == $from-1} then {
               set from [lindex $lift($n) 0]
            } else {
               set SL [lreplace $SL $from $to]
               set to [lindex $lift($n) 0]
               set from $to
            }
            incr applied
            set n0 $n
         }
         if {[info exists from]} then {
            set sprefix [lindex $lift($n0) 1]
            set eprefix [lindex $lift($n0) 2]
         } elseif {[info exists lift([lindex $repl 0])]} then {
            foreach {from sprefix eprefix} $lift([lindex $repl 0])\
              break
            set to [expr {$from-1}]
         } else {
            incr misapplied [llength [lrange $repl 2 end]]
            continue
         }
         set eplen [string length $eprefix]
         set epend [expr {$eplen-1}]
         set cmd [list lreplace $SL $from $to]
         foreach line [lrange $repl 2 end] {
            if {$eprefix eq [string range $line 0 $epend]} then {
               lappend cmd "$sprefix[string range $line $eplen end]"
            } else {
               lappend cmd $line
            }
            incr applied
         }
         set SL [eval $cmd]
      }
      if {$misapplied>0} then {
         if {$applied>0} then {
            lset hunk 5 "(-- was partially applied --)"
         } else {
            lset hunk 5 "(not applied)"
         }
         lappend log $hunk
      }
   }
   set res ""
   foreach hunk [lsort -index 0 -integer $log] {
      foreach {start1 end1 start2 end2 lines msg} $hunk break
      append res [format "@@ -%d,%d +%d,%d @@ %s\n"\
        $start1 [expr {$end1-$start1+1}]\
        $start2 [expr {$end2-$start2+1}] $msg]
      foreach {type line} $lines {
         switch -- $type 0 {
            append res " " $line \n
         } - - + {
            append res $type $line \n
         }
      }
   }
   return $res
}
proc docstrip::util::thefile {fname args} {
   set F [open $fname r]
   if {[llength $args]} then {
      if {[set code [
         catch {eval [linsert $args 0 fconfigure $F]} res
      ]]} then {
         close $F
         return -code $code -errorinfo $::errorInfo -errorcode\
           $::errorCode
      }
   }
   catch {read $F} res
   close $F
   return $res
}
proc docstrip::util::import_unidiff {text {warnvar ""}} {
   if {$warnvar ne ""} then {upvar 1 $warnvar warning}
   set inheader 1
   set res [list]
   set lines [list]
   set end2 "not an integer"
   foreach line [split $text \n] {
      if {$inheader && [regexp {^(---|\+\+\+)} $line]}\
      then {continue}
      switch -glob -- $line { *} {
         lappend lines 0 [string range $line 1 end]
      } {+*} {
         lappend lines + [string range $line 1 end]
      } {-*} {
         lappend lines - [string range $line 1 end]
      } @@* {
         if {[string is integer $end2]} then {
            lappend res [list $start1 $end1 $start2 $end2 $lines]
         }
         set len2 [set len1 ,1]
         if {[
            regexp {^@@ -([0-9]+)(,[0-9]+)? \+([0-9]+)(,[0-9]+)? @@}\
              $line -> start1 len1 start2 len2
         ] && [scan "$start1 $len1,1" {%d ,%d} start1 len1]==2 &&\
              [scan "$start2 $len2,1" {%d ,%d} start2 len2]==2
         } then {
            set end1 [expr {$start1+$len1-1}]
            set end2 [expr {$start2+$len2-1}]
            set inheader 0
         } else {
            set end2 "not an integer"
            append warning "Could not parse hunk header:  " $line \n
         }
         set lines [list]
      } "" {
      } default {
         append warning "Could not parse line:  " $line \n
      }
   }
   if {[string is integer $end2]} then {
      lappend res [list $start1 $end1 $start2 $end2 $lines]
   }
   return $res
}
## 
## 
## End of file `docstrip_util.tcl'.