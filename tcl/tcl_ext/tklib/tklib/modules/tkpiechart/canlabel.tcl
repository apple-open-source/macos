# copyright (C) 1995-2004 Jean-Luc Fontaine (mailto:jfontain@free.fr)

package require Tk 8.3
package require stooop


::stooop::class canvasLabel {

    proc canvasLabel {this canvas args} switched {$args} {
        set ($this,canvas) $canvas
        # use an empty image as an origin marker with only 2 coordinates
        set ($this,origin) [$canvas create image 0 0 -tags canvasLabel($this)]
        set ($this,selectRectangle)\
            [$canvas create rectangle 0 0 0 0 -tags canvasLabel($this)]
        set ($this,rectangle)\
            [$canvas create rectangle 0 0 0 0 -tags canvasLabel($this)]
        set ($this,text) [$canvas create text 0 0 -tags canvasLabel($this)]
        switched::complete $this
    }

    proc ~canvasLabel {this} {
        eventuallyDeleteRelief $this
        $($this,canvas) delete canvasLabel($this)
    }

    proc options {this} {                ;# force font for proper initialization
        return [list\
            [list -anchor center center]\
            [list -background {} {}]\
            [list -bordercolor black black]\
            [list -borderwidth 1 1]\
            [list -bulletwidth 10 10]\
            [list -font {Helvetica -12}]\
            [list -foreground black black]\
            [list -justify left left]\
            [list -minimumwidth 0 0]\
            [list -padding 2 2]\
            [list -scale {1 1} {1 1}]\
            [list -select 0 0]\
            [list -selectrelief flat flat]\
            [list -stipple {} {}]\
            [list -text {} {}]\
            [list -textbackground {} {}]\
            [list -width 0 0]\
        ]
    }

    proc set-background {this value} {
        $($this,canvas) itemconfigure $($this,rectangle) -fill $value
    }

    proc set-bordercolor {this value} {
        $($this,canvas) itemconfigure $($this,rectangle) -outline $value
    }

    proc set-borderwidth {this value} {
        if {\
            ![string equal $switched::($this,-selectrelief) flat] &&\
            ($value > 1)\
        } {
            error "border width greater than 1 is not supported with $switched::($this,-selectrelief) select relief"
        }
        $($this,canvas) itemconfigure $($this,selectRectangle) -width $value
        $($this,canvas) itemconfigure $($this,rectangle) -width $value
        update $this
    }

    proc set-foreground {this value} {
        $($this,canvas) itemconfigure $($this,text) -fill $value
    }

    proc set-scale {this value} {
        # value is a list of ratios of the horizontal and vertical axis
        update $this       ;# refresh display which takes new scale into account
    }

    proc set-stipple {this value} {
        $($this,canvas) itemconfigure $($this,rectangle) -stipple $value
    }

    foreach option {\
        -anchor -bulletwidth -minimumwidth -padding -select -textbackground\
    } {
        proc set$option {this value} {update $this}
    }

    foreach option {-font -justify -text -width} {
        proc set$option {this value} "
            \$(\$this,canvas) itemconfigure \$(\$this,text) $option \$value
            update \$this
        "
    }

    proc set-selectrelief {this value} {
        if {![regexp {^(flat|raised|sunken)$} $value]} {
            error "bad relief value \"$value\": must be flat, raised or sunken"
        }
        if {[string equal $value flat]} {
            eventuallyDeleteRelief $this
        } else {
            if {$switched::($this,-borderwidth) > 1} {
                error "border width greater than 1 is not supported with $value select relief"
            }
        }
        update $this
    }

    proc eventuallyDeleteRelief {this} {
        if {[info exists ($this,relief)]} {
            ::stooop::delete $($this,relief)
            unset ($this,relief)
        }
    }

    proc updateRelief {this coordinates} {
        if {$switched::($this,-select)} {
            set relief $switched::($this,-selectrelief)
            if {[string equal $relief flat]} {
                eventuallyDeleteRelief $this
            } else {
                set canvas $($this,canvas)
                if {![info exists ($this,relief)]} {
                    set ($this,relief) [::stooop::new canvasReliefRectangle\
                        $canvas -relief $relief\
                    ]
                    set reliefTag canvasReliefRectangle($($this,relief))
                    foreach tag [$canvas gettags canvasLabel($this)] {
                        # adopt all label tags so moving along works
                        $canvas addtag $tag withtag $reliefTag
                    }
                }
                set background $switched::($this,-textbackground)
                if {[string length $background] == 0} {
                    # emulate transparent background
                    set background [$canvas cget -background]
                }
                switched::configure $($this,relief)\
                    -background $background -coordinates {0 0 0 0}
                switched::configure $($this,relief) -coordinates $coordinates
            }
        } else {
            eventuallyDeleteRelief $this
        }
    }

    proc update {this} {
        set canvas $($this,canvas)
        set rectangle $($this,rectangle)
        set selectRectangle $($this,selectRectangle)
        set text $($this,text)

        foreach {x y} [$canvas coords $($this,origin)] {}

        set border [$canvas itemcget $rectangle -width]
        set textBox [$canvas bbox $text]
        set textWidth [expr {[lindex $textBox 2] - [lindex $textBox 0]}]
        set padding [winfo fpixels $canvas $switched::($this,-padding)]
        set bulletWidth [winfo fpixels $canvas $switched::($this,-bulletwidth)]

        $canvas itemconfigure $selectRectangle -fill {} -outline {}

        # position rectangle and text as if anchor was center (the default)
        set width [expr {$bulletWidth + $border + $padding + $textWidth}]
        set halfHeight [expr {\
            (([lindex $textBox 3] - [lindex $textBox 1]) / 2.0) + $border\
        }]
        if {$width < $switched::($this,-minimumwidth)} {
            set width $switched::($this,-minimumwidth)
        }
        set halfWidth [expr {$width / 2.0}]
        set left [expr {$x - $halfWidth}]
        set top [expr {$y - $halfHeight}]
        set right [expr {$x + $halfWidth}]
        set bottom [expr {$y + $halfHeight}]
        $canvas coords $text [expr {\
            $left + $bulletWidth + $border + $padding + ($textWidth / 2.0)\
        }] $y
        $canvas coords $selectRectangle $left $top $right $bottom
        $canvas coords $rectangle $left $top\
            [expr {$left + $bulletWidth}] $bottom
        $canvas itemconfigure $selectRectangle\
            -fill $switched::($this,-textbackground)\
            -outline $switched::($this,-textbackground)
        updateRelief $this\
            [list [expr {$left + $bulletWidth + 1}] $top $right $bottom]
        # now move rectangle and text according to anchor
        set anchor $switched::($this,-anchor)
        set xDelta [expr {\
            ([string match *w $anchor] - [string match *e $anchor]) *\
            $halfWidth\
        }]
        set yDelta [expr {\
            ([string match n* $anchor] - [string match s* $anchor]) *\
            $halfHeight\
        }]
        $canvas move $rectangle $xDelta $yDelta
        $canvas move $selectRectangle $xDelta $yDelta
        $canvas move $text $xDelta $yDelta
        if {[info exists ($this,relief)]} {
            $canvas move canvasReliefRectangle($($this,relief)) $xDelta $yDelta
        }
        # finally apply scale
        eval $canvas scale canvasLabel($this) $x $y $switched::($this,-scale)
    }

}
