# $Id: pie.tcl,v 2.25 2006/01/27 19:05:52 andreas_kupries Exp $

package require Tk 8.3
package require stooop


::stooop::class pie {
    set (colors) [list\
        #7FFFFF #FFFF7F #FF7F7F #7FFF7F #7F7FFF #FFBF00 #BFBFBF #FF7FFF #FFFFFF\
    ]
}

proc pie::pie {this canvas x y args} switched {$args} {
    # note: all pie elements are tagged with pie($this)
    set ($this,canvas) $canvas
    set ($this,colorIndex) 0
    set ($this,slices) {}
    # use an empty image as an origin marker with only 2 coordinates
    set ($this,origin) [$canvas create image $x $y -tags pie($this)]
    switched::complete $this
    # wait till all options have been set for initial configuration
    complete $this
}

proc pie::~pie {this} {
    if {[info exists ($this,title)]} {                    ;# title may not exist
        $($this,canvas) delete $($this,title)
    }
    ::stooop::delete $($this,labeler)
    eval ::stooop::delete $($this,slices) $($this,backgroundSlice)
    if {[info exists ($this,selector)]} {              ;# selector may not exist
        ::stooop::delete $($this,selector)
    }
    $($this,canvas) delete $($this,origin)
}

proc pie::options {this} {
    # force height, thickness title font and width options so that corresponding
    # members are properly initialized
    return [list\
        [list -autoupdate 1 1]\
        [list -background {} {}]\
        [list -colors $(colors) $(colors)]\
        [list -height 200]\
        [list -labeler 0 0]\
        [list -selectable 0 0]\
        [list -thickness 0]\
        [list -title {} {}]\
        [list -titlefont {Helvetica -12 bold} {Helvetica -12 bold}]\
        [list -titleoffset 2 2]\
        [list -width 200]\
    ]
}

proc pie::set-autoupdate {this value} {}

# no dynamic options allowed: see complete
foreach option {\
    -background -colors -labeler -selectable -title -titlefont -titleoffset\
} {
    proc pie::set$option {this value} "
        if {\$switched::(\$this,complete)} {
            error {option $option cannot be set dynamically}
        }
    "
}

proc pie::set-thickness {this value} {
    if {$switched::($this,complete)} {
        error {option -thickness cannot be set dynamically}
    }
    # convert to pixels
    set ($this,thickness) [winfo fpixels $($this,canvas) $value]
}

# size is first converted to pixels, then 1 pixel is subtracted since slice size
# is half the pie size and pie center takes 1 pixel
proc pie::set-height {this value} {
    # value is height is slices height not counting thickness
    set ($this,height) [expr {[winfo fpixels $($this,canvas) $value] - 1}]
    if {$switched::($this,complete)} {
        update $this
    } else {      ;# keep track of initial value for latter scaling calculations
        set ($this,initialHeight) $($this,height)
    }
}
proc pie::set-width {this value} {
    set ($this,width) [expr {[winfo fpixels $($this,canvas) $value] - 1}]
    if {$switched::($this,complete)} {
        update $this
    } else {      ;# keep track of initial value for latter scaling calculations
        set ($this,initialWidth) $($this,width)
    }
}

proc pie::complete {this} {                          ;# no user slices exist yet
    set canvas $($this,canvas)

    if {$switched::($this,-labeler) == 0} {
        # use default labeler if user defined none
        set ($this,labeler) [::stooop::new pieBoxLabeler $canvas]
    } else {                                         ;# use user defined labeler
        set ($this,labeler) $switched::($this,-labeler)
    }
    $canvas addtag pie($this) withtag pieLabeler($($this,labeler))
    if {[string length $switched::($this,-background)] == 0} {
        set bottomColor {}
    } else {
        set bottomColor [darken $switched::($this,-background) 60]
    }
    set slice [::stooop::new slice\
        $canvas [expr {$($this,initialWidth) / 2}]\
        [expr {$($this,initialHeight) / 2}]\
        -startandextent {90 360} -height $($this,thickness)\
        -topcolor $switched::($this,-background) -bottomcolor $bottomColor\
    ]
    $canvas addtag pie($this) withtag slice($slice)
    $canvas addtag pieSlices($this) withtag slice($slice)
    set ($this,backgroundSlice) $slice
    if {[string length $switched::($this,-title)] == 0} {
        set ($this,titleRoom) 0
    } else {
        set ($this,title) [$canvas create text 0 0\
            -anchor n -text $switched::($this,-title)\
            -font $switched::($this,-titlefont) -tags pie($this)\
        ]
        set ($this,titleRoom) [expr {\
            [font metrics $switched::($this,-titlefont) -ascent] +\
            [winfo fpixels $canvas $switched::($this,-titleoffset)]\
        }]
    }
    update $this
}

proc pie::newSlice {this {text {}} {color {}}} {
    set canvas $($this,canvas)

    # calculate start radian for new slice
    # (slices grow clockwise from 12 o'clock)
    set start 90
    foreach slice $($this,slices) {
        set start [expr {$start - $slice::($slice,extent)}]
    }
    if {[string length $color] == 0} {
        # get a new color
        set color [lindex $switched::($this,-colors) $($this,colorIndex)]
        set ($this,colorIndex) [expr {\
            ($($this,colorIndex) + 1) % [llength $switched::($this,-colors)]\
        }]                                              ;# circle through colors
    }
    # darken slice top color by 40% to obtain bottom color, as it is done for
    # Tk buttons shadow, for example
    set slice [::stooop::new slice\
        $canvas [expr {$($this,initialWidth) / 2}]\
        [expr {$($this,initialHeight) / 2}] -startandextent "$start 0"\
        -height $($this,thickness) -topcolor $color\
        -bottomcolor [darken $color 60]\
    ]
    # place slice at other slices position in case pie was moved
    eval $canvas move slice($slice) [$canvas coords pieSlices($this)]
    $canvas addtag pie($this) withtag slice($slice)
    $canvas addtag pieSlices($this) withtag slice($slice)
    lappend ($this,slices) $slice
    if {[string length $text] == 0} {     ;# generate label text if not provided
        set text "slice [llength $($this,slices)]"
    }
    set labeler $($this,labeler)
    set label [pieLabeler::new $labeler $slice -text $text -background $color]
    set ($this,sliceLabel,$slice) $label
    # update tags which canvas does not automatically do
    $canvas addtag pie($this) withtag pieLabeler($labeler)
    update $this
    if {$switched::($this,-selectable)} {
        # toggle select state at every button release
        if {![info exists ($this,selector)]} {   ;# create selector if necessary
            set ($this,selector) [::stooop::new objectSelector\
                -selectcommand "pie::setLabelsState $this"\
            ]
        }
        set selector $($this,selector)
        selector::add $selector $label
        $canvas bind canvasLabel($label) <ButtonPress-1>\
            "pie::buttonPress $selector $label"
        $canvas bind slice($slice) <ButtonPress-1>\
            "selector::select $selector $label"
        $canvas bind canvasLabel($label) <Control-ButtonPress-1>\
            "selector::toggle $selector $label"
        $canvas bind slice($slice) <Control-ButtonPress-1>\
            "selector::toggle $selector $label"
        $canvas bind canvasLabel($label) <Shift-ButtonPress-1>\
            "selector::extend $selector $label"
        $canvas bind slice($slice) <Shift-ButtonPress-1>\
            "selector::extend $selector $label"
        $canvas bind canvasLabel($label) <ButtonRelease-1>\
            "pie::buttonRelease $selector $label 0"
        $canvas bind slice($slice) <ButtonRelease-1>\
            "pie::buttonRelease $selector $label 0"
        $canvas bind canvasLabel($label) <Control-ButtonRelease-1>\
            "pie::buttonRelease $selector $label 1"
        $canvas bind slice($slice) <Control-ButtonRelease-1>\
            "pie::buttonRelease $selector $label 1"
        $canvas bind canvasLabel($label) <Shift-ButtonRelease-1>\
            "pie::buttonRelease $selector $label 1"
        $canvas bind slice($slice) <Shift-ButtonRelease-1>\
            "pie::buttonRelease $selector $label 1"
    }
    return $slice
}

proc pie::deleteSlice {this slice} {
    set index [lsearch -exact $($this,slices) $slice]
    if {$index < 0} {
        error "invalid slice $slice for pie $this"
    }
    set ($this,slices) [lreplace $($this,slices) $index $index]
    set extent $slice::($slice,extent)
    ::stooop::delete $slice
    foreach following [lrange $($this,slices) $index end] {
        # rotate the following slices counterclockwise
        slice::rotate $following $extent
    }
    # finally delete label last so that other labels may eventually be
    # repositionned according to remaining slices placement
    pieLabeler::delete $($this,labeler) $($this,sliceLabel,$slice)
    if {$switched::($this,-selectable)} {
        selector::remove $($this,selector) $($this,sliceLabel,$slice)
    }
    unset ($this,sliceLabel,$slice)
    update $this
}

proc pie::sizeSlice {this slice unitShare {valueToDisplay {}}} {
    set index [lsearch -exact $($this,slices) $slice]
    if {$index < 0} {
        error "invalid slice $slice for pie $this"
    }
    # cannot display slices that occupy more than whole pie and less than zero
    set newExtent [expr {[maximum [minimum $unitShare 1] 0] * 360}]
    set growth [expr {$newExtent - $slice::($slice,extent)}]
    switched::configure $slice -startandextent\
        "[expr {$slice::($slice,start) - $growth}] $newExtent" ;# grow clockwise
    if {[string length $valueToDisplay] > 0} {
        # update label after slice for it may need slice latest configuration
        pieLabeler::set $($this,labeler) $($this,sliceLabel,$slice)\
            $valueToDisplay
    } else {
        pieLabeler::set $($this,labeler) $($this,sliceLabel,$slice) $unitShare
    }
    set value [expr {-1 * $growth}]         ;# finally move the following slices
    foreach slice [lrange $($this,slices) [incr index] end] {
        slice::rotate $slice $value
    }
    if {$switched::($this,-autoupdate)} {
        # since label was changed, labeler may need to reorganize labels,
        # for example
        update $this
    }
}

proc pie::labelSlice {this slice text} {
    pieLabeler::label $($this,labeler) $($this,sliceLabel,$slice) $text
    update $this                ;# necessary if number of lines in label changes
}

proc pie::sliceLabelTag {this slice} {
    return canvasLabel($($this,sliceLabel,$slice))
}

proc pie::setSliceBackground {this slice color} {
    switched::configure $slice -topcolor $color -bottomcolor [darken $color 60]
    pieLabeler::labelBackground $($this,labeler) $($this,sliceLabel,$slice)\
        $color
}

proc pie::setSliceLabelBackground {this slice color} {
    pieLabeler::labelTextBackground $($this,labeler) $($this,sliceLabel,$slice)\
        $color
}

proc pie::selectedSlices {this} {  ;# return a list of currently selected slices
    set list {}
    foreach slice $($this,slices) {
        if {[pieLabeler::selectState $($this,labeler)\
            $($this,sliceLabel,$slice)\
        ]} {
            lappend list $slice
        }
    }
    return $list
}

proc pie::setLabelsState {this labels selected} {
    set labeler $($this,labeler)
    foreach label $labels {
        pieLabeler::selectState $labeler $label $selected
    }
}

proc pie::currentSlice {this} {
    # return current slice (slice or its label under the mouse cursor) if any
    set tags [$($this,canvas) gettags current]
    if {\
        ([scan $tags slice(%u) slice] > 0) &&\
        ($slice != $($this,backgroundSlice))\
    } {                                               ;# ignore background slice
        return $slice                                     ;# found current slice
    }
    if {[scan $tags canvasLabel(%u) label] > 0} {
        foreach slice $($this,slices) {
            if {$($this,sliceLabel,$slice) == $label} {
                return $slice              ;# slice is current through its label
            }
        }
    }
    return 0                                                 ;# no current slice
}

proc pie::update {this} {
    # place and scale slices along and with labels array in its current
    # configuration
    set canvas $($this,canvas)
    # retrieve current pie coordinates
    foreach {x y} [$canvas coords $($this,origin)] {}
    set right [expr {$x + $($this,width)}]
    set bottom [expr {$y + $($this,height)}]
    # update labels so that the room that they take can be exactly calculated:
    pieLabeler::update $($this,labeler) $x $y $right $bottom
    pieLabeler::room $($this,labeler) room      ;# take labels room into account
    # move slices in order to leave room for labels
    foreach {xSlices ySlices} [$canvas coords pieSlices($this)] {}
    $canvas move pieSlices($this) [expr {$x + $room(left) - $xSlices}]\
        [expr {$y + $room(top) + $($this,titleRoom) - $ySlices}]
    set scale [list\
        [expr {\
            ($($this,width) - $room(left) - $room(right)) /\
            $($this,initialWidth)\
        }]\
        [expr {\
            (\
                $($this,height) - $room(top) - $room(bottom) -\
                $($this,titleRoom)\
            ) / ($($this,initialHeight) + $($this,thickness))\
        }]\
    ]
    # update scale of background slice
    switched::configure $($this,backgroundSlice) -scale $scale
    foreach slice $($this,slices) {
        switched::configure $slice -scale $scale             ;# and other slices
    }
    # some labelers place labels around slices
    pieLabeler::updateSlices $($this,labeler) $x $y $right $bottom
    if {$($this,titleRoom) > 0} {                                ;# title exists
        # place text above pie and centered
        $canvas coords $($this,title) [expr {$x + ($($this,width) / 2)}] $y
    }
}

proc pie::buttonPress {selector label} {
    foreach selected [selector::selected $selector] {
        # in an already selected label, do not change selection
        if {$selected == $label} return
    }
    selector::select $selector $label
}

proc pie::buttonRelease {selector label extended} {
    # extended means that there is an extended selection in process
    if {$extended} return
    set list [selector::selected $selector]
    if {[llength $list] <= 1} {
        return                ;# nothing to do if there is no multiple selection
    }
    foreach selected $list {
        if {$selected == $label} {               ;# in an already selected label
            selector::select $selector $label     ;# set selection to sole label
            return
        }
    }
}

::stooop::class pie {                       ;# define various utility procedures
    proc maximum {a b} {return [expr {$a > $b? $a: $b}]}
    proc minimum {a b} {return [expr {$a < $b? $a: $b}]}

    catch ::tk::Darken                                  ;# force package loading
    if {[llength [info procs ::tk::Darken]] > 0} {                     ;# Tk 8.4
        proc darken {color percent} {::tk::Darken $color $percent}
    } else {
        proc darken {color percent} {::tkDarken $color $percent}
    }
}
