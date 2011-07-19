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
#            Ron Fox
#            NSCL
#            Michigan State University
#            East Lansing, MI 48824-1321
#
#     Adjusted by Arjen Markus
#
#
#   This package provides an LED
#   widget.  LED widgets are one color when on
#   and another when off.
#   Implementation is just a filled circle on a
#   canvas.
#   Options recognized:
#     (all standard options for a frame).
#     -size      - Radius of the led.
#     -on        - Color of on state.
#     -off       - Color of off state.
#     -variable  - on color when variable is nonzero else off.
#  Methods
#     on         - Turn led on.
#     off        - Turn led off.
#
#  TODO:
#     Add a label
#

package provide led 1.0
package require Tk
package require snit
package require bindDown

namespace eval controlwidget {
    namespace export led
}

snit::widget controlwidget::led {
    delegate option * to hull
    option   -size {17}
    option   -on   green
    option   -off  black
    option   -variable {}


    constructor args {
        $self configurelist $args

        canvas $win.led -width $options(-size) -height $options(-size)
        set border [expr [$win cget -borderwidth] + 2]
        set end [expr $options(-size) - $border]
        $win.led create oval $border $border $end $end -fill $options(-off)
        grid $win.led -sticky nsew

        bindDown $win $win
    }

    # Process the -variable configuration by killing off prior traces
    # and setting an new trace:
    #

    onconfigure -variable name {
        if {$options(-variable) ne ""} {
            trace remove variable ::$options(-variable) write [mymethod varTrace]
        }
        trace add variable ::$name  write [mymethod varTrace]
        set options(-variable) $name

        # set our initial state to the current value of the var:
        # the after is because we could be constructing an need to give
        # the widgets a chance to get built:

        after 10 [list $self varTrace $name "" write]

    }
    # Trace for the led variable..
    #
    method varTrace {name index op} {
        set name ::$name
        set value [set $name]
        if {[string is boolean -strict $value]} {
            $self setstate $value
        }
    }
    #
    # Set the led on.
    #
    method on {} {
        if {$options(-variable) ne ""} {
            set ::$options(-variable) 1
        } else {
            $self setstate 1
        }
    }
    # set the led off
    #
    method off {} {
        if {$options(-variable) ne ""} {
            set ::$options(-variable) 0
        } else {
            $self setstate 0
        }
    }
    #
    # Set the led state
    #
    method setstate {value} {
        if {$value} {
            $win.led itemconfigure 1 -fill $options(-on)
        } else {
            $win.led itemconfigure 1 -fill $options(-off)
        }
    }
}
