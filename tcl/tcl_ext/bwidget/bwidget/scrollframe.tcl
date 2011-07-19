# ----------------------------------------------------------------------------
#  scrollframe.tcl
#  This file is part of Unifix BWidget Toolkit
#  $Id: scrollframe.tcl,v 1.14 2009/10/27 22:15:09 oberdorfer Exp $
# ----------------------------------------------------------------------------
#  Index of commands:
#     - ScrollableFrame::create
#     - ScrollableFrame::configure
#     - ScrollableFrame::cget
#     - ScrollableFrame::getframe
#     - ScrollableFrame::see
#     - ScrollableFrame::xview
#     - ScrollableFrame::yview
#     - ScrollableFrame::_resize
#     - ScrollableFrame::_themechanged
# ----------------------------------------------------------------------------

namespace eval ScrollableFrame {
    Widget::define ScrollableFrame scrollframe

    Widget::declare ScrollableFrame {
        {-background	    Color      "SystemWindowFrame"  0}
        {-width             Int        0  0 {}}
        {-height            Int        0  0 {}}
        {-areawidth         Int        0  0 {}}
        {-areaheight        Int        0  0 {}}
        {-constrainedwidth  Boolean    0 0}
        {-constrainedheight Boolean    0 0}
        {-xscrollcommand    TkResource "" 0 canvas}
        {-yscrollcommand    TkResource "" 0 canvas}
        {-xscrollincrement  TkResource "" 0 canvas}
        {-yscrollincrement  TkResource "" 0 canvas}
        {-bg                Synonym    -background}
    }

    Widget::addmap ScrollableFrame "" :cmd {
        -width {} -height {} 
        -xscrollcommand {} -yscrollcommand {}
        -xscrollincrement {} -yscrollincrement {}
    }

    variable _widget

    bind BwScrollableFrame <Configure> [list ScrollableFrame::_resize %W]
    bind BwScrollableFrame <Destroy>   [list Widget::destroy %W]

    if {[lsearch [bindtags .] ScrollableFrameThemeChanged] < 0} {
        bindtags . [linsert [bindtags .] 1 ScrollableFrameThemeChanged]
    }
}


# ----------------------------------------------------------------------------
#  Command ScrollableFrame::create
# ----------------------------------------------------------------------------
proc ScrollableFrame::create { path args } {
    Widget::init ScrollableFrame $path $args

    set bg [Widget::cget $path -background]

    set canvas [eval [list canvas $path] [Widget::subcget $path :cmd] \
                    -highlightthickness 0 -borderwidth 0 -relief flat \
		    -bg $bg]

    set frame [eval [list frame $path.frame] \
		       -highlightthickness 0 -borderwidth 0 -relief flat \
		       -background $bg]

    $canvas create window 0 0 -anchor nw -window $frame -tags win \
        -width  [Widget::cget $path -areawidth] \
        -height [Widget::cget $path -areaheight]

    bind $frame <Configure> \
        [list ScrollableFrame::_frameConfigure $canvas]
    # add <unmap> binding: <configure> is not called when frame
    # becomes so small that it suddenly falls outside of currently visible area.
    # but now we need to add a <map> binding too
    bind $frame <Map> \
        [list ScrollableFrame::_frameConfigure $canvas]
    bind $frame <Unmap> \
        [list ScrollableFrame::_frameConfigure $canvas 1]

    bindtags $path [list $path BwScrollableFrame [winfo toplevel $path] all]

    bind ScrollableFrameThemeChanged <<ThemeChanged>> \
	   "+ [namespace current]::_themechanged $path"

    return [Widget::create ScrollableFrame $path]
}


# ----------------------------------------------------------------------------
#  Command ScrollableFrame::configure
# ----------------------------------------------------------------------------
proc ScrollableFrame::configure { path args } {
    set res [Widget::configure $path $args]
    set upd 0

    if { [Widget::hasChanged $path -background bg] } {
        $path:cmd configure -background $bg
	$path.frame configure -background $bg
    }

    set modcw [Widget::hasChanged $path -constrainedwidth cw]
    set modw  [Widget::hasChanged $path -areawidth w]
    if { $modcw || (!$cw && $modw) } {
        if { $cw } {
            set w [winfo width $path]
        }
        set upd 1
    }

    set modch [Widget::hasChanged $path -constrainedheight ch]
    set modh  [Widget::hasChanged $path -areaheight h]
    if { $modch || (!$ch && $modh) } {
        if { $ch } {
            set h [winfo height $path]
        }
        set upd 1
    }

    if { $upd } {
        $path:cmd itemconfigure win -width $w -height $h
    }
    return $res
}


# ----------------------------------------------------------------------------
#  Command ScrollableFrame::cget
# ----------------------------------------------------------------------------
proc ScrollableFrame::cget { path option } {
    return [Widget::cget $path $option]
}


# ----------------------------------------------------------------------------
#  Command ScrollableFrame::getframe
# ----------------------------------------------------------------------------
proc ScrollableFrame::getframe { path } {
    return $path.frame
}

# ----------------------------------------------------------------------------
#  Command ScrollableFrame::see
# ----------------------------------------------------------------------------
proc ScrollableFrame::see { path widget {vert top} {horz left} {xOffset 0} {yOffset 0}} {
    set x0  [winfo x $widget]
    set y0  [winfo y $widget]
    set x1  [expr {$x0+[winfo width  $widget]}]
    set y1  [expr {$y0+[winfo height $widget]}]
    set xb0 [$path:cmd canvasx 0]
    set yb0 [$path:cmd canvasy 0]
    set xb1 [$path:cmd canvasx [winfo width  $path]]
    set yb1 [$path:cmd canvasy [winfo height $path]]
    set dx  0
    set dy  0
    
    if { [string equal $horz "left"] } {
	if { $x1 > $xb1 } {
	    set dx [expr {$x1-$xb1}]
	}
	if { $x0 < $xb0+$dx } {
	    set dx [expr {$x0-$xb0}]
	}
    } elseif { [string equal $horz "right"] } {
	if { $x0 < $xb0 } {
	    set dx [expr {$x0-$xb0}]
	}
	if { $x1 > $xb1+$dx } {
	    set dx [expr {$x1-$xb1}]
	}
    }

    if { [string equal $vert "top"] } {
	if { $y1 > $yb1 } {
	    set dy [expr {$y1-$yb1}]
	}
	if { $y0 < $yb0+$dy } {
	    set dy [expr {$y0-$yb0}]
	}
    } elseif { [string equal $vert "bottom"] } {
	if { $y0 < $yb0 } {
	    set dy [expr {$y0-$yb0}]
	}
	if { $y1 > $yb1+$dy } {
	    set dy [expr {$y1-$yb1}]
	}
    }

    if {($dx + $xOffset) != 0} {
	set x [expr {($xb0+$dx+$xOffset)/[winfo width $path.frame]}]
	$path:cmd xview moveto $x
    }
    if {($dy + $yOffset) != 0} {
	set y [expr {($yb0+$dy+$yOffset)/[winfo height $path.frame]}]
	$path:cmd yview moveto $y
    }
}


# ----------------------------------------------------------------------------
#  Command ScrollableFrame::xview
# ----------------------------------------------------------------------------
proc ScrollableFrame::xview { path args } {
    return [eval [list $path:cmd xview] $args]
}


# ----------------------------------------------------------------------------
#  Command ScrollableFrame::yview
# ----------------------------------------------------------------------------
proc ScrollableFrame::yview { path args } {
    return [eval [list $path:cmd yview] $args]
}


# ----------------------------------------------------------------------------
#  Command ScrollableFrame::_resize
# ----------------------------------------------------------------------------
proc ScrollableFrame::_resize { path } {
    if { [Widget::getoption $path -constrainedwidth] } {
        $path:cmd itemconfigure win -width [winfo width $path]
    }
    if { [Widget::getoption $path -constrainedheight] } {
        $path:cmd itemconfigure win -height [winfo height $path]
    }
    # scollregion must also be reset when canvas size changes
    _frameConfigure $path
}


# ----------------------------------------------------------------------------
#  Command ScrollableFrame::_frameConfigure
# ----------------------------------------------------------------------------
proc ScrollableFrame::_max {a b} {return [expr {$a <= $b ? $b : $a}]}
proc ScrollableFrame::_frameConfigure {canvas {unmap 0}} {
    # This ensures that we don't get funny scrollability in the frame
    # when it is smaller than the canvas space
    # use [winfo] to get height & width of frame

    # [winfo] doesn't work for unmapped frame
    set frameh [expr {$unmap ? 0 : [winfo height $canvas.frame]}]
    set framew [expr {$unmap ? 0 : [winfo width  $canvas.frame]}]

    set height [_max $frameh [winfo height $canvas]]
    set width  [_max $framew [winfo width  $canvas]]

    $canvas:cmd configure -scrollregion [list 0 0 $width $height]
}

# ----------------------------------------------------------------------------
#  Command ScrollableFrame::_themechanged
# ----------------------------------------------------------------------------
proc ScrollableFrame::_themechanged { path } {

    if { ![winfo exists $path] } { return }
    BWidget::set_themedefaults
    $path configure -background $BWidget::colors(SystemWindowFrame)
}
