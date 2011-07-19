# controlwidget.tcl --
#     Set up the requirements for the controlwidget module/package
#     and source the individual files
#

package require Tk 8.5
package require snit

source [file join [file dirname [info script]] bindDown.tcl]

source [file join [file dirname [info script]] vertical_meter.tcl]
source [file join [file dirname [info script]] led.tcl]
source [file join [file dirname [info script]] rdial.tcl]
source [file join [file dirname [info script]] tachometer.tcl]
source [file join [file dirname [info script]] voltmeter.tcl]
source [file join [file dirname [info script]] radioMatrix.tcl]

package provide controlwidget 0.1
