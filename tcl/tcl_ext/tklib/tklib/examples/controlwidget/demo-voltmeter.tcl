# demo-voltmeter.tcl --

package require controlwidget

# main --
#     Demonstration of the voltmeter object
#
proc main { argc argv } {
    global     forever

    wm withdraw .
    wm title    . "A voltmeter-like widget"
    wm geometry . +10+10

    ::controlwidget::voltmeter .t1 -variable value1 -labels { 0 50 100 } -title "Voltmeter (V)"
    scale .s1 -command "set ::value1" -variable value1

    ::controlwidget::voltmeter .t2 -variable value2 -labels { 0 {} 2.5 {} 5 } \
       -width 80m -height 40m -title "Ampere (mA)" -dialcolor lightgreen -scalecolor white \
       -min 0 -max 5
    scale .s2 -command "set ::value2" -variable value2

    button .b -text Quit -command "set ::forever 1"

    grid .t1 .s1 .t2 .s2 .b
    wm deiconify .
    vwait forever
    .t1 destructor
    .t2 destructor
    exit 0
}

main $argc $argv
