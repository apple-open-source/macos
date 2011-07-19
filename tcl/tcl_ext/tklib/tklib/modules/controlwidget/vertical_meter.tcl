# vertical_meter.tcl --
#    Implement various meter types
#
#    This software is Copyright by the Board of Trustees of Michigan
#    State University (c) Copyright 2005.
#
#    You may use this software under the terms of the GNU public license
#    (GPL) ir the Tcl BSD derived license  The terms of these licenses
#     are described at:
#
#     GPL:  http://www.gnu.org/licenses/gpl.txt
#     Tcl:  http://www.tcl.tk/softare/tcltk/license.html
#     Start with the second paragraph under the Tcl/Tk License terms
#     as ownership is solely by Board of Trustees at Michigan State University.
#
#     Author:
#             Ron Fox
#             NSCL
#             Michigan State University
#             East Lansing, MI 48824-1321
#
#     Adjusted by Arjen Markus
#
#     TODO:
#     Add options:
#     -readonly, -arrowthickness, -arrowcolor, -background/-bg
#     -majorticklength, -minorticklength
#     -drawaxle
#
#     Add features/TODO:
#     - proper update if to/from changes
#     - unit tests
#     - check behaviour if no variable defined
#
#     Add widgets:
#     - shiftbar (or what is the best name?)
#     - equalizer bars
#
#
#

#  Implements a 'meter' megawidget.  A meter is a
#  box with a needle that goes up and down between
#  two possible limits.
#
# This is drawn in a canvas as follows:
#    +-------+
#    |       |
#    |  <----|
#    | ...
#    +-------+
#
#
# OPTIONS:
#    -from        - Value represented by the lower limit of the meter.  (dynamic)
#    -to          - Value represented by the upper limit of the meter.  (dynamic)
#    -height      - Height of the meter.                                (static)
#    -width       - Width of the meter.                                 (static)
#    -variable    - Variable the meter will track.                      (dynamic)
#    -majorticks  - Interval between major (labelled) ticks.            (dynamic)
#    -minorticks  - Number of minor ticks drawn between major ticks.    (dynamic)
#    -log         - True if should be log scale                         (dynamic).
#
# Methods:
#    set value    - Set the meter to a specific value (if -variable is defined it is modified).
#    get          - Returns the current value of the meter.

package provide meter 1.0
package require Tk
package require snit
package require bindDown

namespace eval controlwidget {
    namespace export meter
    namespace export slider
    namespace export equalizerBar
    namespace export thermometer
}

# verticalAxis --
#     Private type for handling a vertical axis
#     Some options are obligatory
#
snit::type controlwidget::verticalAxis {

    option -canvas       {}
    option -x            {}
    option -xright       {}
    option -ytop         {}
    option -ybottom      {}
    option -axisformat   -default %.2g    -configuremethod SetAxisProperty
    option -axisfont     -default {fixed} -configuremethod SetAxisProperty
    option -axiscolor    black
    option -drawaxle     1
    option -from         -default -1.0    -configuremethod SetAxisRange
    option -to           -default  1.0    -configuremethod SetAxisRange
    option -majorticks   -default  1.0    -configuremethod SetAxisProperty
    option -minorticks   -default  4      -configuremethod SetAxisProperty
    option -log          -default  false  -configuremethod SetAxisType
    option -axisstyle    -default  left

    variable majorlength 7
    variable valueRange

    constructor args {
         $self configurelist $args
    }

    method drawAxis {} {
         if { $options(-drawaxle) } {
             $options(-canvas) create line $options(-x) $options(-ytop) $options(-x) $options(-ybottom) \
                 -fill $options(-axiscolor) -tags axis
         }
         $self drawTicks
    }


    # Draw the tick marks on the axis face.  The major ticks are
    # labelled, while the minor ticks are just some length.
    # Major ticks extend from the meter left edge to 1/5 the width of the meter
    # while minor ticks extend from the meter left edge to 1/10 the width of the meter.
    # Tick labels are drawn at x coordinate 0.
    #
    method drawTicks {} {

        if {!$options(-log)} {
            $self drawLinearTicks
        } else {
            $self drawLogTicks
        }
    }
    #
    #  Draw the ticks for a log scale.
    #
    method drawLogTicks {} {
        set decades     [$self computeDecades];       # Range of axis ...
        set majorRight  [$self getMajorRight];        # Right end coordinate of major tick.
        set minorRight  [$self getMinorRight];        # Right end coord of minor tick.
        set xleft       $options(-x)

        #  Major ticks are easy.. they are at the decades.

        set range    [expr $options(-ytop) - $options(-ybottom)]
        set interval [expr $range/([llength $decades] -1) ];  # Space decades evenly.

        set pos   $options(ybottom)
        foreach decade $decades {
            $options(-canvas) create text $xleft $pos -text $decade -anchor e -font $options(-axisfont) \
                -fill $options(-axiscolor) -tags ticks
            $options(-canvas) create line $xleft $pos $majorRight $pos -fill $options(-axiscolor) -tags ticks]
            #
            #  Now the minor ticks... we draw for 1-9. of them in log spacing.
            #
            foreach mant [list 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0] {
                set ht [expr $pos + $interval*log10($mant)]
                $options(-canvas) create line $xleft $ht $minorRight $ht -fill $options(-axiscolor) -tags ticks]
            }
            set pos [expr $pos + $interval]
        }
    }
    #
    #  Draw the ticks for a linear scale:
    #
    method drawLinearTicks {} {
        set first $options(-from)
        set last  $options(-to)
        set major $options(-majorticks)
        set xleft $options(-x)


        # minor ticks are just given in terms of the # ticks between majors so:

        set minor [expr 1.0*$major/($options(-minorticks)+1)]

        # Figure out the right most coordinates of the tick lines.

        set majorRight [$self getMajorRight]
        set minorRight [$self getMinorRight]

        # the for loop is done the way it is in order to reduce
        # the cumulative roundoff error from repetitive summing.
        #
        set majorIndex 0
        for {set m $first} {$m <= $last} {set m [expr $first + $majorIndex*$major]} {
            # Draw a major tick label and the tick mark itself
            # major ticks are formatted in engineering notation (%.1e).

            set label [format $options(-axisformat) $m]
            set height [$self computeHeight $m]
            $options(-canvas) create text  $xleft $height -text $label -anchor e -font $options(-axisfont) \
                -fill $options(-axiscolor) -tags ticks]
            $options(-canvas) create line  $xleft $height $majorRight $height \
                -fill $options(-axiscolor) -tags ticks]

            for {set i 1} {$i <=  $options(-minorticks)} {incr i} {
                set minorH [expr $m + 1.0*$i*$minor]
                set minorH [$self computeHeight $minorH]
                $options(-canvas) create line $xleft $minorH $minorRight $minorH \
                -fill $options(-axiscolor) -tags ticks]
            }
            incr majorIndex
        }
    }
    #
    #  Erase the Tick ids from the meter:
    #
    method eraseTicks {} {
        $options(-canvas) delete ticks
    }
    #
    #     Compute the right x coordinate of the major ticks:
    #
    method getMajorRight {} {
        set majorRight  [expr {$options(-x) + $majorlength}]

        return $majorRight
    }
    #
    #    Compute the right x coordinate of the minor ticks:
    #
    method getMinorRight {} {
        set minorlength [expr  $majorlength/2]
        set minorRight  [expr $options(-x) + $minorlength]
        return $minorRight
    }

    # compute the decades in the plot.  This is also where we will complain if the
    # range covers 0 or a negative range as for now we only support positive log scales.
    # Returns a list of the decades e.g. {1.0e-9 1.0e-08 1.0e-7}  that cover the range.
    # The low decade truncates.  The high one is a ceil.
    #

    method computeDecades {} {
        set low $options(-from)

        if {$low <= 0.0} {
            return -code error "Log scale with negative or zero -from value is not supported"
        }
        set high $options(-to)
        if {$high <= 0.0} {
            return -code error "Log scale with negative or zero -to value no"
        }
        #
        set lowDecade  [expr log10($low)]
        if {$lowDecade < 0} {
            set lowDecade [expr $lowDecade - 0.5]
        }
        set lowDecade [expr int($lowDecade)]

        set result     [format "1.0e%02d" $lowDecade]
        set highDecade [expr log10($high)];               # Don't truncate...
        while {$lowDecade < $highDecade} {
            incr lowDecade
            lappend result [format "1.0e%02d" $lowDecade]
        }
        set decadeLow  [lindex $result 0]
        set decadeHigh [lindex $result end]
        return $result
    }

    # Compute the correct height of the needle given
    # A new coordinate value for it in needle units:

    method computeHeight needleCoords {
        if {$options(-log)} {
            return [$self computeLogHeight  $needleCoords]
        } else {
            return [$self computeLinearHeight $needleCoords]
        }
    }

    #  Compute the needle height if the scale is log.

    method computeLogHeight needleCoords {
        $self computeDecades
        #
        #  The following protect against range errors as well as
        #  negative/0 values:
        #
        if {$needleCoords < $decadeLow} {
            set needleCoords $decadeLow
        }
        if {$needleCoords > $decadeHigh} {
            set needleCoords $decadeHigh
        }

        #  Now it should be safe to do the logs:
        #  the scaling is just linear in log coords:

        set valueRange [expr {log10($decadeHigh) - log10($decadeLow)}]
        set value      [expr {log10($needleCoords) - log10($decadeLow)}]

        set pixelRange [expr {1.0*($options(-ybottom) - $options(-ytop)}]

        set height [expr {$value*$pixelRange/$valueRange}]
        return [expr {$options(-ybottom) - $height}]

    }

    #  Compute the needle height if the scale is linear
    #
    method  computeLinearHeight needleCoords {

        #
        # Peg the needle to the limits:
        #
        if {$needleCoords > $options(-to)}  {
            return $options(-ytop)
        }
        if {$needleCoords < $options(-from)} {
            return $options(-ybottom)
        }
        set pixelRange [expr {1.0*($options(-ybottom) - $options(-ytop))}]

        # Transform the coordinates:

        set valueRange [expr {1.0*($options(-to) - $options(-from))}]
        set height [expr {($needleCoords - $options(-from))*$pixelRange/$valueRange}]
        return [expr {$options(-ybottom) - $height}]
    }

    # Compute the correct value of the needle given the position

    method computeValue needleCoords {
        if {$options(-log)} {
            return [$self computeLogValue $needleCoords]
        } else {
            return [$self computeLinearValue $needleCoords]
        }
    }

    #  Compute the needle's value if the scale is log.

    method computeLogValue needleCoords {
        $self computeDecades
        #
        #  The following protect against range errors as well as
        #  negative/0 values:
        #
        if {$needleCoords < $options(-ytop)} {
            set needleCoords $options(-ytop)
        }
        if {$needleCoords > $options(-ybottom)} {
            set needleCoords $options(-ybottom)
        }

        set logScale   [expr {log10($decadeHigh/$decadeLow)}]
        set yratio     [expr {($y - $ymin) / double($ymax - $ymin)}]

        set value      [expr {$decadeLow * pow(10.0,$logScale*$yratio)}]

        return $value
    }

    #  Compute the needle's value if the scale is linear
    #
    method computeLinearValue needleCoords {

        #
        # Peg the needle to the limits:
        #
        if {$needleCoords < $options(-ytop)}  {
            return $options(-to)
        }
        if {$needleCoords > $options(-ybottom)} {
            return $options(-from)
        }

        set pixelRange [expr {1.0*($options(-ybottom) - $options(-ytop))}]

        # Transform the coordinates:

        set scaleFactor [expr {($options(-to) - $options(-from)) / $pixelRange}]
        set value       [expr {$options(-from) + ($options(-ybottom) - $needleCoords)*$scaleFactor}]

        return $value
    }

    #------------------------ Configuration handlers for dynamic options  ----
    #    -from        - Value represented by the lower limit of the meter.  (dynamic)
    #    -to          - Value represented by the upper limit of the meter.  (dynamic)
    #    -log         - Type of axis (linear or logarithmic)                (dynamic)
    #    -majorticks  - Interval between major (labelled) ticks.            (dynamic)
    #    -minorticks  - Number of minor ticks drawn between major ticks.    (dynamic)


    # Handle configure -to and -from
    # Need to set the stuff needed to scale the meter positions and reset the meter position.
    # Need to redraw ticks as well.
    #
    method SetAxisRange {option value} {
        set options($option) $value
        if {![winfo exists $win.c]} return;     # Still constructing.
        $self eraseTicks
        if { $option == "-from" } {
            set valueRange [expr $options(-to) - $value]
        } else {
            set valueRange [expr $value - $options(-from)]
        }
        $self drawTicks

        $self needleTo $lastValue
    }

    #  Handle configure -log
    #  Set the log flag accordingly and then redraw the ticks and value:
    #  Note that we must check the -from/-to and figure out the first decade
    #  and the last decade.
    #
    method SetAxisType {option value} {
        #  No change return.

        if {$value == $options(-log)}  return;    # short cut exit.

        # require booleanness.

        if {![string is boolean $value]} {
            return -error "meter.tcl - value of -log flag must be a boolean"
        }
        #  Set the new value and update the meter:

        set options(-log) $value
        if {!$constructing} {
            $self computeDecades
            $self eraseTicks
            $self drawTicks
            $self needleTo $lastValue
        }
    }

    # Handle a change in the axis' properties ... we just need to set the option and redraw the ticks.
    #
    method SetAxisProperty {option value} {
        set options($option) $value
        if {![winfo exists $options(-canvas)]} return;     # Still constructing.
        $self eraseTicks
        $self drawTicks
    }
}


# move indicator --
#     Collection of procedures to move an item
#

# installVerticalMoveBindings --
#     Install the move bindings for a particular set of items
#
# Arguments:
#     widget         Widget containing the items
#     object         Snit object controlling the items
#     indicatorTag   Tag common to the items
#     ymin           Minimum y coordinate
#     ymax           Maximum y coordinate
#
# Note:
#     The object must define a method NewPosition that takes two arguments:
#     The pixel value of the new position and the tag it belongs to
#
proc ::controlwidget::installVerticalMoveBindings {widget object indicatorTag ymin ymax} {
    variable grab

    if { [info exists grab($object,$indicatorTag)] } {
        unset grab($object,$indicatorTag)
    }

    $widget bind $indicatorTag <ButtonPress-1> [list ::controlwidget::GetIndicator     $widget $object $indicatorTag $ymin $ymax %y]
    $widget bind $indicatorTag <ButtonRelease> [list ::controlwidget::ReleaseIndicator $widget $object $indicatorTag $ymin $ymax %y]
    $widget bind $indicatorTag <Motion>        [list ::controlwidget::MoveIndicator    $widget $object $indicatorTag $ymin $ymax %y]
}

proc ::controlwidget::GetIndicator {w object tag ymin ymax y} {
    variable grab
   # console show
   # puts "Got needle"

    set readonly 0
    catch {
        set readonly [$object cget -readonly]
    }
    if { ! $readonly } {
        set grab($object,$tag) $y
    }
}
proc ::controlwidget::ReleaseIndicator {w object tag ymin ymax y} {
    variable grab
   # puts "Released needle"
    unset grab($object,$tag)
}

proc ::controlwidget::MoveIndicator {w object tag ymin ymax y} {
    variable grab

    if { [info exists grab($object,$tag)] } {
        #
        # Determine the middle of the tagged canvas items
        # - we must limit the repositioning
        #
        set bbox    [$w bbox $tag]
        set ycentre [expr {([lindex $bbox 1] + [lindex $bbox 3]) / 2}]

        set dy [expr {$y - $grab($object,$tag)}]

        if { $ycentre + $dy < $ymin } {
            set dy [expr {$ymin - $ycentre}]
            #set y  [expr {$y + $dy}]
        }
        if { $ycentre + $dy > $ymax } {
            set dy [expr {$ymax - $ycentre}]
            #set y  [expr {$y + $dy}]
        }


        # This should be done by the trace procedure ...
        # TODO: what if there is no variable?
        $w move $tag 0 $dy
        set grab($object,$tag) $y

       # puts "move: $dy -- $y -- [$w bbox $tag]"
        $object NewPosition $y $tag
    }
}


# meter --
#     Type for displaying and controlling a vertical meter
#
snit::widget controlwidget::meter {
    option -height         {2i}
    option -width          {1.5i}
    option -background     white
    option -arrowthickness -default 1      -configuremethod SetArrow
    option -arrowcolor     -default black  -configuremethod SetArrow
    option -variable       -default {}     -configuremethod VariableName
    option -readonly       -default 0      -type snit::boolean

    component axis
    foreach option {-from -to -majorticks -minorticks -log -axisfont -axiscolor -axisformat} {
        delegate option $option to axis
    }

    variable constructing   1

    variable needleId       {}
    variable topY           {}
    variable bottomY        {}
    variable valueRange     {}
    variable needleLeft     {}
    variable meterLeft      {}
    variable majorlength

    variable tickIds        {}
    variable lastValue       0

    variable decadeLow       0;       # e.g. 1 -10... this is the low end exponent.
    variable decadeHigh      1;        # e.g. 10-100.

    variable fontList

    # Construct the widget:

    constructor args {
        install axis using verticalAxis %AUTO% -canvas $win.c
        $self configurelist $args

        # In order to get the font info, we need to create an invisible
        # label so we can query the default font.. we'll accept that
        # but ensure that the font size is 10.

        label $win.hidden
        set fontList [$win.hidden cget -font]
        set fontList [font actual $fontList]
        set fontList [lreplace $fontList 1 1 10];    # Force size to 10pt.

        # Create the canvas and draw the meter into the canvas.
        # The needle is drawn at 1/2 of the rectangle height.
        # 3/4 width.
        # We'll store the resulting size back in the options asn
        # pixels since their much easier to work with:

        canvas $win.c   \
            -width $options(-width)   \
            -height $options(-height) \
            -background white

        set to         [$axis cget -to]
        set from       [$axis cget -from]
        set log        [$axis cget -log]
        set valueRange [expr {1.0*($to - $from)}]


        set options(-height) [$win.c cget -height]
        set options(-width)  [$win.c cget -width]

        # In order to support label we need to create a left margin
        # the margin will be 8chars worth of 8's  in the font we've used
        # and a top/bottom margin of 5pt.. the assumption is that the labels
        # will be drawn in 10pt font.

        set leftmargin [font measure $fontList 88888888]

        set leftmargin [$win.c canvasx $leftmargin]
        set vmargin    [$win.c canvasy 5p]

        # Compute the coordinates of the rectangle and the top/bottom limits
        # (for scaling the arrow position).

        set meterLeft  $leftmargin
        set topY       $vmargin
        set meterRight $options(-width)
        set bottomY    [expr $options(-height) - $vmargin]

        $axis configure -x       $meterLeft
        $axis configure -ybottom $bottomY
        $axis configure -ytop    $topY
        $axis drawAxis


        # draw the frame of the meter as a rectangle:

        $win.c create rectangle $meterLeft $topY $meterRight $bottomY

        # figure out how to put the needle in the middle of the
        # height of the meter allowing 1/4 of the meter for ticks.
        #

        set needleWidth   [expr {3*($meterRight - $meterLeft)/4}]
        set needleHeight  [$axis computeHeight   \
                             [expr {($to + $from)/2}]]
        set needleLeft   [expr $options(-width) - $needleWidth]

        set needleId [$win.c create line $needleLeft $needleHeight      \
                                         $options(-width) $needleHeight -tags {needle arrow} \
                                        -arrow first -fill $options(-arrowcolor) -width $options(-arrowthickness)]]

        set needleHalo [$win.c create rectangle $needleLeft [expr {$needleHeight-3}]      \
                                                $options(-width) [expr {$needleHeight+3}] -fill $options(-background) \
                                                -outline $options(-background) -tags needle]
        $win.c lower $needleHalo


        grid $win.c -sticky nsew

        $axis drawTicks

        if {$options(-variable) ne ""} {
            trace add variable ::$options(-variable) write [mymethod variableChanged]
            if { [info exists ::$options(-variable)] } {
                $self needleTo [set ::$options(-variable)]
            }
        }
        bindDown $win $win

        installVerticalMoveBindings $win.c $self needle $topY $bottomY

        set constructing 0
    }

    #-------------------------------------------------------------------------------
    # public methods
    #

    # Set a new value for the meter... this moves the pointer to a new value.
    # if a variable is tracing the meter, it is changed
    #
    method set newValue {
        if {$options(-variable) ne ""} {
            set ::$options(-variable) $newValue;      # This updates meter too.
        } else {
            $self needleTo $newValue
        }
    }

    # Get the last meter value.
    #
    method get {} {
        return $lastValue
    }

    #-------------------------------------------------------------------------------
    # 'private' methods.

    # trace on -variable being modified.

    method variableChanged {name1 name2 op} {

        $self needleTo [set ::$options(-variable)]
    }

    # Set a new position for the needle:

    method needleTo newCoords {
        set lastValue $newCoords

        set height [$axis computeHeight $newCoords]
        $win.c coords $needleId $needleLeft $height $options(-width) $height
    }


    #  Configure the variable for the meter.
    #  Any prior variable must have its trace removed.
    #  The new variable gets a trace established and the meter position
    #  is updated from it.
    #  Note that if the new variable is "" then the meter will have
    #  no variable associated with it.

    method VariableName {option name} {

        # Could be still constructing in which case
        # $win.c does not exist:

        if {![winfo exists $win.c]} {
            set options(-variable) $name
            return;
        }

        # Remove any old traces


        if {$options(-variable) ne ""} {
            trace remove variable ::$options(-variable) write [mymethod variableChanged]
        }

        # Set new trace if appropriate and update value.

        set options(-variable) $name
        if {$options(-variable) ne ""} {
            trace add variable ::$options(-variable) write [mymethod variableChanged]
            $self needleTo [set ::$options(-variable)]
        }
    }

    # Configure the arrow
    method SetArrow {option value} {
        switch -- $option {
            "-arrowthickness" {
                 $win.c itemconfigure arrow -width $value
            }
            "-arrowcolor" {
                 $win.c itemconfigure arrow -fill $value
            }
        }
    }

    # React to the dragging of the needle
    method NewPosition {y tag} {
        if { $options(-variable) ne "" } {
            set ::$options(-variable) [$axis computeValue $y]
        }
    }
}


# slider --
#     Type for displaying and controlling a vertical slider
#     (It actually supports one or several sliders at once)
#
snit::widget controlwidget::slider {
    option -height         200
    option -width          150
    option -background      -default grey
    option -sliderthickness -default 10     -readonly true -type snit::double
    option -sliderwidth     -default 20     -readonly true -type snit::double
    option -troughwidth     -default 10     -readonly true -type snit::double
    option -variable        -default {}     -configuremethod VariableName
    option -number          -default 1      -readonly true -type snit::integer

    component axis
    foreach option {-from -to -majorticks -minorticks -log -axisfont -axiscolor -axisformat} {
        delegate option $option to axis
    }

    variable constructing   1

    variable topY           {}
    variable bottomY        {}

    variable lastValue      {}
    variable lastHeight     {}

    variable decadeLow       0;       # e.g. 1 -10... this is the low end exponent.
    variable decadeHigh      1;        # e.g. 10-100.

    variable fontList

    # Construct the widget:

    constructor args {
        install axis using verticalAxis %AUTO% -canvas $win.c
        $self configurelist $args

        # In order to get the font info, we need to create an invisible
        # label so we can query the default font.. we'll accept that
        # but ensure that the font size is 10.

        label $win.hidden
        set fontList [$win.hidden cget -font]
        set fontList [font actual $fontList]
        set fontList [lreplace $fontList 1 1 10];    # Force size to 10pt.

        # Create the canvas and draw the slider(s) into the canvas.
        #
        # The geometry of the sliders determines the size of the canvas
        #
        canvas $win.c

        set leftmargin [font measure $fontList 88888888]

        set leftmargin [$win.c canvasx $leftmargin]
        set vmargin    [$win.c canvasy 5p]

        set height [expr {$options(-height) + $vmargin + $options(-sliderthickness)}]
        set width  [expr {$leftmargin + $options(-number) * $options(-sliderwidth) * 1.5 + 0.25* $options(-sliderwidth)}]

        $win.c configure \
            -width $width \
            -height $height \
            -background $options(-background)

        set to         [$axis cget -to]
        set from       [$axis cget -from]
        set log        [$axis cget -log]
        set valueRange [expr {1.0*($to - $from)}]

        set meterLeft  $leftmargin
        set topY       [expr {$vmargin  + 0.5 * $options(-sliderthickness)}]
        set meterRight $options(-width)
        set bottomY    [expr {$height - $vmargin - 0.5 * $options(-sliderthickness)}]

        $axis configure -x       $meterLeft
        $axis configure -ybottom $bottomY
        $axis configure -ytop    $topY
        $axis drawAxis


        # draw the sliders and the troughs

        set sliderThickness $options(-sliderthickness)
        set sliderWidth     $options(-sliderwidth)
        set troughWidth     $options(-troughwidth)
        set number          $options(-number)

        set sliderCentre       [expr {($bottomY + $topY)/2.0}]
        set sliderTop          [expr {$sliderCentre - $sliderThickness/2.0}]
        set sliderCentreTop    [expr {$sliderCentre - 1}]
        set sliderCentreBottom [expr {$sliderCentre + 1}]
        set sliderBottom       [expr {$sliderCentre + $sliderThickness/2.0}]

        set lastHeight {}
        for { set i 0 } { $i < $number } { incr i } {

            set troughLeft      [expr {$meterLeft + ($i*1.5+0.75) * $sliderWidth}]
            set troughRight     [expr {$troughLeft                + $troughWidth}]
            set sliderLeft      [expr {$meterLeft + ($i*1.5+0.5)  * $sliderWidth - 1}]
            set sliderRight     [expr {$sliderLeft                + $sliderWidth}]

            #
            # Trough holding the slider bar
            #
            $win.c create rectangle [expr {$troughLeft-2}] [expr {$topY-2}] $troughRight $bottomY -fill black   ;# Slightly shifted for shadow effect
            $win.c create rectangle $troughLeft $topY $troughRight $bottomY -fill gray40

            #
            # Slider
            #
            $win.c create rectangle $sliderLeft $sliderTop          $sliderRight $sliderCentreTop    -fill gray90 -tag slider$i -outline {}
            $win.c create rectangle $sliderLeft $sliderCentreBottom $sliderRight $sliderBottom       -fill gray30 -tag slider$i -outline {}
            $win.c create rectangle $sliderLeft $sliderCentreTop    $sliderRight $sliderCentreBottom -fill white  -tag slider$i -outline {}
            $win.c create rectangle $sliderLeft $sliderTop          $sliderRight $sliderBottom       -fill {}     -tag slider$i -outline black

            installVerticalMoveBindings $win.c $self slider$i $topY $bottomY

            lappend lastHeight $sliderCentre
        }

        grid $win.c -sticky nsew

        $axis drawTicks

        if {$options(-variable) ne ""} {
            trace add variable ::$options(-variable) write [mymethod variableChanged]
            if { [info exists ::$options(-variable)] } {
                $self sliderTo [set ::$options(-variable)]
            }
        }
        bindDown $win $win

        set constructing 0
    }

    #-------------------------------------------------------------------------------
    # public methods
    #

    # Set a new value for the meter... this moves the pointer to a new value.
    # if a variable is tracing the meter, it is changed
    #
    method set newValue {
        if {$options(-variable) ne ""} {
            set ::$options(-variable) $newValue;      # This updates meter too.
        } else {
            $self sliderTo $newValue
        }
    }

    # Get the last meter value.
    #
    method get {} {
        return $lastValue
    }

    #-------------------------------------------------------------------------------
    # 'private' methods.

    # trace on -variable being modified.

    method variableChanged {name1 name2 op} {

        $self sliderTo [set ::$options(-variable)]
    }

    # Set a new position for the slider:
    #
    # NOTE:
    # Current implementation causes the slider to shift twice as
    # fast! That should not happen of course
    #
    method sliderTo newCoords {

        set move 1
        if { [llength [array names ::controlwidget::grab $self,slider*]] > 0 } {
            set move 0
        }

        set idx       0
        set newheight {}
        foreach coord $newCoords currentHeight $lastHeight {
            set height [$axis computeHeight $coord]
            set dy     [expr {$height - $currentHeight}]

            if { $move } {
                $win.c move slider$idx 0 $dy
            }

            lappend newHeight $height
            incr idx
        }

        set lastValue  $newCoords
        set lastHeight $newHeight
       # puts "sliderTo: [$win.c bbox slider2]"
    }


    #  Configure the variable for the meter.
    #  Any prior variable must have its trace removed.
    #  The new variable gets a trace established and the meter position
    #  is updated from it.
    #  Note that if the new variable is "" then the meter will have
    #  no variable associated with it.

    method VariableName {option name} {

        # Could be still constructing in which case
        # $win.c does not exist:

        if {![winfo exists $win.c]} {
            set options(-variable) $name
            return;
        }

        # Remove any old traces


        if {$options(-variable) ne ""} {
            trace remove variable ::$options(-variable) write [mymethod variableChanged]
        }

        # Set new trace if appropriate and update value.

        set options(-variable) $name
        if {$options(-variable) ne ""} {
            trace add variable ::$options(-variable) write [mymethod variableChanged]
            $self needleTo [set ::$options(-variable)]
        }
    }

    # React to the dragging of the needle
    method NewPosition {y tag} {
        if { $options(-variable) ne "" } {
            set idx [string range $tag 6 end]
            lset ::$options(-variable) $idx [$axis computeValue $y]
            set lastValue [set ::$options(-variable)]
            lset lastHeight $idx $y
          #  puts "$y -- $lastValue -- [$win.c bbox slider2]"
        }
    }
}


# equalizerBar --
#     Type for displaying and controlling a set of coloured bars
#     like the ones found on the display of a hifi equalizer
#
snit::widget controlwidget::equalizerBar {
    option -height         200
    option -width          150
    option -background      -default darkgrey
    option -barwidth        -default 15     -readonly true -type snit::double
    option -segments        -default 10     -readonly true -type snit::integer
    option -variable        -default {}     -configuremethod VariableName
    option -safecolor       -default green
    option -warningcolor    -default red
    option -warninglevel    -default 1.0
    option -number          -default 1      -readonly true -type snit::integer

    component axis
    foreach option {-from -to -majorticks -minorticks -log -axisfont -axiscolor -axisformat} {
        delegate option $option to axis
    }

    variable constructing   1

    variable topY           {}
    variable bottomY        {}

    variable lastValue      {}
    variable lastHeight     {}

    variable decadeLow       0;        # e.g. 1 -10... this is the low end exponent.
    variable decadeHigh      1;        # e.g. 10-100.

    variable segmentIds     {}

    variable fontList

    # Construct the widget:

    constructor args {
        install axis using verticalAxis %AUTO% -canvas $win.c
        $self configurelist $args

        # In order to get the font info, we need to create an invisible
        # label so we can query the default font.. we'll accept that
        # but ensure that the font size is 10.

        label $win.hidden
        set fontList [$win.hidden cget -font]
        set fontList [font actual $fontList]
        set fontList [lreplace $fontList 1 1 10];    # Force size to 10pt.

        # Create the canvas and draw the slider(s) into the canvas.
        #
        # The geometry of the sliders determines the size of the canvas
        #
        canvas $win.c

        set leftmargin [font measure $fontList 88888888]

        set leftmargin [$win.c canvasx $leftmargin]
        set vmargin    [$win.c canvasy 5p]

        set height [expr {$options(-height) + $vmargin}]
        set width  [expr {$leftmargin + $options(-number) * $options(-barwidth) * 1.2 + $options(-barwidth)}]

        set segmentHeight [expr {$options(-height)/double($options(-segments)) - 2}]

        $win.c configure \
            -width $width \
            -height $height \
            -background $options(-background)

        set to         [$axis cget -to]
        set from       [$axis cget -from]
        set log        [$axis cget -log]
        set valueRange [expr {1.0*($to - $from)}]

        set meterLeft  $leftmargin
        set topY       $vmargin
        set meterRight $options(-width)
        set bottomY    [expr {$height - $vmargin}]

        $axis configure -x       $meterLeft
        $axis configure -ybottom $bottomY
        $axis configure -ytop    $topY
        $axis drawAxis

        # draw the bar segments - keep track of the IDs

        set barWidth        $options(-barwidth)
        set numberSegments  $options(-segments)
        set numberBars      $options(-number)

        set lastHeight {}
        set segmentIds {}
        for { set i 0 } { $i < $numberBars } { incr i } {

            set barLeft         [expr {$meterLeft + 10 + $i*1.2 * $barWidth}]
            set barRight        [expr {$barLeft                 + $barWidth}]

            set segmentColumn   {}

            for { set j 0 } { $j < $numberSegments } { incr j } {
                set segmentTop      [expr {$bottomY    - $j * ($segmentHeight+1)}]
                set segmentBottom   [expr {$segmentTop -       $segmentHeight}]

                lappend segmentColumn \
                    [$win.c create rectangle $barLeft $segmentTop $barRight $segmentBottom \
                        -fill $options(-background) -outline $options(-background)]
            }

            lappend segmentIds $segmentColumn
        }

        grid $win.c -sticky nsew

        $axis drawTicks

        if {$options(-variable) ne ""} {
            trace add variable ::$options(-variable) write [mymethod variableChanged]
            if { [info exists ::$options(-variable)] } {
                $self barsTo [set ::$options(-variable)]
            }
        }
        bindDown $win $win

        set constructing 0
    }

    #-------------------------------------------------------------------------------
    # public methods
    #

    # Set a new value for the meter... this moves the pointer to a new value.
    # if a variable is tracing the meter, it is changed
    #
    method set newValue {
        if {$options(-variable) ne ""} {
            set ::$options(-variable) $newValue;      # This updates meter too.
        } else {
            $self barsTo $newValue
        }
    }

    # Get the last meter value.
    #
    method get {} {
        return $lastValue
    }

    #-------------------------------------------------------------------------------
    # 'private' methods.

    # trace on -variable being modified.

    method variableChanged {name1 name2 op} {

        $self barsTo [set ::$options(-variable)]
    }

    # Set a new position for the slider:

    method barsTo newCoords {

        set lowerLimit [$axis cget -from]
        set valueStep  [expr {([$axis cget -to] - $lowerLimit) / double($options(-segments))}]

        set background $options(-background)

        foreach value $newCoords barIds $segmentIds {

            for { set i 0 } { $i < $options(-segments) } { incr i } {
                set limitValue [expr {$lowerLimit + ($i+1) * $valueStep}]

                if { $limitValue <= $value } {
                    set color $options(-safecolor)
                    if { $limitValue > $options(-warninglevel) } {
                        set color $options(-warningcolor)
                    }

                    $win.c itemconfigure [lindex $barIds $i] -fill $color -outline black
                } else {
                    $win.c itemconfigure [lindex $barIds $i] -fill $background -outline $background
                }
            }
        }
    }


    #  Configure the variable for the meter.
    #  Any prior variable must have its trace removed.
    #  The new variable gets a trace established and the meter position
    #  is updated from it.
    #  Note that if the new variable is "" then the meter will have
    #  no variable associated with it.

    method VariableName {option name} {

        # Could be still constructing in which case
        # $win.c does not exist:

        if {![winfo exists $win.c]} {
            set options(-variable) $name
            return;
        }

        # Remove any old traces


        if {$options(-variable) ne ""} {
            trace remove variable ::$options(-variable) write [mymethod variableChanged]
        }

        # Set new trace if appropriate and update value.

        set options(-variable) $name
        if {$options(-variable) ne ""} {
            trace add variable ::$options(-variable) write [mymethod variableChanged]
            $self barsTo [set ::$options(-variable)]
        }
    }
}


# thermometer --
#     Type for displaying and controlling a thermometer
#
snit::widget controlwidget::thermometer {
    option -height         200
    option -width          100
    option -background     white
    option -linethickness  -default 5      -type snit::integer
    option -linecolor      -default red
    option -variable       -default {}     -configuremethod VariableName
    option -readonly       -default 1      -type snit::boolean

    component axis
    foreach option {-from -to -majorticks -minorticks -log -axisfont -axiscolor -axisformat} {
        delegate option $option to axis
    }

    variable constructing   1

    variable topY           {}
    variable bottomY        {}
    variable valueRange     {}
    variable lineId         {}
    variable lineCentre     {}
    variable lineBottom     {}
    variable meterLeft      {}
    variable meterRight     {}
    variable majorlength

    variable lastValue       0

    variable decadeLow       0;       # e.g. 1 -10... this is the low end exponent.
    variable decadeHigh      1;        # e.g. 10-100.

    variable fontList

    # Construct the widget:

    constructor args {
        install axis using verticalAxis %AUTO% -canvas $win.c -axisstyle both
        $self configurelist $args

        # In order to get the font info, we need to create an invisible
        # label so we can query the default font.. we'll accept that
        # but ensure that the font size is 10.

        label $win.hidden
        set fontList [$win.hidden cget -font]
        set fontList [font actual $fontList]
        set fontList [lreplace $fontList 1 1 10];    # Force size to 10pt.

        # Create the canvas and draw the thermometer into the canvas.

        canvas $win.c   \
            -width $options(-width)   \
            -height $options(-height) \
            -background $options(-background)

        set to         [$axis cget -to]
        set from       [$axis cget -from]
        set log        [$axis cget -log]
        set valueRange [expr {1.0*($to - $from)}]


        set options(-height) [$win.c cget -height]
        set options(-width)  [$win.c cget -width]

        # In order to support labels we need to create both a left margin
        # and a right margin
        # the margin will be 8chars worth of 8's  in the font we've used
        # and a top/bottom margin of 5pt.. the assumption is that the labels
        # will be drawn in 10pt font.

        set leftmargin [font measure $fontList 88888888]

        set leftmargin    [$win.c canvasx $leftmargin]
        set topmargin     [expr { 5 + [$win.c canvasy 5p]}]
        set bottommargin  [expr {10 + [$win.c canvasy 5p]}]

        # Compute the coordinates of the rectangle and the top/bottom limits
        # (for scaling the arrow position).

        set meterLeft  $leftmargin
        set meterRight [expr {$leftmargin + $options(-linethickness) + 2}]
        set topY       $topmargin
        set bottomY    [expr $options(-height) - $bottommargin]

        $axis configure -x       $meterLeft
        $axis configure -ybottom $bottomY
        $axis configure -ytop    $topY
        $axis configure -xright  $meterRight
        $axis drawAxis


        # draw the "glass" frame of the thermometer as a double line
        # and some curves

        set lineCentre  [expr {($meterLeft + $meterRight)/2.0}]

        $win.c create line $meterLeft  [expr {$topY - 2}] $meterLeft  [expr {$bottomY + 5}]
        $win.c create line $meterRight [expr {$topY - 2}] $meterRight [expr {$bottomY + 5}]
        $win.c create arc  $meterLeft  [expr {$topY - 5}] $meterRight [expr {$topY + 3}] \
            -start 0 -extent 180 -style arc
        $win.c create oval [expr {$lineCentre - 5}] [expr {$bottomY +  0}] \
                           [expr {$lineCentre + 5}] [expr {$bottomY + 10}] \
            -fill $options(-linecolor) -outline black


        # figure out how to put the needle in the middle of the
        # height of the meter allowing 1/4 of the meter for ticks.
        #

        set lineBottom  [expr {$bottomY + 3}]
        set lineTop     [$axis computeHeight [expr {($to + $from)/2}]]

        set lineId [$win.c create rectangle [expr {$meterLeft+1}] $lineTop $meterRight $lineBottom \
                        -fill $options(-linecolor) -outline {} -tags line]

        set lineHalo [$win.c create rectangle $meterLeft  [expr {$lineTop-3}] $meterRight [expr {$lineTop+3}] \
                        -fill $options(-background) -outline $options(-background) -tags linetop]

        $win.c lower $lineHalo

        grid $win.c -sticky nsew

        $axis drawTicks

        if {$options(-variable) ne ""} {
            trace add variable ::$options(-variable) write [mymethod variableChanged]
            if { [info exists ::$options(-variable)] } {
                $self needleTo [set ::$options(-variable)]
            }
        }
        bindDown $win $win

      # NOT YET
      # installVerticalMoveBindings $win.c $self needle $topY $bottomY

        set constructing 0
    }

    #-------------------------------------------------------------------------------
    # public methods
    #

    # Set a new value for the meter... this moves the pointer to a new value.
    # if a variable is tracing the meter, it is changed
    #
    method set newValue {
        if {$options(-variable) ne ""} {
            set ::$options(-variable) $newValue;      # This updates meter too.
        } else {
            $self lineTo $newValue
        }
    }

    # Get the last meter value.
    #
    method get {} {
        return $lastValue
    }

    #-------------------------------------------------------------------------------
    # 'private' methods.

    # trace on -variable being modified.

    method variableChanged {name1 name2 op} {

        $self lineTo [set ::$options(-variable)]
    }

    # Set a new position for the needle:

    method needleTo newCoords {
        set lastValue $newCoords

        set height [$axis computeHeight $newCoords]
        $win.c coords $lineId [expr {$meterLeft+1}] $lineBottom $meterRight $height
    }


    #  Configure the variable for the meter.
    #  Any prior variable must have its trace removed.
    #  The new variable gets a trace established and the meter position
    #  is updated from it.
    #  Note that if the new variable is "" then the meter will have
    #  no variable associated with it.

    method VariableName {option name} {

        # Could be still constructing in which case
        # $win.c does not exist:

        if {![winfo exists $win.c]} {
            set options(-variable) $name
            return;
        }

        # Remove any old traces


        if {$options(-variable) ne ""} {
            trace remove variable ::$options(-variable) write [mymethod variableChanged]
        }

        # Set new trace if appropriate and update value.

        set options(-variable) $name
        if {$options(-variable) ne ""} {
            trace add variable ::$options(-variable) write [mymethod variableChanged]
            $self needleTo [set ::$options(-variable)]
        }
    }

    # Configure the arrow
    method SetArrow {option value} {
        switch -- $option {
            "-arrowthickness" {
                 $win.c itemconfigure arrow -width $value
            }
            "-arrowcolor" {
                 $win.c itemconfigure arrow -fill $value
            }
        }
    }

    # React to the dragging of the needle
    method NewPosition {y tag} {
        if { $options(-variable) ne "" } {
            set ::$options(-variable) [$axis computeValue $y]
        }
    }
}
