# Utilities for GDBtk.
# Copyright 1997, 1998, 1999 Cygnus Solutions
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License (GPL) as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.


# ----------------------------------------------------------------------
# Misc routines
#
#   PROCS:
#
#     keep_raised - keep a window raised
#     sleep - wait a certain number of seconds and return
#     toggle_debug_mode - turn debugging on and off
#     freeze - make a window modal
#     bp_exists - does a breakpoint exist on linespec?
#
# ----------------------------------------------------------------------
#


# A helper procedure to keep a window on top.
proc keep_raised {top} {
  if {[winfo exists $top]} {
    raise $top
    wm deiconify $top
    after 1000 [info level 0]
  }
}

# sleep - wait a certain number of seconds then return
proc sleep {sec} {
  global __sleep_timer
  set __sleep_timer 0
  after [expr {1000 * $sec}] set __sleep_timer 1
  vwait __sleep_timer
}


# ------------------------------------------------------------------
#  PROC:  auto_step - automatically step through a program
# ------------------------------------------------------------------

# FIXME FIXME
proc auto_step {} {
  global auto_step_id

  set auto_step_id [after 2000 auto_step]
  gdb_cmd next
}

# ------------------------------------------------------------------
#  PROC:  auto_step_cancel - cancel auto-stepping
# ------------------------------------------------------------------

proc auto_step_cancel {} {
  global auto_step_id

  if {[info exists auto_step_id]} {
    after cancel $auto_step_id
    unset auto_step_id
  }
}

# ------------------------------------------------------------------
#  PROC:  tfind_cmd -- to execute a tfind command on the target
# ------------------------------------------------------------------
proc tfind_cmd {command} {
  gdbtk_busy
  # need to call gdb_cmd because we want to ignore the output
  set err [catch {gdb_cmd $command} msg]
  if {$err || [regexp "Target failed to find requested trace frame" $msg]} {
    tk_messageBox -icon error -title "GDB" -type ok \
      -modal task -message $msg
    gdbtk_idle
    return
  } else {
    gdbtk_update
    gdbtk_idle
  }
}

# ------------------------------------------------------------------
#  PROC:  save_trace_command -- Saves the current trace settings to a file
# ------------------------------------------------------------------
proc save_trace_commands {} {
  
  set out_file [tk_getSaveFile -title "Enter output file for trace commands"]
  debug "Got outfile: $out_file"
  if {$out_file != ""} {
    gdb_cmd "save-tracepoints $out_file"
  }
}

# ------------------------------------------------------------------
#  PROC:  do_test - invoke the test passed in
#           This proc is provided for convenience. For any test
#           that uses the console window (like the console window
#           tests), the file cannot be sourced directly using the
#           'tk' command because it will block the console window
#           until the file is done executing. This proc assures
#           that the console window is free for input by wrapping
#           the source call in an after callback.
#           Users may also pass in the verbose and tests globals
#           used by the testsuite.
# ------------------------------------------------------------------
proc do_test {{file {}} {verbose {}} {tests {}}} {
  global _test

  if {$file == {}} {
    error "wrong \# args: should be: do_test file ?verbose? ?tests ...?"
  }

  if {$verbose != {}} {
    set _test(verbose) $verbose
  } elseif {![info exists _test(verbose)]} {
    set _test(verbose) 0
  }

  if {$tests != {}} {
    set _test(tests) $tests
  }

  set _test(interactive) 1
  after 500 [list source $file]
}

# ------------------------------------------------------------------
#  PROCEDURE:  gdbtk_read_defs
#        Reads in the defs file for the testsuite. This is usually
#        the first procedure called by a test file. It returns
#        1 if it was successful and 0 if not (if run interactively
#        from the console window) or exits (if running via dejagnu).
# ------------------------------------------------------------------
proc gdbtk_read_defs {} {
  global _test env

  if {[info exists env(DEFS)]} {
    set err [catch {source $env(DEFS)} errTxt]
  } else {
    set err [catch {source defs} errTxt]
  }

  if {$err} {
    if {$_test(interactive)} {
      tk_messageBox -icon error -message "Cannot load defs file:\n$errTxt" -type ok
      return 0
    } else {
      puts stdout "cannot load defs files: $errTxt\ntry setting DEFS"
      exit 1
    }
  }

  return 1
}

# ------------------------------------------------------------------
#  PROCEDURE:  bp_exists
#            Returns BPNUM if a breakpoint exists at LINESPEC or
#            -1 if no breakpoint exists there
# ------------------------------------------------------------------
proc bp_exists {linespec} {

  lassign $linespec foo function filename line_number addr pc_addr

  set bps [gdb_get_breakpoint_list]
  foreach bpnum $bps {
    set bpinfo [gdb_get_breakpoint_info $bpnum]
    lassign $bpinfo file func line pc type enabled disposition \
      ignore_count commands cond thread hit_count
    if {$filename == $file && $function == $func && $addr == $pc} {
      return $bpnum
    }
  }

  return -1
}


# Scrolled Listbox - this could be in libgui,
# but we'll probably just start using new iwidgets stuff 
# soon so keep it here temporarily.  This is based on
# code from Welch's book.

proc CygScrolledListbox { win args } {
  frame $win
  # Create listbox attached to scrollbars, pass thru $args
  eval {listbox $win.list -yscrollcommand [list $win.sy set]} $args
  scrollbar $win.sy -orient vertical -command [list $win.list yview]
  
  # Create padding based on the scrollbar width and border
  set pad [expr [$win.sy cget -width] + 2* \
	     ([$win.sy cget -bd] + \
		[$win.sy cget -highlightthickness])]

  frame $win.pad -width $pad -height $pad
  pack $win.sy -side right -fill y
  pack $win.list -side left -fill both -expand true
  return $win.list
}

# gridCGet - This provides the missing grid cget
# command.

proc gridCGet {slave option} {
  set config_list [grid info $slave]
  return [lindex $config_list [expr [lsearch $config_list $option] + 1]] 
}

# ------------------------------------------------------------------
# PROC: find_iwidgets_library - Find the IWidgets library.
#
# This is a little bit of bogosity which is necessary so we
# can find the iwidgets libraries if we are not installed:
# The problem is that the iwidgets are really weird.  The init file is 
# in the build tree, but all the library files are in the source tree...
#
# ------------------------------------------------------------------
proc find_iwidgets_library {} {
  global errMsg

  set IwidgetsOK 1

  if {[catch {package require Iwidgets 3.0} errMsg]} {

    # OK, we are not installed or this would have succeeded...
    # Lets try to do it by hand:
    set IwidgetsOK 0

    set iwidgetsSrcDir [glob -nocomplain [file join \
					    [file dirname [file dirname $::tcl_library]] \
					    itcl iwidgets*]]
    
    # Canonicalize the executable's directory name.  It turns out that on Solaris, 
    # info nameofexecutable returns /foo/bar/real_dir/./gdb when gdb is launched from
    # another gdb session, so we have to fix this up.

    set exec_name [info nameofexecutable]
    set curdir [pwd] 
    if {[string compare [file type $exec_name] "link"] == 0} {
      set exec_name [file readlink $exec_name]
      if {[string compare [file pathtype $exec_name] "relative"] == 0} {
	set exec_name [file join [pwd] $exec_name]
      }
    }
    
    cd [file dirname $exec_name]
    set exec_name [pwd]
    cd $curdir

    set iwidgetsBuildDir [glob -nocomplain [file join \
					      [file dirname $exec_name] \
					      itcl iwidgets*]]
    
    if {[llength $iwidgetsSrcDir] == 1 && [llength $iwidgetsBuildDir] == 1} {
      # The lindex is necessary because the path may have spaces in it...
      set initFile [file join [lindex $iwidgetsBuildDir 0] \
		      $::tcl_platform(platform) iwidgets.tcl]
      set libDir [file join [lindex $iwidgetsSrcDir 0] generic]
      if {[file exists $initFile] && [file isdirectory $libDir]} {
	if {![catch {source $initFile} err]} {
	  # Now fix up the stuff the Iwidgets init file got wrong...
	  set libPos [lsearch $::auto_path [file join $::iwidgets::library scripts]]
	  if {$libPos >= 0} {
	    set auto_path [lreplace $::auto_path $libPos $libPos $libDir]
	  } else {
	    lappend ::auto_path $libDir
	  }
	  set ::iwidgets::library $libDir
	  set IwidgetsOK 1
	} else {
	  append errMsg "\nError in iwidgets.tcl file: $err"
	}
      }
    } else {
      append errMsg "\nCould not find in-place versions of the Iwidgets files\n"
      append errMsg "Looked at: $iwidgetsSrcDir\n"
      append errMsg "and: $iwidgetsBuildDir\n"
    }

  }
  return $IwidgetsOK
}

# ------------------------------------------------------------------
#  PROC:  get_disassembly_flavor - gets the current disassembly flavor.
#         The set disassembly-flavor command is assumed to exist.  This
#         will error out if it does not.
# ------------------------------------------------------------------
proc get_disassembly_flavor {} {
  if {[catch {gdb_cmd "show disassembly-flavor"} ret]} {
    return ""
  } else {
    regexp {\"([^\"]*)\"\.} $ret dummy gdb_val
    return $gdb_val
  }
}
 
# ------------------------------------------------------------------
#  PROC:  list_disassembly_flavors - Lists the current disassembly flavors.
#         Returns an empty list if the set disassembly-flavor is not supported.
# ------------------------------------------------------------------
proc list_disassembly_flavors {} {
  catch {gdb_cmd "set disassembly-flavor"} ret_val
  if {[regexp {Requires an argument\. Valid arguments are (.*)\.} \
	 $ret_val dummy list]} {
    foreach elem  [split $list ","] {
      lappend vals [string trim $elem]
    }
    return [lsort $vals]
  } else {
    return {}
  }    
}

# ------------------------------------------------------------------
#  PROC:  init_disassembly_flavor - Synchs up gdb's internal disassembly
#         flavor with the value in the preferences file.
# ------------------------------------------------------------------
proc init_disassembly_flavor {} { 
  set gdb_val [get_disassembly_flavor]
  if {$gdb_val != ""} {
    set def_val [pref get gdb/src/disassembly-flavor]
    if {[string compare $def_val ""] != 0} {
      if {[catch "gdb_cmd \"set disassembly-flavor $def_val\""]} {
	pref set gdb/src/disassembly-flavor $gdb_val
      }
    } else {
      pref set gdb/src/disassembly-flavor $gdb_val
    }
  }
}

# ------------------------------------------------------------------
#  PROC:  list_element_strcmp - to be used in lsort -command when the
#         elements are themselves lists, and you always want to look at
#         a particular item.
# ------------------------------------------------------------------
proc list_element_strcmp {index first second} {
  set theFirst [lindex $first $index]
  set theSecond [lindex $second $index]

  return [string compare $theFirst $theSecond]
}
