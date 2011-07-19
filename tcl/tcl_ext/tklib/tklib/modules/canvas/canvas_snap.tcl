# *- tcl -*-
# ### ### ### ######### ######### #########

# Copyright (c) 2004 George Petasis
# Origin http://wiki.tcl.tk/1404 [24-10-2004]
# BSD licensed.

# ### ### ### ######### ######### #########
## Requisites

package require Tcl 8.5
package require Tk  8.5
package require img::window

namespace eval ::canvas {}

# ### ### ### ######### ######### #########
## Implementation.

proc ::canvas::snap {canvas} {

    # Ensure that the window is on top of everything else, so as not
    # to get white ranges in the image, due to overlapped portions of
    # the window with other windows...

    raise [winfo toplevel $canvas] 
    update

    # XXX: Undo the raise at the end ?!

    set border [expr {[$canvas cget -borderwidth] +
                      [$canvas cget -highlightthickness]}]

    set view_height [expr {[winfo height $canvas]-2*$border}]
    set view_width  [expr {[winfo width  $canvas]-2*$border}]

    lassign [$canvas bbox all] x1 y1 x2 y2
    #foreach {x1 y1 x2 y2} [$canvas bbox all] break

    set x1 [expr {int($x1-10)}]
    set y1 [expr {int($y1-10)}]
    set x2 [expr {int($x2+10)}]
    set y2 [expr {int($y2+10)}]

    set width  [expr {$x2-$x1}]
    set height [expr {$y2-$y1}]

    set image [image create photo -height $height -width $width]

    # Arrange the scrollregion of the canvas to get the whole window
    # visible, so as to grab it into an image...

    # Save the scrolling state, as this will be overidden in short order.
    set scrollregion   [$canvas cget -scrollregion]
    set xscrollcommand [$canvas cget -xscrollcommand]
    set yscrollcommand [$canvas cget -yscrollcommand]

    $canvas configure -xscrollcommand {}
    $canvas configure -yscrollcommand {}

    set grabbed_x $x1
    set grabbed_y $y1
    set image_x   0
    set image_y   0

    while {$grabbed_y < $y2} {
	while {$grabbed_x < $x2} {
	    set newregion [list \
			       $grabbed_x \
			       $grabbed_y \
			       [expr {$grabbed_x + $view_width}] \
			       [expr {$grabbed_y + $view_height}]]

	    $canvas configure -scrollregion $newregion
	    update

	    # Take a screenshot of the visible canvas part...
	    set tmp [image create photo -format window -data $canvas]

	    # Copy the screenshot to the target image...
	    $image copy $tmp -to $image_x $image_y -from $border $border

	    # And delete the temporary image (leak in original code)
	    image delete $tmp

	    incr grabbed_x $view_width
	    incr image_x   $view_width
	}

	set grabbed_x $x1
	set image_x 0

	incr grabbed_y $view_height
	incr image_y   $view_height
    }

    # Restore the previous scrolling state of the canvas.

    $canvas configure -scrollregion   $scrollregion
    $canvas configure -xscrollcommand $xscrollcommand
    $canvas configure -yscrollcommand $yscrollcommand

    # At last, return the fully assembled snapshot
    return $image
}

# ### ### ### ######### ######### #########
## Ready

package provide canvas::snap 1.0.1
return
