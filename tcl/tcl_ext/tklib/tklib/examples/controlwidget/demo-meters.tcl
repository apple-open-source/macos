# demo-meters.tcl --
#     Straightforward demonstration of various meters

package require controlwidget

# main --
#     Test it
#
#     Note: variable must exist! This is a bug
#
set ::metervar 1.0
set ::slidervar {1.0 0.5 7}
set ::barvar {1.0 4.0 7.0}
set ::thmvar 10.0
pack [::controlwidget::meter .meter -variable metervar -from 0.0 -to 10.0 -axisformat %.1f -axiscolor green] \
     [::controlwidget::slider .slider -variable slidervar -from 0.0 -to 10.0 -number 3 -axisformat %.1f -axiscolor green] \
     [::controlwidget::equalizerBar .bar -variable barvar -from 0.0 -to 10.0 -number 3 -warninglevel 5] \
     [::controlwidget::thermometer  .thm -variable thmvar -from -10.0 -to 30.0 -majorticks 5] -side left

after 1000 {
    set ::metervar 5.0
    .meter configure -arrowthickness 3
    .meter configure -arrowcolor blue
}

set ledvar 0
pack [::controlwidget::led .led -variable ledvar -off red] -side top

after 2000 {
    set ::ledvar 1
}

proc changeBars {v} {
    set ::barvar [list [expr {5.0 + 5.0*cos($v)}] [expr {5.0 + 5.0*sin($v)}] [expr {4.0 + 2.5*cos(2*$v)}]]

    after 100 [list changeBars [expr {$v+0.1}]]
}

after 500 [list changeBars 0.0]
