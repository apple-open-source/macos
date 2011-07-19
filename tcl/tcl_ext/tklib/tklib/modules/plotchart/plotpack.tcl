# plotpack.tcl --
#     Implement a pack-like geometry manager for Plotchart
#
#     Note:
#     The canvas:* procedures are taken directly from the Wiki,
#     these procedures were written by Maurice (ulis) Bredelet
#     I have not changed the formatting or the naming convention,
#     as a small token of my appreciation for his Tcl work
#     - he died in February 2008.
#
namespace eval ::Plotchart {
    variable packing
}

# GetCanvas --
#     Destill the name of the canvas from the plot command
#
# Arguments:
#     cmd             Plot command
#
# Result:
#     Name of the widget
#
proc ::Plotchart::GetCanvas {cmd} {
    regsub {^[^_]+_} $cmd "" w
    return $w
}


# plotpack --
#     Copy an existing plot/chart into another canvas widget
#
# Arguments:
#     w               Canvas widget to copy to
#     dir             Direction to attach the new plot to the existing contents
#     args            List of plots/charts to be copied
#
# Result:
#     None
#
proc ::Plotchart::plotpack {w dir args} {
    variable packing

    if { ![info exists packing($w,top)] } {
        set packing($w,top)    0
        set packing($w,left)   0
        set packing($w,right)  [WidthCanvas  $w]
        set packing($w,bottom) [HeightCanvas $w]
    }
    set top    $packing($w,top)
    set left   $packing($w,left)
    set right  $packing($w,right)
    set bottom $packing($w,bottom)

    foreach p $args {
        set save [canvas:save [GetCanvas $p]]
        switch -- $dir {
            "top" {
                 set xmove 0
                 set ymove $top
                 canvas:restore $w $save
                 $w move __NEW__ $xmove $ymove
                 $w dtag all __NEW__
                 set cwidth [WidthCanvas [GetCanvas $p]]
                 if { $left < $cwidth } {
                     set left $cwidth
                 }
                 set top  [expr {$top+[HeightCanvas [GetCanvas $p]]}]
            }
            "bottom" {
                 set xmove 0
                 set ymove [expr {$bottom-[HeightCanvas [GetCanvas $p]]}]
                 canvas:restore $w $save
                 $w move __NEW__ $xmove $ymove
                 $w dtag all __NEW__
                 set cwidth [WidthCanvas [GetCanvas $p]]
                 if { $left < $cwidth } {
                     set left $cwidth
                 }
                 set bottom $ymove
            }
            "left" {
                 set xmove $left
                 set ymove 0
                 canvas:restore $w $save
                 $w move __NEW__ $xmove $ymove
                 $w dtag all __NEW__
                 set left [expr {$left+[WidthCanvas [GetCanvas $p]]}]
                 set cheight [HeightCanvas [GetCanvas $p]]
                 if { $top < $cheight } {
                     set top $cheight
                 }
            }
            "right" {
                 set xmove [expr {$right-[WidthCanvas [GetCanvas $p]]}]
                 set ymove 0
                 canvas:restore $w $save
                 $w move __NEW__ $xmove $ymove
                 $w dtag all __NEW__
                 set right $xmove
                 if { $top < $cheight } {
                     set top $cheight
                 }
            }
        }
    }
    set packing($w,top)    $top
    set packing($w,left)   $left
    set packing($w,right)  $right
    set packing($w,bottom) $bottom
}

# canvas:* --
#     Procedures for copying the contents of a canvas widget - by "ulis"
#
namespace eval ::Plotchart {

# ==============================
#
#   clone a canvas widget
#
# ==============================

# ----------
#  canvas:clone proc
# ----------
# parm1: canvas widget
# parm2: clone canvas widget
# ----------

proc canvas:clone {canvas clone} { canvas:restore $clone [canvas:save $canvas] }

# ----------
#  options proc
#
#  return non empty options
# ----------
# parm: options list
# ----------
# return: non empty options list
# ----------

proc options {options} \
{
  set res {}
  foreach option $options \
  {
    set key   [lindex $option 0]
    set value [lindex $option 4]
    if {$value != ""} { lappend res [list $key $value] }
  }
  return $res
}

# ----------
#  canvas:save proc
#
#  serialize a canvas widget
# ----------
# parm1: canvas widget path
# ----------
# return: serialized widget
# ----------

proc canvas:save {w} \
{
  # canvas name
  lappend save $w
  # canvas option
  lappend save [options [$w configure]]
  # canvas focus
  lappend save [$w focus]
  # canvas items
  foreach id [$w find all] \
  {
    set item {}
    # type & id
    set type [$w type $id]
    lappend item [list $type $id]
    # coords
    lappend item [$w coords $id]
    # tags
    set tags [concat __NEW__ [$w gettags $id]] ;# AM: My change
    lappend item $tags
    # binds
    set binds {}
      # id binds
    set events [$w bind $id]
    foreach event $events \
    { lappend binds [list $id $event [$w bind $id $event]] }
      # tags binds
    foreach tag $tags \
    {
      set events [$w bind $tag]
      foreach event $events \
      { lappend binds [list $tag $event [$w bind $tag $event]] }
    }
    lappend item $binds
    # options
    lappend item [options [$w itemconfigure $id]]
    # type specifics
    set specifics {}
    switch -- $type {
      arc       {}
      bitmap    {}
      image     \
      {
        # image name
        set iname [$w itemcget $id -image]
        lappend specifics $iname
        # image type
        lappend specifics [image type $iname]
        # image options
        lappend specifics [options [$iname configure]]
      }
      line      {}
      oval      {}
      polygon   {}
      rectangle {}
      text      \
      {
        foreach index {insert sel.first sel.last} \
        {
          # text indexes
          catch \
          { lappend specifics [$w index $id $index] }
        }
      }
      window    \
      {
        # window name
        set wname [$w itemcget $id -window]
        lappend specifics $wname
        # window type
        lappend specifics [string tolower [winfo class $wname]]
        # window options
        lappend specifics [options [$wname configure]]
      }
    }
    lappend item $specifics
    lappend save $item
  }
  # return serialized canvas
  return $save
}

# ----------
#  canvas:restore proc
#
#  restore a serialized canvas widget
# ----------
# parm1: canvas widget path
# parm2: serialized widget to restore
# ----------

proc canvas:restore {w save} \
{
  # create canvas options
  # eval canvas $w [join [lindex $save 1]] ;# AM: My change
  # items
  foreach item [lrange $save 3 end] \
  {
    foreach {typeid coords tags binds options specifics} $item \
    {
      # get type
      set type [lindex $typeid 0]
      # create bitmap or window
      switch -- $type {
        image   \
        {
          foreach {iname itype ioptions} $specifics break
          if {![image inuse $iname]} \
          { eval image create $itype $iname [join $ioptions] }
        }
        window  \
        {
          foreach {wname wtype woptions} $specifics break
          if {![winfo exists $wname]} \
          { eval $wtype $wname [join $woptions] }
          raise $wname
        }
      }
      # create item
      set id [eval $w create $type $coords [join $options]]
      $w itemconfigure $id -tags $tags ;# AM: "options" may contain the old list of tags
      # item bindings
      foreach bind $binds \
      {
        foreach {id event script} $bind { $w bind $id $event $script }
      }
      # item specifics
      if {$specifics != ""} \
      {
        switch -- $type {
          text    \
          {
            foreach {insert sel.first sel.last} $specifics break
            $w icursor $id $insert
            if {${sel.first} != ""} \
            {
              $w select from $id ${sel.first}
              $w select to   $id ${sel.last}
            }
          }
        }
      }
    }
  }
  # focused item
  set focus [lindex $save 2]
  if {$focus != ""} \
  {
    $w focus [lindex $save 2]
    focus -force $w
  }
  # return path
  return $w
}

# ----------
#  canvas:dump proc
#
#  dump a canvas widget
# ----------
# parm: canvas widget path
# ----------
# return: widget dump
# ----------

proc canvas:dump {w} \
{
  # canvas name
  lappend res [lindex $w 0]
  # canvas options
  foreach option [lindex $w 1] { lappend res [join $option \t] }
  # focused item
  lappend res [join [lindex $w 2] \t]
  # items
  foreach item [lrange $w 3 end] \
  {
    foreach {type coords tags binds options specifics} $item \
    {
      # item type
      lappend res [join $type \t]
      # item coords
      lappend res \tcoords\t$coords
      # item tags
      lappend res \ttags\t$tags
      # item bindings
      lappend res \tbinds
      foreach bind $binds { lappend res \t\t$bind }
      # item options
      lappend res \toptions
      foreach option $options \
      {
        set key [lindex $option 0]
        set value [lindex $option 1]
        lappend res \t\t$key\t$value
      }
      # item specifics
      if {$specifics != ""} \
      {
        lappend res \tspecifics
        foreach specific $specifics \
        {
          if {[llength $specific] == 1}  { lappend res \t\t$specific } \
          else { foreach token $specific { lappend res \t\t$token } }
        }
      }
    }
  }
  # return dump
  return [join $res \n]
}
}
