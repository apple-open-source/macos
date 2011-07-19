## -*- tcl -*-
# ### ### ### ######### ######### #########

# Known issue :: It is unspecified who is responsible for the images
#                after they are used in the canvas. The canvas
#                currently doesn't delete them. Meaning, this is
#                likely leaking memory like mad when switching between
#                sources, and dragging around.

# sqmap = square map.

# Ideas to work on ...

# -- Factor the low-level viewport tracking and viewport stabilization
#    across scroll-region changes out into its own canvas class.

# -- Factor the grid layer handling into its own class. That is a
#    requisite for the handling of multiple layers,

# -- Create a hexmap, i.e. hexagonal tiling. This can be done with
#    images as well, with parts properly transparent and then
#    positioned to overlap. Regarding coordinates this can be seen
#    as a skewed cartesian system, so only 2 coordinates required

# -- Consider viewport stabilization for when the canvas is resized.

# ### ### ### ######### ######### #########
## Requisites

package require Tcl 8.4          ; # No {*}-expansion! :(
package require Tk
package require snit             ; # 
package require uevent::onidle   ; # Some defered actions.
package require cache::async 0.3 ; # Internal tile cache.

# ### ### ### ######### ######### #########
##

snit::widgetadaptor canvas::sqmap {
    # ### ### ### ######### ######### #########
    ## API

    # All canvas options, except for -scrollregion are accepted by
    # this widget(adaptor), and propagated to the embedded canvas. The
    # region is always implicitly (0,0,w,h), with w and h computed
    # from the number of grid rows, columns and the cell dimensions.

    delegate option * to hull except -scrollregion

    # All canvas methods are accepted and propagated to the embedded
    # canvas. Some of them we intercept however, to either impose
    # restrictions (*), or get information we need and not available
    # otherwise (**).

    # (*) The images used as background have to stay lower than all
    #     user-created items, to be that background. We cannot allow
    #     them to be raised, nor must others go below them.

    #     If we were extremely rigourous we would have to intercept
    #     all methods and filter out our internal tags and items ids,
    #     to make them completely invisible to the user. The last 5%
    #     needing 90% of the effort. *** Defered ***

    # (**) Dragging changes the viewport, we do not see this without
    #      interception.

    delegate method *           to hull except {lower raise scan xview yview}
    delegate method {scan mark} to hull as {scan mark}

    # New options: Information about the grid, and where to get the
    # images.
    # rows    = number of rows the grid consists of. <0 <=> unlimited
    # columns = s.a., columns
    # cell-width   = width of a cell in the grid, in pixels
    # cell-height  = s.a., height
    # cell-source  = command prefix called to get the image for a cell in the grid.

    option -grid-cell-width   -default 0  -configuremethod O-ReconfigureNum -type {snit::integer -min 0}
    option -grid-cell-height  -default 0  -configuremethod O-ReconfigureNum -type {snit::integer -min 0}
    option -grid-cell-command -default {} -configuremethod O-ReconfigureStr
    option -scrollregion      -default {} -configuremethod O-ReconfigureStr

    # NOTE AK, maybe, for the future.
    # rows/columns - we may wish to have min/max values, if any to represent
    #              - grid boundaries.
    #option -grid-rows        -default 0  -configuremethod O-ReconfigureNum
    #option -grid-columns     -default 0  -configuremethod O-ReconfigureNum

    # NOTE !!! Use -grid-show-borders only for short-term debugging.
    # NOTE !!! The items created when true are never deleted, i.e. leaking memory

    option -grid-show-borders -default 0 -type snit::boolean

    option -viewport-command -default {} -configuremethod O-vp-command

    option -image-on-load  -default {}
    option -image-on-unset -default {}

    constructor {args} {
	installhull using canvas

	install reconfigure using uevent::onidle ${selfns}::reconfigure \
	    [mymethod Reconfigure]

	install redraw using uevent::onidle ${selfns}::redraw \
	    [mymethod Redraw]

	install tilecache using cache::async ${selfns}::tilecache \
	    [mymethod Tile] -full-async-results 0
	# Configuration means synchronous return of in-cache results.
	# This is needed to get proper use and disposal of ->
	# myfreeitems.

	bind $win <Configure> [mymethod Configure]

	$self configurelist $args
	return
    }

    # ### ### ### ######### ######### #########
    ## API. Define/Remove images from grid cells. These are the main
    ## commands to control grid appearance. The -grid-cell-command should
    ## use these commands as well to provide its results to the
    ## widget.

    method {image set} {at image} {
	$tilecache set $at $image

	# Nothing more is required for an invisible cell.
	if {![info exists myvisible($at)]} return

	# For empty cells we create proper items now.
	set theitem $myvisible($at)
	if {$theitem eq ""} {
	    set theitem [$self GetItem [GridToPixel $at]]
	    set myvisible($at) $theitem
	}

	# Show the chosen image
	$hull itemconfigure $theitem -image $image
	return
    }

    method {image unset} {at} {
	# Show an image signaling that 'this tile is not valid/found' ...
	if {$options(-image-on-unset) ne {}} {
	    $self image set $at $options(-image-on-unset)
	    return
	}

	$tilecache unset $at

	# Nothing more is required for an invisible cell.
	if {![info exists myvisible($at)]} return

	# Nothing more is required for an empty cell.
	set theitem $myvisible($at)
	if {$theitem eq ""} return

	# Mark the cell as empty and drop the associated item.
	set myvisible($at) ""
	$hull delete $theitem
	return
    }

    # ### ### ### ######### ######### #########
    ## Force a full reload of all (visible) cells.

    method flush {} {
	$tilecache clear
	set mypixelview {}
	#puts REDRAW-RQ/flush
	$redraw request
	return
    }

    # ### ### ### ######### ######### #########
    ## Intercepting the methods changing the display order, to ensure
    ## that our grid is kept at the bottom. It is the background after
    ## all.

    method raise {args} {
	eval [linsert $args 0 $hull raise]
	# Ensure that our cells stay at the bottom.
	$hull lower $ourtag
	return
    }

    method lower {args} {
	eval [linsert $args 0 $hull lower]
	# Ensure that our cells stay at the bottom.
	$hull lower $ourtag
	return
    }

    # ### ### ### ######### ######### #########
    ## Intercepting the dragto command to keep track of the
    ## viewport. See the scroll method interception below too.

    # NOTE: 'scan mark' interception will be needed if we wish to
    # allow items to float in place regardless of dragging (i.e. as UI
    # elements, for example a zoom-scale).

    method {scan dragto} {x y {gain 1}} {
	# Regular handling of dragging ...
	$hull scan dragto $x $y $gain

	# ... then compute and record the changed viewport, and
	# request a redraw to be done when the system has time for it
	$self SetPixelView
	return
    }

    # ### ### ### ######### ######### #########
    ## Intercepting the scroll methods to keep track of the viewport.
    ## The canvas has no way to report changes on its own. No
    ## callbacks, nothing. See the dragto interception above too.

    method xview {args} {
	# Regular handling of scrolling ...
	set res [eval [linsert $args 0 $hull xview]]
	# Keep track of the viewport in case of changes.
	if {[llength $args]} { $self SetPixelView }
	return $res
    }

    method yview {args} {
	# Regular handling of scrolling ...
	set res [eval [linsert $args 0 $hull yview]]
	# Keep track of the viewport in case of changes.
	if {[llength $args]} { $self SetPixelView }
	return $res
    }

    # ### ### ### ######### ######### #########
    ## Intercept <Configure> events on the canvas. This changes the
    ## viewport. At the time the event happens the new viewport is not
    ## yet known, as this is done in a canvas-internal idle-handler. We
    ## simply trigger our redraw in our idle-handler, and force it to
    ## recompute the viewport.

    method Configure {} {
	set mypixelview {} ; # Force full recalculation.
	#puts REDRAW-RQ/configure
	$redraw request
	return
    }

    # ### ### ### ######### ######### #########

    method O-vp-command {o v} {
        #puts $o=$v
        if {$options($o) eq $v} return
        set  options($o) $v
        set myhasvpcommand [expr {!![llength $v]}]
        if {!$myhasvpcommand} return
        # Callback changed and ok, request first call with current
        # settings.
        $self PixelViewExport
        return
    }

    variable myhasvpcommand     0 ; # Track use of viewport-command callback

    method PixelViewExport {} {
        if {!$myhasvpcommand} return
	if {![llength $mypixelview]} return
        foreach {xl yt xr yb} $mypixelview break
        uplevel \#0 [linsert $options(-viewport-command) end $xl $yt $xr $yb]
        return
    }

    method SetPixelView {} {
	set mypixelview [PV]
	$self PixelViewExport
	# Viewport changes imply redraws
	#puts REDRAW-RQ/set-pixel-view
	$redraw request
	return
    }

    proc PV {} {
        upvar 1 hull hull win win
        return [list \
                    [$hull canvasx 0] \
                    [$hull canvasy 0] \
                    [$hull canvasx [winfo width $win]] \
                    [$hull canvasy [winfo height $win]]]
    }

    # ### ### ### ######### ######### #########
    ## Option processing. Any changes force a refresh of the grid
    ## information, and then a redraw.

    method O-ReconfigureNum {o v} {
	#puts $o=$v
	if {$options($o) == $v} return
	set  options($o) $v
	$reconfigure request
	return
    }

    method O-ReconfigureStr {o v} {
	#puts $o=$v
	if {$options($o) eq $v} return
	set  options($o) $v
	$reconfigure request
	return
    }

    component reconfigure
    method Reconfigure {} {
	#puts /reconfigure

	# The grid definition has changed, in parts, or all. We have
	# to redraw the background, even if nothing else was changed.
	# Here we commit all changed option values to the engine.
	# This is the only place accessing the options array.

	set oldsr $myscrollregion

	set mygridwidth    $options(-grid-cell-width)
	set mygridheight   $options(-grid-cell-height)
	set mygridcmd      $options(-grid-cell-command)
	set myscrollregion $options(-scrollregion)

	# Commit region change to the canvas itself

	$hull configure -scrollregion $myscrollregion

	# Flush the cache to force a reload of the entire visible
	# area now, and of the invisible part later when scrolling.
	$tilecache clear

	# Now save and restore the view, keeping the center of the
	# view as stable as possible across the transition. Note, the
	# scrapyard at the end of this file contains the same
	# calculations in long form, i.e. all steps written out. Here
	# the various expressions are inlined and simplified.

	foreach { sxl  syt  sxr  syb} $oldsr break
	if {[llength $oldsr] && (($sxr - $sxl) > 0) && (($syb - $syt) > 0)} {
	    # Old and new scroll regions.
	    foreach {nsxl nsyt nsxr nsyb} $myscrollregion break

	    #puts OSR=($oldsr)
	    #puts NSR=($myscrollregion)

	    # Get current pixel view, and limit it to the old
	    # scrollregion (as the canvas may show more than the
	    # scrollregion).
	    foreach {xl yt xr yb} $mypixelview break
	    if {$xl < $sxl} { set xl $sxl }
	    if {$xr > $sxr} { set xr $sxr }
	    if {$yt < $syt} { set yt $syt }
	    if {$yb > $syb} { set yb $syb }

	    # Determine the center of the pixel view, as fractions
	    # relative to old scroll origin.
	    set xcfrac [expr {double((($xr + $xl)/2) - $sxl) / ($sxr - $sxl)}]
	    set ycfrac [expr {double((($yt + $yb)/2) - $syt) / ($syb - $syt)}]

	    # The fractions for the topleft corner are the fractions
	    # of the center less the (fractional manhattan radii
	    # around the center, relative to the new region).
	    set nxlfrac [expr {$xcfrac - double(($xr - $xl)/2) / ($nsxr - $nsxl)}]
	    set nytfrac [expr {$ycfrac - double(($yb - $yt)/2) / ($nsyb - $nsyt)}]

	    # Limit the fractions to the scroll origin (>= 0).
	    if {$nxlfrac < 0} { set nxlfrac 0 }
	    if {$nytfrac < 0} { set nytfrac 0 }

	    # Adjust canvas view to keep the center as stable as
	    # possible across the transition. Note that this goes
	    # through our own xview/yview method, calls SetPixelView,
	    # and through that requests a redraw. No need to have the
	    # redraw done by this method.

	    #puts MOVETO\t$nxlfrac,$nytfrac
	    $self xview moveto $nxlfrac
	    $self yview moveto $nytfrac

	    # Note however that we still have to force the redraw to
	    # be fully done.
	    set mypixelview {}
	} else {
	    # Nearly last, redraw full. This happens only because no
	    # view adjustments were done which would have forced it
	    # (see above), so in this cause we have to do it
	    # ourselves.
	    $self Redraw 1
	}
	#puts reconfigure/done
	return
    }

    # ### ### ### ######### ######### #########
    ## Grid redraw. This is done after changes to the viewport,
    ## and when the system is idle.

    component redraw
    method Redraw {{forced 0}} {
	#puts /redraw/$forced

	# Compute viewport in tile coordinates and compare to last.
	# This will tell us where to update and how, if any.

	if {![llength $mypixelview]} {
	    # Undefined viewport, generate baseline, and force
	    # redraw. Scheduling another redraw is however not needed,
	    # so we are inlining only parts of SetPixelView.
	    set mypixelview [PV]
	    $self PixelViewExport
	    #puts \tforce-due-undefined-viewport
	    set forced 1
	}

	set gridview [PixelToGrid $mypixelview]
	foreach {xl yt xr yb} $gridview        break
	foreach {ll lt lr lb} $myshowngridview break

	#puts \tVP=($mypixelview)
	#puts \tVG=($gridview)
	#puts \tVL=($myshowngridview)
	#puts \tF'=$forced

	if {!$forced} {
	    # Viewport unchanged, nothing to do.
	    if {($xl == $ll) && ($xr == $lr) &&
		($yt == $lt) && ($yb == $lb)} {
		#puts \tunchanged,ignore
		return
	    }
	}

	set myfreeitems {}

	# NOTE. The code below is suboptimal. While already better
	# than dropping and recreating all items, we could optimize by
	# using the structure of the viewport (rectangles) to
	# determine directly which grid cells became (in)visible, from
	# the viewport boundary coordinates. This will however be also
	# quite more complex, with all the possible cases of
	# overlapping old and new views.

	if {$forced} {
	    # Forced redraw, simply make all items available
	    # for the upcoming fill.

	    foreach at [array names myvisible] {
		$self FreeCell $at
	    }
	} elseif {[llength $myshowngridview]} {
	    # Scan through the grid cells of the view used at the ast
	    # redraw, and check which of them have become
	    # invisible. Put these on the list of items we can reuse
	    # for the cells which just became visible and thus in need
	    # of items.

	    for {set r $lt} {$r <= $lb} {incr r} {
		for {set c $ll} {$c <= $lr} {incr c} {
		    if {($r < $yt) || ($yb < $r) || ($c < $xl) || ($xr < $c)} {
			# The grid cell dropped out of the viewport.
			$self FreeCell [list $r $c]
			#puts /drop/$idx
		    }
		}
	    }
	}

	# Remember location for next redraw.
	set myshowngridview $gridview

	for {set r $yt} {$r <= $yb} {incr r} {
	    for {set c $xl} {$c <= $xr} {incr c} {
		# Now scan through the cells of the new viewport.
		# Ignore those which are still visible, and create the
		# remainder.
		set at [list $r $c]
		if {[info exists myvisible($at)]} continue
		#puts /make/$idx
		set myvisible($at) "" ; # placeholder

		# Show an image signaling that 'we are loading this tile' ...
		if {$options(-image-on-load) ne {}} {
		    set theitem [$self GetItem [GridToPixel $at]]
		    set myvisible($at)  $theitem
		    $hull itemconfigure $theitem \
			-image $options(-image-on-load)
		}

		after 0 [list $tilecache get $at [mymethod image]]
		# This cache access re-uses the items in myfreeitems
		# as images already in the cache are delivered
		# synchronously, going through 'image set' and
		# GetItem. Only unknown cells will come later.
	    }
	}

	# Delete all items which were not reused.

	# No, no need. Canvas image items without an image configured
	# for display are effectively invisible, regardless of
	# location. Keep them around for late coming provider results.
	#$self DropFreeItems
	#puts redraw/done
	return
    }

    method FreeCell {at} {
	# Ignore already invisible cells
	if {![info exists myvisible($at)]} return

	# Clear empty cells, nothing more
	set theitem $myvisible($at)
	unset myvisible($at)
	if {$theitem eq ""} return

	# Record re-usable item and clear the image it used. Note that
	# this doesn't delete the image!
	lappend myfreeitems $theitem
	$hull itemconfigure $theitem -image {}
	return
    }

    method {Tile get} {at donecmd} {
	# Tile cache provider callback. The request is routed to the
	# canvas's own tile provider. Responses go to the cache. The
	# cache is set up that its responses go to the 'image ...'
	# methods.

	if {![llength $mygridcmd]} return
	#puts \t\t\t\tGet($at)
	uplevel #0 [linsert $mygridcmd end get $at $donecmd]
	return
    }

    method GetItem {location} {
	# location = pixel position, list (x y)
	if {[llength $myfreeitems]} {
	    # Free items were found, reuse one of them.

	    set theitem     [lindex   $myfreeitems end]
	    set myfreeitems [lreplace $myfreeitems end end]

	    $hull coords        $theitem $location
	    $hull itemconfigure $theitem -image {}
	} else {
	    # Nothing available for reuse, create a new item.

	    if {$options(-grid-show-borders)} {
		# Helper markers for debugging, showing cell borders
		# and coordinates.

		# NOTE !!! Use -grid-show-borders only for short-term debugging.
		# NOTE !!! The items create here are never deleted, i.e. leaking memory

		foreach {x y} $location break
		set x [expr {int($x)}]
		set y [expr {int($y)}]
		set t "<[expr {$y/$mygridheight}],[expr {$x/$mygridwidth}]>"

		incr x 2 ; incr y 2
		set x1 $x ; incr x1 $mygridwidth  ; incr x1 -2
		set y1 $y ; incr y1 $mygridheight ; incr y1 -2

		$hull create rectangle $x $y $x1 $y1 -outline red
		incr x 4 ; incr y 4
		set t [$hull create text $x $y -fill red -anchor nw -text $t]
		$hull raise $t
	    }

	    set theitem [$hull create image $location -anchor nw -tags [list $ourtag]]
	    $hull lower $theitem
	}
	return $theitem
    }

    method DropFreeItems {} {
	if {[llength $myfreeitems]} {
	    eval [linsert $myfreeitems 0 $hull delete]
	    set myfreeitems {}
	}
	return
    }

    # ### ### ### ######### ######### #########

    proc PixelToGrid {pixelview} {
	# Import grid definitions ...
	upvar 1 mygridwidth gcw mygridheight gch
	foreach {xl yt xr yb} $pixelview break

	set coll [expr {int($xl / double($gcw))}]
	set colr [expr {int($xr / double($gcw))}]
	set rowt [expr {int($yt / double($gch))}]
	set rowb [expr {int($yb / double($gch))}]

	# NOTE AK: Maybe limit cell coordinates to boundaries, if
	# NOTE AK: so requested.

	return [list $coll $rowt $colr $rowb]
    }

    proc GridToPixel {at} {
	# Import grid definitions ...
	upvar 1 mygridwidth gcw mygridheight gch
	foreach {r c} $at break
	set y [expr {int($r * double($gch))}]
	set x [expr {int($c * double($gcw))}]
	return [list $x $y]
    }

    # ### ### ### ######### ######### #########
    ## State

    # Active copies of various options. Their use prevents races in
    # the redraw logic using new option values while other parts are
    # not adapted to the changes. The 'Reconfigure' method is
    # responsible for the atomic commit of external changes to the
    # internal engine.

    variable mygridwidth    {} ; # Grid definition used by the engine.
    variable mygridheight   {} ; # s.a.
    variable mygridcmd      {} ; # s.a.
    variable myscrollregion {} ; # s.a.

    # All arrays using grid cells as keys, i.e. 'myvisible', use grid
    # cell coordinates to reference grid cell, in the form
    # 	tuple(row, col)
    #
    # This is the same form taken by the grid-cell-command command prefix and makes
    # use of keys easier as it they are the same across the board.

    # Cache for quick lookup of images and image misses we have seen
    # before, to avoid async round-trips through the
    # grid-cell-command, aka image provider.

    component tilecache

    # Tracking the viewport, i.e. the visible area of the canvas
    # within the scrollregion.

    variable mypixelview     {} ; # Current viewport of the hull, in pixels.
    variable myshowngridview {} ; # Viewport set by last Redraw, in grid cell coordinates

    # Tracking the grid cells shown in the viewport and their canvas
    # items.

    variable myvisible -array {} ; # Visible grid cells, mapped to their canvas items.

    # Transient list of items available for reassignment.

    variable myfreeitems {}

    # Tag used to mark all canvas items used for the grid cell display.

    typevariable ourtag canvas::sqmap::cells

    # ### ### ### ######### ######### #########
}

# ### ### ### ######### ######### #########
## Ready

package provide canvas::sqmap 0.3.1
return
