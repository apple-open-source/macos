# plotgantt.tcl --
#    Facilities to draw Gantt charts in a dedicated canvas
#
# Note:
#    This source file contains the private functions for Gantt charts.
#    It is the companion of "plotchart.tcl"
#    Some functions have been derived from the similar time chart
#    functions.
#

# GanttColor --
#    Set the color of a component
# Arguments:
#    w           Name of the canvas
#    component   Component in question
#    color       New colour
# Result:
#    None
# Side effects:
#    Items with a tag equal to that component are changed
#
proc ::Plotchart::GanttColor { w component color } {
    variable settings

    set settings($w,color,$component) $color

    switch -- $component {
    "description" -
    "summary"     {
        $w itemconfigure $component -foreground $color
    }
    "odd"         -
    "even"        {
        $w itemconfigure $component -fill $color -outline $color
    }
    "completed"   -
    "left"        {
        $w itemconfigure $component -fill $color
    }
    }
}

# GanttFont --
#    Set the font of a component
# Arguments:
#    w           Name of the canvas
#    component   Component in question
#    font        New font
# Result:
#    None
# Side effects:
#    Items with a tag equal to that component are changed
#
proc ::Plotchart::GanttFont { w component font } {
    variable settings

    set settings($w,font,$component) $font

    switch -- $component {
    "description" -
    "summary"     {
        $w itemconfigure $component -font $font
    }
    }
}

# DrawGanttPeriod --
#    Draw a period
# Arguments:
#    w           Name of the canvas
#    text        Text to identify the "period" item
#    time_begin  Start time
#    time_end    Stop time
#    completed   Fraction completed (in %)
# Result:
#    List of item numbers, for further manipulation
# Side effects:
#    Data bars drawn in canvas
#
proc ::Plotchart::DrawGanttPeriod { w text time_begin time_end completed } {
    variable settings
    variable data_series
    variable scaling

    #
    # Draw the text first
    #
    set ytext [expr {$scaling($w,current)-0.5}]
    foreach {x y} [coordsToPixel $w $scaling($w,xmin) $ytext] {break}

    set items {}
    lappend items \
        [$w create text 5 $y -text $text -anchor w \
                                    -tag {description vertscroll above} \
                                    -font $settings($w,font,description)]

    #
    # Draw the bar to indicate the period
    #
    set xmin  [clock scan $time_begin]
    set xmax  [clock scan $time_end]
    set xcmp  [expr {$xmin + $completed*($xmax-$xmin)/100.0}]
    set ytop  [expr {$scaling($w,current)-0.5*(1.0-$scaling($w,dy))}]
    set ybott [expr {$scaling($w,current)-0.5*(1.0+$scaling($w,dy))}]

    foreach {x1 y1} [coordsToPixel $w $xmin $ytop ] {break}
    foreach {x2 y2} [coordsToPixel $w $xmax $ybott] {break}
    foreach {x3 y2} [coordsToPixel $w $xcmp $ybott] {break}

    lappend items \
        [$w create rectangle $x1 $y1 $x3 $y2 -fill $settings($w,color,completed) \
                                             -tag {completed vertscroll horizscroll below}] \
        [$w create rectangle $x3 $y1 $x2 $y2 -fill $settings($w,color,left) \
                                             -tag {left vertscroll horizscroll below}] \
        [$w create text      [expr {$x2+10}] $y -text "$completed%" \
                                             -anchor w \
                                             -tag {description vertscroll horizscroll below} \
                                             -font $settings($w,font,description)]

    set scaling($w,current) [expr {$scaling($w,current)-1.0}]

    ReorderChartItems $w

    return $items
}

# DrawGanttVertLine --
#    Draw a vertical line with a label
# Arguments:
#    w           Name of the canvas
#    text        Text to identify the line
#    time        Time for which the line is drawn
# Result:
#    None
# Side effects:
#    Line drawn in canvas
#
proc ::Plotchart::DrawGanttVertLine { w text time {colour black}} {
    variable settings
    variable data_series
    variable scaling

    #
    # Draw the text first
    #
    set xtime [clock scan $time]
    set ytext [expr {$scaling($w,ymax)-0.5*$scaling($w,dy)}]
    foreach {x y} [coordsToPixel $w $xtime $ytext] {break}

    $w create text $x $y -text $text -anchor w -font $settings($w,font,scale) \
        -tag {horizscroll timeline}

    #
    # Draw the line
    #
    foreach {x1 y1} [coordsToPixel $w $xtime $scaling($w,ymin)] {break}
    foreach {x2 y2} [coordsToPixel $w $xtime $scaling($w,ymax)] {break}

    $w create line $x1 $y1 $x2 $y2 -fill black -tag {horizscroll timeline tline}

    $w raise topmask
}

# DrawGanttMilestone --
#    Draw a "milestone"
# Arguments:
#    w           Name of the canvas
#    text        Text to identify the line
#    time        Time for which the milestone is drawn
#    colour      Optionally the colour
# Result:
#    None
# Side effects:
#    Triangle drawn in canvas
#
proc ::Plotchart::DrawGanttMilestone { w text time {colour black}} {
    variable settings
    variable data_series
    variable scaling

    #
    # Draw the text first
    #
    set ytext [expr {$scaling($w,current)-0.5}]
    foreach {x y} [coordsToPixel $w $scaling($w,xmin) $ytext] {break}

    set items {}
    lappend items \
       [$w create text 5 $y -text $text -anchor w -tag {description vertscroll above} \
             -font $settings($w,font,description)]
       # Colour text?

    #
    # Draw an upside-down triangle to indicate the time
    #
    set xcentre [clock scan $time]
    set ytop    [expr {$scaling($w,current)-0.2}]
    set ybott   [expr {$scaling($w,current)-0.8}]

    foreach {x1 y1} [coordsToPixel $w $xcentre $ybott] {break}
    foreach {x2 y2} [coordsToPixel $w $xcentre $ytop]  {break}

    set x2 [expr {$x1-0.4*($y1-$y2)}]
    set x3 [expr {$x1+0.4*($y1-$y2)}]
    set y3 $y2

    lappend items \
        [$w create polygon $x1 $y1 $x2 $y2 $x3 $y3 -fill $colour \
            -tag {vertscroll horizscroll below}]

    set scaling($w,current) [expr {$scaling($w,current)-1.0}]

    ReorderChartItems $w

    return $items
}

# DrawGanttConnect --
#    Draw a connection between two entries
# Arguments:
#    w           Name of the canvas
#    from        The from item
#    to          The to item
# Result:
#    List of item numbers, for further manipulation
# Side effects:
#    Arrow drawn in canvas
#
proc ::Plotchart::DrawGanttConnect { w from to } {
    variable settings
    variable data_series
    variable scaling

    foreach {xf1 yf1 xf2 yf2} [$w coords [lindex $from 2]] {break}
    foreach {xt1 yt1 xt2 yt2} [$w coords [lindex $to   1]] {break}

    set yfc [expr {($yf1+$yf2)/2.0}]
    set ytc [expr {($yt1+$yt2)/2.0}]

    if { $xf2 > $xf1-15 } {
        set coords [list $xf2             $yfc            \
                         [expr {$xf2+5}]  $yfc            \
                         [expr {$xf2+5}]  [expr {$yf2+5}] \
                         [expr {$xt1-10}] [expr {$yf2+5}] \
                         [expr {$xt1-10}] $ytc            \
                         $xt1             $ytc            ]
    } else {
        set coords [list $xf2             $yfc            \
                         [expr {$xf2+5}]  $yfc            \
                         [expr {$xt2+5}]  $ytc            \
                         $xt1             $ytc            ]
    }

    ReorderChartItems $w

    return [$w create line $coords -arrow last -tag {vertscroll horizscroll below}]
}

# DrawGanttSummary --
#    Draw a summary entry
# Arguments:
#    w           Name of the canvas
#    text        Text to describe the summary
#    args        List of items belonging to the summary
# Result:
#    List of canvas items making up the summary
# Side effects:
#    Items are shifted down to make room for the summary
#
proc ::Plotchart::DrawGanttSummary { w text args } {
    variable settings
    variable data_series
    variable scaling

    #
    # Determine the coordinates of the summary bar
    #
    set xmin {}
    set xmax {}
    set ymin {}
    set ymax {}
    foreach entry $args {
        foreach {x1 y1}             [$w coords [lindex $entry 1]] {break}
        foreach {dummy dummy x2 y2} [$w coords [lindex $entry 2]] {break}

        if { $xmin == {} || $xmin > $x1 } { set xmin $x1 }
        if { $xmax == {} || $xmax < $x2 } { set xmax $x2 }
        if { $ymin == {} || $ymin > $y1 } {
            set ymin  $y1
            set yminb $y2
        }
    }

    #
    # Compute the vertical shift
    #
    set yfirst $scaling($w,ymin)
    set ynext  [expr {$yfirst-1.0}]
    foreach {x y1} [coordsToPixel $w $scaling($w,xmin) $yfirst] {break}
    foreach {x y2} [coordsToPixel $w $scaling($w,xmin) $ynext ] {break}
    set dy [expr {$y2-$y1}]

    #
    # Shift the items
    #
    foreach entry $args {
        foreach item $entry {
            $w move $item 0 $dy
        }
    }

    #
    # Draw the summary text first
    #
    set ytext [expr {($ymin+$yminb)/2.0}]
    set ymin  [expr {$ymin+0.3*$dy}]

    set items {}
    lappend items \
        [$w create text 5 $ytext -text $text -anchor w -tag {summary vertscroll above} \
              -font $settings($w,font,summary)]
        # Colour text?

    #
    # Draw the bar
    #
    set coords [list [expr {$xmin-5}] [expr {$ymin-5}]  \
                     [expr {$xmax+5}] [expr {$ymin-5}]  \
                     [expr {$xmax+5}] [expr {$ymin+5}]  \
                     $xmax            [expr {$ymin+10}] \
                     [expr {$xmax-5}] [expr {$ymin+5}]  \
                     [expr {$xmin+5}] [expr {$ymin+5}]  \
                     $xmin            [expr {$ymin+10}] \
                     [expr {$xmin-5}] [expr {$ymin+5}]  ]

    lappend items \
        [$w create polygon $coords -tag {summarybar vertscroll horizscroll below} \
              -fill $settings($w,color,summarybar)]

    set scaling($w,current) [expr {$scaling($w,current)-1.0}]

    ReorderChartItems $w

    return $items
}
