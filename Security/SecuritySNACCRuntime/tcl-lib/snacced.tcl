# file: .../tcl-lib/snacced.tcl
#
# $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/tcl-lib/snacced.tcl,v 1.1.1.1 2001/05/18 23:14:11 mb Exp $
# $Log: snacced.tcl,v $
# Revision 1.1.1.1  2001/05/18 23:14:11  mb
# Move from private repository to open source repository
#
# Revision 1.1.1.1  1999/03/16 18:06:56  aram
# Originals from SMIME Free Library.
#
# Revision 1.2  1997/02/28 13:39:57  wan
# Modifications collected for new version 1.3: Bug fixes, tk4.2.
#
# Revision 1.1  1997/01/01 23:12:00  rj
# first check-in
#

# todo:
#	int, enum and bit string editors with scrollbar

#\[banner "initialization"]---------------------------------------------------------------------------------------------------------

set version 1.0

#tk colormodel . monochrome

# check all types whether they were marked as PDU.
# collect them in an associative array (indexed by module name)
foreach t [snacc types] \
{ 
  if {[lindex [snacc type $t] 1] == {pdu}} \
  { 
    set module [lindex $t 0]
    set type [lindex $t 1]
    lappend pdus($module) $type
  }
}

#foreach n [array names pdus] \
#{
#  debug "module $n: $pdus($n)"
#}

#\[banner "debugging aid"]----------------------------------------------------------------------------------------------------------

set debug 0

proc debug {text} \
{
  global debug
  if $debug {puts $text}
}

#\[banner "help texts"]-------------------------------------------------------------------------------------------------------------

set helptext(about) "SnaccEd $version"

set helptext(manoeuv) \
"Button 1
  on label
    show/hide subnodes (except for lists)
  on list
    perform action (selected with button 3's popup)

Button 2
  on label
    open/close value editor
  on canvas, list or text
    drag view

Button 3
  on label
    show/hide parent
  on list
    select action mode (for button 1)
  on text
    pops up menu for text import/export
"

#\[banner "File loading and saving"]------------------------------------------------------------------------------------------------

# called from file_reload and file_load_from
# clears the display so that only the file's root gets shown
proc file_prune {fileref} \
{
  upvar #0 $fileref file

  set tree $file(tree)
  set handle $file(handle)

  list_cleanup /$handle $handle
  $tree prune {}

  ed_addnode $tree {} {} {} $handle $handle valid
  $tree draw
}

# this function is called from the "File" menu.
# it reloads the file contents from its old origin:
proc file_reload {fileref} \
{
  set rc 1
  upvar #0 $fileref file
  # file_prune must be called before the snacc object is modified:
  file_prune $fileref
  $file(toplevel) config -cursor watch
  update idletasks
  if {[catch {snacc read $file(handle)} msg]} \
  {
    tk_dialog .d load "Couldn't reload: $msg" warning 0 Dismiss
  } \
  else \
  {
    set file(modified) 0
    set rc 0
  }
  $file(toplevel) config -cursor arrow
  return $rc
}

# this function is called from the "File" menu.
# it lets the user select a file and loads its contents
proc file_load_from {fileref} \
{
  set rc 1
  upvar #0 $fileref file
  if {[selbox fn ct]} \
  {
    # file_prune must be called before the snacc object is modified:
    file_prune $fileref
    $file(toplevel) config -cursor watch
    update idletasks
    if {[catch {snacc read $file(handle) $ct $fn} msg]} \
    {
      tk_dialog .d load "Couldn't load $fn: $msg" warning 0 Dismiss
    } \
    else \
    {
      set file(modified) 0
      set rc 0
    }
    $file(toplevel) config -cursor arrow
  }
  return $rc
}

# this function is called from the "File" menu.
# it saves the file contents to its old origin:
proc file_save {fileref} \
{
  set rc 1
  upvar #0 $fileref file
  $file(toplevel) config -cursor watch
  update idletasks
  if {[catch {snacc write $file(handle)} msg]} \
  {
    tk_dialog .d save "Couldn't save: $msg" warning 0 Dismiss
  } \
  else \
  {
    set file(modified) 0
    set rc 0
  }
  $file(toplevel) config -cursor arrow
  return $rc
}

# this function is called from the "File" menu.
# it lets the user select a file and saves the file's contents
proc file_save_as {fileref} \
{
  set rc 1
  upvar #0 $fileref file
  if {[selbox fn {}]} \
  {
    $file(toplevel) config -cursor watch
    update idletasks
    if {[catch {snacc write $file(handle) $fn} msg]} \
    {
      tk_dialog .d save "Couldn't save $fn: $msg" warning 0 Dismiss
    } \
    else \
    {
      set file(modified) 0
      set rc 0
    }
    $file(toplevel) config -cursor arrow
  }
  return $rc
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
# this function is called from prune_or_add_children, list_click, new_choice, toggle_se* and set_or_add_root
# it adds the node's children to the display
# some of the children may already be displayed (this is usually the case when the function gets called from list_click or set_or_add_root).
# ed_addnode will be called for every child.

proc ed_expand {tree treepath snaccpath} \
{
  set canvas [$tree canvas]

  set info [snacc info $snaccpath]
  set type [lindex $info 2]

  switch $type \
  {
    SEQUENCE -
    SET \
    {
      debug "$type:"
      foreach elem [lindex $info 3] \
      {
	set name [lindex $elem 0]
	set validity [lindex $elem 1]
	debug "  $validity $name"
	ed_addnode $tree $treepath $treepath $snaccpath $name $name $validity
      }
    }
    SEQUENCE\ OF -
    SET\ OF \
    {
      set len [lindex $info 3]
      set varname var:$treepath
      upvar #0 $varname var
debug [list treepath=$treepath]
debug [list varname=$varname]
debug [list idlist=$var(idlist)]
debug [list expand list ($type) len=$len]
      for {set i 0} {$i < $len} {incr i} \
      {
	set id [lindex $var(idlist) $i]
debug [list index $i id $id]
	if {$id} \
	{
	  ed_addnode $tree $treepath $treepath $snaccpath $id $i valid
	}
      }
    }
    CHOICE \
    {
      set name [lindex $info 3]
      set validity [lindex $info 4]
      debug "  $validity $name"
      ed_addnode $tree $treepath $treepath $snaccpath $name $name $validity
    }
  }
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------

# ed_addnode is called from set_or_add_root, new_file, file_prune and ed_expand.
# the node may already be displayed (this is usually the case when the function ed_expand gets called from list_click or set_or_add_root). in this case the node gets moved to the right position.
# otherwise the node is created at the right place.

# the arguments are:
#   tree		name of the tree widget
#   treeparent		tag of the displayed parent node. this is usually the same as the treeparentpath, except when the display gets extended into the parent direction where the root tag is {}
#   treeparentpath	tag of the logical parent node.
#   snaccparentpath	names of the 
#   treenode		node's name, gets appended to the treeparentpath
#   snaccnode		node's name, gets appended to the snaccparentpath
#   validity

proc ed_addnode {tree treeparent treeparentpath snaccparentpath treenode snaccnode validity} \
{
  set canvas [$tree canvas]

  set treepath "$treeparentpath/$treenode"
  set snaccpath "$snaccparentpath $snaccnode"

  if [llength [$canvas find withtag $treepath]] \
  {
debug [list movelink $treepath $treeparent]
    $tree movelink $treepath $treeparent
  } \
  else \
  {
#debug [list addnode $snaccpath]
    if {[llength $snaccparentpath] > 0} \
    {
      set nodelabeltext $snaccnode
    } \
    else \
    {
      set finfo [snacc finfo [string range $snaccpath 1 end]]
      if {[lindex $finfo 0] == {}} \
      {
	set nodelabeltext {(unnamed)}
      } \
      else \
      {
	set nodelabeltext [lindex $finfo 0]
      }
    }
    $canvas create text 0 0 -text $nodelabeltext -tags [list $validity-label $treepath $treepath:label]

    set line [$canvas create line 0 0 0 0]

    # fix for canvas bug: for reverse video, the canvas displays black items on a black background
    if {[tk colormodel .] == {monochrome} && [lindex [$canvas config -background] 4] == {black}} \
    {
      $canvas itemconfigure $treepath -fill white
      $canvas itemconfigure $line -fill white
    }

    if {$validity == {void}} \
    {
      if {[tk colormodel .] == {color}} \
      {
	# #b0b0b0 is the light grey of disabled checkbuttons:
	$canvas itemconfigure $treepath -fill #b0b0b0
	$canvas itemconfigure $line -fill #b0b0b0
      } \
      else \
      {
	$canvas itemconfigure $treepath -stipple gray50
	$canvas itemconfigure $line -stipple gray50
      }
    }

debug [list addlink $treeparent $treepath $line]
    $tree addlink $treeparent $treepath $line
  }
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
# open/drop subtree
proc prune_or_add_children {canvas} \
{
  set tree $canvas.t
#  debug $canvas
  set id [$canvas find withtag current]
  if {$id == {}} \
  {
    debug "no item"
  } \
  else \
  {
    set treepath [lindex [$canvas gettags $id] 1]
    set snaccpath [tree2snacc $treepath]
    set type [lindex [snacc info $snaccpath] 2]
    switch $type \
    {
      SEQUENCE\ OF - SET\ OF
      {}
      default
      {
#    debug $treepath
	if {[$tree isleaf $treepath]} \
	{
	  debug [list expanding $treepath $snaccpath]
	  ed_expand $tree $treepath $snaccpath
	} \
	else \
	{
	  debug [list cutting $treepath]
	  # !!! list_cleanup usually has to be called with the node that gets removed!
	  # in this case calling it with the node that stays around doesn't hurt because it is guaranteed not to be a SEQUENCE OF or SET OF type (they are handled a few lines above)
	  list_cleanup $treepath $snaccpath
	  $tree prune $treepath
	}
      }
    }
  }
  $tree draw
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
# this function has to be called whenever a subtree that may contain SET OF or SEQUENCE of types gets removed from the display
# it must be called *before* the snacc object gets destroyed, the function examines it!
proc list_cleanup {treepath snaccpath} \
{
  set info [snacc info $snaccpath]
  set type [lindex $info 2]
  switch $type \
  {
    SET - SEQUENCE - CHOICE
    {
      foreach elem [lindex $info 3] \
      {
	set name [lindex $elem 0]
	set validity [lindex $elem 1]
	if {$validity == {valid}} \
	{
	  set subtreepath "$treepath/$name"
	  set subsnaccpath "$snaccpath $name"
	  list_cleanup $subtreepath $subsnaccpath
	}
      }
    }
    SET\ OF - SEQUENCE\ OF
    {
      set varname var:$treepath
      global $varname
debug [list varname=$varname]
      if {[info exists $varname]} \
      {
	set idlist [set $varname\(idlist)]
debug [list idlist=$idlist]
	set i 0
	foreach id $idlist \
	{
	  if {$id != 0} \
	  {
	    set subtreepath "$treepath/$id"
	    set subsnaccpath "$snaccpath $i"
	    list_cleanup $subtreepath $subsnaccpath
	  }
	  incr i
	}
	unset $varname
      }
    }
  }
}

# this function must be called when calling "$tree root $treepath".
# it calls list_cleanup for all nodes that are neither parent nor in the subtree pointed to by $treepath.
proc list_cleanup_not_me {treepath snaccpath} \
{
  if {[set i [llength $snaccpath]] > 1} \
  {
    incr i -1
    set parenttreepath [join [lrange [split $treepath /] 0 $i] /]
    incr i -1
    set parentsnaccpath [lrange $snaccpath 0 $i]

    set info [snacc info $parentsnaccpath]
    set type [lindex $info 2]
    switch $type \
    {
      SET - SEQUENCE - CHOICE
      {
	foreach elem [lindex $info 3] \
	{
	  set name [lindex $elem 0]
	  set validity [lindex $elem 1]
	  if {$validity == {valid}} \
	  {
	    set subparenttreepath "$parenttreepath/$name"
	    set subparentsnaccpath "$parentsnaccpath $name"
	    if {$subparenttreepath != $treepath} \
	    {
	      list_cleanup $subparenttreepath $subparentsnaccpath
	    }
	  }
	}
      }
      SET\ OF - SEQUENCE\ OF
      {
	set varname var:$parenttreepath
	global $varname
  debug [list varname=$varname]
	set idlist [set $varname\(idlist)]
debug [list idlist=$idlist]
	set i 0
	foreach id $idlist \
	{
	  if {$id != 0} \
	  {
	    set subparenttreepath "$parenttreepath/$id"
	    set subparentsnaccpath "$parentsnaccpath $i"
	    if {$subparenttreepath != $treepath} \
	    {
	      list_cleanup $subparenttreepath $subparentsnaccpath
	      set $varname\(idlist) [lreplace [set $varname\(idlist)] $i $i 0]
	    }
	  }
	  incr i
	}
      }
    }
    # recursion:
    list_cleanup_not_me $parenttreepath $parentsnaccpath
  }
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
# /file0/files/1/name \(-> { file0 files 0 name}

proc tree2snacc {treepath} \
{
  set subtreepath {}
  foreach elem [lrange [split $treepath /] 1 end] \
  {
    set treeelem $elem
    if {[regexp {^[0-9]} $elem]} \
    {
      set varname var:$subtreepath
      global $varname
      set idlist [set $varname\(idlist)]
      set id $elem
      set index 0
      foreach lid $idlist \
      {
	if {$lid == $id} break
	incr index
      }
      if {$index == [llength $idlist]} \
      {
	error "tree2snacc: id $id not found in idlist [list $idlist]"
      }
      set snaccelem $index
    } \
    else \
    {
      set snaccelem $elem
    }
    append subtreepath /$treeelem
    append subsnaccpath " $snaccelem"
  debug [list >>$subtreepath--$subsnaccpath<<]
  }
  debug [list >>$subtreepath--$subsnaccpath<<]
  return $subsnaccpath
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc import_text {text_w treepath} \
{
  if {[selbox fn {}]} \
  {
    if {[catch {set text [snacc import $fn]} msg]} \
    {
      tk_dialog .d import "Couldn't import $fn: $msg" warning 0 Dismiss
    } \
    else \
    {
      $text_w delete 0.0 end
      $text_w insert end $text
      snacc set [tree2snacc $treepath] $text
    }
  }
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc export_text {text_w} \
{
  if {[selbox fn {}]} \
  {
    if {[catch {snacc export [$text_w get 0.0 end] $fn} msg]} \
    {
      tk_dialog .d import "Couldn't export $fn: $msg" warning 0 Dismiss
    }
  }
}

proc frame_resize_bindings {fileref treepath} \
{
  upvar #0 $fileref file

  set frame $file(canvas).edit$treepath

  bind $frame <ButtonPress-1> [list frame_resize_start $fileref %x %y]
  bind $frame <Button1-Motion> [list frame_resize_cont $fileref $treepath %x %y]
  bind $frame <ButtonRelease-1> [list frame_resize_end $fileref $treepath]

  $frame config -cursor bottom_right_corner
}

proc frame_resize_start {fileref x y} \
{
#debug [list frame_resize_start $fileref $x $y]

  upvar #0 $fileref file

  set file(resize_x) $x
  set file(resize_y) $y
}

proc frame_resize_cont {fileref treepath x y} \
{
#debug [list frame_resize_cont $fileref $treepath $x $y]

  upvar #0 $fileref file

  set frame $file(canvas).edit$treepath
  set frametag $treepath:edit

  set oldw [lindex [$file(canvas) itemconfig $frametag -width] 4]
  set oldh [lindex [$file(canvas) itemconfig $frametag -height] 4]
debug "old: $oldw x $oldh"
  set neww [max 1 [expr $oldw+$x-$file(resize_x)]]
  set newh [max 1 [expr $oldh+$y-$file(resize_y)]]
debug "new: $neww x $newh"
  $file(canvas) itemconfig $frametag -width $neww -height $newh
  set file(resize_x) $x
  set file(resize_y) $y
}

proc frame_resize_end {fileref treepath} \
{
#debug [list frame_resize_end $fileref $treepath]

  upvar #0 $fileref file

  $file(tree) nodeconfig $treepath
  $file(tree) draw
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
# toggle content editor
proc toggle_editor {canvas} \
{
  set tree $canvas.t
  set id [$canvas find withtag current]
  if {$id == {}} \
  {
    debug "no item"
  } \
  else \
  {
    set treepath [lindex [$canvas gettags $id] 1]
    set snaccpath [tree2snacc $treepath]
    set frame $canvas.edit$treepath
    if [llength [$canvas find withtag $treepath:edit]] \
    {
debug "removing editor for [list $treepath]"
      $canvas delete $treepath:edit
      destroy $frame
      $tree nodeconfig $treepath -remove {}
    } \
    else \
    {
debug "opening editor for [list $treepath]"

      set fileref [lindex [split $treepath /] 1]

      frame $frame -borderwidth 3 -bg #cdb79e
      set cleanup [list [list destroy $frame]]

      set info [snacc info $snaccpath]
      set type [lindex $info 2]

      switch $type \
      {
	NULL \
	{
	  set label $frame.label
	  label $label -text NULL
	  pack $label
	}
	BOOLEAN \
	{
	  set value [snacc get $snaccpath]

	  set var var:$treepath
	  global $var
	  set $var $value

	  set button $frame.button
	  #checkbutton $button -variable $var
	  checkbutton $button -onvalue TRUE -offvalue FALSE -variable $var -textvariable $var -command [list debug [list $canvas $treepath]]
	  pack $button

	  trace variable $var w change_simple
	}
	INTEGER \
	{
	  set value [snacc get $snaccpath]

	  set var var:$treepath
	  global $var
	  set $var $value

	  if {[lindex $info 0] != {{} {}}} \
	  {
	    set typeinfo [snacc type [lindex $info 0]]

	    foreach elem [lindex $typeinfo 3] \
	    {
	      set en [lindex $elem 0]
	      set ev [lindex $elem 1]
	      set button $frame.button$en
	      radiobutton $button -text $en -variable $var -value $ev -anchor w
	      pack $button -fill x
	    }
	  }

	  set entry $frame.entry
	  entry $entry -textvariable $var -width 9 -relief sunken

	  int_entry_bindings $entry

	  pack $entry -anchor w -fill x

	  focus $entry

	  trace variable $var w change_simple
	}
	ENUMERATED \
	{
	  set typeinfo [snacc type [lindex $info 0]]

	  if {[catch {set value [snacc get $snaccpath]} msg] == 1} \
	  {
	    global errorInfo errorCode
	    if {$errorCode == {SNACC ILLENUM}} \
	    {
	      set value [lindex [lindex $typeinfo 3] 0]
	      snacc set $snaccpath $value
	      append msg "--setting to first legal symbolic value \"$value\""
	      tk_dialog .d illenum "$msg" warning 0 Dismiss
	    } \
	    else \
	    {
	      error $msg $errorInfo $errorCode
	    }
	  }

	  set var var:$treepath
	  global $var
	  set $var $value

	  foreach ev [lindex $typeinfo 3] \
	  {
	    set button $frame.button$ev
	    radiobutton $button -text $ev -variable $var -value $ev -anchor w
	    pack $button -fill x
	  }

	  trace variable $var w change_simple
	}
	REAL \
	{
	  set value [snacc get $snaccpath]

	  set var var:$treepath
	  global $var
	  set $var $value

	  set entry $frame.entry
	  entry $entry -textvariable $var -relief sunken
	  pack $entry

	  frame_resize_bindings $fileref $treepath

	  focus $entry

	  trace variable $var w change_simple
	}
	BIT\ STRING \
	{
	  set value [snacc get $snaccpath]

	  set var var:$treepath
	  global $var
	  set $var $value

	  set max_ev 0
	  if {[lindex $info 0] != {{} {}}} \
	  {
	    set typeinfo [snacc type [lindex $info 0]]

	    foreach elem [lindex $typeinfo 3] \
	    {
	      set en [lindex $elem 0]
	      set ev [lindex $elem 1]
	      set max_ev [max $ev $max_ev]
	      set button $frame.button$en
	      checkbutton $button -text $en -variable $var:$ev -command [list toggle_bit $var $ev] -anchor w
	      pack $button -fill x
	    }
	  }

	  set entry $frame.entry
	  entry $entry -textvariable $var -relief sunken
	  set len [max 8 [string length $value] [expr $max_ev + 1]]
	  if {$len > 0} \
	  {
debug [list length of entry is $len]
	    $entry config -width $len
	  }
	  pack $entry -anchor w -fill x

	  bit_string_entry_bindings $entry

	  focus $entry

	  trace variable $var w change_bits
	  set $var $value; # trigger the trace
	}
	OBJECT\ IDENTIFIER \
	{
	  set value [snacc get $snaccpath]

	  set var var:$treepath
	  global $var
	  set $var $value

	  set entry $frame.entry
	  entry $entry -textvariable $var -relief sunken
	  pack $entry -fill both

	  frame_resize_bindings $fileref $treepath

	  focus $entry

	  trace variable $var w change_simple
	}
	OCTET\ STRING \
	{
	  set value [snacc get $snaccpath]

	  set text $frame.text
	  set sb $frame.sb

	  text $text -borderwidth 2 -relief sunken -yscrollcommand [list $sb set] -width 32 -height 8
	  scrollbar $sb -relief sunken -command [list $text yview] -width 10 -cursor arrow

	  pack $sb -side right -fill y
	  pack $text -side left -expand true -fill both

	  bind $text <ButtonPress-2> [list $text scan mark %y]
	  bind $text <Button2-Motion> [list $text scan dragto %y]

	  bind $text <Leave> "snacc set \[tree2snacc $treepath\] \[$text get 0.0 end\]"
	  bind $text <FocusOut> "snacc set \[tree2snacc $treepath\] \[$text get 0.0 end\]"

	  set m $frame.menu
	  menu $m
	  $m add command -label Load... -command "[list import_text $text $treepath]; [list $m unpost]"
	  $m add command -label Save... -command "[list export_text $text]; [list $m unpost]"

	  bind $text <ButtonPress-3> "[list $m] post \[expr %X -16\] \[expr %Y -8\]"
	  bind $m <ButtonPress-3> [list $m unpost]
	  bind $m <Any-Leave> [list $m unpost]

	  $text insert end $value
	  focus $text

	  frame_resize_bindings $fileref $treepath
	}
	SEQUENCE -
	SET \
	{
	  set typeinfo [snacc type [lindex $info 0]]

	  debug "$type:"

	  set varelems [lindex $info 3]
	  set typeelems [lindex $typeinfo 3]

	  for {set i 0; set len [llength $varelems]} {$i < $len} {incr i} \
	  {
	    set varelem [lindex $varelems $i]
	    set typeelem [lindex $typeelems $i]

	    set name [lindex $varelem 0]
	    set validity [lindex $varelem 1]
	    debug "  $validity $name"

	    set var var:$treepath:$name
	    global $var
	    set $var $validity

	    set button $frame.$name
	    checkbutton $button -text $name -onvalue valid -offvalue void -variable $var -command [list toggle_se* $canvas $treepath $name] -anchor w
	    if {[lindex $typeelem 4] == {mandatory}} \
	    {
	      #$button configure -disabledforeground [lindex [$button configure -fg] 4] -state disabled
	      $button configure -state disabled
	    }
	    pack $button -fill x
	  }
	}
	SEQUENCE\ OF -
	SET\ OF \
	{
	  set len [lindex $info 3]

	  set varname var:$treepath
	  upvar #0 $varname var
	  if {![info exists var(idlist)]} \
	  {
	    set var(idlist) {}
	    set var(lastid) 0
	  }
	  # no! needs a longer lifetime!
	  #lappend cleanup [list global $varname] [list unset $varname]

#	  set mbar $frame.mbar
	  set list $frame.list
	  set sb $frame.sb

	  scrollbar $sb -command [list $list yview] -width 10 -relief sunken -cursor arrow
#	  listbox $list -yscroll [list $sb set] -relief sunken -width 4 -height 5
	  text $list -borderwidth 2 -relief sunken -yscrollcommand [list $sb set] -width 4 -height 8 -exportselection 0
	  pack $sb -side right -fill y
	  pack $list -side left -expand true -fill both

#	  frame $mbar -relief raised -bd 2
#	  pack $mbar -side top -fill x

#	  set mode $mbar.mode
#	  set mode $frame.mode
#	  set m $mode.m
#	  menubutton $mode -text Mode -menu $m
	  set m $frame.mode
	  menu $m
	  set lm "[list list_mode $canvas $treepath]; [list $m unpost]"
	  $m add radiobutton -label Display -variable ${varname}(mode) -value display -command $lm
	  $m invoke last
	  $m add radiobutton -label Insert -variable ${varname}(mode) -value insert -command $lm
	  $m add radiobutton -label Append -variable ${varname}(mode) -value append -command $lm
	  $m add radiobutton -label Delete -variable ${varname}(mode) -value delete -command $lm
#	  pack $mode -side left

#	  pack $mode -side top -fill x

	  $list tag config display -background #b2dfee -relief raised
	  bind $list <Button-1> [list list_click $canvas $treepath]
	  bind $list <Double-Button-1> { }
	  bind $list <Triple-Button-1> { }
	  bind $list <Button1-Motion> { }

	  bind $list <ButtonPress-3> "[list $m] post \[expr %X-16\] \[expr %Y-8\]"
	  bind $m <ButtonPress-3> [list $m unpost]
	  bind $m <Any-Leave> [list $m unpost]
	  debug $m

	  for {set i 0} {$i < $len} {incr i} \
	  {
	    $list insert end [format "%4d\n" $i]

	    if {[llength $var(idlist)] > $i} \
	    {
	      if {[set id [lindex $var(idlist) $i]]} \
	      {
		set line [expr $i + 1]
		$list tag add display $line.0 $line.end
	      }
	    } \
	    else \
	    {
	      set var(idlist) [linsert $var(idlist) $i 0]
	    }
	  }

	  frame_resize_bindings $fileref $treepath
	}
	CHOICE \
	{
	  set name [lindex $info 3]
	  set validity [lindex $info 4]
	  set typeinfo [snacc type [lindex $info 0]]

	  set var var:$treepath
	  set oldvar oldvar:$treepath
	  global $var $oldvar
	  set $var $name
	  set $oldvar $name

	  foreach elem [lindex $typeinfo 3] \
	  {
	    set en [lindex $elem 0]
	    set button $frame.button$en
	    radiobutton $button -text $en -variable $var -value $en -command [list new_choice $canvas $treepath] -anchor w
	    pack $button -fill x
	  }
	  debug "  $validity $name"
	}
	default \
	{
	  error "unexpected type $type"
	}
      }

      scan [$canvas bbox $treepath:label] "%d%d%d%d" lx uy rx ly
      $canvas create window $lx $ly -anchor nw -tags [list edit $treepath $treepath:edit] -window $frame

      update idletasks;		# calculate frame's size (needed by tree widget)

      # explicitly set the frame's width&height to avoid nasty effects when resizing:
      scan [$canvas bbox $treepath:edit] "%d%d%d%d" lx uy rx ly
      $canvas itemconfig $treepath:edit -width [expr $rx - $lx] -height [expr $ly - $uy]

#debug [list cleanup = [join $cleanup \;]]
      $tree nodeconfig $treepath -remove [join $cleanup \;]
    }
  }
  $tree draw
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc list_click {canvas treepath} \
{
  set tree $canvas.t
  set snaccpath [tree2snacc $treepath]

debug [list treepath=$treepath]
debug [list snaccpath=$snaccpath]
debug [list tree2snacc: [tree2snacc $treepath]]
  set varname var:$treepath
  upvar #0 $varname var
  set frame $canvas.edit$treepath
  set list $frame.list

  debug [list list_click: $list]
debug [list varname=$varname]
debug [list idlist=$var(idlist)]
#  debug [$list tag ranges display]
  set text_index [$list index current]
#debug [list index: $index]
  if {$text_index != ""} \
  {
    # strip the column number:
    set line [lindex [split $text_index .] 0]
    # lines numbers start at 1, indices at 0:
    set index [expr $line - 1]
    set len [llength $var(idlist)]
    set tags [$list tag names $text_index]
    switch $var(mode) \
    {
      display \
      {
debug [list tags: $tags]
debug [list line: $line]
	if {$index < $len} \
	{
	  set id [lindex $var(idlist) $index]
debug [list index $index id $id]
	  if {$id} \
	  {
	    $list tag remove display $line.0 $line.end
	    list_cleanup $treepath/$id "$snaccpath $index"
debug [list $tree rmlink $treepath/$id]
	    $tree rmlink $treepath/$id
	    set var(idlist) [lreplace $var(idlist) $index $index 0]
	  } \
	  else \
	  {
	    $list tag add display $line.0 $line.end
	    set var(idlist) [lreplace $var(idlist) $index $index [incr var(lastid)]]
	    ed_expand $tree $treepath $snaccpath
	  }
	}
      }
      insert -
      append \
      {
	if {$var(mode) == {append}} {incr index}
debug [list insert $index 0]
	set var(idlist) [linsert $var(idlist) $index 0]
debug [list $var(idlist)]
debug [list catch [list snacc set "$snaccpath {insert $index}" {}]]
	catch [list snacc set "$snaccpath {insert $index}" {}]
	set file(modified) 1
debug [list [snacc get $snaccpath]]

	$list insert end [format "%4d\n" [expr [lindex [split [$list index end] .] 0] - 1]]

	for {set i $len} {$i > $index} {incr i -1} \
	{
	  set line [expr $i + 1]
	  if {[set id [lindex $var(idlist) $i]]} \
	  {
debug [list $canvas itemconfigure $treepath/$id:label -text $i]
	    $canvas itemconfigure $treepath/$id:label -text $i
	    if {![lindex $var(idlist) [expr $i - 1]]} \
	    {
debug [list $list tag add display $line.0 $line.end]
	      $list tag add display $line.0 $line.end
	    }
	  } \
	  else \
	  {
	    if {![lindex $var(idlist) [expr $i - 1]]} \
	    {
debug [list $list tag remove display $line.0 $line.end]
	      $list tag remove display $line.0 $line.end
	    }
	  }
	}
	set line [expr $index + 1]
debug [list $list tag remove display $line.0 $line.end]
	$list tag remove display $line.0 $line.end
      }
      delete \
      {
	if {$index < $len} \
	{
debug [list delete $index]

	  if {[set id [lindex $var(idlist) $index]]} \
	  {
	    # list_cleanup must be called before the snacc object is modified:
	    list_cleanup $treepath/$id "$snaccpath $index"
	    $tree rmlink $treepath/$id
	  }
	  incr len -1
	  for {set i $index} {$i < $len} {incr i} \
	  {
	    set line [expr $i + 1]
	    if {[set id [lindex $var(idlist) [expr $i + 1]]]} \
	    {
debug [list $canvas itemconfigure $treepath/$id:label -text $i]
	      $canvas itemconfigure $treepath/$id:label -text $i
	      if {![lindex $var(idlist) $i]} \
	      {
debug [list $list tag add display $line.0 $line.end]
		$list tag add display $line.0 $line.end
	      }
	    } \
	    else \
	    {
	      if {[lindex $var(idlist) $i]} \
	      {
debug [list $list tag remove display $line.0 $line.end]
		$list tag remove display $line.0 $line.end
	      }
	    }
	  }

	  set var(idlist) [lreplace $var(idlist) $index $index]
debug [list $var(idlist)]
debug [list snacc unset "$snaccpath $index"]
	  snacc unset "$snaccpath $index"
	  set file(modified) 1
debug [list [snacc get $snaccpath]]
	  $list delete [$list index {end - 1 line}] [$list index end]
	}
      }
    }
    $tree draw
  }
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc list_mode {canvas treepath} \
{
  set var var:$treepath
  global $var
  set mode [set ${var}(mode)]
  set frame $canvas.edit$treepath
  set list $frame.list

  switch $mode \
  {
    display {set cursor arrow}
    insert {set cursor based_arrow_up}
    append {set cursor based_arrow_down}
    delete {set cursor pirate}
  }
  $list config -cursor $cursor

  debug [list list_mode: ${var}(mode) set to $mode]
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc toggle_bit {var i} \
{
  global $var:$i $var
  set bit [set $var:$i]
  set val [set $var]

debug [list toggle_bit $val $i to $bit]

  set pre [string range $val 0 [expr $i - 1]]

  set fill {}
  for {set l [string length $val]} {$l < $i} {incr l} \
  {
    append fill 0
debug [list appending: $val]
  }

  set post [string range $val [expr $i + 1] end]

debug [list toggle_bit combining $pre $fill $bit $post]
  set $var $pre$fill$bit$post
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc change_bits {var element op} \
{
  global $var
  set val [set $var]
debug [list change_bits $var set to $val]

debug [list set l [string length $val]]
  set l [string length $val]
  for {set i 0} {$i < $l} {incr i} \
  {
    global $var:$i
    if {[info exists $var:$i]} \
    {
debug [list set $var:$i [string index $val $i]]
      set $var:$i [string index $val $i]
    } \
    else \
    {
debug [list non-exist: $var:$i]
    }
  }

  foreach bitvar [info globals $var:*] \
  {
    set i [lindex [split $bitvar :] 2]
    if {$i >= $l} \
    {
      global $bitvar
      set $bitvar 0
    }
  }

  change_simple $var $element $op
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc change_simple {var element op} \
{
  global $var
  set val [set $var]
debug [list change_simple $var set to $val]

  set treepath [lindex [split $var :] 1]
debug [list treepath= $treepath]
  set fileref [lindex [split $treepath /] 1]
  upvar #0 $fileref file
  set canvas $file(canvas)
debug [list canvas= $canvas]
  set snaccpath [tree2snacc $treepath]
debug [list snaccpath= $snaccpath]
  snacc set $snaccpath $val
  set file(modified) 1
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc new_choice {canvas treepath} \
{
  set tree $canvas.t
  set snaccpath [tree2snacc $treepath]

  set var var:$treepath
  set oldvar oldvar:$treepath
  global $var $oldvar
  set val [set $var]
  set oldval [set $oldvar]

  set fileref [lindex [split $treepath /] 1]
  upvar #0 $fileref file

debug "$file(modified)"
  debug [list new choice: $snaccpath = $val]

  # list_cleanup must be called before the snacc object is modified:
  list_cleanup $treepath/$oldval "$snaccpath $oldval"

  catch {snacc set $snaccpath [list $val {}]}
  set file(modified) 1
debug "$file(modified)"

  if {[llength [$canvas find withtag "$treepath/$oldval"]]} \
  {
    $tree rmlink "$treepath/$oldval"
    ed_expand $tree $treepath $snaccpath
    $tree draw
  }

  set $oldvar $val
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc toggle_se* {canvas treepath name} \
{
  set tree $canvas.t
  set snaccpath [tree2snacc $treepath]

  set var var:$treepath:$name
  global $var
  set val [set $var]

  set fileref [lindex [split $treepath /] 1]
  upvar #0 $fileref file

  debug "$snaccpath $name = $val"

  # this procedure is called after the button value has changed, so adjust the display to the current (new) setting:
  if {$val == {void}} \
  {
    # (change valid \(-> void)
    # list_cleanup must be called before the snacc object is modified:
    list_cleanup $treepath/$name "$snaccpath $name"
    snacc unset "$snaccpath $name"
  } \
  else \
  {
    # (change void \(-> valid)
    catch {snacc set "$snaccpath $name" {}}
  }
  set file(modified) 1

  if {[llength [$canvas find withtag "$treepath/$name"]]} \
  {
debug [list rmlink "$treepath/$name"]
    $tree rmlink "$treepath/$name"
    # a bug in the tree widget requires us to redraw here:
      $tree draw
    ed_expand $tree $treepath $snaccpath
    $tree draw
  }
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
# add/drop parent and siblings
proc set_or_add_root {canvas} \
{
  set tree $canvas.t
  set id [$canvas find withtag current]
  if {$id == {}} \
  {
    debug "no item"
  } \
  else \
  {
    set treepath [lindex [$canvas gettags $id] 1]
    set snaccpath [tree2snacc $treepath]
#    debug $path
    if {[llength $snaccpath] == 1} \
    {
      debug "at root already"
    } \
    else \
    {
      if {[$tree isroot $treepath]} \
      {
	# show the parent:
debug [list expanding [list $treepath $snaccpath]]
	set i [llength $snaccpath]

	incr i -1

	set treeparentpath [join [lrange [split $treepath /] 0 $i] /]
	set treeparentnode [lindex [split $treepath /] $i]
	incr i -1
	set snaccparentpath [lrange $snaccpath 0 $i]
	set snaccparentnode [lindex $snaccpath $i]

	set treeparentparentpath [join [lrange [split $treepath /] 0 $i] /]
	incr i -1
	set snaccparentparentpath [lrange $snaccpath 0 $i]

#debug [list ed_addnode $tree {} $parentparentpath $parentnode valid]
	ed_addnode $tree {} $treeparentparentpath $snaccparentparentpath $treeparentnode $snaccparentnode valid
#debug [list ed_expand $tree $parentpath]
	ed_expand $tree $treeparentpath $snaccparentpath
      } \
      else \
      {
	# hide everything above this subtree:
#	debug "cutting $path"
	list_cleanup_not_me $treepath $snaccpath
	$tree root $treepath
      }
#      debug [snacc info $path]
    }
  }
  $tree draw
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc file_open {} \
{
  if {[selbox fn ct nullfn]} \
  {
debug "fn=$fn ct=$ct"
    if {$fn != {}} \
    {
      if {[catch {set f [snacc open $ct $fn create]} msg]} \
      {
	tk_dialog .d load "Couldn't open $fn {$ct}: $msg" warning 0 Dismiss
	return -1
      }
    } \
    else \
    {
      if {[catch {set f [snacc create $ct]} msg]} \
      {
	tk_dialog .d create "Couldn't create {$ct}: $msg" warning 0 Dismiss
	return -1
      }
    }
    new_file $f
    return 0
  }
  return -1
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------

set #file 0
set #files 0

# returns 1 on `cancel', otherwise exits or returns 0
proc close_file {fileref} \
{
  upvar #0 $fileref file

  if {$file(modified)} \
  {
    set fi [snacc finfo $file(handle)]
    set fn [lindex $fi 0]
    set hasfn [expr {$fn != {}}]
    set isrw [expr {[lindex $fi 1] == {rw}}]
    set msg {There are unsaved changes}
    if {$hasfn} \
    {
      append msg " in `$fn'"
    }
    append msg {. Save them?}
    switch [lindex {save discard cancel} [tk_dialog .d modified $msg questhead 0 Yes No Cancel]] \
    {
      cancel \
      {
        return 1
      }
      save \
      {
	if {$hasfn && $isrw} \
	{
	  if {[file_save $fileref]} \
	  {
	    return 1
	  }
	} \
	else \
	{
	  if {[file_save_as $fileref]} \
	  {
	    return 1
	  }
	}
      }
    }
  }

  destroy $file(toplevel)
  global #files
  if {![incr #files -1]} \
  {
    exit
  }
  return 0
}

proc file_quit {} \
{
  global #files
  for {set i 0} {${#files}} {incr i} \
  {
    if {[winfo exists .[set fileref file$i]]} \
    {
      if {[close_file $fileref]} \
      {
	return
      }
    }
  }
}

proc new_file {handle} \
{
  global #file

  while {[winfo exists [set toplevel .[set fileref file${#file}]]]} \
  {
    incr #file
  }

  #global $fileref
  upvar #0 $fileref file

  set file(handle) $handle

  set file(toplevel) [toplevel $toplevel]
  wm title $toplevel snaccEd
  wm minsize $toplevel 150 100
  wm geometry $toplevel 500x500

  global #files
  incr #files

  set file(modified) 0

  $toplevel config -cursor arrow

  set menubar $toplevel.menu
  frame $menubar -relief raised -bd 2
  pack $menubar -side top -fill x

  set filem $menubar.file
  set m $filem.m
  menubutton $filem -text File -menu $m
  menu $m
  $m add command -label Reload -command [list file_reload $fileref]
  $m add command -label Load... -command [list file_load_from $fileref]
  $m add command -label Save -command [list file_save $fileref]
  $m add command -label {Save As...} -command [list file_save_as $fileref]
  $m add command -label Close -command [list close_file $fileref]
  $m add separator
  $m add command -label Open... -command file_open
  $m add separator
  $m add command -label Quit -command file_quit
  set fi [snacc finfo $handle]
  set hasnofn [expr {[lindex $fi 0] == {}}]
  set isro [expr {[lindex $fi 1] == {ro}}]
  if {$hasnofn} \
  {
    $m entryconfigure Reload -state disabled
  }
  if {$hasnofn || $isro} \
  {
    $m entryconfigure Save -state disabled
  }
  pack $filem -side left

  set help $menubar.help
  set m $help.m
  menubutton $help -text Help -menu $help.m
  menu $m
  $m add command -label About -command "help [list $m] \$helptext(about)"
  $m add command -label Manoeuvering -command "help [list $m] \$helptext(manoeuv)"
  pack $help -side right

  tk_menuBar $menubar $filem $help

  frame $toplevel.f0
  frame $toplevel.f1

  pack $toplevel.f0 -expand true -fill both
  pack $toplevel.f1 -fill x

  set file(canvas) [set canvas [canvas $toplevel.c -width 0 -height 0]]

  set hsb [scrollbar $toplevel.hsb -orient horiz -relief sunken -command [list $canvas xview]]
  set vsb [scrollbar $toplevel.vsb -relief sunken -command [list $canvas yview]]

  $canvas config -xscroll [list $hsb set] -yscroll [list $vsb set]

  set blind [frame $toplevel.blind -width [lindex [$vsb config -width] 4]]

  pack $vsb -in $toplevel.f0 -side right -fill y
  pack $canvas -in $toplevel.f0 -side left -expand true -fill both

  pack $blind -in $toplevel.f1 -side right
  pack $hsb -in $toplevel.f1 -side left -expand true -fill x

  bind $canvas <ButtonPress-2> [list $canvas scan mark %x %y]
  bind $canvas <Button2-Motion> [list $canvas scan dragto %x %y]

  $canvas bind valid-label <Button-1> {prune_or_add_children %W}
  $canvas bind valid-label <Button-2> {toggle_editor %W}
  $canvas bind valid-label <Button-3> {set_or_add_root %W}

  set file(tree) [set tree [tree $canvas.t]]

  ed_addnode $tree {} {} {} $handle $handle valid

  $tree draw

  tkwait visibility $toplevel
}

proc snacced {} \
{
  wm withdraw .

  global argc argv

  if {$argc == 0} \
  {
    if {[file_open]} \
    {
      exit 1
    }
  } \
  else \
  {
    if {$argc == 3} \
    {
      set ct [lrange $argv 0 1]
      set fn [lindex $argv 2]
      if {[catch {set f [snacc open $ct $fn create]} msg]} \
      {
	tk_dialog .d load "Couldn't open $fn {$ct}: $msg" warning 0 Dismiss
	exit 1
      }
    } \
    elseif {$argc == 2} \
    {
      set ct [lrange $argv 0 1]
      if {[catch {set f [snacc create $ct]} msg]} \
      {
	tk_dialog .d create "Couldn't create {$ct}: $msg" warning 0 Dismiss
	exit 1
      }
    } \
    else \
    {
      exit 1
    }
    new_file $f
  }
}
