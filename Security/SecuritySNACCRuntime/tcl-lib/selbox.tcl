# file: selbox.tcl
# file and content type selection box (ASN.1)
#
# $Header: /cvs/root/Security/SecuritySNACCRuntime/tcl-lib/Attic/selbox.tcl,v 1.1.1.1 2001/05/18 23:14:10 mb Exp $
# $Log: selbox.tcl,v $
# Revision 1.1.1.1  2001/05/18 23:14:10  mb
# Move from private repository to open source repository
#
# Revision 1.1.1.1  1999/03/16 18:06:56  aram
# Originals from SMIME Free Library.
#
# Revision 1.2  1997/02/28 13:39:56  wan
# Modifications collected for new version 1.3: Bug fixes, tk4.2.
#
# Revision 1.1  1997/01/01 23:11:59  rj
# first check-in
#

proc selbox_newfn {sbref} \
{
  upvar #0 $sbref sb

  set fn $sb(toplevel).f.fn.name

  set name [$fn get]

  debug $name
}

proc selbox_newbase {sbref} \
{
  global $sbref
  upvar #0 $sbref sb

  set fb_list $sb(toplevel).f.lists.basename
  set bs [$fb_list curselection]
  if {[llength $bs] == 1} \
  {
    set base [$fb_list get $bs]

debug base=$base
    set path [split $sb(fn) /]
    set len [llength $path]
    set last [expr $len-1]
debug len=$len
    if {$base == {..}} \
    {
      if {$len == 0} \
      {
	set $sbref\(fn) ..
      } \
      else \
      {
#	set sb [join [lrange $path 0 $last] /]
	if {[lindex $path $last] == {..}} \
	{
	  append $sbref\(fn) /..
	} \
	else \
	{
	  set $sbref\(fn) [join [lrange $path 0 $last] /]
	}
      }
    } \
    else \
    {
      if {$len == 0} \
      {
	set $sbref\(fn) $base
      } \
      else \
      {
incr last -1
#	set sb [join [concat [lrange $path 0 $last] $base] /]
debug [list set $sbref\(fn) [join [concat [lrange $path 0 $last] $base] /]]
	set $sbref\(fn) [join [concat [lrange $path 0 $last] $base] /]
      }
    }
debug "sb(fn)=$sb(fn)"
  }
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc selbox_update {name elem op} \
{
debug ">selbox_update $name $elem $op"
  upvar #0 $name sb

#debug "$name=$sb"
  set fb_list $sb(toplevel).f.lists.basename
  $fb_list delete 0 end
  $fb_list insert 0 ..
  set dir [file dirname $sb(fn)]
  set base [file tail $sb(fn)]
  set names [lsort [glob $dir/{.*,*}]]
  foreach name $names \
  {
    set name [file tail $name]
#    debug $name
    if {$name != {.} && $name != {..}} \
    {
      $fb_list insert end $name
      if {$name == $base} \
      {
	$fb_list select from end
	$fb_list yview end
      }
    }
  }
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc selbox_tm_click {sbref} \
{
  upvar #0 $sbref sb

  global pdus

  set t $sb(toplevel).t.lists
  set tm $t.modules
  set tt $t.types

  set ms [$tm curselection]
  if {[llength $ms] == 1} \
  {
    $tt delete 0 end
    eval $tt insert 0 $pdus([$tm get $ms])
  }
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc selbox_ok {sbref} \
{
  upvar #0 $sbref sb

  set fn $sb(toplevel).f.fn.name
  set t $sb(toplevel).t.lists
  set m $t.modules
  set t $t.types

  if {$sb(want_fn) && $sb(fn) == {} && $sb(force_fn)} \
  {
    tk_dialog .d {select filename} "You need to enter a file name" warning 0 Ok
    return
  }

  if {$sb(want_ct)} \
  {
    set ms [$m curselection]
    set ts [$t curselection]

    if {[llength $ms] == 1 && [llength $ts] == 1} \
    {
      set sb(ct) "[$m get $ms] [$t get $ts]"
    } \
    else \
    {
      tk_dialog .d {select content type} "You need to select a content type" warning 0 Ok
      return
    }
  }

  set sb(rc) 1
  destroy $sb(toplevel)
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
proc selbox_cancel {sbref} \
{
  upvar #0 $sbref sb

  set sb(rc) 0
  destroy $sb(toplevel)
}

#\[sep]-----------------------------------------------------------------------------------------------------------------------------
# the selbox (short for `file and content type selection box')
# selbox has to be called with two arguments, which may be either empty or be the name of a global variable.
# the selbox can display two sections: one for selecting a file name, a second for selecting a content type.
# the selbox arguments denote the variable names for the the two sections.
# if a variable name is empty, its corresponding section will not be displayed.
# if filename_ref is non-empty, a filename will forced to be entered unless `nullfn' is given in args.

# the 1x1 geometry for the listboxes below allows them to shrink when the selbox is resized.
# (otherwise, the buttons and the second listbox will disappear!)

set #sb 0

proc selbox {filename_ref conttype_ref args} \
{
  # change this if you get widget or variable name collisions:
  set prefix selbox

  # choose a unique variable and widget name:
  global #sb
  while {[winfo exists [set toplevel .[set sbref $prefix${#sb}]]]} \
  {
    incr #sb
  }

  global $sbref
  upvar #0 $sbref sb

  if {$filename_ref != {}} \
  {
    set sb(want_fn) 1
    set sb(force_fn) 1
    upvar $filename_ref filename
  } \
  else \
  {
    set sb(want_fn) 0
  }

  if {$conttype_ref != {}} \
  {
    set sb(want_ct) 1
    upvar $conttype_ref conttype
  } \
  else \
  {
    set sb(want_ct) 0
  }

  foreach arg $args \
  {
    switch $arg \
    {
      nullfn \
      {
	set sb(force_fn) 0
      }
      default \
      {
	error "selbox: illegal argument $arg"
      }
    }
  }

  set sb(toplevel) [toplevel $toplevel]
  wm minsize $toplevel 1 1
  wm geometry $toplevel 300x300

  #--- up to three frames, for the file name, for the content type, and for a row of buttons:
  set borderwidth 5
  set relief ridge
  if {$sb(want_fn)} \
  {
    set f [frame $toplevel.f -relief $relief -bd $borderwidth]
  }
  if {$sb(want_ct)} \
  {
    set t [frame $toplevel.t -relief $relief -bd $borderwidth]
  }
  set btns [frame $toplevel.btns -relief $relief -bd $borderwidth]

  #--- fill the upper file frame:

  if {$sb(want_fn)} \
  {
  #  set c [canvas $f.c -bg blue]
    set flabel [label $f.label -text {File name:}]
    set flists [frame $f.lists]
    set fnf [frame $f.fn]
  #$c create window 0 0 -window $flists -anchor nw
  #set hsb [scrollbar $f.sb -orient horizontal -command "$c xview"]
  #  set fd_list [listbox $flists.dirname -relief sunken]
    set fb_list [listbox $flists.basename -relief sunken -width 1 -height 1 -selectmode single]

  #  set fd_sb [scrollbar $flists.dir_sb]
    set fb_sb [scrollbar $flists.base_sb]

    $fb_list configure -yscrollcommand "$fb_sb set"
    $fb_sb configure -command "$fb_list yview"

  #  tk_listboxSingleSelect $fd_list $fb_list
  #  tk_listboxSingleSelect $fb_list
  #  bind $fd_list <Double-Button-1> "sb_newdir $sb"
    bind $fb_list <Double-Button-1> "selbox_newbase $sbref"

    set fn [entry $fnf.name -relief sunken -textvariable $sbref\(fn)]

    #bind $fn <Return> "sb_newfn $sb"

  #  pack $fd_list $fd_sb $fb_list $fb_sb -side left -expand 1 -fill y
    pack $fb_list -side left -expand 1 -fill both
    pack $fb_sb -side left -fill y
    pack $fn

    pack $flabel -fill x
    pack $fnf -fill x
    pack $flists -expand 1 -fill both
  #  pack $c $hsb -expand 1 -fill both

    trace variable $sbref\(fn) w selbox_update
    # ``set sb(fn) {}'' doesn't work! (selbox_update will be called with the alias, not the global name!)
    if {[info exists filename]} \
    {
      set $sbref\(fn) $filename
    } \
    else \
    {
      set $sbref\(fn) {}
    }

    pack $f -expand 1 -fill both
  }

  #--- fill the middle type frame:

  if {$sb(want_ct)} \
  {
    set tlabel [label $t.label -text {Content type:}]
    set tlists [frame $t.lists]

    set tm [listbox $tlists.modules -exportselection 0 -relief sunken -width 1 -height 1 -selectmode single]
    set tt [listbox $tlists.types -exportselection 0 -relief sunken -width 1 -height 1 -selectmode single]

    set tm_sb [scrollbar $tlists.mod_sb]
    set tt_sb [scrollbar $tlists.type_sb]

    # tk_listboxSingleSelect $tm $tt
    $tm configure -yscrollcommand "$tm_sb set"
    $tm_sb configure -command "$tm yview"

    global pdus
    eval $tm insert 0 [array names pdus]
    bind $tm <1> "[bind Listbox <1>]; selbox_tm_click $sbref"

    pack $tm $tm_sb $tt $tt_sb -side left
      pack configure $tm $tt -expand 1 -fill both
      pack configure $tm_sb $tt_sb -fill y
    pack $tlabel -fill x
    pack $tlists -expand 1 -fill both

    pack $t -expand 1 -fill both
  }

  #--- fill the lower button frame:

  button $btns.ok -text Ok -command "selbox_ok $sbref"
  button $btns.cancel -text Cancel -command "selbox_cancel $sbref"

  pack $btns.ok $btns.cancel -side left -padx 3m

  pack $btns -fill x

  #--- now we're set up, let's go to work:

  set of [focus]
  focus $fn

  tkwait window $toplevel
  # if we got an affirmative response, export the selection:
  if $sb(rc) \
  {
    if {$sb(want_fn)} { set filename $sb(fn) }
    if {$sb(want_ct)} { set conttype $sb(ct) }
  }
  focus $of
  return $sb(rc)
}
