#!/bin/sh
#\
exec tclsh8.1 "$0" "$@"

# This tests the function of the error facility

lappend auto_path [file dirname [info script]]
package require expat

proc element {tag name {attrs {}}} {
    array set at $attrs
    if {[info exists at(class)]} {
        switch $at(class) {
	    continue {
	        return -code continue
	    }
	    break {
	        return -code break
	    }
	    error {
	        return -code error "error condition in XML"
	    }
        }
    }
}
proc pcdata pcdata {
    if {[string length [string trim $pcdata]]} {
	puts $pcdata
    }
}

set data(error) {<?xml version="1.0"?>
<!DOCTYPE Test SYSTEM "test.dtd">
<Test>
<Element>Should see this data</Element>
<Element class="error">Should not see this data</Element>
<Element>Should not see this data</Element>
</Test>}

set parser [expat xmlparser \
	-elementstartcommand {element start}	\
	-elementendcommand {element end}	\
	-characterdatacommand pcdata		\
	-final yes				\
]

puts {*** Test error}
if {[catch {$parser parse $data(error)} err]} {
    if {$err ne "error condition in XML" } {
	puts [list test failed, incorrect error message: $err]
    } else {
	puts [list test passed]
    }
} else {
    puts [list test failed, no error returned]
}

exit 0