# Trace dump window for GDBtk.
# Copyright 1998, 1999 Cygnus Solutions
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
# Implements Tdump window for gdb
#
#   PUBLIC ATTRIBUTES:
#
#
#   METHODS:
#
#     reconfig ....... called when preferences change
#
#
#   X11 OPTION DATABASE ATTRIBUTES
#
#
# ----------------------------------------------------------------------

itcl_class TdumpWin {
  # ------------------------------------------------------------------
  #  CONSTRUCTOR - create new tdump window
  # ------------------------------------------------------------------
  constructor {config} {
    #
    #  Create a window with the same name as this object
    #
    set class [$this info class]
    set hull [namespace tail $this]
    set old_name $this
    ::rename $this $this-tmp-
    ::frame $hull -class $class 
    ::rename $hull $old_name-win-
    ::rename $this $old_name

    set top [winfo toplevel [namespace tail $this]]
    wm withdraw $top

    build_win
    add_hook gdb_update_hook "$this update"
    after idle [list wm deiconify $top]

  }


  # ------------------------------------------------------------------
  #  METHOD:  build_win - build the main tdump window
  # ------------------------------------------------------------------
  method build_win {} {

     tixScrolledText [namespace tail $this].stext -scrollbar y -height 200 -width 500
     set twin [[namespace tail $this].stext subwidget text]
    
    # make window non editable
    $twin configure -insertwidth 0 

    pack append  [namespace tail $this] [namespace tail $this].stext {left expand fill}
    update
  }


  # ------------------------------------------------------------------
  #  METHOD:  update - update widget when PC changes
  # ------------------------------------------------------------------
  method update {} {
    #debug "tdump: update"
    gdbtk_busy
    set tframe_num [gdb_get_trace_frame_num]

    if { $tframe_num!=-1 } {
      debug "doing tdump"
      $twin delete 1.0 end

      if {[catch {gdb_cmd "tdump $tframe_num" 0} tdump_output]} {
	tk_messageBox -title "Error" -message $tdump_output -icon error \
	  -type ok
      } else {
	#debug "tdum output is $tdump_output"
	
	$twin insert end $tdump_output
	$twin see insert
      }
    }
    gdbtk_idle
  }

  # ------------------------------------------------------------------
  #  DESTRUCTOR - destroy window containing widget
  # ------------------------------------------------------------------
  destructor {
    remove_hook gdb_update_hook "$this update"
    set top [winfo toplevel [namespace tail $this]]
    destroy $this
    destroy $top
  }

  # ------------------------------------------------------------------
  #  METHOD:  config - used to change public attributes
  # ------------------------------------------------------------------
  method config {config} {}
    
  # ------------------------------------------------------------------
  #  METHOD:  reconfig - used when preferences change
  # ------------------------------------------------------------------
  method reconfig {} {
  }
  
  #
  #  PROTECTED DATA
  #
  protected maxwidth 0
  protected twin
}

