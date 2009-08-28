#!/bin/sh
# the next line restarts using the interpreter \
exec wish "$0" "$@"

# copyright (C) 1995-2004 Jean-Luc Fontaine (mailto:jfontain@free.fr)


package require stooop 4.1
namespace import stooop::*
package require switched 2.2
package require tkpiechart 6.3


pack [label .m -relief sunken -text\
    "you may move a pie by holding down mouse button 1 over any part of it"\
] -fill x

set canvas [canvas .c -highlightthickness 0]
pack $canvas -fill both -expand 1

set pie1 [new pie\
    $canvas 0 0 -height 100 -thickness 20 -background gray\
    -labeler [new pieBoxLabeler $canvas -justify center -offset 10]\
    -title "this is pie #1" -titlefont fixed -titleoffset 6 -selectable 1\
]
# create a few slices
set slice11 [pie::newSlice $pie1]
set slice12 [pie::newSlice $pie1]
set slice13 [pie::newSlice $pie1]
set slice14 [pie::newSlice $pie1 {some text}]

set pie2 [new pie\
    $canvas 0 0 -height 100 -thickness 10 -background white\
    -labeler [\
        new piePeripheralLabeler $canvas\
            -font {-weight bold -family Helvetica -size -20}\
            -smallfont {-family Helvetica -size -8} -bulletwidth 1c\
    ]\
    -title "this is pie #2" -titleoffset 10\
]
set slice21 [pie::newSlice $pie2]
set slice22 [pie::newSlice $pie2]

# move pies through their tags
$canvas move pie($pie1) 10 40
$canvas move pie($pie2) 240 40

# move pie when holding mouse button 1 in pie/labels area
for {set index 1} {$index <= 2} {incr index} {
    $canvas bind pie([set pie$index]) <ButtonPress-1> "
        set xLast($index) %x
        set yLast($index) %y
    "
    $canvas bind pie([set pie$index]) <Button1-Motion> "
        $canvas move pie([set pie$index])\
            \[expr %x - \$xLast($index)\] \[expr %y - \$yLast($index)\]
        set xLast($index) %x
        set yLast($index) %y
    "
}

# add a couple of buttons
button .d -text {Delete Pies} -command "
    # delete pies thus freeing pie data and destroying pie widgets
    delete $pie1 $pie2
    .d configure -state disabled
    set delete 1
"
button .q -text Exit -command exit
pack .d .q -side left -fill x -expand 1

# now start some animation

set delete 0
set u 1

proc refresh {} {
    if {$::delete} return
    # size the slices in a semi randow way (slice size in per cent of whole pie)
    set ::u [expr (3 * $::u) % 31]
    pie::sizeSlice $::pie1 $::slice11 [expr $::u / 100.0]
    set ::u [expr (5 * $::u) % 31]
    pie::sizeSlice $::pie1 $::slice12 [expr $::u / 100.0]
    set ::u [expr (7 * $::u) % 31]
    # display lebel value in percent for this slice
    pie::sizeSlice $::pie1 $::slice13 [expr $::u / 100.0] "$::u %"
    pie::sizeSlice $::pie2 $::slice21 [expr $::u / 100.0] $::u
    set ::u [expr (11 * $::u) % 31]
    pie::sizeSlice $::pie1 $::slice14 [expr $::u / 100.0]
    pie::sizeSlice $::pie2 $::slice22 [expr $::u / 100.0] $::u
    update
    after 3000 refresh
}

proc resize {width height} {
    set width [expr {$width / 2.0}]
    set height [expr {$height / 2.0}]
    switched::configure $::pie1 -width $width -height $height
    switched::configure $::pie2 -width $width -height $height
    $::canvas configure -scrollregion [$::canvas bbox all]
}

$canvas configure -width 400 -height 300
bind $canvas <Configure> "resize %w %h"
refresh
