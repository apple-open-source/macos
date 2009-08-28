#!/bin/sh
#\
exec tclsh8.1 "$0" "$@"

# This tests the function of the break facility

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
proc pi {name args} {
    if {$name eq "break"} {
	return -code break
    }
}
proc pcdata pcdata {
    if {[string length [string trim $pcdata]]} {
	puts $pcdata
    }
}

set data(test1) {<?xml version="1.0"?>
<!DOCTYPE Test SYSTEM "test.dtd">
<Test>
<Element>Should see this data</Element>
<Element class="break">Should not see this data</Element>
<Element>Should not see this data</Element>
</Test>}
set data(test2) {<?xml version="1.0"?>
<!DOCTYPE Test SYSTEM "test.dtd">
<Test>
<Element>Should see this data</Element>
<Element>Should see this data
    <Element class="break">Should not see this data</Element>
</Element>
<Element>Should not see this data</Element>
</Test>}
set data(test3) {<?xml version="1.0"?>
<!DOCTYPE Test SYSTEM "test.dtd">
<Test>
<Element>Should see this data</Element>
<Element>Should see this data</Element>
<?break?>
<Element>Should not see this data</Element>
</Test>}

set parser [expat xmlparser \
	-elementstartcommand {element start}	\
	-elementendcommand {element end}	\
	-characterdatacommand pcdata		\
	-processinginstructioncommand pi	\
	-final yes				\
]

foreach {testName testData} [array get data] {
    puts "*** $testName"
    $parser reset
    if {[catch {$parser parse $testData} err]} {
	puts [list test failed due to $err]
    } else {
	puts [list test passed]
    }
}

exit 0