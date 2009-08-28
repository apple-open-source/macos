# copyright (C) 1995-2004 Jean-Luc Fontaine (mailto:jfontain@free.fr)

package require Tk 8.3
package require stooop


::stooop::class piePeripheralLabeler {

    variable PI 3.14159265358979323846

    proc piePeripheralLabeler {this canvas args} pieLabeler {$canvas $args}\
        switched {$args} {
        switched::complete $this
        ::set ($this,array) [::stooop::new canvasLabelsArray $canvas\
            -justify $switched::($this,-justify)\
        ]
        ::set ($this,valueWidth) [font measure\
            $switched::($this,-smallfont) $switched::($this,-widestvaluetext)\
        ]
        ::set ($this,valueHeight)\
            [font metrics $switched::($this,-smallfont) -ascent]
    }

    proc ~piePeripheralLabeler {this} {
        ::stooop::delete $($this,array)
        # delete remaining items (should be in pieLabeler destructor)
        $pieLabeler::($this,canvas) delete pieLabeler($this)
    }

    proc options {this} {
        # bullet width, font and justify options are used when creating a new
        # canvas label
        # justify option is used for both the labels array and the labels
        return [list\
            [list -bulletwidth 20 20]\
            [list -font\
                $pieLabeler::(default,font) $pieLabeler::(default,font)\
            ]\
            [list -justify left left]\
            [list -offset 5 5]\
            [list -smallfont {Helvetica -10} {Helvetica -10}]\
            [list -widestvaluetext 0.00 0.00]\
        ]
    }

    foreach option {\
        -bulletwidth -font -justify -offset -smallfont -widestvaluetext\
    } {                                            ;# no dynamic options allowed
        proc set$option {this value} "
            if {\$switched::(\$this,complete)} {
                error {option $option cannot be set dynamically}
            }
        "
    }

    proc set-smallfont {this value} {
        if {$switched::($this,complete)} {
            error {option -smallfont cannot be set dynamically}
        }
    }

    proc new {this slice args} {
        # variable arguments are for the created canvas label object
        ::set canvas $pieLabeler::($this,canvas)
        ::set text [$canvas create text 0 0\
            -font $switched::($this,-smallfont) -tags pieLabeler($this)\
        ]                                                  ;# create value label
        ::set label [eval ::stooop::new canvasLabel\
            $pieLabeler::($this,canvas) $args\
            [list\
                -justify $switched::($this,-justify)\
                -bulletwidth $switched::($this,-bulletwidth)\
                -font $switched::($this,-font) -selectrelief sunken\
            ]\
        ]
        canvasLabelsArray::manage $($this,array) $label
        $canvas addtag pieLabeler($this)\
            withtag canvasLabelsArray($($this,array))        ;# refresh our tags
        # value text item is the only one to update
        ::set ($this,textItem,$label) $text
        ::set ($this,slice,$label) $slice
        ::set ($this,selected,$label) 0
        return $label
    }

    proc anglePosition {degrees} {
        # quadrant specific index with added value for exact quarters
        return [expr {(2 * ($degrees / 90)) + (($degrees % 90) != 0)}]
    }

    ::set index 0     ;# build angle position / value label anchor mapping array
    foreach anchor {w sw s se e ne n nw} {
        ::set (anchor,[anglePosition [expr {$index * 45}]]) $anchor
        incr index
    }
    unset index anchor

    proc set {this label value} {
        ::set text $($this,textItem,$label)
        position $this $text $($this,slice,$label)
        $pieLabeler::($this,canvas) itemconfigure $text -text $value
    }

    proc label {this label args} {
        if {[llength $args] == 0} {
            return [switched::cget $label -text]
        } else {
            switched::configure $label -text [lindex $args 0]
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

    proc position {this text slice} {
        # place the value text item next to the outter border of the
        # corresponding slice
        variable PI

        # retrieve current slice position and dimensions
        slice::data $slice data
        # calculate text closest point coordinates in normal coordinates system
        # (y increasing in north direction)
        ::set midAngle [expr {$data(start) + ($data(extent) / 2.0)}]
        ::set radians [expr {$midAngle * $PI / 180}]
        ::set x [expr {\
            ($data(xRadius) + $switched::($this,-offset)) * cos($radians)\
        }]
        ::set y [expr {\
            ($data(yRadius) + $switched::($this,-offset)) * sin($radians)\
        }]
        ::set angle [expr {round($midAngle) % 360}]
        if {$angle > 180} {
            ::set y [expr {$y - $data(height)}]     ;# account for pie thickness
        }

        ::set canvas $pieLabeler::($this,canvas)
        # now transform coordinates according to canvas coordinates system
        ::set coordinates [$canvas coords $text]
        $canvas move $text\
            [expr {$data(xCenter) + $x - [lindex $coordinates 0]}]\
            [expr {$data(yCenter) - $y - [lindex $coordinates 1]}]
        # finally set anchor according to which point of the text is closest to
        # pie graphics
        $canvas itemconfigure $text -anchor $(anchor,[anglePosition $angle])
    }

    proc delete {this label} {
        canvasLabelsArray::delete $($this,array) $label
        $pieLabeler::($this,canvas) delete $($this,textItem,$label)
        unset\
            ($this,textItem,$label) ($this,slice,$label) ($this,selected,$label)
        # finally reposition the remaining value text items next to their slices
        foreach label [canvasLabelsArray::labels $($this,array)] {
            position $this $($this,textItem,$label) $($this,slice,$label)
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
        # arguments: whole pie coordinates, includings labeler labels
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

    proc updateSlices {this left top right bottom} {
        foreach label [canvasLabelsArray::labels $($this,array)] {
            # position peripheral labels
            position $this $($this,textItem,$label) $($this,slice,$label)
        }
    }

    proc room {this arrayName} {
        upvar 1 $arrayName data

        ::set data(left)\
            [expr {$($this,valueWidth) + $switched::($this,-offset)}]
        ::set data(right) $data(left)
        ::set data(top)\
            [expr {$switched::($this,-offset) + $($this,valueHeight)}]
        ::set box\
            [$pieLabeler::($this,canvas) bbox canvasLabelsArray($($this,array))]
        if {[llength $box] == 0} {                              ;# no labels yet
            ::set data(bottom) $data(top)
        } else {                    ;# room taken by all labels including offset
            ::set data(bottom)\
                [expr {$data(top) + [lindex $box 3] - [lindex $box 1]}]
        }
    }

}
