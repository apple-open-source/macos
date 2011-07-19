# voltmeter.tcl --
#
# Adapted by Arjen Markus (snitified), july 2010
#
#
#
#
# Part of: The TCL'ers Wiki
# Contents: a voltmeter-like widget
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
# $Id: voltmeter.tcl,v 1.2 2010/08/10 11:05:51 arjenmarkus Exp $
#

package require Tk   8.5
package require snit

namespace eval controlwidget {
    namespace export voltmeter
}

# voltmeter --
#     Voltmeter-like widget
#
snit::widget controlwidget::voltmeter {

    #
    # widget default values
    #
    option -borderwidth           -default        1
    option -background            -default        gray
    option -dialcolor             -default        white
    option -needlecolor           -default        black
    option -scalecolor            -default        black
    option -indexid               -default         {}

    option -variable              -default         {}         -configuremethod VariableName
    option -min                   -default        0.0
    option -max                   -default      100.0
    option -labelcolor            -default        black
    option -titlecolor            -default        black
    option -labelfont             -default      {Helvetica 8}
    option -titlefont             -default      {Helvetica 9}
    option -labels                -default         {}
    option -title                 -default         {}
    option -width                 -default        50m
    option -height                -default        25m
    option -highlightthickness    -default        0
    option -relief                -default        raised

    variable pi [expr {3.14159265359/180.0}]
    variable motion
    variable xc
    variable yc

    constructor args {

        #
        # Configure the widget
        #
        $self configurelist $args

        canvas $win.c -background $options(-background) -width $options(-width) -height $options(-height) \
                      -relief $options(-relief) -borderwidth $options(-borderwidth)
        grid $win.c -sticky news -padx 2m -pady 2m

        if {$options(-variable) ne ""} {
            trace add variable ::$options(-variable) write [mymethod tracer $options(-variable)]
        }

        set width   [$win.c cget -width]
        set height  [$win.c cget -height]
        set xcentre [expr {$width*0.5}]
        set ycentre [expr {$width*1.4}]
        set t       1.15
        set t1      1.25

        $win.c create arc \
               [expr {$xcentre-$width*$t}] [expr {$ycentre-$width*$t}] \
               [expr {$xcentre+$width*$t}] [expr {$ycentre+$width*$t}] \
               -start 70.5 -extent 37 -style arc -outline lightgray \
               -width [expr {$ycentre*0.245}]
        $win.c create arc \
               [expr {$xcentre-$width*$t}] [expr {$ycentre-$width*$t}] \
               [expr {$xcentre+$width*$t}] [expr {$ycentre+$width*$t}] \
               -start 71 -extent 36 -style arc -outline $options(-dialcolor) \
               -width [expr {$ycentre*0.23}]
        $win.c create arc \
               [expr {$xcentre-$width*$t1}] [expr {$ycentre-$width*$t1}] \
               [expr {$xcentre+$width*$t1}] [expr {$ycentre+$width*$t1}] \
               -start 75 -extent 30 \
               -fill black -outline $options(-scalecolor) -style arc -width 0.5m

        set num    [llength $options(-labels)]
        set angle  255.0
        set delta  [expr {30.0/($num-1)}]
        set l1     [expr {$width*$t1}]
        set l2     [expr {$width*$t1*0.95}]
        set l3     [expr {$width*$t1*0.92}]
        for {set i 0} {$i < $num} {incr i} {
           set a [expr {($angle+$delta*$i)*$pi}]

           set x1 [expr {$xcentre+$l1*cos($a)}]
           set y1 [expr {$ycentre+$l1*sin($a)}]
           set x2 [expr {$xcentre+$l2*cos($a)}]
           set y2 [expr {$ycentre+$l2*sin($a)}]
           $win.c create line $x1 $y1 $x2 $y2 -fill $options(-scalecolor) -width 0.5m

           set x1 [expr {$xcentre+$l3*cos($a)}]
           set y1 [expr {$ycentre+$l3*sin($a)}]

           set label [lindex $options(-labels) $i]
           if { [string length $label] } {
               $win.c create text $x1 $y1 \
                       -anchor center -justify center -fill $options(-labelcolor) \
                       -text $label -font $options(-labelfont)
           }
        }

        set title $options(-title)
        if { [string length $title] } {
           $win.c create text $xcentre [expr {$ycentre-$width*1.05}] \
                   -anchor center -justify center -fill $options(-titlecolor) \
                   -text $title -font $options(-titlefont)
        }

        rivet $win.c 10 10
        rivet $win.c    [expr {$width-10}] 10
        rivet $win.c 10 [expr {$height-10}]
        rivet $win.c    [expr {$width-10}] [expr {$height-10}]

        set motion 0
        set xc $xcentre
        set yc $ycentre
        bind $win.c <ButtonRelease>  [list $self needleRelease %W]
        bind $win.c <Motion>         [list $self needleMotion %W %x %y]

        set value 0
        $self drawline $win $value
    }

    method destructor {} {
        set varname ::$options(-variable)]
        trace remove variable $varname write \
            [namespace code "mymethod tracer $win $varname"]
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

    method drawline { widget value } {
        set id     $options(-indexid)
        set min    $options(-min)
        set max    $options(-max)

        set c      $widget.c

        set v [expr { ($value <= ($max*1.05))? $value : ($max*1.05) }]

        set angle [expr {((($v-$min)/(1.0*($max-$min)))*30.0+165.0)*$pi}]

        set width   [$c cget -width]
        set xcentre [expr {$width/2.0}]
        set ycentre [expr {$width*1.4}]
        set l1      [expr {$ycentre*0.85}]
        set l2      [expr {$ycentre*0.7}]

        set xl      [expr {$xcentre-$l1*sin($angle)}]
        set yl      [expr {$ycentre+$l1*cos($angle)}]
        set xs      [expr {$xcentre-$l2*sin($angle)}]
        set ys      [expr {$ycentre+$l2*cos($angle)}]

        catch {$c delete $id}
        set id [$c create line $xs $ys $xl $yl -fill $options(-needlecolor) -width 0.6m]
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
        if { $angle >= 15.0 } {
            set angle 15.0
        }
        if { $angle < -15.0 } {
            set angle -15.0
        }
        set ::$options(-variable) [expr {$options(-min) + ($options(-max)-$options(-min))*(15.0-$angle) / 30.0}]
    }


    proc rivet { c xc yc } {
        shadowcircle $c \
            [expr {$xc-4}] [expr {$yc-4}] [expr {$xc+4}] [expr {$yc+4}] \
            5 0.5m -45.0
    }

    proc shadowcircle { canvas x1 y1 x2 y2 ticks width orient } {
        set radius [expr {($x2-$x1)/2.0}]

        set angle $orient
        set delta [expr {180.0/$ticks}]
        for {set i 0} {$i <= $ticks} {incr i} {
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
#     Demonstration of the voltmeter object
#
proc main { argc argv } {
    global     forever

    wm withdraw .
    wm title    . "A voltmeter-like widget"
    wm geometry . +10+10

    ::controlwidget::voltmeter .t1 -variable value1 -labels { 0 50 100 } -title "Voltmeter (V)"
    scale .s1 -command "set ::value1" -variable value1

    ::controlwidget::voltmeter .t2 -variable value2 -labels { 0 {} 2.5 {} 5 } \
       -width 80m -height 40m -title "Ampere (mA)" -dialcolor lightgreen -scalecolor white \
       -min 0 -max 5
    scale .s2 -command "set ::value2" -variable value2

    button .b -text Quit -command "set ::forever 1"

    grid .t1 .s1 .t2 .s2 .b
    wm deiconify .
    vwait forever
    .t1 destructor
    .t2 destructor
    exit 0
}

main $argc $argv
}

### end of file
# Local Variables:
# mode: tcl
# page-delimiter: "^#PAGE"
# End:
