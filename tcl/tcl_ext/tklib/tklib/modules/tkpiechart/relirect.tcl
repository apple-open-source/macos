# copyright (C) 1995-2004 Jean-Luc Fontaine (mailto:jfontain@free.fr)

package require Tk 8.3
package require stooop


::stooop::class canvasReliefRectangle {

    proc canvasReliefRectangle {this canvas args} switched {$args} {
        set ($this,topLeft)\
            [$canvas create line 0 0 0 0 0 0 -tags canvasReliefRectangle($this)]
        set ($this,bottomRight)\
            [$canvas create line 0 0 0 0 0 0 -tags canvasReliefRectangle($this)]
        set ($this,canvas) $canvas
        switched::complete $this
    }

    proc ~canvasReliefRectangle {this} {
        $($this,canvas) delete canvasReliefRectangle($this)
    }

    proc options {this} {
        # force background initialization for color calculations
        return [list\
            [list -background white]\
            [list -coordinates {0 0 0 0} {0 0 0 0}]\
            [list -relief flat flat]\
        ]
    }

    proc set-background {this value} {       ;# algorithm stolen from tkUnix3d.c
        set intensity 65535                                 ;# maximum intensity
        foreach {red green blue} [winfo rgb $($this,canvas) $value] {}
        if {\
            (\
                ($red * 0.5 * $red) + ($green * 1.0 * $green) +\
                ($blue * 0.28 * $blue)\
            ) < ($intensity * 0.05 * $intensity)\
        } {
            set ($this,dark) [format {#%04X%04X%04X}\
                [expr {($intensity + (3 * $red)) / 4}]\
                [expr {($intensity + (3 * $green)) / 4}]\
                [expr {($intensity + (3 * $blue)) / 4}]\
            ]
        } else {
            set ($this,dark) [format {#%04X%04X%04X}\
                [expr {(60 * $red) / 100}] [expr {(60 * $green) / 100}]\
                [expr {(60 * $blue) / 100}]\
            ]
        }
        if {$green > ($intensity * 0.95)} {
            set ($this,light) [format {#%04X%04X%04X}\
                [expr {(90 * $red) / 100}] [expr {(90 * $green) / 100}]\
                [expr {(90 * $blue) / 100}]\
        ]
        } else {
            set tmp1 [expr {(14 * $red) / 10}]
            if {$tmp1 > $intensity} {set tmp1 $intensity}
            set tmp2 [expr {($intensity + $red) / 2}]
            set lightRed [expr {$tmp1 > $tmp2? $tmp1: $tmp2}]
            set tmp1 [expr {(14 * $green) / 10}]
            if {$tmp1 > $intensity} {set tmp1 $intensity}
            set tmp2 [expr {($intensity + $green) / 2}]
            set lightGreen [expr {$tmp1 > $tmp2? $tmp1: $tmp2}]
            set tmp1 [expr {(14 * $blue) / 10}]
            if {$tmp1 > $intensity} {set tmp1 $intensity}
            set tmp2 [expr {($intensity + $blue) / 2}]
            set lightBlue [expr {$tmp1 > $tmp2? $tmp1: $tmp2}]
            set ($this,light)\
                [format {#%04X%04X%04X} $lightRed $lightGreen $lightBlue]
        }
        update $this
    }

    proc set-coordinates {this value} {
        foreach {left top right bottom} $value {}
        $($this,canvas) coords $($this,topLeft)\
            $left $bottom $left $top $right $top
        $($this,canvas) coords $($this,bottomRight)\
            $right $top $right $bottom $left $bottom
    }

    proc set-relief {this value} {
        if {![info exists ($this,dark)]} return     ;# colors not yet calculated
        update $this
    }

    proc update {this} {
        switch $switched::($this,-relief) {
            flat {
                $($this,canvas) itemconfigure canvasReliefRectangle($this)\
                    -fill $switched::($this,-background)
            }
            raised {
                $($this,canvas) itemconfigure $($this,topLeft)\
                    -fill $($this,light)
                $($this,canvas) itemconfigure $($this,bottomRight)\
                    -fill $($this,dark)
            }
            sunken {
                $($this,canvas) itemconfigure $($this,topLeft)\
                    -fill $($this,dark)
                $($this,canvas) itemconfigure $($this,bottomRight)\
                    -fill $($this,light)
            }
            default {
                error "bad relief value \"$value\": must be flat, raised or sunken"
            }
        }
    }

}
