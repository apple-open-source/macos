## -*- tcl -*-
## (C) 2010 Andreas Kupries <andreas_kupries@users.sourceforge.net>
## BSD Licensed
# # ## ### ##### ######## ############# ######################

#
# diagram core, using direction and element databases, plus layout
# engine. Implements the base language (concrete attributes and
# elements are specified outside, the core only has the pertinent
# extensibility features).
#
# Uses an instance specific namespace to encapsulate the commands of
# the drawing language, and the drawing state (variables for points,
# elements, etc.).
#

##
# # ## ### ##### ######## ############# ######################
## Requisites

package require Tcl 8.5              ; # Want the nice things it
                                       # brings (dicts, {*}, etc.)
package require Tk
package require snit                 ; # Object framework.
package require diagram::direction   ; # Database of named directions
package require diagram::element     ; # Database of drawn elements
package require diagram::navigation  ; # State of automatic layouting
package require diagram::point       ; # Point validation and processing.
package require diagram::attribute   ; # Database of element attributes
package require namespacex           ; # Namespace utility functions
package require struct::set          ; # Set arithemetics (blocks)
package require math::geometry 1.1.2 ; # Vector math (points, line
				       # (segments), poly-lines).

# # ## ### ##### ######## ############# ######################
## Implementation

snit::type ::diagram::core {

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Core extensibility (drawing elements, attributes,
    ##               special attribute forms)

    method {new direction} {name args} {
	$dir new direction $name {*}$args
	return
    }

    method {new shape} {name} {
	$elm shape $name
	return
    }

    method {new element} {name attrcmd drawcmd} {
	$elm shape $name
	$self new alias $name [mymethod Element $name $attrcmd $drawcmd]
	return
    }

    method {new alias} {name cmdprefix} {
	#$self new command $name args "$cmdprefix {*}\$args"
	$self new command $name args "uplevel 1 \[list $cmdprefix {*}\$args\]"
	return
    }

    method {new command} {name arguments body} {
	proc ${mylangns}::$name $arguments $body
	return
    }

    method {new attribute} {name args} {
	$att new $name {*}$args
	return
    }

    method {unknown attribute} {hook} {
	$att unknown + $hook
	return
    }

    # # ## ### ##### ######## ############# ######################
    ##

    method snap {} {
	return [namespacex state get $mylangns]
    }

    method restore {state} {
	return [namespacex state set $mylangns $state]
    }

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Draw

    method draw {script} {
	#set script [list block $script with nw at [diagram::point at 0 0]]
	return [uplevel 1 [list namespace eval $mylangns $script]]
    }

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Instance construction

    constructor {canvas args} {
	set mycanvas $canvas
	set mylangns ${selfns}::$ourlang

	install dir using ::diagram::direction  ${selfns}::DIR
	install elm using ::diagram::element    ${selfns}::ELM $dir
	install nav using ::diagram::navigation ${selfns}::NAV $dir
	install att using ::diagram::attribute  ${selfns}::ATT $self

	$self SetupLanguage

	if {![llength $args]} return
	$self draw $args
	return
    }

    destructor {
	if {$mycanvas eq {}} return

	# This object has not been detached from the drawing engine
	# (canvas), therefor its destruction implies the destruction
	# of the drawn diagram as well.
	catch {
	    $self drop
	}
	return
    }

    method detach {} {
	set mycanvas {}
	return
    }

    method drop {} {
	# Destroy all elements and their items.
	$mycanvas delete {*}[$elm items {*}[$elm elements]]
	$elm drop
	$nav reset
	return
    }

    # # ## ### ##### ######## #############

    method {state set} {varname value} {
	#puts \tState($varname):=|$value|

	namespace upvar $mylangns $varname x
	set x $value
	return
    }

    method {state get} {varname} {
	namespace upvar $mylangns $varname x

	#puts \tState($varname)->|$x|
	return $x
    }

    # # ## ### ##### ######## #############

    method where {} {
	return [list [$nav at] [$dir get [$nav direction] angle]]
    }

    method move {delta corners} {
	return [$elm move $delta $corners]
    }

    method map {corners c} {
	return [$dir map $corners $c]
    }

    # # ## ### ##### ######## #############
    ## Internal :: Setup of core language

    method SetupLanguage {} {
	# Language encapsulation
	namespace eval $mylangns {}

	# Standard elements and operations

	$self new alias set       [mymethod Set]
	$self new alias unset     [mymethod Unset]
	$self new alias move      [mymethod Move]
	$self new alias block     [mymethod Block]
	$self new alias group     [mymethod Group]
	$self new alias here      [mymethod At]
	$self new alias direction [list $nav direction]
	$self new alias by        [mymethod By]
	$self new alias intersect [mymethod Intersect]

	$elm shape move
	$elm shape block

	# Standard attributes (element appearance, location).

	# keep here ... / type == snit validation type!

	$att new movelength type {snit::double -min 1} linked [list movelength [Unit 2 cm]]

	# XXX refactor the mymethod calls out, use variables
	$att new with                                                       default [mymethod Placement]
	$att new at   type diagram::point transform [mymethod DerefElement] default [mymethod Placement]
	$att new from type diagram::point transform [mymethod DerefElement] default [mymethod Waypoints]
	$att new to   type diagram::point transform [mymethod DerefElement] default [mymethod Waypoints]	    
	$att new then type diagram::point transform [mymethod DerefElement] default [mymethod Waypoints] \
	    get [mymethod GetPoints] aggregate 1

	$att unknown + [mymethod Directions]

	# Now special forms of commands, handled via 'namespace
	# unknown'. Making, for example, elements and points into
	# pseudo-objects.

	namespacex hook add $mylangns [mymethod CatchAll]

	# syntax: [<direction>] --> ()
	namespacex hook on $mylangns [mymethod DCGuard] [mymethod DCRun]

	# Global commands for named directions. The commands are
	# created on first use. That allows extension packages
	# declaring their own directions to do this after the core has
	# booted. Just creating the direction commands at boot time
	# will miss the directions of extensions.

	# (%%) Commands to access the history (n'th ...)

	# Visible syntax:
	#
	# <n>th      <shape> ?<corner>? | 2/3 | (1)
	# <n>th last <shape> ?<corner>? | 3/4 | (2)
	#       last <shape> ?<corner>? | 2/3 | (3)
	# <n>th last         ?<corner>? | 2/3 | (4)
	#       last         ?<corner>? | 1/2 | (5)
	#
	# Note: The form <shape> ?<corner>? is NOT possible.
	#       <shape> is the drawing command.
	#
	# Note 2: Because of (xx) the internal syntax is simpler, as
	#         the argument <n>th is always present, and not
	#         optional.
	#
	# <n>th      <shape> ?<corner>? | 2-3
	# <n>th last <shape> ?<corner>? | 3-4
	# <n>th last         ?<corner>? | 2-3
	#

	$self new alias 1st 1th
	$self new alias 2nd 2th
	$self new alias 3rd 3th
	$self new alias last [mymethod Recall 1th last] ; # (xx)
	namespacex hook on $mylangns [mymethod RecallGuard] [mymethod Recall]

	# Pseudo object commands for points
	#
	# syntax: [<number> cm|mm|point|inch]         --> <number>
	# syntax: [<number> <number>]                 --> <point>
	# syntax: [<number> between <point> <point>]  --> <point>
	# syntax: [<point> by <distance> <direction>] --> <point>
	# syntax: [<point> +|- <point>]               --> <point>

	namespacex hook on $mylangns [myproc   IsUnit]          [myproc Unit]
	namespacex hook on $mylangns [myproc   IsPointCons]     {diagram::point at}
	namespacex hook on $mylangns [myproc   IsInterpolation] [mymethod Interpolation]
	namespacex hook on $mylangns [mymethod IsPointArithBy]  [mymethod PointArithBy]
	namespacex hook on $mylangns [myproc   IsPointArithOp]  [mymethod PointArithOp]

	# Pseudo object commands for elements.
	#
	# syntax: [<element> ?<corner>...? ?names ?<pattern>??] --> <point>|<element>|...

	namespacex hook on $mylangns [myproc IsElementOp] [mymethod ElementOp]
	return
    }

    # # ## ### ##### ######## ############# ######################
    ## Internal :: Implementation of the core language commands.

    method CatchAll {args} {
	#puts |||$args|||
	# Unknown commands are compiled as text elements
	# --> Calls out into basic, assumes its presence.
	return [$self draw [list text text {*}$args]]
    }

    method Move {args} {
	set attributes [$att attributes move $args {from to then}]
	set w [dict get $attributes waypoints]

	# XXX share corner generation with line - sub packages.
	lappend corners start [diagram::point at {*}[lindex $w 0]]
	lappend corners end   [diagram::point at {*}[lindex $w end]]
	set n 1
	foreach p $w {
	    lappend corners $n [diagram::point at {*}$p]
	    incr n
	}

	# note: move is a bit special. It has neither child elements,
	# nor canvas items. We define it actually only to make it
	# visible in the history, and to block corner creation.
	set eid	[$elm new move $corners {} {}]
	$nav move $corners
	return $eid
    }

    method Set {args} {
	#puts SET|$args|
	# Run builtin for the regular behaviour of the intercepted command.

	set result [uplevel 1 [list ::set {*}$args]]

	# During block processing we save variable re-definitions as
	# the block's corners
	if {$myinblock && ([llength $args] == 2)} {
	    lappend mycorners {*}$args
	}
	return $result
    }

    method Unset {args} {
	#puts UNSET|$args|
	# Run builtin for the regular behaviour of the intercepted command.

	set result [uplevel 1 [list ::unset {*}$args]]

	# During block processing we are saving variable
	# re-definitions as the block's corners, so have to remove
	# that definition too.
	if {$myinblock} {
	    foreach c $args {
		dict unset mycorners $c
	    }
	}
	return $result
    }

    method Block {script args} {
	# args = attributes.

	# Save current state
	set old [$elm elements]
	set ehi [$elm history get]
	set lst [namespacex state get $mylangns]
	$nav save

	# Process the attributes, and store the changed settings into
	# their linked variables (if any), to make them proper
	# defaults inside of the block.

	set attributes [$att attributes block $args {at with}]
	$att set $attributes
	set at   [dict get $attributes at]
	set with [dict get $attributes with]

	# Run the block definition, prepare for the capture of corners.
	set inblock $myinblock
	set myinblock 1
	set mycorners {}

	#$self draw $script
	uplevel 1 $script

	# Remember the captured corners and reset capture system.
	set myinblock $inblock
	set corners [dict merge $mycorners]
	set mycorners {}

	# Extract the set of newly drawn elements.
	set new [struct::set difference [$elm elements] $old]

	#puts |$new|bb|[$elm bbox {*}$new]|

	# Get the block's bbox from the union of its elements' bboxes.
	lassign [$elm bbox {*}$new] xnw ynw xse yse

	# XXX see BoxCornersRect of basic, share code
	set xns [expr {($xnw + $xse) / 2.0}]
	set yew [expr {($ynw + $yse) / 2.0}]
	set w   [expr {$xse - $xnw}]
	set h   [expr {$yse - $ynw}]

	set compass [list \
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

	#puts COMPASS|$compass|
	#puts CORNERS|$corners|

	set corners [dict merge $compass $corners]

	#puts BLOCK__\t($corners)
	#puts __BLOCK

	# Restore the system state to what it was before we entered
	# the block.
	$nav restore
	namespacex state set $mylangns $lst
	$elm history set $ehi

	# Now save the block as element, aggregating the children, and
	# move it into position, based on the placement attributes.
	set eid [$elm new block $corners {} $new]
	$elm relocate $eid $at $with $mycanvas
	$nav move [$elm corners $eid]

	return $eid
    }

    method Group {script} {
	# A group is similar to a block, except that only the state of
	# the layout engine is saved across it, not the whole element
	# history, etc. The elements created here are not aggregated
	# either. Further, changes to any attributes made inside the
	# group are visible after it as well.

	$nav save
        #$self draw $script
	uplevel 1 $script
	$nav restore
	return
    }

    method Element {shape required drawcmd args} {
	# args = attributes.

	# attrcmd :: attr-dict -> attr-dict
	# drawcmd :: canvas -> attr-dict -> 
	#            (attr-dict canvas-item-list corner-dict ?placement-mode ?layout-direction??)

	set newdirection {}
	set mode         {}
	set attributes   [$att attributes $shape $args $required]
	lassign [{*}$drawcmd $mycanvas $attributes] \
	    items corners mode newdirection
	if {$mode eq {}} { set mode relative }

	# Allow the user's commands to override the element type. For
	# example, an 'arrow' element not only exapnd to 'line
	# arrowhead ->', but also set the attribute '/shape arrow' to
	# distinguish them from plain lines in the history.

	if {[dict exists $attributes /shape]} {
	    set shape [dict get $attributes /shape]
	}

	set eid [$elm new $shape $corners $items {}]

	#puts $shape=$eid\t/mode=$mode/

	if {$mode eq "relative"} {
	    # Determine the final location of the new element and move
	    # it there, as it was not created at its absolute/final
	    # location already by its drawing command.

	    set at   [dict get $attributes at]
	    set with [dict get $attributes with]

	    #puts "shift such $with at ($at)"
	    $elm relocate $eid $at $with $mycanvas
	}

	# Update the layout engine with new position, and possibly a
	# new direction to follow.

	$nav move [$elm corners $eid] ; # This also discards the
	# intermediate location set
	# for any turns done during
	# attribute processing.

	if {$newdirection ne {}} {
	    # The new element changed direction, notify the layout
	    # engine. Commit immediately to the location for the
	    # direction.

	    $nav turn $newdirection 1
	}

	return $eid
    }

    method At {} {
	return [diagram::point at {*}[$nav at]]
    }

    # # ## ### ##### ######## ############# ######################

    method Corners {elements} {
	set results {}
	foreach e $elements {
	    foreach {k v} [$elm corners $e] {
		lappend result $e.$k $v
	    }
	}
	return $result
    }

    # # ## ### ##### ######## ############# ######################
    ## Handling of directions as attributes and global commands.

    method Directions {shape words} {
	#puts AU||$shape|u(([{*}$words peek [{*}$words size]]))

	# Try to process like for a 'then' attribute, and if that
	# succeeds stuff the result back to run it through the actual
	# handling of the implicit 'then'.

	if {![catch {
	    $self ProcessPoints $words newdirection
	} p]} {
	    #puts <<ok|$p>>

	    {*}$words unget $p
	    {*}$words unget then

	    #puts AU|||x(([{*}$words peek [{*}$words size]]))

	    if {$newdirection ne {}} {
		$nav turn $newdirection
	    }
	    #puts AU|done
	    return 1
	}

	#puts AU<<$p>>
	#puts $::errorInfo
	return 0
    }

    # syntax: [<direction>] --> ()
    method DCGuard {args} {
	#puts DCG|$args|[llength $args]|
	return [expr {([llength $args] == 1) &&
		      [$dir isStrict [lindex $args 0]]}]
    }

    method DCRun {direction} {
	#puts DCR|$direction|
	$nav turn $direction 1
	$self new command $direction {} \
	    [list $nav turn $direction 1]
	return
    }

    # # ## ### ##### ######## ############# ######################

    method RecallGuard {args} {
	#puts RecallGuard|$args|[llength $args]|[regexp {(\d+)th} [lindex $args 0]]
	return [regexp {(\d+)th} [lindex $args 0]]
    }
    method Recall {offset args} {
	#puts RECALL|$offset|$args|______________________________________________________________

	# Syntax (internal!). See comments at (%%) in this file for
	# the differences between internal and user visible syntax,
	# and how the translation is made.
	#
	# <n>th      <shape> ?<corner>? | 2-3 | 1-2 | (a)
	# <n>th last <shape> ?<corner>? | 3-4 | 2-3 | (b)
	# <n>th last         ?<corner>? | 2-3 | 1-2 | (c)
	#

	set n [llength $args]
	if {$n < 1 || $n > 3} {
	    return -code error "wrong\#args: should be \"?n'th? ?last? ?shape? ?corner?\""
	}

	regexp {(\d+)th} $offset -> offset

	# forward/backward search ?
	if {[lindex $args 0] eq "last"} {
	    set args [lassign $args _]
	    set offset -$offset
	}

	# specific shape/all shapes ?
	if {[$elm isShape [lindex $args 0]]} {
	    set args [lassign $args shape]
	} else {
	    set shape {} ;# Search all shapes.
	}

	# corner yes/no ?
	set corner {}
	set n [llength $args]
	if {$n == 1} {
	    lassign $args corner
	} elseif {$n > 1} {
	    return -code error "wrong\#args: should be \"?n'th? ?last? ?shape? ?corner?\""
	}

	#puts H|recall|o|$offset|
	#puts H|recall|s|$shape|
	#puts H|recall|c|$corner|

	# ... And access the history files ...

	set eid [$elm history find $shape $offset]

	#puts H|recall|e|$eid|

	# ... at last return result, resolving the corner, if any such
	# was specified.

	if {$corner ne {}} {
	    #puts H|recall|p|[$elm corner $eid $corner]
	    return [$elm corner $eid $corner]
	} else {
	    #puts H|recall|x|$eid|
	    return $eid
	}
    }

    # # ## ### ##### ######## ############# ######################

    # syntax: [<number> <unit>] --> <number>
    proc IsUnit {args} {
	#puts IsUnit|$args|[llength $args]|[string is double -strict [lindex $args 0]]|[info exists ourunit([lindex $args 1])]
	return [expr {([llength $args] == 2) &&
		      [string is double -strict [lindex $args 0]] &&
		      [info exists ourunit([lindex $args 1])]}]
    }

    proc Unit {n unit} {
	#puts "Unit $unit ($n)"
	return [expr {$n * $ourunit($unit)}]
    }

    method unit {n unit} { return [Unit $n $unit] }

    # syntax: [<number> <number>] --> <point>
    proc IsPointCons {args} {
	#puts IsPointCons|$args|[llength $args]|[string is double -strict [lindex $args 0]]|[string is double -strict [lindex $args 1]]
	return [expr {([llength $args] == 2) &&
		      [string is double -strict [lindex $args 0]] &&
		      [string is double -strict [lindex $args 1]]}]
    }

    # syntax: [<number> between <point> <point>] --> <point>
    proc IsInterpolation {args} {
	#puts IsInterpolation|$args|[llength $args]|[string is double -strict [lindex $args 0]]|[string is double -strict [lindex $args 1]]
	return [expr {([llength $args] == 4) &&
		      [string is double -strict [lindex $args 0]] &&
		      ([lindex $args 1] eq "between") &&
		      [diagram::point is [lindex $args 2]] &&
		      [diagram::point is [lindex $args 3]]}]
    }

    method Interpolation {s __between__ a b} {
	set a [diagram::point resolve [$nav at] $a]
	set b [diagram::point resolve $a $b]
	return [diagram::point at {*}[geo::between $a $b $s]]
    }

    method By {distance direction} {
	if {[$dir isStrict $direction]} {
	    set angle [$dir get $direction angle]
	} else {
	    set angle $direction
	}
	return [diagram::point by $distance $angle]
    }

    # syntax: [<point> by <distance> <direction>] --> <point>
    method IsPointArithBy {args} {
	#puts IsPointArith|$args|[llength $args]|
	return [expr {([llength $args] == 4) &&
		      [diagram::point is [lindex $args 0]] &&
		      ([lindex $args 1] eq "by") &&
		      [string is double -strict [lindex $args 2]] &&
		      [$dir is [lindex $args 3]]}]
    }

    method PointArithBy {p __by__ distance direction} {
	if {[$dir isStrict $direction]} {
	    set angle [$dir get $direction angle]
	} else {
	    set angle $direction
	}
	set delta [diagram::point by $distance $angle]

	#puts PointArith|$p|++|D/$direction|A/$angle|d/$delta|==|[diagram::point + $p $delta]|
	return [diagram::point + $p $delta]
    }

    # syntax: [<point> by <distance> <direction>] --> <point>
    proc IsPointArithOp {args} {
	#puts IsPointArithOp|$args|[llength $args]|
	# See ElementOp for similar code.
	return [expr {([llength $args] == 3) &&
		      [diagram::point is [lindex $args 0]] &&
		      ([lindex $args 1] in {+ - |}) &&
		      [diagram::point is [lindex $args 2]]}]
    }

    method PointArithOp {pa op pb} {
	#puts PointArithOp|$pa|$op|$pb|=|[diagram::point $op $pa $pb]|
	return [diagram::point $op $pa $pb]
    }

    method Intersect {ea eb} {
	set pas [diagram::point unbox [$elm corner $ea start]]
	set pae [diagram::point unbox [$elm corner $ea end]]
	set pbs [diagram::point unbox [$elm corner $eb start]]
	set pbe [diagram::point unbox [$elm corner $eb end]]

	#puts |$pas|---|$pae|
	#puts |$pbs|---|$pbe|

	set linea [list {*}$pas {*}$pae]
	set lineb [list {*}$pbs {*}$pbe]

	set p [geo::findLineIntersection $linea $lineb]
	#puts |$p|

	if {$p eq "none"} {
	    return -code error "Intersection failure, parallel lines have none"
	} elseif {$p eq "coincident"} {
	    return -code error "Intersection failure, unable to choose among infinite set of points of coincident lines"
	}

	return [diagram::point at {*}$p]
    }

    # # ## ### ##### ######## ############# ######################

    # syntax: [<element> ?<corner>...? ?names ?<pattern>??] --> <point>|<element>|...
    proc IsElementOp {args} {
	#puts IsElementOp|$args|[llength $args]|[diagram::element is [lindex $args 0]]
	return [expr {([llength $args] > 1) &&
		      [diagram::element is [lindex $args 0]]}]
    }

    method ElementOp {eid args} {
	#puts Element|$eid|$corner|=|[$elm corner $eid $corner]|
	#array set c [$elm corners $eid];parray c

	# See IsPointArithOp guard for similar code.
	if {([llength $args] == 2) &&
	    ([lindex $args 0] in {+ - |}) &&
	    [diagram::point is [lindex $args 1]]} {

	    # Point arithmetic on an element is based in the
	    # element's center. Resolve and divert.
	    lassign $args op p
	    return [$self PointArithOp [$elm corner $eid center] $op $p]
	}

	set stop 0
	foreach operation $args next [lrange $args 1 end] {
	    if {$stop} {
		if {$stop == 2} { incr stop -1 ; continue }
		return -code error "wrong#args: should be \"?corner...? ?names ?pattern??\""
	    }
	    if {$operation eq "names"} {
		if {$next eq {}} { set next * }
		set eid [$elm names $eid $next]
		set stop 2
		# stop => error out if there is an argument after next
	    } else {
		set eid [$elm corner $eid $operation]
	    }
	}
	return $eid
    }

    # # ## ### ##### ######## ############# ######################

    method DerefElement {p} {
	# Convert element references to the elements' center point.
	# Used when processing the attributes 'from', 'to', 'then',
	# and 'at'.

	if {[diagram::element is $p]} {
	    return [dict get [$elm corners $p] center]
	} else {
	    return $p
	}
    }

    # # ## ### ##### ######## ############# ######################

    method {Placement init} {}             {} ; # Nothing to
    # initialize
    method {Placement set}  {key newvalue} {} ; # in the language
    # namespace, nor to
    # set.
    method {Placement fill} {av} {
	upvar 1 $av attributes

	if {[dict exists $attributes .withat]} return
	dict set attributes .withat .

	# at/with - rules
	#

	# (1) If the user did not specify 'at', nor 'with', then both
	#     are filled with the information from the layout engine.
	#
	# (2) If 'with' was specified, but not 'at', then 'at' is
	#     filled from the layout engine.
	#
	# (3) If 'at' was specified, but not 'with' then 'with'
	#     defaults to the 'center', and the layout engine is
	#     ignored.
	#
	# (4) If both have been specified, then nothing is done.
	#
	# (5) The data for 'at' is an untagged absolute location.
	#     A user specified value is a diagram::point/delta.
	#     This is resolved as well.

	if {![dict exists $attributes at]} {
	    dict set attributes at [$nav at] ; # (1,2)
	    if {[dict exists $attributes with]} return
	    dict set attributes with [$nav corner] ; # (1)
	} else {
	    # (5) User specified location. Resolve to untagged
	    #     absolute location.
	    dict set attributes at \
		[diagram::point resolve \
		     [$nav at] [dict get $attributes at]]

	    if {![dict exists $attributes with]} {
		dict set attributes with center ; # (3)
	    } ; # else (4)
	}
	return
    }

    # # ## ### ##### ######## ############# ######################

    method {Waypoints init} {}             {}  ; # Nothing to
    # initialize
    method {Waypoints set}  {key newvalue} {}  ; # in the language
    # namespace, nor to
    # set.
    method {Waypoints fill} {av} {
	upvar 1 $av attributes

	# from/then/to - rules
	# Bail out quickly when done already.
	if {[dict exists $attributes waypoints]} return

	# Determine a starting point if not specified, and/or make a
	# relative specification absolute.

	set awaypoints {}
	set last [$nav at] ; # absolute location, untagged.

	if {[dict exists $attributes from]} {
	    set last [diagram::point resolve $last [dict get $attributes from]]
	}

	dict set attributes from $last
	lappend waypoints $last

	if {[dict exists $attributes then]} {
	    #puts |then|[dict get $attributes then]|
	    foreach p [dict get $attributes then] {
		#puts \t|$p|
		set last [diagram::point resolve $last $p]
		lappend waypoints $last
	    }
	}

	if {![dict exists $attributes to]} {
	    # Use a default if and only if no intermediate waypoints
	    # have been specified. For if they have, then the last of
	    # the intermediates will serve as the 'to'.

	    if {![dict exists $attributes then]} {
		# Compute a location based on direction and defaults

		set distance [$self state get movelength]
		set angle    [$dir get [$nav direction] angle]
		set delta    [diagram::point by $distance $angle]
		set last     [diagram::point resolve $last $delta]
		lappend waypoints $last
	    }
	} else {
	    set last [diagram::point resolve $last [dict get $attributes to]]
	    lappend waypoints $last
	}

	dict set attributes waypoints $waypoints
	dict set attributes to        $last

	# If chop values have been specified then now is the time to
	# process their effect on the waypoints.

	if {[dict exists $attributes chop]} {
	    set choplist [dict get $attributes chop]
	    if {[llength $choplist] > 2} {
		set choplist [lrange $choplist end-1 end]
	    } elseif {[llength $choplist] < 2} {
		lappend choplist [lindex $choplist end]
	    }

	    #puts w|||$waypoints|||
	    #puts c|||$choplist|||

	    lassign $choplist chopstart chopend

	    # We have to handle multi-segment lines. First we chop
	    # whole segments until the length to chop is less than the
	    # length of the current first/last segment. Note that we
	    # may be left with an empty path.

	    while {[llength $waypoints] >= 2} {
		lassign $waypoints pa pb
		set seglen [geo::distance $pa $pb]
		if {$seglen > $chopstart} break
		set waypoints [lrange $waypoints 1 end]
		set chopstart [expr {$chopstart - $seglen}]
	    }
	    while {[llength $waypoints] >= 2} {
		lassign [lrange $waypoints end-1 end] pa pb
		set seglen [geo::distance $pa $pb]
		if {$seglen > $chopend} break
		set waypoints [lrange $waypoints 0 end-1]
		set chopend [expr {$chopend - $seglen}]
	    }

	    #puts w'|||$waypoints|||
	    #puts c'|||$choplist|||

	    if {[llength $waypoints] > 2} {
		# Ok, we have enough segments left, now actually chop
		# the first and last segments.

		# Relative chop positions, translated to actual
		# position through interpolation.
		lassign $waypoints pa pb
		set s [expr {double($chopstart)/$seglen}]
		set anew [geo::between $pa $pb $s]

		lassign [lrange $waypoints end-1 end] a b
		set s [expr {1-double($chopend)/$seglen}]
		set bnew [geo::between $pa $pb $s]

		set waypoints [lreplace [lreplace $waypoints 0 0 $anew] end end $bnew]

	    } elseif {[llength $waypoints] == 2} {
		# There is only one segment left in the
		# poly-line. Check that chopping the ends doesn't
		# leave it empty.

		lassign $waypoints pa pb
		set seglen [geo::distance $pa $pb]
		if {($chopstart + $chopend) > $seglen} {
		    set waypoints {}
		} else {
		    # Relative chop positions.
		    set ss [expr {double($chopstart)/$seglen}]
		    set se [expr {1-double($chopend)/$seglen}]

		    #puts s|$ss
		    #puts e|$se

		    # Translate to actual position through interpolation.
		    set anew [geo::between $pa $pb $ss]
		    set bnew [geo::between $pa $pb $se]

		    set waypoints [list $anew $bnew]
		}
	    } else {
		set waypoints {}
	    }

	    dict set attributes waypoints $waypoints
	    dict set attributes from      [lindex $waypoints 0]
	    dict set attributes to        [lindex $waypoints end]
	}

	# Note: Keeping from, and to. direct access to these points
	# could be beneficial.

	#puts WP
	#puts ______________________________________________________
	#array set a $attributes ; parray a
	#puts ______________________________________________________

	return
    }

    method GetPoints {words} {
	set p [$self ProcessPoints $words newdirection]
	if {$newdirection ne {}} {
	    $nav turn $newdirection
	}
	return $p
    }

    method ProcessPoints {words nv} {
	upvar 1 $nv newdirection
	set newdirection {}

	# words = P ... !P
	# P = <point>
	#   | <directionname> <double>
	#   | <directionname>

	if {![{*}$words size]} {
	    return -code error "wrong\#args, expected a point"
	}

	set p [{*}$words peek]
	if {[diagram::point is $p]} {
	    # Got an immediate location (absolute or relative). As we
	    # expect only one of such we stop processing input and
	    # return.

	    {*}$words get
	    return $p
	}

	# Not a proper location. Check if we have a series
	# of <direction> ?<distance>? words.

	set point [diagram::point delta 0 0]
	set resok 0

	while {[{*}$words size]} {

	    set p [{*}$words peek]
	    if {![$dir isStrict $p]} {
		# Not a direction. If we had delta specs before then
		# we just have found the end and can stop processing.
		# Otherwise there was no spec at at all, which is an
		# error.
		break
	    }

	    set direction [$dir validate $p]

	    # We have a direction, check if there is a distance coming
	    # after, then add to the sum of previous deltas,
	    # i.e. integrate the path.

	    {*}$words get
	    if {[{*}$words size] && [string is double -strict [{*}$words peek]]} {
		set distance [{*}$words get]
	    } else {
		set distance [$self state get movelength]
	    }

	    set angle [$dir get $direction angle]
	    set v     [diagram::point by $distance $angle]
	    set point [diagram::point + $point $v]
	    set resok 1

	    # Keep track of the last direction used. When we are done
	    # the caller will push this to the layout engine, so that
	    # it tracks turns specified in the attributes of an
	    # element.

	    set newdirection $direction
	}

	if {$resok} {
	    return $point
	} else {
	    return -code error "Expected point/delta specification, got \"$p\""
	}
    }

    # # ## ### ##### ######## ############# ######################
    ## Instance data, database tables as arrays, keyed by direction
    ## and alias names.

    variable mycanvas  {} ; # Drawing backend
    variable mylangns  {} ; # Name of the namespace holding the drawing state.

    variable myinblock 0  ; # Boolean flag. True when processing a block.
    variable mycorners {} ; # Corner dictionary during block processing.

    component dir        ; # Knowledge base of named directions.
    component elm        ; # Database of drawn elements.
    component nav        ; # State of automatic layout engine
    component att        ; # Database of attributes

    typevariable ourlang LANG

    typevariable ourunit -array {} ; # database for unit conversion

    typeconstructor {
	# [tk scaling] is in pixels/point, with point defined as 1/72 inch
	foreach {unit s} {
	    mm    2.83464566929
	    cm    28.3464566929
	    inch  72
	    point 1
	} {
	    set ourunit($unit) [expr {$s * [tk scaling]}]
	}
    }

    ##
    # # ## ### ##### ######## ############# ######################
}

# # ## ### ##### ######## ############# ######################
## Ready

namespace eval ::diagram::core::geo {
    namespace import ::math::geometry::*
}

package provide diagram::core 1
