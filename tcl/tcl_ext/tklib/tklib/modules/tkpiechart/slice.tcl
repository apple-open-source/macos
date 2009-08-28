# copyright (C) 1995-2004 Jean-Luc Fontaine (mailto:jfontain@free.fr)

package require Tk 8.3
package require stooop


::stooop::class slice {
    variable PI 3.14159265358979323846
}

proc slice::slice {this canvas xRadius yRadius args} switched {$args} {
    # all parameter dimensions must be in pixels
    # note: all slice elements are tagged with slice($this)
    set ($this,canvas) $canvas
    set ($this,xRadius) $xRadius
    set ($this,yRadius) $yRadius
    switched::complete $this
    # wait till all options have been set for initial configuration
    complete $this
    update $this
}

proc slice::~slice {this} {
    if {[string length $switched::($this,-deletecommand)] > 0} {
        # always invoke command at global level
        uplevel #0 $switched::($this,-deletecommand)
    }
    $($this,canvas) delete slice($this)
}

proc slice::options {this} {
    return [list\
        [list -bottomcolor {} {}]\
        [list -deletecommand {} {}]\
        [list -height 0 0]\
        [list -scale {1 1} {1 1}]\
        [list -startandextent {0 0} {0 0}]\
        [list -topcolor {} {}]\
    ]
}

proc slice::set-height {this value} {      ;# not a dynamic option: see complete
    if {$switched::($this,complete)} {
        error {option -height cannot be set dynamically}
    }
}

proc slice::set-bottomcolor {this value} {
    if {![info exists ($this,startBottomArcFill)]} return
    set canvas $($this,canvas)
    $canvas itemconfigure $($this,startBottomArcFill)\
        -fill $value -outline $value
    $canvas itemconfigure $($this,startPolygon) -fill $value
    $canvas itemconfigure $($this,endBottomArcFill) -fill $value -outline $value
    $canvas itemconfigure $($this,endPolygon) -fill $value
}

proc slice::set-topcolor {this value} {
    if {![info exists ($this,topArc)]} return
    $($this,canvas) itemconfigure $($this,topArc) -fill $value
}

# data is stored at switched level
proc slice::set-deletecommand {this value} {}

proc slice::set-scale {this value} {
    if {$switched::($this,complete) && ($value > 0)} {
        # check for valid value following a non reproducible bug report
        update $this                   ;# requires initialization to be complete
    }
}

proc slice::set-startandextent {this value} {
    foreach {start extent} $value {}
    set ($this,start) [normalizedAngle $start]
    if {$extent < 0} {
        set ($this,extent) 0                 ;# a negative extent is meaningless
    } elseif {$extent >= 360} {
        # get as close as possible to 360, which would not work as it is
        # equivalent to 0
        set ($this,extent) [expr {360 - pow(10, -$::tcl_precision + 3)}]
    } else {
        set ($this,extent) $extent
    }
    if {$switched::($this,complete)} {
        update $this                   ;# requires initialization to be complete
    }
}

proc slice::normalizedAngle {value} {
    # normalize value between -180 and 180 degrees (not included)
    while {$value >= 180} {
        set value [expr {$value - 360}]
    }
    while {$value < -180} {
        set value [expr {$value + 360}]
    }
    return $value
}

proc slice::complete {this} {
    set canvas $($this,canvas)
    set xRadius $($this,xRadius)
    set yRadius $($this,yRadius)
    set bottomColor $switched::($this,-bottomcolor)
    # use an empty image as an origin marker with only 2 coordinates
    set ($this,origin)\
        [$canvas create image -$xRadius -$yRadius -tags slice($this)]
    if {$switched::($this,-height) > 0} {                                  ;# 3D
        set ($this,startBottomArcFill) [$canvas create arc\
            0 0 0 0 -style chord -extent 0 -fill $bottomColor\
            -outline $bottomColor -tags slice($this)\
        ]
        set ($this,startPolygon) [$canvas create polygon 0 0 0 0 0 0\
            -fill $bottomColor -tags slice($this)\
        ]
        set ($this,startBottomArc) [$canvas create arc 0 0 0 0\
            -style arc -extent 0 -fill black -tags slice($this)\
        ]
        set ($this,endBottomArcFill) [$canvas create arc 0 0 0 0\
            -style chord -extent 0 -fill $bottomColor\
            -outline $bottomColor -tags slice($this)\
        ]
        set ($this,endPolygon) [$canvas create polygon 0 0 0 0 0 0\
            -fill $bottomColor -tags slice($this)\
        ]
        set ($this,endBottomArc) [$canvas create arc 0 0 0 0\
            -style arc -extent 0 -fill black -tags slice($this)\
        ]
        set ($this,startLeftLine)\
            [$canvas create line 0 0 0 0 -tags slice($this)]
        set ($this,startRightLine)\
            [$canvas create line 0 0 0 0 -tags slice($this)]
        set ($this,endLeftLine) [$canvas create line 0 0 0 0 -tags slice($this)]
        set ($this,endRightLine)\
            [$canvas create line 0 0 0 0 -tags slice($this)]
    }
    set ($this,topArc) [$canvas create arc\
        -$xRadius -$yRadius $xRadius $yRadius\
        -fill $switched::($this,-topcolor) -tags slice($this)\
    ]
    # move slice so upper-left corner is at requested coordinates
    $canvas move slice($this) $xRadius $yRadius
}

proc slice::update {this} {
    set canvas $($this,canvas)
    # first store slice position in case it was moved as a whole
    set coordinates [$canvas coords $($this,origin)]
    set xRadius $($this,xRadius)
    set yRadius $($this,yRadius)
    $canvas coords $($this,origin) -$xRadius -$yRadius
    $canvas coords $($this,topArc) -$xRadius -$yRadius $xRadius $yRadius
    $canvas itemconfigure $($this,topArc)\
        -start $($this,start) -extent $($this,extent)
    if {$switched::($this,-height) > 0} {                                  ;# 3D
        updateBottom $this
    }
    # now position slice at the correct coordinates
    $canvas move slice($this) [expr {[lindex $coordinates 0] + $xRadius}]\
        [expr {[lindex $coordinates 1] + $yRadius}]
    # finally apply scale
    eval $canvas scale slice($this) $coordinates $switched::($this,-scale)
}

proc slice::updateBottom {this} {
    variable PI

    set start $($this,start)
    set extent $($this,extent)

    set canvas $($this,canvas)
    set xRadius $($this,xRadius)
    set yRadius $($this,yRadius)
    set height $switched::($this,-height)

    # first make all bottom parts invisible
    $canvas itemconfigure $($this,startBottomArcFill) -extent 0
    $canvas coords $($this,startBottomArcFill)\
        -$xRadius -$yRadius $xRadius $yRadius
    $canvas move $($this,startBottomArcFill) 0 $height
    $canvas itemconfigure $($this,startBottomArc) -extent 0
    $canvas coords $($this,startBottomArc) -$xRadius -$yRadius $xRadius $yRadius
    $canvas move $($this,startBottomArc) 0 $height
    $canvas coords $($this,startLeftLine) 0 0 0 0
    $canvas coords $($this,startRightLine) 0 0 0 0
    $canvas itemconfigure $($this,endBottomArcFill) -extent 0
    $canvas coords $($this,endBottomArcFill)\
        -$xRadius -$yRadius $xRadius $yRadius
    $canvas move $($this,endBottomArcFill) 0 $height
    $canvas itemconfigure $($this,endBottomArc) -extent 0
    $canvas coords $($this,endBottomArc) -$xRadius -$yRadius $xRadius $yRadius
    $canvas move $($this,endBottomArc) 0 $height
    $canvas coords $($this,endLeftLine) 0 0 0 0
    $canvas coords $($this,endRightLine) 0 0 0 0
    $canvas coords $($this,startPolygon) 0 0 0 0 0 0 0 0
    $canvas coords $($this,endPolygon) 0 0 0 0 0 0 0 0

    set startX [expr {$xRadius * cos($start * $PI / 180)}]
    set startY [expr {-$yRadius * sin($start * $PI / 180)}]
    set end [normalizedAngle [expr {$start + $extent}]]
    set endX [expr {$xRadius * cos($end * $PI / 180)}]
    set endY [expr {-$yRadius * sin($end * $PI / 180)}]

    set startBottom [expr {$startY + $height}]
    set endBottom [expr {$endY + $height}]

    if {(($start >= 0) && ($end >= 0)) || (($start < 0) && ($end < 0))} {
        # start and end angles are on the same side of the 0 abscissa
        if {$extent <= 180} {                ;# slice size is less than half pie
            if {$start < 0} {    ;# slice is facing viewer, so bottom is visible
                $canvas itemconfigure $($this,startBottomArcFill)\
                    -start $start -extent $extent
                $canvas itemconfigure $($this,startBottomArc)\
                    -start $start -extent $extent
                # only one polygon is needed
                $canvas coords $($this,startPolygon)\
                    $startX $startY $endX $endY\
                    $endX $endBottom $startX $startBottom
                $canvas coords $($this,startLeftLine)\
                    $startX $startY $startX $startBottom
                $canvas coords $($this,startRightLine)\
                    $endX $endY $endX $endBottom
            }                                        ;# else only top is visible
        } else {                             ;# slice size is more than half pie
            if {$start < 0} {
                # slice opening is facing viewer, so bottom is in 2 parts
                $canvas itemconfigure $($this,startBottomArcFill)\
                    -start 0 -extent $start
                $canvas itemconfigure $($this,startBottomArc)\
                    -start 0 -extent $start
                $canvas coords $($this,startPolygon)\
                    $startX $startY $xRadius 0\
                    $xRadius $height $startX $startBottom
                $canvas coords $($this,startLeftLine)\
                    $startX $startY $startX $startBottom
                $canvas coords $($this,startRightLine)\
                    $xRadius 0 $xRadius $height

                set bottomArcExtent [expr {$end + 180}]
                $canvas itemconfigure $($this,endBottomArcFill)\
                    -start -180 -extent $bottomArcExtent
                $canvas itemconfigure $($this,endBottomArc)\
                    -start -180 -extent $bottomArcExtent
                $canvas coords $($this,endPolygon)\
                    -$xRadius 0 $endX $endY $endX $endBottom -$xRadius $height
                $canvas coords $($this,endLeftLine)\
                    -$xRadius 0 -$xRadius $height
                $canvas coords $($this,endRightLine)\
                    $endX $endY $endX $endBottom
            } else {
                # slice back is facing viewer, so bottom occupies half the pie
                $canvas itemconfigure $($this,startBottomArcFill)\
                    -start 0 -extent -180
                $canvas itemconfigure $($this,startBottomArc)\
                    -start 0 -extent -180
                # only one polygon is needed
                $canvas coords $($this,startPolygon)\
                    -$xRadius 0 $xRadius 0 $xRadius $height -$xRadius $height
                $canvas coords $($this,startLeftLine)\
                    -$xRadius 0 -$xRadius $height
                $canvas coords $($this,startRightLine)\
                    $xRadius 0 $xRadius $height
            }
        }
    } else {     ;# start and end angles are on opposite sides of the 0 abscissa
        if {$start < 0} {                        ;# slice start is facing viewer
            $canvas itemconfigure $($this,startBottomArcFill)\
                -start 0 -extent $start
            $canvas itemconfigure $($this,startBottomArc)\
                -start 0 -extent $start
            # only one polygon is needed
            $canvas coords $($this,startPolygon) $startX $startY $xRadius 0\
                $xRadius $height $startX $startBottom
            $canvas coords $($this,startLeftLine)\
                $startX $startY $startX $startBottom
            $canvas coords $($this,startRightLine) $xRadius 0 $xRadius $height
        } else {                                   ;# slice end is facing viewer
            set bottomArcExtent [expr {$end + 180}]
            $canvas itemconfigure $($this,endBottomArcFill)\
                -start -180 -extent $bottomArcExtent
            $canvas itemconfigure $($this,endBottomArc)\
                -start -180 -extent $bottomArcExtent
            # only one polygon is needed
            $canvas coords $($this,endPolygon)\
                -$xRadius 0 $endX $endY $endX $endBottom -$xRadius $height
            $canvas coords $($this,startLeftLine) -$xRadius 0 -$xRadius $height
            $canvas coords $($this,startRightLine) $endX $endY $endX $endBottom
        }
    }
}

proc slice::rotate {this angle} {
    if {$angle == 0} return
    set ($this,start) [normalizedAngle [expr {$($this,start) + $angle}]]
    update $this
}

# return actual sizes and positions after scaling
proc slice::data {this arrayName} {
    upvar 1 $arrayName data

    set data(start) $($this,start)
    set data(extent) $($this,extent)
    foreach {x y} $switched::($this,-scale) {}
    set data(xRadius) [expr {$x * $($this,xRadius)}]
    set data(yRadius) [expr {$y * $($this,yRadius)}]
    set data(height) [expr {$y * $switched::($this,-height)}]
    foreach {x y} [$($this,canvas) coords $($this,origin)] {}
    set data(xCenter) [expr {$x + $data(xRadius)}]
    set data(yCenter) [expr {$y + $data(yRadius)}]
}
