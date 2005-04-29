# ext-test.tcl --
#
#	Implementation of test extension
#
# $Id: ext-test.tcl,v 1.1.1.1 2001/08/14 00:45:27 balls Exp $

package require xslt

namespace eval extTest {
    namespace export test
}

proc extTest::test args {
    return "extTest::test passed [llength $args] arguments"
}

set ch [open ext-test.xsl]
set xsl [read $ch]
close $ch

puts [list register extension]
::xslt::extension add http://tclxml.sf.net/XSLT/Test ::extTest

puts [list do transformation]
::xslt::transform $xsl <Test/>
