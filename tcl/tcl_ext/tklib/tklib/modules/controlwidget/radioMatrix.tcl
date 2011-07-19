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


#  Provide a megawidget that is a matrix of radio buttons
#  and a variable that is tracked.  The idea is that this
#  can be used to control a device that has an enumerable
#  set of values.
#
# OPTIONS:
#   -orient  Determines the order in which the radio buttons are
#            laid out:
#            vertical - buttons run from top to bottom then left to right.
#            horizontal - buttons run from left to right top to bottom.
#   -columns Number of columns.
#   -rows    Number of rows.
#   -values  Contains a list of values.  Each element of the list is either
#            a single element, which represents the value of the button or
#            is a pair of values that represent a name/value pair for the button.
#            If -values is provided, only one of -rows/-columns can be provided.
#            If -values is not provided, both -rows and -columns must be provided
#            and the label name/value pairs are 1,2,3,4,5...
#   -variable Variable to track in the widget.
#   -command  Script to run when a radio button is clicked.
#
# METHODS:
#    get   - Gets the current button value.
#    set   - Sets the current button value (-command is invoked if defined).
# NOTES:
#   1. See the constraints on the options described above.
#   2. If, on entry, the variable (either global or fully namespace qualified
#      is set and matches a radio button value, that radio button is initially
#      lit.
#   3. The geometric properties of the widget can only be established at
#      construction time, and are therefore static.

package provide radioMatrix 1.0
package require Tk
package require snit
package require bindDown

namespace eval controlwidget {
    namespace export radioMatrix
}

snit::widget  ::controlwidget::radioMatrix {

    delegate option -variable to label as -textvariable
    delegate option * to hull


    option -orient   horizontal
    option -rows     {1}
    option -columns  {}
    option -values   [list]
    option -command  [list]


    variable radioVariable;             # for the radio button.

    # Construct the widget.

    constructor args {

        # The buttons go in a frame just to make it easy to lay them out.:

        set bf [frame $win.buttons]
        install label using label $win.label

        # Process the configuration.

        $self configurelist $args


        # Ensure that the option constraints are met.

        $self errorIfConstraintsNotMet

        # If the values have not been provided, then use the rows/columns
        # to simluate them.

        if {$options(-values) eq ""}  {
            set totalValues [expr $options(-columns) * $options(-rows)]
            for {set i 0} {$i < $totalValues} {incr i} {
                lappend options(-values) $i
            }
        }

        # Top level layout decision based on orientation.

        if {$options(-orient) eq "horizontal"} {
            $self arrangeHorizontally
        } elseif {$options(-orient) eq "vertical"} {
            $self arrangeVertically
        } else {
            error "Invalid -orient value: $options(-orient)"
        }

        grid $bf
        grid $win.label

        # If the label has a text variable evaluate it to see
        # if we can do a set with it:

        set labelvar [$win.label cget -textvariable]
        if {$labelvar ne ""} {
            $self Set [set ::$labelvar]
        }
        bindDown $win $win
    }

    # Public methods:

    method get {} {
        return $radioVariable
    }
    method set value {

        set radioVariable $value

    }


    # Private methods and procs.

    # Ensure the constraints on the options are met.

    method errorIfConstraintsNotMet {} {
        if {$options(-values) eq "" &&
            ($options(-rows) eq "" || $options(-columns) eq "")} {
            error "If -values is not supplied, but -rows and -coumns must be."
        }
        if {($options(-rows) ne "" && $options(-columns) ne "") &&
            $options(-values) ne ""} {
            error "If both -rows and -coumns were supplied, -values cannot be"
        }
    }


    # Process radio button change.
    #
    method onChange {} {
        set script $options(-command)
        if {$script ne ""} {
            eval $script
        }
    }
    # Manage horizontal layout

    method arrangeHorizontally {} {
        #
        # Either both rows and columns are defined, or
        # one is defined and the other must be computed from the
        # length of the values list (which by god was defined).
        # If both are defined, values was computed from them.

        set rows $options(-rows)
        set cols $options(-columns)

        # Only really need # of cols.

        set len  [llength $options(-values)]
        if {$cols eq ""} {
            set cols [expr ($len + $rows  - 1)/$rows]
        }
        set index  0
        set rowNum 0

        while {$index < $len} {
            for {set i 0} {$i < $cols} {incr i} {
                if {$index >= $len} {
                    break
                }
                set item [lindex $options(-values) $index]

                if {[llength $item] > 1} {
                    set label [lindex $item 0]
                    set value [lindex $item 1]
                } else {
                    set value [lindex $item 0]
                    set label $value
                }
                radiobutton $win.buttons.cb$index \
                    -command [mymethod onChange]  \
                    -variable ${selfns}::radioVariable  \
                    -value $value -text $label
                grid $win.buttons.cb$index -row $rowNum -column $i
                incr index
            }
            incr rowNum
        }

    }


    # manage vertical layout

    method arrangeVertically {} {
        #
        # See arrangeHorizontally for the overall picture, just swap cols
        # and rows.

        set rows $options(-rows)
        set cols $options(-columns)

        set len [llength $options(-values)]
        if {$rows eq ""} {
            set rows [expr ($len + $cols -1)/$cols]
        }
        set index  0
        set colNum 0
        while {$index < $len} {
            for {set i 0} {$i < $rows} {incr i} {
                if {$index >= $len} {
                    break
                }
                set item [lindex $options(-values) $index]
                if {[llength $item] > 1} {
                    set label [lindex $item 0]
                    set value [lindex $item 1]
                } else {
                    set value [lindex $item 0]
                    set label $value
                }

                radiobutton $win.buttons.cb$index \
                    -command [mymethod onChange]  \
                    -variable ${selfns}::radioVariable \
                    -value $value -text $label
                grid $win.buttons.cb$index -row $i -column $colNum
                incr index
            }
            incr colNum
        }
    }
}
