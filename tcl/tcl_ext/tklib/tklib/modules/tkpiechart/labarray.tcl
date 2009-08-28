# copyright (C) 1995-2004 Jean-Luc Fontaine (mailto:jfontain@free.fr)

package require Tk 8.3
package require stooop


::stooop::class canvasLabelsArray {

    proc canvasLabelsArray {this canvas args} switched {$args} {
        set ($this,canvas) $canvas
        # use an empty image as an origin marker with only 2 coordinates
        set ($this,origin)\
            [$canvas create image 0 0 -tags canvasLabelsArray($this)]
        set ($this,labels) {}
        switched::complete $this
    }

    proc ~canvasLabelsArray {this} {
        eval ::stooop::delete $($this,labels)
        # delete remaining items
        $($this,canvas) delete canvasLabelsArray($this)
    }

    proc options {this} {
        # force width initialization for internals initialization:
        return [list\
            [list -justify left left]\
            [list -width 100]\
        ]
    }

    proc set-justify {this value} {
        if {$switched::($this,complete)} {
            error {option -justify cannot be set dynamically}
        }
    }

    proc set-width {this value} {
        set ($this,width) [winfo fpixels $($this,canvas) $value]
        update $this
    }

    proc manage {this label} {                          ;# must be a canvasLabel
        $($this,canvas) addtag canvasLabelsArray($this)\
            withtag canvasLabel($label)
        lappend ($this,labels) $label
        update $this
    }

    proc delete {this label} {
        set index [lsearch -exact $($this,labels) $label]
        if {$index < 0} {
            error "invalid label $label for canvas labels array $this"
        }
        set ($this,labels) [lreplace $($this,labels) $index $index]
        ::stooop::delete $label
        update $this
    }

    proc update {this} {
        set canvas $($this,canvas)
        set halfWidth [expr {round($($this,width) / 2.0)}]
        foreach {xOrigin yOrigin} [$canvas coords $($this,origin)] {}
        set x 0; set y 0
        set height 0
        set column 0
        foreach label $($this,labels) {
            foreach {left top right bottom}\
                [$canvas bbox canvasLabel($label)] {}
            set wide [expr {($right - $left) > $halfWidth}]
            if {$wide} {
                # label does not fit in a half width so open a new line
                set x 0; incr y $height; set height 0
            }
            switched::configure $label -anchor nw
            # do an absolute positioning using label tag:
            foreach {xDelta yDelta} [$canvas coords canvasLabel($label)] {}
            $canvas move canvasLabel($label) [expr {$xOrigin + $x - $xDelta}]\
                [expr {$yOrigin + $y - $yDelta}]
            set value [expr {$bottom - $top}]
            if {$value > $height} {         ;# keep track of current line height
                set height $value
            }
            if {([incr x $halfWidth] > $halfWidth) || $wide} {
                set x 0; incr y $height; set height 0
            }
        }
    }

    proc labels {this} {
        return $($this,labels)
    }

    proc height {this} {
        set list [$($this,canvas) bbox canvasLabelsArray($this)]
        if {[llength $list] == 0} {
            return 0
        }
        foreach {left top right bottom} $list {}
        return [expr {$bottom - $top}]
    }

}
