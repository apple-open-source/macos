## -*- tcl -*-
## (C) 2010 Andreas Kupries <andreas_kupries@users.sourceforge.net>
## BSD Licensed
# # ## ### ##### ######## ############# ######################

#
# diagram, basic elements (line, arc, box, circle, ellipse, diamond, drum, text)
#

##
# # ## ### ##### ######## ############# ######################
## Requisites

package require Tcl 8.5              ; # Want the nice things it
				       # brings (dicts, {*}, etc.)
package require snit                 ; # Object framework.
package require math::geometry 1.1.2 ; # Vector math (points, line
				       # (segments), poly-lines).
package require diagram::point       ; # Tagged geometry data and ops

# # ## ### ##### ######## ############# ######################
## Implementation

snit::type ::diagram::basic {

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Construction, attach to the specified core.

    constructor {thecore} {
	set core $thecore

	# Basic elements ... First the closed elements (closed curves) ...

	DefE $core box     {textcolor textfont anchor justify stroke style color fillcolor at with width height slant}
	DefE $core circle  {textcolor textfont anchor justify stroke style color fillcolor at with circle::radius} 
	DefE $core diamond {textcolor textfont anchor justify stroke style color fillcolor at with diamond::width diamond::height diamond::aspect}
	DefE $core drum    {textcolor textfont anchor justify stroke style color fillcolor at with width height drum::aspect}
	DefE $core ellipse {textcolor textfont anchor justify stroke style color fillcolor at with width height}
	DefE $core text    {textcolor textfont anchor justify text at with}

	# ... and then the open elements (open curves).

	DefE $core line    {textcolor textfont anchor justify stroke style color fillcolor from to then smooth arrowhead noturn}
	DefE $core arc     {textcolor textfont anchor justify stroke style color fillcolor arc::radius clockwise arc::from arc::to}

	$core new shape arrow
	$core new shape spline

	# Note 1: The attribute order is important for arc elements.
	#         We wish to resolve both clockwise and radius before
	#         the from/to points, as we need this data available
	#         for when we have to determine defaults.

	# Note 2: text elements do not require defaults for width and
	#         height, see the marker (%%) for more information.

	# ... and their attributes ...

	# common validation types
	set dzero [snit::double  ${selfns}::D0 -min 0]
	set dmin  [snit::double  ${selfns}::D1 -min 0];# 0 exclusive.
	set imin  [snit::integer ${selfns}::I1 -min 1]

	# general element style

	$core new attribute stroke    linked {linewidth  1}  type $imin
	$core new attribute style     linked {linestyle  {}} transform [myproc LineStyle]
	$core new attribute color     linked {linecolor  black}
	$core new attribute fillcolor linked {fillcolor  {}}

	$core new attribute text      aggregate 1
	$core new attribute textcolor linked {textcolor black}
	$core new attribute textfont  linked {textfont  {Helvetica 12}}
	$core new attribute anchor    linked {anchor    center}
	$core new attribute justify   linked {justify   left}

	# box geometry, width/height shared with ellipse, drum

	$core new attribute width  linked [Link boxwidth  2 cm] type $dmin
	$core new attribute height linked [Link boxheight 2 cm] type $dmin
	$core new attribute slant  linked {slant     90} type snit::double;# degrees - range normalization - transform ?

	# circle geometry

	$core new attribute arc::radius      linked [Link arcradius    1 cm] type $dmin
	$core new attribute circle::radius   linked [Link circleradius 1 cm] type $dmin
	$core new attribute diameter key circle::radius type $dmin \
	    transform [myproc CircleRadiusByDiameter]

	# diamond geometry

	set dd [mymethod Diamond]
	$core new attribute diamond::width  type $dmin  default $dd
	$core new attribute diamond::height type $dmin  default $dd
	$core new attribute diamond::aspect type $dzero default $dd

	# drum geometry, width, height, see box.

	$core new attribute drum::aspect type $dzero linked {drumaspect 0.35}

	# line style. geometry see core, shared with move command.
	# Note that chop processing happens in the 'Waypoints' ensemble, in core!

	$core new attribute chop aggregate 1 type $dzero get [mymethod Chop]
	$core new attribute arrowhead transform [myproc LineArrows] linked {arrowhead none}
	$core new attribute smooth    type snit::boolean            linked {smooth 0} \
	    get [myproc Smooth]
	$core new attribute noturn    type snit::boolean \
	    get     [myproc NoTurn]
	#default [myproc NoTurnDefault]

	# arc location, and direction (counter(clockwise))

	set al [mymethod ArcLocation]
	$core new attribute arc::from type diagram::point default $al
	$core new attribute arc::to   type diagram::point default $al
	$core new attribute clockwise type snit::boolean linked {clockwise 0} \
	    get [myproc ClockWise 1]
	$core new attribute counterclockwise key clockwise type snit::boolean \
	    get [myproc ClockWise 0]

	# Further a number of shorthands for some commands and
	# attributes, and commands using the unicode glyphs looking
	# like the elements.

	$core new alias spline {line /shape spline smooth}
	$core new alias arrow  {line /shape arrow  arrowhead ->}
	$core new alias \u21d2 {line /shape arrow  arrowhead <-}
	$core new alias \u27f6 {line /shape arrow  arrowhead ->}
	$core new alias -->    {line /shape arrow  arrowhead ->}
	$core new alias <--    {line /shape arrow  arrowhead <-}
	$core new alias <-->   {line /shape arrow  arrowhead <->}
	$core new alias O      circle
	$core new alias --     line
	$core new alias <>     diamond
	$core new alias \u25cb circle
	$core new alias \u25fb box
	$core new alias \u25c7 diamond
	$core new alias \u2312 arc
	$core new alias \u21b6 arc
	$core new alias \u21b7 {arc clockwise}
	$core new alias \u2780 1th
	$core new alias \u2781 2th
	$core new alias \u2782 3th
	$core new alias \u2783 4th
	$core new alias \u2784 5th
	$core new alias \u2785 6th
	$core new alias \u2786 7th
	$core new alias \u2787 8th
	$core new alias \u2788 9th
	$core new alias \u2789 10th
	$core new alias \u2776 {1th last}	
	$core new alias \u2777 {2th last}	
	$core new alias \u2778 {3th last}	
	$core new alias \u2779 {4th last}	
	$core new alias \u277a {5th last}	
	$core new alias \u277b {6th last}	
	$core new alias \u277c {7th last}	
	$core new alias \u277d {8th last}	
	$core new alias \u277e {9th last}	
	$core new alias \u277f {10th last}	

	# The hooks are run in the specified order, first to last,
	# until one takes the element, or the system runs out of of
	# hooks.
	$core unknown attribute [myproc Styles]
	$core unknown attribute [myproc Arrowheads]
	$core unknown attribute [myproc Shorthands]
	$core unknown attribute [myproc Label]
	return
    }

    # # ## ### ##### ######## ############# ######################
    ## Internal :: Register a shape.

    proc DefE {core name required} {
	upvar 1 selfns selfns
	$core new element $name $required [mymethod $name]
	return
    }

    # # ## ### ##### ######## ############# ######################

    proc CircleRadiusByDiameter {diameter} {
	return [expr {double($diameter)/2}]
    }

    proc LineStyle {s} {
	switch -exact -- $s {
	    solid        { return {}  }
	    dot          { return .   }
	    dotted       { return .   }
	    dash         { return -   }
	    dashed       { return -   }
	    dash-dot     { return -.  }
	    dash-dot-dot { return -.. }
	    default      { return $s }
	}
    }

    proc Styles {shape words} {
	set w [{*}$words peek]
	if {![info exists ourstyles($w)]} {return 0}
	{*}$words unget style
	return 1
    }

    proc LineArrows {s} {
	switch -exact -- $s {
	    start   { return first }
	    end     { return last  }
	    ->      { return last  }
	    <-      { return first }
	    <->     { return both  }
	    -       { return none  }
	    \u21a6  { return last  }
	    \u21a4  { return first }
	    \u21ae  { return both  }
	    default { return $s }
	}
    }

    proc Arrowheads {shape words} {
	set w [{*}$words peek]
	if {![info exists ourarrows($w)]} {return 0}
	{*}$words unget arrowhead
	return 1
    }

    proc Shorthands {shape words} {
	set w [{*}$words peek]
	if {![info exists ourshorts($w)]} {return 0}
	# Drop the alias name and then stuff the replacement in.
	{*}$words get
	foreach str [lreverse $ourshorts($w)] {
	    {*}$words unget $str
	}
	return 1
    }

    proc Label {shape words} {
	# Catch all attribute hook. Register last, as no hook coming
	# after it will be run. Any unknown attribute is taken to be a
	# text label associated with the element.
	{*}$words unget text
	return 1
    }

    # # ## ### ##### ######## ############# ######################
    ## Internal :: Shape implementations.

    method box {canvas attributes} {
	array set a $attributes

	set styling [list \
			 -fill    $a(fillcolor) \
			 -outline $a(color)     \
			 -width   $a(stroke)    \
			 -dash    $a(style)]

	if {$a(slant) != 90} {
	    lassign [BoxSlantedCorners a] corners polygon

	    lappend items [$canvas create polygon \
			       {*}$polygon {*}$styling]
	} else {
	    lassign [BoxCorners a] corners rect

	    lappend items [$canvas create rectangle \
			       {*}$rect {*}$styling]
	}

	HandleText $canvas $attributes items [dict get $corners center]
	return [list $items $corners]
    }

    # # ## ### ##### ######## ############# ######################

    proc BoxSlantedCorners {av} {
	upvar 1 $av a

	lassign [BoxCorners a] corners rect

	set s $a(slant)
	set w $a(width)
	set h $a(height)

	set dx    [expr {cos($s * (4*atan(1))/180.) * $h}]
	set shift [geo::h $dx]
	set up    [geo::s* -0.5 [geo::v $h]]
	set right [geo::s*  0.5 [geo::h $w]]

	set nw [list \
		    [expr {-$w/2.0}] \
		    [expr {-$h/2.0}]]
	set se [list \
		    [expr { $w/2.0}] \
		    [expr { $h/2.0}]]

	# We compute all the corner points as well, given that they
	# have custom locations.

	set center {0 0};#[geo::between $nw $se 0.5]

	if {$dx > 0} {
	    #             xc
	    #          xnw xn  xne
	    # ynw     (*)--*---*
	    #         /   /   /
	    #        *--<*>--* yc
	    #       /   /   /
	    #      *---*--(*) yse
	    #      xsw xs  xse

	    set vne [geo::+ [geo::+ $right $shift] $up]

	    set northwest $nw
	    set northeast [geo::+ $center $vne]
	    set southeast $se
	    set southwest [geo::- $center $vne]

	} else {
	    #             xc
	    #      xnw xn  xne
	    # ynw (*)--*---*
	    #       \   \   \.
	    #        *--<*>--* yc
	    #         \   \   \.
	    #          *---*--(*) yse
	    #          xsw xs  xse

	    lassign $nw xnw ynw
	    lassign $se xse yse

	    set northwest [geo::+ $nw $shift]
	    set northeast [geo::p $xse $ynw]
	    set southeast [geo::- $se $shift]
	    set southwest [geo::p $xnw $yse]
	}

	set north [geo::between $northwest $northeast 0.5]
	set east  [geo::between $northeast $southeast 0.5]
	set south [geo::between $southwest $southeast 0.5]
	set west  [geo::between $northwest $southwest 0.5]

	set polygon [list \
			 {*}$northwest {*}$northeast \
			 {*}$southeast {*}$southwest]

	set corners [list \
			 north     [diagram::point at {*}$north] \
			 northeast [diagram::point at {*}$northeast] \
			 east      [diagram::point at {*}$east] \
			 southeast [diagram::point at {*}$southeast] \
			 south     [diagram::point at {*}$south] \
			 southwest [diagram::point at {*}$southwest] \
			 west      [diagram::point at {*}$west] \
			 northwest [diagram::point at {*}$northwest] \
			 center    [diagram::point at {*}$center]]

	return [list $corners $polygon]
    }

    proc BoxCorners {av} {
	upvar 1 $av a

	#      xnw xns
	# ynw (*)--*---*
	#      |   |   |
	#      *--<*>--* yew
	#      |   |   |
	#      *---*--(*) yse
	#              xse

	set w $a(width)
	set h $a(height)

	set rect [list \
		      [expr {-$w/2.0}] \
		      [expr {-$h/2.0}] \
		      [expr { $w/2.0}] \
		      [expr { $h/2.0}]]

	return [list [BoxCornersRect $rect] $rect]
    }

    proc BoxCornersRect {rect} {
	lassign $rect xnw ynw xse yse

	set xns [expr {($xnw + $xse) / 2.0}]
	set yew [expr {($ynw + $yse) / 2.0}]

	set w [expr {$xse - $xnw}]
	set h [expr {$yse - $ynw}]

	return [list \
		    north     [diagram::point at $xns $ynw] \
		    northeast [diagram::point at $xse $ynw] \
		    east      [diagram::point at $xse $yew] \
		    southeast [diagram::point at $xse $yse] \
		    south     [diagram::point at $xns $yse] \
		    southwest [diagram::point at $xnw $yse] \
		    west      [diagram::point at $xnw $yew] \
		    northwest [diagram::point at $xnw $ynw] \
		    center    [diagram::point at $xns $yew] \
		    width     $w \
		    height    $h]
    }

    # # ## ### ##### ######## ############# ######################

    method circle {canvas attributes} {
	array set a $attributes

	lassign [CircleCorners a] corners rect

	lappend items [$canvas create oval {*}$rect \
			   -fill    $a(fillcolor) \
			   -outline $a(color)     \
			   -width   $a(stroke)    \
			   -dash    $a(style)]

	HandleText $canvas $attributes items [dict get $corners center]
	return [list $items $corners]
    }

    # # ## ### ##### ######## ############# ######################

    proc CircleCorners {av} {
	upvar 1 $av a

	#      xnw xns
	# ynw (*)--*---*
	#      |   |   |
	#      *--<*>--* yew
	#      |   |   |
	#      *---*--(*) yse
	#              xse

	set r  $a(circle::radius)
	set rm [expr {-1 * $r}]
	set di [expr { 2 * $r}]

	set rect [list $rm $rm $r $r]

	# The 90-angles are trivial, no need for big floating of math.
	set corners [list \
			 north  [diagram::point at 0   $rm] \
			 east   [diagram::point at $r  0]   \
			 south  [diagram::point at 0   $r]  \
			 west   [diagram::point at $rm 0]   \
			 center [diagram::point at 0   0]   \
			 radius $r  \
			 width  $di \
			 height $di]

	foreach {dir angle} {
	    northeast   45
	    southeast  -45
	    southwest -135
	    northwest  135
	} {
	    lappend corners $dir [diagram::point at {*}[geo::s* $r [geo::direction $angle]]]
	}

	return [list $corners $rect]
    }

    # # ## ### ##### ######## ############# ######################

    method ellipse {canvas attributes} {
	array set a $attributes

	lassign [EllipseCorners a] corners rect

	lappend items [$canvas create oval {*}$rect \
			   -fill    $a(fillcolor) \
			   -outline $a(color)     \
			   -width   $a(stroke)    \
			   -dash    $a(style)]

	HandleText $canvas $attributes items [dict get $corners center]
	return [list $items $corners]
    }

    # # ## ### ##### ######## ############# ######################

    proc EllipseCorners {av} {
	upvar 1 $av a

	# Like CircleCorners, except taking the different radii into account.
	# ra = w/2
	# rb = h/2

	set ra [expr {$a(width)  / 2.0}]
	set rb [expr {$a(height) / 2.0}]

	set rect [list -$ra -$rb $ra $rb]

	# The 90-degree angles are trivial, no need for floating-point math.
	set corners [list \
			 north  [diagram::point at  0   -$rb] \
			 east   [diagram::point at  $ra  0]   \
			 south  [diagram::point at  0    $rb] \
			 west   [diagram::point at -$ra  0]   \
			 center [diagram::point at  0    0]   \
			 width  $a(width) \
			 height $a(height)]

	# For the 45-degree angles we use precomputed values we just
	# have to stretch per the actual ellipse radii
	foreach {dir cos} $ourecos {_ sin} $ouresin {
	    set x [expr {$ra * $cos}]
	    set y [expr {$rb * $sin}]
	    lappend corners $dir [diagram::point at $x $y]
	}

	return [list $corners $rect]
    }

    # # ## ### ##### ######## ############# ######################

    method diamond {canvas attributes} {
	array set a $attributes

	lassign [DiamondCorners a] corners poly

	lappend items [$canvas create polygon {*}$poly \
			   -fill    $a(fillcolor) \
			   -outline $a(color)     \
			   -width   $a(stroke)    \
			   -dash    $a(style)]

	HandleText $canvas $attributes items [dict get $corners center]
	list $items $corners
    }

    # # ## ### ##### ######## ############# ######################

    proc DiamondCorners {av} {

	#      *
	#     /|\.
	#    * | *
	#   / \|/ \.
	#  *--<*>--*
	#   \ /|\ /
	#    * | *
	#     \|/
	#      *

	upvar 1 $av a

	set w $a(diamond::width)
	set h $a(diamond::height)

	# No calculation of aspect here. This was handled in
	# DiamondDefaults. Well, in DefaultDiamondGeometry it
	# delegated this to.

	set hh [expr {0.5 * $h}]
	set hw [expr {0.5 * $w}]

	# Cardinal points.
	set north [geo::p 0 -$hh]
	set south [geo::p 0  $hh]
	set east  [geo::p  $hw 0]
	set west  [geo::p -$hw 0]

	# 45-angled points, interpolated between the cardinals.
	set northeast [geo::between $north $east 0.5]
	set northwest [geo::between $north $west 0.5]
	set southeast [geo::between $south $east 0.5]
	set southwest [geo::between $south $west 0.5]

	set poly    [list {*}$north {*}$east {*}$south {*}$west]
	set corners [list \
			 north     [diagram::point at {*}$north] \
			 northeast [diagram::point at {*}$northeast] \
			 east      [diagram::point at {*}$east] \
			 southeast [diagram::point at {*}$southeast] \
			 south     [diagram::point at {*}$south] \
			 southwest [diagram::point at {*}$southwest] \
			 west      [diagram::point at {*}$west] \
			 northwest [diagram::point at {*}$northwest] \
			 center    [diagram::point at 0 0] \
			 width     $w \
			 height    $h]

	return [list $corners $poly]
    }

    # # ## ### ##### ######## ############# ######################

    method {Diamond init} {} {
	# boxwidth, boxheight - Handled by the box attributes.
	$core state set diamondaspect 2
	return
    }

    method {Diamond set} {key newvalue} {
	if {$key ne "diamond::aspect"} return
	$core state set diamondaspect $newvalue
	return
    }

    method {Diamond fill} {av} {
	upvar 1 $av attributes

	# Note: In contrast to box we have to see what we have in toto
	# before pulling the missing pieces out of the defaults,
	# because for some combinations the missing data is derived
	# from what we have. Box otoh can handle each attribute (key)
	# independently.

	set hw [dict exists $attributes diamond::width]
	set hh [dict exists $attributes diamond::height]

	if {$hw && $hh} {
	    # Both width and height were specified, we can ignore the
	    # aspect, if any. The aspect is implicit in the specified
	    # geometry.
	    return
	}

	set ha [dict exists $attributes diamond::aspect]

	# Pull the known values into locals for quicker access below,
	# also, and more importantly making the code more readable.
	if {$hw} { set w [dict get $attributes diamond::width] }
	if {$hh} { set h [dict get $attributes diamond::height] }
	if {$ha} { set a [dict get $attributes diamond::aspect] }

	if {$hw && $ha} {
	    # Derive height from aspect and width.
	    dict set attributes diamond::height [expr {$w / double($a)}]
	} elseif {$hh && $ha} {
	    # Derive width from aspect and height.
	    dict set attributes diamond::width [expr {$h * $a}]
	} elseif {$ha} {
	    # Get default width, and derive height.
	    dict set attributes diamond::width  [set w [$core state get boxwidth]]
	    dict set attributes diamond::height [expr {$w / double($a)}]
	} elseif {$hw} {
	    # Get default aspect, and derive height.
	    dict set attributes diamond::aspect [set a [$core state get diamondaspect]]
	    dict set attributes diamond::height [expr {$w / double($a)}]
	} elseif {$hh} {
	    # Get default aspect, and derive width.
	    dict set attributes diamond::aspect [set a [$core state get diamondaspect]]
	    dict set attributes diamond::width  [expr {$h * $a}]
	} else {
	    # Get defaults for aspect and width, and derive height.
	    dict set attributes diamond::width  [set w [$core state get boxwidth]]
	    dict set attributes diamond::aspect [set a [$core state get diamondaspect]]
	    dict set attributes diamond::height [expr {$w / double($a)}]
	}
	return
    }

    # # ## ### ##### ######## ############# ######################

    method drum {canvas attributes} {
	array set a $attributes

	lassign [DrumCorners a] corners mbody vlinel vliner top bottom

	# Main body, background (no outline!)
	lappend items [$canvas create rectangle {*}$mbody \
			   -fill    $a(fillcolor) \
			   -outline {}]

	# Left vertical line of the main drum body
	lappend items [$canvas create line {*}$vlinel \
			   -fill $a(color)]

	# Right vertical line of the main drum body
	lappend items [$canvas create line {*}$vliner \
			   -fill $a(color)]

	# Drum top, full ellipsis
	lappend items [$canvas create oval {*}$top \
			   -fill    $a(fillcolor) \
			   -outline $a(color)  \
			   -width   $a(stroke) \
			   -dash    $a(style) ]

	# Drum bottom, background (no outline!)
	lappend items [$canvas create arc  {*}$bottom \
			   -fill    $a(fillcolor)  \
			   -outline {}   \
			   -dash    $a(style)  \
			   -start   175  \
			   -extent  190  \
			   -style   chord]

	# Drum bottom arc (partial ellipsis, outline only)
	lappend items [$canvas create arc  {*}$bottom \
			   -fill    $a(fillcolor) \
			   -outline $a(color)  \
			   -width   $a(stroke) \
			   -dash    $a(style) \
			   -start   175 \
			   -extent  190 \
			   -style   arc ]

	HandleText $canvas $attributes items [dict get $corners center]
	return [list $items $corners]
    }

    # # ## ### ##### ######## ############# ######################

    proc DrumCorners {av} {
	upvar 1 $av a

	set w $a(width)
	set h $a(height)

	set rect [list \
		      [expr {-$w/2.0}] \
		      [expr {-$h/2.0}] \
		      [expr { $w/2.0}] \
		      [expr { $h/2.0}]]

	lassign [geo::nwse $rect] nw se
	lassign $nw xnw ynw
	lassign $se xse yse

	set width   $w
	set height  [expr {$h + $a(drum::aspect) * $w}]
	set hellips [expr {$height * $a(drum::aspect)}]
	# hellips = as*(h+as*w) = h*as+w*as^2

	set center {0 0};#[geo::between $nw $se 0.5]

	set uphe   [geo::s* -0.5 [geo::v $hellips]]
	set up     [geo::s* -0.5 [geo::v $height]]
	set right  [geo::s*  0.5 [geo::h $width]]

	# topne = center + (up + (uphe + right))
	# topsw = center + (up - (uphe + right))
	# botne = center - (up - (uphe + right))
	# botsw = center - (up + (uphe + right))

	set hr  [geo::+ $uphe $right]
	set uhr [geo::+ $up $hr]
	set dhr [geo::- $up $hr]

	set topne [geo::+ $center $uhr]
	set topsw [geo::+ $center $dhr]
	set botne [geo::- $center $dhr]
	set botsw [geo::- $center $uhr]

	# mnw = center + (up - right)
	# mne = center + (up + right)
	# mse = center - (up - right)
	# msw = center - (up + right)

	set ur  [geo::+ $up $right]
	set dr  [geo::- $up $right]

	set mnw [geo::+ $center $dr]
	set mne [geo::+ $center $ur]
	set mse [geo::- $center $dr]
	set msw [geo::- $center $ur]

	# Complete corner and rect/poly calculations.

	set northeast $topne
	set north     [geo::- $topne $right]
	set northwest [geo::- $topne [geo::s* 2 $right]]
	set southwest $botsw
	set south     [geo::+ $botsw $right]
	set southeast [geo::+ $botsw [geo::s* 2 $right]]
	set east      [geo::between $northeast $southeast 0.5]
	set west      [geo::between $northwest $southwest 0.5]

	set corners [list \
			 north     [diagram::point at {*}$north] \
			 northeast [diagram::point at {*}$northeast] \
			 east      [diagram::point at {*}$east] \
			 southeast [diagram::point at {*}$southeast] \
			 south     [diagram::point at {*}$south] \
			 southwest [diagram::point at {*}$southwest] \
			 west      [diagram::point at {*}$west] \
			 northwest [diagram::point at {*}$northwest] \
			 center    [diagram::point at {*}$center] \
			 width     $width \
			 height    $height]

	set mbody  [list {*}$mnw   {*}$mse]
	set vlinel [list {*}$mnw   {*}$msw]
	set vliner [list {*}$mne   {*}$mse]
	set top    [list {*}$topne {*}$topsw]
	set bottom [list {*}$botne {*}$botsw]

	return [list $corners $mbody $vlinel $vliner $top $bottom]
    }

    # # ## ### ##### ######## ############# ######################

    method text {canvas attributes} {
	array set a $attributes

	set label [join $a(text) \n]
	lappend items [$canvas create text 0 0    \
			   -text    $label        \
			   -font    $a(textfont)  \
			   -fill    $a(textcolor) \
			   -justify $a(justify)]

	# (%%)
	# The text's box defaults to the canvas item's box. This is
	# different from the other closed elements, which use standard
	# values for their defaults, handled by the attribute processor.

	if {![info exists a(width)]||![info exists a(height)]} {
	    lassign [$canvas bbox [lindex $items end]] xnw ynw xse yse
	    if {![info exists a(width)]}  { set a(width)  [expr {$xse - $xnw}] }
	    if {![info exists a(height)]} { set a(height) [expr {$yse - $ynw}] }
	}

	lassign [BoxCorners a] corners __dummy_rect__

	return [list $items $corners]
    }

    proc HandleText {canvas attributes iv at} {
	upvar 1 $iv items self self core core
	array set a $attributes

	# Ignore this if there is no text.
	if {![info exists a(text)]} return

	# Note: Caller may not have width/height data (open
	# elements). Force defaults.

	# At the language level the code here is equivalent to
	#     text <text> justify <justify> textcolor <textcolor> with <anchor> at <at>.
	#     (Where <at> is [last center]).

	lassign [$self text $canvas $attributes] textitems corners

	# Now perform a simplified 'relocate' (See diagram::element)
	# (no sub-elements, ignore the corners, just the one item).

	# Find current location of the specified corner.
	set at     [diagram::point unbox $at]
	set with   [$core map $corners $a(anchor)]
	set origin [diagram::point unbox [dict get $corners $with]]

	# Determine movement vector
	set delta [geo::- $at $origin]

	# And do it.
	foreach i $textitems {
	    $canvas move $i {*}$delta
	}

	# At last make the item part of the calling element.
	lappend items {*}$textitems
	return
    }

    # # ## ### ##### ######## ############# ######################

    method line {canvas attributes} {
	array set a $attributes

	lassign [LineCorners a] corners poly newdirection

	lappend items [$canvas create line {*}$poly \
			   -arrow   $a(arrowhead) \
			   -fill    $a(color) \
			   -smooth  $a(smooth) \
			   -width   $a(stroke) \
			   -dash    $a(style) ]

	# Check for optional shift of line.
	if {[info exists a(at)]} {
	    set at   $a(at)
	    set with [expr {[info exists a(with)] ? $a(with) : "start"}]
	    Relocate $with $at $canvas $items corners
	}

	HandleText $canvas $attributes items [dict get $corners center]

	if {[info exists a(noturn)] && $a(noturn)} {
	    return [list $items $corners absolute]
	} else {
	    return [list $items $corners absolute $newdirection]
	}
    }

    # # ## ### ##### ######## ############# ######################

    proc LineCorners {av} {
	upvar 1 $av a

	# Convert waypoints into canvas polyline, generating the basic
	# corners at the same time.

	# XXX share parts with basic::move command

	set poly    {}
	set corners {}
	set n 1

	#puts LC<$a(from)>|<$a(waypoints)>|<$a(to)>

	lappend corners start  [diagram::point at {*}$a(from)]
	lappend corners end    [diagram::point at {*}$a(to)]
	lappend corners center [diagram::point at {*}[geo::between $a(from) $a(to) 0.5]]

	foreach p $a(waypoints) {
	    lassign $p x y
	    lappend poly $x $y
	    lappend corners $n [diagram::point at $x $y]
	    incr n
	}

	# Lines have trivial corners. The 'end' key is recognized by
	# the layout engine as a magic overide, it keeps getting used
	# regardless of the direction turned to.

	lassign [geo::nwse [lrange $poly end-3 end]] pa pb
	set direction [geo::octant [geo::- $pb $pa]]

	return [list $corners $poly $direction]
    }

    # # ## ### ##### ######## ############# ######################

    proc Smooth {words_dummy} { return 1 }

    proc NoTurn        {words_dummy} { return 1 }
    #proc NoTurnDefault {args}        { return 0 }

    method Chop {words} {
	if {![{*}$words size] ||
	    ![string is double -strict [set v [{*}$words peek]]]} {
	    return [$core state get circleradius]
	}
	{*}$words get
	return $v
    }

    # # ## ### ##### ######## ############# ######################

    method arc {canvas attributes} {
	array set a $attributes
	#parray a

	set corners [ArcCorners a]

	# For debugging purposes, draw a number of helper elements
	# showing the construction of the arc (from, to, center,
	# f-c/t-c radii, bounding box, whole circle, and corners).
	if {0} {
	    lassign $a(rect) w n e s
	    $core draw [subst -nocommands {
		circle at [$a(center)] radius $a(arc::radius) color black dotted
		circle at [$a(arc::from)] radius 5 color orange 
		circle at [$a(center)]    radius 5 color green
		circle at [$a(arc::to)]   radius 5 color blue
		line from [$a(arc::from)] then [$a(center)] to [$a(arc::to)] dashed color red
		line from [$w $n] then [$e $n] then [$e $s] then [$w $s] to [$w $n] color yellow
	    }]
	    foreach {k v} $corners {
		if {![llength $v] == 2} continue
		lassign $v cmd detail
		if {$cmd ne "point"} continue
		$core draw [subst -nocommands {
		    circle color red radius 3 at [$detail]
		}]
	    }
	}

	# arc start  = 0-360, 0 == east, 90 == north.
	# arc extent = offset from start.

	lappend items [$canvas create arc {*}$a(rect) \
			   -start   $a(start)     \
			   -extent  $a(extent)    \
			   -fill    $a(fillcolor) \
			   -outline $a(color)     \
			   -width   $a(stroke)    \
			   -dash    $a(style)     \
			   -style   arc]

	HandleText $canvas $attributes items [dict get $corners center]
	return [list $items $corners absolute $a(direction)]
    }

    # # ## ### ##### ######## ############# ######################

    proc ArcCorners {av} {
	upvar 1 $av a core core

	# Arcs have trivial corners. The 'end' key is recognized by
	# 'navigation move' as magic overide, it keeps getting used
	# regardless of the chosen direction. The center is the center
	# of the arc's circle, and this also provides the compass
	# points. We only have to provide the proper radius element,
	# and then translate them per the actual center.

	set a(circle::radius) $a(arc::radius)
	set center  $a(center)
	lassign [CircleCorners a] corners __dummy_rect__
	set corners [$core move $center $corners]

	lappend corners \
	    start  [diagram::point at {*}$a(arc::from)] \
	    end    [diagram::point at {*}$a(arc::to)] \
	    center [diagram::point at {*}$center]

	return $corners
    }

    # # ## ### ##### ######## ############# ######################

    method {ArcLocation init} {}             {}  ; # Nothing to
						   # initialize
    method {ArcLocation set}  {key newvalue} {}  ; # in the language
						   # namespace, nor to
						   # set.
    method {ArcLocation fill} {av} {
	upvar 1 $av attributes

	# Bail out quickly when done already.
	if {[dict exists $attributes center]} return

	#puts AL|_________________________________________________________________________

	array set a $attributes
	#parray a

	# Note: We assume that both radius and clockwise have been
	# resolved already. This means that they have to come before
	# arc::{from,to} in the list of required attributes (see DefE
	# calls in the constructor).

	lassign [$core where] at angle

	set from $at
	if {[info exists a(arc::from)]} {
	    set from [diagram::point resolve $from $a(arc::from)]
	}

	#puts AL|from|$from|

	if {![info exists a(arc::to)]} {
	    # Do a (counter)clockwise 90-degree arc beginning at from,
	    # with radius, using the layout engine's current direction
	    # for the baseline.

	    # Note how we are able to directly compute the arc's
	    # center as well.

	    set cangle $angle
	    set tangle $angle
	    set radius $a(arc::radius)

	    if {$a(clockwise)} {
		incr cangle -90
	    } else {
		incr cangle 90
	    }

	    #puts C/angle\t$cangle
	    #puts T/angle\t$tangle

	    set center [diagram::point resolve $from   [diagram::point by $radius $cangle]]
	    set to     [diagram::point resolve $center [diagram::point by $radius $tangle]]
	} else {
	    set to [diagram::point resolve $at $a(arc::to)]

	    #puts AL|to|$to|

	    # Here we know from, to, and radius, and now have to find
	    # the circle's center. That is in essence an intersection
	    # of two circles, around the two points. If the distance
	    # between them is greater than 2*radius we have no center,
	    # strictly speaking. In that case we put the center in the
	    # geometric middle and make it an 180-degree arc, with
	    # adjusted larger radius.

	    set d [geo::distance $from $to]

	    #puts AL|dist|$d|\tr|$a(arc::radius)|

	    if {$d >= (2*$a(arc::radius))} {
		set center [geo::between $from $to 0.5]
		set radius [expr {$d/2.}]
	    } else {
		# Reference
		# http://local.wasp.uwa.edu.au/~pbourke/geometry/2circle/

		set ad [expr {$d/2.}]
		# a = (r0^2 - r1^2 + d^2 ) / (2 d) |r0==r1 ==> a = d/2

		set p [geo::between $from $to 0.5]
		# P2 = P0 + a/d ( P1 - P0 )
		# a/d = (d/2)/d = 1/2

		set radius $a(arc::radius)
		set hd [expr {sqrt($radius*$radius - $ad*$ad)/$d}]
		# h^2 = r0^2 - a^2, hd = h/d

		#P3 = center
		#x3 = x2 +/- h/d ( y1 - y0 )
		#y3 = y2 -/+ h/d ( x1 - x0 )

		lassign $p    mx my ; # P2
		lassign $from fx fy ; # P0
		lassign $to   tx ty ; # P1

		if {$a(clockwise)} {
		    set cx [expr {$mx - $hd * ($ty - $fy)}]
		    set cy [expr {$my + $hd * ($tx - $fx)}]
		} else {
		    set cx [expr {$mx + $hd * ($ty - $fy)}]
		    set cy [expr {$my - $hd * ($tx - $fx)}]
		}

		set center [geo::p $cx $cy]
	    }
	}

	# We now have to, from, center, and radius for our arc. The
	# last two pieces are now used to define the bounding box,
	# i.e. the rectangle we need for the canvas item, and the
	# from/center, to/center angles define the start and extent
	# information.

	set d    [geo::p $radius $radius]
	set nw   [geo::- $center $d]
	set se   [geo::+ $center $d]
	set rect [list {*}$nw {*}$se]

	# NOTE: The angle proc assumes that positive y is north. The
	# canvas coordinate system has positive y as south. By
	# conjugating the point along the y-axis we get the proper
	# angles for the canvas.

	set cf [geo::conjy $center]
	set ff [geo::conjy $from]
	set tf [geo::conjy $to]
	set s  [math::geometry::angle [list {*}$cf {*}$ff]]
	set e  [math::geometry::angle [list {*}$cf {*}$tf]]

	# Reorder the angles for direction and use by the canvas.
	if {$s < 0} { set s [expr {$s + 360}] }
	if {$e < 0} { set e [expr {$e + 360}] }
	if {($e < $s) && !$a(clockwise)} { set e [expr {$e + 360}] }
	if {($s < $e) && $a(clockwise)}  { set s [expr {$s + 360}] }

	#puts start=$s
	#puts end===$e

	set direction [geo::octant [geo::- $center $from]]
	set start     $s
	set extent    [expr {$e - $s}]

	# Save the new state back to the attributes, both original and
	# derived keys.

	dict set attributes arc::radius $radius
	dict set attributes arc::from   $from
	dict set attributes arc::to     $to

	dict set attributes direction   $direction
	dict set attributes rect        $rect
	dict set attributes center      $center
	dict set attributes start       $start
	dict set attributes extent      $extent
	return
    }

    # # ## ### ##### ######## ############# ######################

    proc ClockWise {v words_dummy} { return $v }

    proc Link {v n unit} {
	upvar 1 core core
	return [list $v [$core unit $n $unit]]
    }

    # Factor this with proc 'element::Move'
    proc Relocate {with at canvas items cv} {
	upvar 1 $cv corners core core
	set at     [diagram::point unbox $at]
	set origin [diagram::point unbox [dict get $corners $with]]

	# Determine movement vector
	set delta [geo::- $at $origin]

	# And do it.
	foreach i $items {
	    $canvas move $i {*}$delta
	}

	set corners [$core move $delta $corners]
	return
    }

    # # ## ### ##### ######## ############# ######################

    component core ; # diagram core

    # # ## ### ##### ######## ############# ######################
    ## Type construction (pre-computed tables for ellipsis corners)

    typevariable ouresin 
    typevariable ourecos
    typeconstructor {
	::variable ::math::geometry::torad
	foreach {dir angle} {
	    northeast   45
	    southeast  -45
	    southwest -135
	    northwest  135
	} {
	    lappend ourecos $dir [expr {  cos($angle * $torad)}]
	    lappend ouresin $dir [expr {- sin($angle * $torad)}]
	}
    }

    typevariable ourstyles -array {
	solid        .
	dot          .
	dotted       .
	dash         .
	dashed       .
	dash-dot     .
	dash-dot-dot .
    }

    typevariable ourarrows -array {
	start   .
	end     .
	->      .
	<-      .
	<->     .
	-       .
	\u21a6  .
	\u21a4  .
	\u21ae  .
    }

    typevariable ourshorts -array {
	cw           clockwise
	\u21bb       clockwise
	ccw          counterclockwise
	\u21ba       counterclockwise
	wid          width
	ht           height
	rad          radius
	diam         diameter
	\u2300	     diameter
	ljust        {anchor west}
	rjust        {anchor east}
	above        {anchor south}
	below        {anchor north}
    }

    ##
    # # ## ### ##### ######## ############# ######################
}

namespace eval ::diagram::basic::geo {
    namespace import ::math::geometry::*
}

# # ## ### ##### ######## ############# ######################
## Ready

package provide diagram::basic 1
