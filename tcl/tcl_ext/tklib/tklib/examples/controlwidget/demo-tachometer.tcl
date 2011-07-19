# demo-tachometer.tcl --

package require controlwidget

# main --
#     Demonstration of the tachometer object
#
proc main { argc argv } \
{
    global forever

    wm withdraw .
    wm title . "A tachometer-like widget"
    wm geometry . +10+10

    controlwidget::tachometer .t1 -variable ::value1 -labels { 0 10 20 30 40 50 60 70 80 90 100 } \
       -pincolor green -dialcolor lightpink
    scale .s1 -command "set ::value1" -variable ::value1

    #
    # Note: the labels are not used in the scaling of the values
    #
    controlwidget::tachometer .t2 -variable ::value2 -labels { 0 {} {} 5 {} {} 10 } -width 100m -height 100m \
        -min 0 -max 10 -dangerlevel 3
    scale .s2 -command "set ::value2" -variable ::value2 -from 0 -to 10

    button .b -text Quit -command "set ::forever 1"

    grid .t1 .s1 .t2 .s2 .b -padx 2 -pady 2
    wm deiconify .

    console show


    vwait forever
    #tachometer::destructor .t1
    #tachometer::destructor .t2
    exit 0
}

main $argc $argv
