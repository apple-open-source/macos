# copyright (C) 1995-2004 Jean-Luc Fontaine (mailto:jfontain@free.fr)

package require Tk 8.3
package require stooop


::stooop::class pieBoxLabeler {

    proc pieBoxLabeler {this canvas args} pieLabeler {$canvas $args} switched {
        $args
    } {
        ::set ($this,array) [::stooop::new canvasLabelsArray $canvas]
        switched::complete $this
    }

    proc ~pieBoxLabeler {this} {
        ::stooop::delete $($this,array)
    }

    proc options {this} {
        # font and justify options are used when creating a new canvas label
        # justify option is used for both the labels array and the labels
        return [list\
            [list -font\
                $pieLabeler::(default,font) $pieLabeler::(default,font)\
            ]\
            [list -justify left left]\
            [list -offset 5 5]\
            [list -xoffset 0 0]\
        ]
    }

    foreach option {-font -justify -offset -xoffset} {
        # no dynamic options allowed
        proc set$option {this value} "
            if {\$switched::(\$this,complete)} {
                error {option $option cannot be set dynamically}
            }
        "
    }

    proc new {this slice args} {
        # variable arguments are for the created canvas label object
        ::set label [eval ::stooop::new canvasLabel\
            $pieLabeler::($this,canvas) $args\
            [list\
                -justify $switched::($this,-justify)\
                -font $switched::($this,-font) -selectrelief sunken\
            ]\
        ]
        canvasLabelsArray::manage $($this,array) $label
        # refresh our tags
        $pieLabeler::($this,canvas) addtag pieLabeler($this)\
            withtag canvasLabelsArray($($this,array))
        # always append semi-column to label:
        switched::configure $label -text [switched::cget $label -text]:
        ::set ($this,selected,$label) 0
        return $label
    }

    proc delete {this label} {
        canvasLabelsArray::delete $($this,array) $label
        unset ($this,selected,$label)
    }

    proc set {this label value} {
        # update string part after last semi-column
        regsub {:[^:]*$} [switched::cget $label -text] ": $value" text
        switched::configure $label -text $text
    }

    proc label {this label args} {
        ::set text [switched::cget $label -text]
        if {[llength $args] == 0} {
            regexp {^(.*):} $text dummy text
            return $text
        } else {                   ;# update string part before last semi-column
            regsub {^.*:} $text [lindex $args 0]: text
            switched::configure $label -text $text
        }
    }

    proc labelBackground {this label args} {
        if {[llength $args] == 0} {
            return [switched::cget $label -background]
        } else {
            switched::configure $label -background [lindex $args 0]
        }
    }

    proc labelTextBackground {this label args} {
        if {[llength $args] == 0} {
            return [switched::cget $label -textbackground]
        } else {
            switched::configure $label -textbackground [lindex $args 0]
        }
    }

    proc selectState {this label {selected {}}} {
        if {[string length $selected] == 0} {
            # return current state if no argument
            return $($this,selected,$label)
        }
        switched::configure $label -select $selected
        ::set ($this,selected,$label) $selected
    }

    proc update {this left top right bottom} {
        # whole pie coordinates, includings labeler labels
        ::set canvas $pieLabeler::($this,canvas)
        # first reposition labels array below pie graphics
        ::set array $($this,array)
        ::set width [expr {$right - $left}]
        if {$width != [switched::cget $array -width]} {
            switched::configure $array -width $width            ;# fit pie width
        } else {
            canvasLabelsArray::update $array
        }
        foreach {x y} [$canvas coords canvasLabelsArray($array)] {}
        $canvas move canvasLabelsArray($array) [expr {$left - $x}]\
            [expr {$bottom - [canvasLabelsArray::height $array] - $y}]
    }

    proc room {this arrayName} {
        upvar 1 $arrayName data

        ::set data(left) 0                        ;# no room taken around slices
        ::set data(right) 0
        ::set data(top) 0
        ::set box\
            [$pieLabeler::($this,canvas) bbox canvasLabelsArray($($this,array))]
        if {[llength $box] == 0} {                              ;# no labels yet
            ::set data(bottom) 0
        } else {                    ;# room taken by all labels including offset
            ::set data(bottom) [expr {\
                [lindex $box 3] - [lindex $box 1] + $switched::($this,-offset)\
            }]
        }
    }

}
