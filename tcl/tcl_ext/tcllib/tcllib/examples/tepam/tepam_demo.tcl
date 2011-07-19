#!/bin/sh
# Evaluating the wish origin \
exec `which wish` "$0" "$@"
##########################################################################
# TEPAM - Tcl's Enhanced Procedure and Argument Manager
##########################################################################
#
# tepam_demo.tcl:
# This file provides a graphical demo framework for the enhanced procedure
# and argument manager.
#
# Copyright (C) 2009, 2010 Andreas Drollinger
# 
# RCS: @(#) $Id: tepam_demo.tcl,v 1.1 2010/02/11 21:54:38 droll Exp $
##########################################################################
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
##########################################################################

package require Tk
package require tepam

######################## Regression test GUI ########################

proc DisplayResult {Result Type} {
   regsub -line -all {^(.)} $Result "$Type: -> \\1" Result
   regsub -line -all {[\t ]+$} $Result "" Result
   .rightside.code insert insert $Result Result_$Type
   # .rightside.code see insert
   update
}

rename puts puts_orig
proc puts {args} {
   set EndLine "\n"
   if {[llength $args]>1} {
      if {[lindex $args 0]=="-nonewline"} {
         set EndLine ""
      }
   }
   DisplayResult [lindex $args end]$EndLine s
}

proc ExecuteExampleStep {Step} {
   global ExampleScript IsExecutable ExecutedSteps
   set CmdNbr 0
   foreach es $ExampleScript($Step) {
      .rightside.code mark set insert step$Step-cmd$CmdNbr
      if {[catch {set CmdRes [uplevel #0 $es]} ErrorRes]} {
         DisplayResult "$ErrorRes \n" e
      } else {
         DisplayResult "$CmdRes \n" r
      }
      incr CmdNbr
   }
   lappend ExecutedSteps $Step

   foreach Step [array names IsExecutable] {
      set Executed [expr [lsearch -exact $ExecutedSteps $Step]>=0]
      if $IsExecutable($Step) {
         # Activate the section and add the binds
         .rightside.code tag configure step$Step -background white -relief flat
         .rightside.code tag bind step$Step <Any-Enter> ".rightside.code tag configure step$Step -background #43ce80 -relief raised -borderwidth 1"
         .rightside.code tag bind step$Step <Any-Leave> ".rightside.code tag configure step$Step -background {} -relief flat"
         .rightside.code tag bind step$Step <1> "ExecuteExampleStep $Step"
      } else {
         # Deactivate the section and remove the binds
         .rightside.code tag configure step$Step -background gray80 -relief flat
         .rightside.code tag bind step$Step <Any-Enter> {}
         .rightside.code tag bind step$Step <Any-Leave> {}
         .rightside.code tag bind step$Step <1> {}
      }
   }
}

proc SelectExample {example} {
   global RegTestDir ExampleScript LastExecutedExampleStep IsExecutable ExecutedSteps

   wm title . "TEPAM Demo - $example"

   catch {unset ExampleScript}
   .rightside.code delete 0.0 end
   .rightside.code configure -background white -relief flat
   foreach tag [.rightside.code tag names] {
      if {[regexp -- {^(step)|(title)\d$} $tag]} {
         .rightside.code tag delete $tag
      }
   }

   .rightside.code insert end "This demo example uses the following styles and colors: " Introduction
   .rightside.code insert end "<<descriptions and comments>>" "Introduction Comment" ", " Introduction
   .rightside.code insert end "<<not executed program code>>" "Introduction Code" ", " Introduction
   .rightside.code insert end "<<already executed, or not yet executable program code>>" "Introduction Code Executed" ", " Introduction
   .rightside.code insert end "<<r: command return value>>" "Introduction Result_r" ", " Introduction
   .rightside.code insert end "<<e: command return error>>" "Introduction Result_e" ", " Introduction
   .rightside.code insert end "<<s: standard output (stsd) print>>" "Introduction Result_s" ".\n" Introduction
   .rightside.code insert end "Click now on each demo example section, one after " Introduction
   .rightside.code insert end "the other. This will execute the program code of the " Introduction
   .rightside.code insert end "section and insert the results and outputs into the " Introduction
   .rightside.code insert end "listing.\n\n" Introduction

   set f [open $RegTestDir/$example]
   set Step -1
   set Script ""
   set ExampleStep ""
   set LastExecutedExampleStep -1
   set InitSteps {}
   catch {array unset IsExecutable}
   set ExecutedSteps {}
   while {![eof $f]} {
      if {[gets $f line]<0} break
      if {[regexp {^\s*\#{4}\s*([^#]*)\s*\#{4}$} $line {} ExampleStep]} {
         incr Step
         set ExampleStep [string trim $ExampleStep]
         .rightside.code insert end "#### $ExampleStep ####\n" "SectionTitle title$Step"
         set ExampleScript($Step) {}
      } elseif {[regexp {^\s*DemoControl\((\w+)\)\s+(.*)\s*} $line {} ControlType ControlExpr]} {
         regexp {^\{\s*(.*)\s*\}$} $ControlExpr {} ControlExpr
         switch $ControlType {
            IsExecutable {set IsExecutable($Step) $ControlExpr}
            Initialization {lappend InitSteps $Step}
         }
      } elseif {$ExampleStep!=""} {
         if {[regexp {^\s*\#{8,100}$} $line]} {
            set ExampleStep ""
            continue
         }
         # regsub $LineStart $line {} line
         regsub -all {\t} $line {   } line
         if {![regexp {^(.*?\{\s*#.*#\s*\}.*?)(#.*){0,1}$} $line {} ScriptLine ScriptComment]} {
            regexp {^(.*?)(#.*){0,1}$} $line {} ScriptLine ScriptComment
         }
         .rightside.code insert end $ScriptLine "Code step$Step" "$ScriptComment\n" "step$Step Comment"

         if {[string trim $ScriptLine]==""} continue

         append Script "$ScriptLine\n"
         if {[info complete $Script]} {
            set Mark "step$Step-cmd[llength $ExampleScript($Step)]"
            .rightside.code mark set $Mark "end - 1 lines"
            .rightside.code mark gravity $Mark left
            lappend ExampleScript($Step) [string trim $Script]
            set Script ""
         }
      }
   }
   close $f
   
   # Execute the initialization step if existing
   foreach Step $InitSteps {
      ExecuteExampleStep $Step
   }
}

proc OpenConsole {} {
   if {[catch {set ::tkcon::PRIV(root)}]} {
      # define PRIV(root) to an existing window to avoid a console creation
      namespace eval ::tkcon {
         set PRIV(root) .tkcon
         set OPT(exec) ""
         set OPT(slaveexit) "close"
      }
      catch {set TkConPath [exec csh -f -c {which tkcon.tcl}]}
      catch {package require registry; set TkConPath [registry get {HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\tclsh.exe} Path]/tkcon.tcl; regsub -all {\\} $TkConPath {/} TkConPath}
      if {$TkConPath!=""} {
         # hide the standard console (only windows)
         catch {console hide}

         # Source tkcon. "Usually" this should also start the tkcon window.
         set ::argv ""
         uplevel #0 "source \{$TkConPath\}"

         # TkCon versions have been observed that doesn't open the tkcon window during sourcing of tkcon. Initialize
         # tkcon explicitly:
         if {[lsearch [winfo children .] ".tkcon"]<0 && [lsearch [namespace children ::] "::tkcon"]} {
            ::tkcon::Init
         }
         tkcon show
      } else {
         if {$tcl_platform(platform)=={windows}} {
            console show
         } else {
            error "Cannot source tkcon.tcl."
         }
      }
   } else {
      if {[catch {wm deiconify $::tkcon::PRIV(root)}]} {
         if {$tcl_platform(platform)=={windows}} {
            console show
         } else {
            error "Cannot deiconify tkcon!"
         }
      }
   }
}

set RegTestDir [file dirname [info script]]

pack [frame .leftside] -side left -fill y
   pack [label .leftside.step1 -text "(1) Choose one of the demo \nexamples bellow.\n\n" -anchor w] -fill x
   pack [label .leftside.label1 -text "Demo examples:" -anchor w] -fill x
   set NbrExamples 0
   foreach example [lsort -dictionary [glob $RegTestDir/*.demo]] {
      set example [file tail $example]
      pack [button .leftside.start$NbrExamples -command "SelectExample $example" -text $example] -fill x
      incr NbrExamples
   }
   pack [button .leftside.exit -command exit -text "Exit"] -side bottom -fill x
   pack [button .leftside.console -command {OpenConsole} -text "Open console"] -side bottom -fill x

pack [frame .rightside] -side left -expand yes -fill both
   grid [label .rightside.step2 -text "(2) Execute the selected demo.\n\n" -anchor w] -row 0 -column 0 -sticky ew
   
   grid [text .rightside.code -height 1 -padx 3 -wrap none -font {Courier 9} -background white \
              -yscrollcommand ".rightside.scrolly set" \
              -xscrollcommand ".rightside.scrollx set" ] -row 1 -column 0 -sticky news
      .rightside.code tag configure Introduction -foreground blue -font {Courier 9} -wrap word
      .rightside.code tag configure Comment -foreground blue -font {Courier 9}
      .rightside.code tag configure Code -foreground black -font {Courier 9 bold}
      .rightside.code tag configure SectionTitle -foreground black -background yellow -font {Courier 9 bold}
      .rightside.code tag configure Result_r -foreground black -background gray80 -font {Courier 9 italic}
      .rightside.code tag configure Result_e -foreground red -background gray80 -font {Courier 9 italic}
      .rightside.code tag configure Result_s -foreground green4 -background gray80 -font {Courier 9 italic}
      .rightside.code tag configure Executed -background gray80

   grid [scrollbar .rightside.scrolly -command ".rightside.code yview" -orient vertical] -row 1 -column 1 -sticky ns
   grid [scrollbar .rightside.scrollx -command ".rightside.code xview" -orient horizontal] -row 2 -column 0 -sticky new

   grid rowconfigure .rightside 1 -weight 70
#  grid rowconfigure .rightside 1 -weight 0
#  grid rowconfigure .rightside 2 -weight 30
   grid columnconfigure .rightside 0 -weight 1

wm geometry . 900x800
wm title . "TEPAM Demo"

##########################################################################
# $RCSfile: tepam_demo.tcl,v $ - ($Name:  $)
# $Id: tepam_demo.tcl,v 1.1 2010/02/11 21:54:38 droll Exp $
# Modifications:
# $Log: tepam_demo.tcl,v $
# Revision 1.1  2010/02/11 21:54:38  droll
# TEPAM module checkin
#
##########################################################################
