# tachometer.tcl --
#
# Adapted by Arjen Markus (snitified), july 2010
#
# TODO:
#     motion through the start and end - it can jump through the gap
#     scaling (scale widget)
#     deal with sizes of the widget (aspect ratio != 1)
#
#
# Part of: The TCL'ers Wiki
# Contents: a tachometer-like widget
# Date: Fri Jun 13, 2003
#
# Abstract
#
#
#
# Copyright (c) 2003 Marco Maggi
#
# The author  hereby grant permission to use,  copy, modify, distribute,
# and  license this  software  and its  documentation  for any  purpose,
# provided that  existing copyright notices  are retained in  all copies
# and that  this notice  is included verbatim  in any  distributions. No
# written agreement, license, or royalty  fee is required for any of the
# authorized uses.  Modifications to this software may be copyrighted by
# their authors and need not  follow the licensing terms described here,
# provided that the new terms are clearly indicated on the first page of
# each file where they apply.
#
# IN NO  EVENT SHALL THE AUTHOR  OR DISTRIBUTORS BE LIABLE  TO ANY PARTY
# FOR  DIRECT, INDIRECT, SPECIAL,  INCIDENTAL, OR  CONSEQUENTIAL DAMAGES
# ARISING OUT  OF THE  USE OF THIS  SOFTWARE, ITS DOCUMENTATION,  OR ANY
# DERIVATIVES  THEREOF, EVEN  IF THE  AUTHOR  HAVE BEEN  ADVISED OF  THE
# POSSIBILITY OF SUCH DAMAGE.
#
# THE  AUTHOR  AND DISTRIBUTORS  SPECIFICALLY  DISCLAIM ANY  WARRANTIES,
# INCLUDING,   BUT   NOT  LIMITED   TO,   THE   IMPLIED  WARRANTIES   OF
# MERCHANTABILITY,    FITNESS   FOR    A    PARTICULAR   PURPOSE,    AND
# NON-INFRINGEMENT.  THIS  SOFTWARE IS PROVIDED  ON AN "AS  IS" BASIS,
# AND  THE  AUTHOR  AND  DISTRIBUTORS  HAVE  NO  OBLIGATION  TO  PROVIDE
# MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
#
# $Id: tachometer.tcl,v 1.3 2010/08/10 11:05:51 arjenmarkus Exp $
#

package require Tk  8.5
package require snit

namespace eval controlwidget {
    namespace export tachometer
}

# tachometer --
#     Tachometer-like widget
#
snit::widget controlwidget::tachometer {

    #
    # widget default values
    #
    option -borderwidth    -default 1
    option -title          -default speed
    option -labels         -default {}
    option -resolution     -default 1
    option -showvalue      -default 1
    option -variable       -default {}      -configuremethod VariableName

    option -min            -default 0.0
    option -max            -default 100.0
    option -dangerlevel    -default 90.0
    option -dangercolor    -default red
    option -dangerwidth    -default 3m
    option -dialcolor      -default white
    option -pincolor       -default red
    option -indexid        -default {}

    option -background         -default gray
    option -width              -default 50m
    option -height             -default 50m
    option -foreground         -default black
    option -highlightthickness -default 0
    option -relief             -default raised

    variable pi [expr {3.14159265359/180.0}]
    variable xc
    variable yc
    variable motion

    constructor args {

        #
        # Configure the widget
        #
        $self configurelist $args

        canvas $win.c -background $options(-background) -width $options(-width) -height $options(-height) \
                      -relief $options(-relief) -borderwidth $options(-borderwidth)
        grid $win.c -sticky news

        if {$options(-variable) ne ""} {
            trace add variable ::$options(-variable) write [mymethod tracer $options(-variable)]
        }

        #
        # Draw the tachometer
        #
        set width  [$win.c cget -width]
        set height [$win.c cget -height]
        set num    [llength $options(-labels)]
        set delta  [expr {(360.0-40.0)/($num-1)}]

        # display
        set x1 [expr {$width/50.0*2.0}]
        set y1 [expr {$width/50.0*2.0}]
        set x2 [expr {$width/50.0*48.0}]
        set y2 [expr {$width/50.0*48.0}]
        $win.c create oval $x1 $y1 $x2 $y2 -fill $options(-dialcolor) -width 1 -outline lightgray
        shadowcircle $win.c $x1 $y1 $x2 $y2 40 0.7m 135.0

        # pin
        set x1 [expr {$width/50.0*23.0}]
        set y1 [expr {$width/50.0*23.0}]
        set x2 [expr {$width/50.0*27.0}]
        set y2 [expr {$width/50.0*27.0}]
        $win.c create oval $x1 $y1 $x2 $y2 -width 1 -outline lightgray -fill $options(-pincolor)
        shadowcircle $win.c $x1 $y1 $x2 $y2 40 0.7m -45.0

        # danger marker
        if { $options(-dangerlevel) != {} && $options(-dangerlevel) < $options(-max)} {

            set deltadanger [expr {(360.0-40.0)*($options(-max)-$options(-dangerlevel))/(1.0*$options(-max)-$options(-min))}]

            # Transform the thickness into a plain number (if given in mm for instance)
            set id [$win.c create line 0 0 1 0]
            $win.c move $id $options(-dangerwidth) 0
            set coords    [$win.c coords $id]
            set thickness [expr {[lindex $coords 0]/2.0}]
            $win.c delete $id

            # Create the arc for the danger level
            $win.c create arc \
                [expr {$width/50.0*4.0+$thickness}]  [expr {$width/50.0*4.0+$thickness}] \
                [expr {$width/50.0*46.0-$thickness}] [expr {$width/50.0*46.0-$thickness}] \
                -start -70 -extent $deltadanger -style arc \
                -outline $options(-dangercolor) -fill $options(-dangercolor) -width $options(-dangerwidth)
        }

        # graduate line
        set x1 [expr {$width/50.0*4.0}]
        set y1 [expr {$width/50.0*4.0}]
        set x2 [expr {$width/50.0*46.0}]
        set y2 [expr {$width/50.0*46.0}]
        $win.c create arc $x1 $y1 $x2 $y2 \
            -start -70 -extent 320 -style arc \
            -outline black -width 0.5m
        set xc [expr {($x2+$x1)/2.0}]
        set yc [expr {($y2+$y1)/2.0}]

        set motion 0
        bind $win.c <ButtonRelease>  [list $self needleRelease %W]
        bind $win.c <Motion>         [list $self needleMotion %W %x %y]

        set half [expr {$width/2.0}]
        set l1 [expr {$half*0.85}]
        set l2 [expr {$half*0.74}]
        set l3 [expr {$half*0.62}]

        set angle  110.0
        for {set i 0} {$i < $num} {incr i} \
        {
            set a [expr {($angle+$delta*$i)*$pi}]

            set x1 [expr {$half+$l1*cos($a)}]
            set y1 [expr {$half+$l1*sin($a)}]
            set x2 [expr {$half+$l2*cos($a)}]
            set y2 [expr {$half+$l2*sin($a)}]
            $win.c create line $x1 $y1 $x2 $y2 -fill black -width 0.5m

            set x1 [expr {$half+$l3*cos($a)}]
            set y1 [expr {$half+$l3*sin($a)}]

            set label [lindex $options(-labels) $i]
            if { [string length $label] } \
            {
               $win.c create text $x1 $y1 \
                   -anchor center -justify center -fill black \
                   -text $label -font { Helvetica 10 }
            }
        }

        rivet $win.c 10 10
        rivet $win.c [expr {$width-10}] 10
        rivet $win.c 10 [expr {$height-10}]
        rivet $win.c [expr {$width-10}] [expr {$height-10}]

        set value 0
        $self drawline $win $value
    }

    method destructor { widget } \
    {
        set varname [option get $widget varname {}]
        trace remove variable $varname write \
         [namespace code "tracer $widget $varname"]
    }

    #
    # public methods --
    #
    method set {newValue} {
        if { $options(-variable) != "" } {
            set ::$options(-variable) $newValue   ;#! This updates the dial too
        } else {
            set options(-value) $newValue
            $self draw $win.c $options(-value)
        }
    }
    method get {} {
        return $options(-value)
    }


    #
    # private methods --
    #

    method VariableName {option name} {

        # Could be still constructing in which case
        # $win.c does not exist:

        if {![winfo exists $win.c]} {
            set options(-variable) $name
            return;
        }

        # Remove any old traces

        if {$options(-variable) ne ""} {
            trace remove variable ::$options(-variable) write [mymethod tracer $options(-variable)]
        }

        # Set new trace if appropriate and update value.

        set options(-variable) $name
        if {$options(-variable) ne ""} {
            trace add variable ::$options(-variable) write [mymethod tracer $options(-variable)]
            $self drawline $win.c [set ::$options(-variable)]
        }
    }
    method tracer { varname args } \
    {
        set options(-value) [set ::$varname]
        $self drawline $win [set ::$varname]
    }

    method drawline { widget value } \
    {
        set c $widget.c

        set min  $options(-min)
        set max  $options(-max)
        set id   $options(-indexid)

        set v [expr { ($value <= ($max*1.02))? $value : ($max*1.02) }]
        set angle [expr {((($v-$min)/($max-$min))*320.0+20.0)*$pi}]

        set width  [$c cget -width]
        set half [expr {$width/2.0}]
        set length [expr {$half*0.8}]

        set xl [expr {$half-$length*sin($angle)}]
        set yl [expr {$half+$length*cos($angle)}]

        set xs [expr {$half+0.2*$length*sin($angle)}]
        set ys [expr {$half-0.2*$length*cos($angle)}]

        catch {$c delete $id}
        set id [$c create line $xs $ys $xl $yl -fill $options(-pincolor) -width 0.6m]
        $c bind $id <ButtonPress> [list $self needlePress %W]
        set options(-indexid) $id
    }

    method needlePress {w} \
    {
        set motion 1
    }

    method needleRelease {w} \
    {
        set motion 0
    }

    method needleMotion {w x y} \
    {
        if {! $motion} { return }
        if {$y == $yc && $x == $xc} { return }

        #
        # Compute the angle with the positive y-axis - easier to examine!
        #
        set angle [expr {atan2($xc - $x,$yc - $y) / $pi}]
        if { $angle >= 160.0 } {
            set angle 160.0
        }
        if { $angle < -160.0 } {
            set angle -160.0
        }
        set ::$options(-variable) [expr {$options(-min) + ($options(-max)-$options(-min))*(160.0-$angle) / 320.0}]
    }

    proc rivet { c xc yc } \
    {
        set width 5
        set bevel 0.5m
        set angle -45.0
        set ticks 7
          shadowcircle $c \
           [expr {$xc-$width}] [expr {$yc-$width}] [expr {$xc+$width}] [expr {$yc+$width}] \
           $ticks $bevel $angle
    }

    proc shadowcircle { canvas x1 y1 x2 y2 ticks width orient } \
    {
        set angle $orient
        set delta [expr {180.0/$ticks}]
        for {set i 0} {$i <= $ticks} {incr i} \
        {
           set a [expr {($angle+$i*$delta)}]
           set b [expr {($angle-$i*$delta)}]

           set color [expr {40+$i*(200/$ticks)}]
           set color [format "#%x%x%x" $color $color $color]

           $canvas create arc $x1 $y1 $x2 $y2 -start $a -extent $delta \
             -style arc -outline $color -width $width
           $canvas create arc $x1 $y1 $x2 $y2 -start $b -extent $delta \
             -style arc -outline $color -width $width
        }
    }
}

if {0} {
# main --
#     Demonstration of the tachometer object
#
proc main { argc argv } \
{
    global forever

    wm withdraw .
    wm title . "A tachometer-like widget"
    wm geometry . +10+10

    controlwidget::tachometer .t1 -variable ::value1 -labels { 0 10 20 30 40 50 60 70 80 90 100 } \
       -pincolor green -dialcolor lightpink
    scale .s1 -command "set ::value1" -variable ::value1

    #
    # Note: the labels are not used in the scaling of the values
    #
    controlwidget::tachometer .t2 -variable ::value2 -labels { 0 {} {} 5 {} {} 10 } -width 100m -height 100m \
        -min 0 -max 10 -dangerlevel 3
    scale .s2 -command "set ::value2" -variable ::value2 -from 0 -to 10

    button .b -text Quit -command "set ::forever 1"

    grid .t1 .s1 .t2 .s2 .b -padx 2 -pady 2
    wm deiconify .

    console show


    vwait forever
    #tachometer::destructor .t1
    #tachometer::destructor .t2
    exit 0
}

main $argc $argv
}

### end of file
# Local Variables:
# mode: tcl
# page-delimiter: "^#PAGE"
# End:
