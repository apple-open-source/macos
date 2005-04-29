# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: test035.tcl,v 1.2 2004/03/30 01:24:08 jtownsen Exp $
#
# TEST	test035
# TEST	Test033 with off-page duplicates
# TEST	DB_GET_BOTH functionality with off-page duplicates.
proc test035 { method {nentries 10000} args} {
	set pgindex [lsearch -exact $args "-pagesize"]
	if { $pgindex != -1 } {
		puts "Test035: Skipping for specific pagesizes"
		return
	}
	# Test with off-page duplicates
	eval {test033 $method $nentries 20 "035" -pagesize 512} $args
	# Test with multiple pages of off-page duplicates
	eval {test033 $method [expr $nentries / 10] 100 "035" -pagesize 512} \
	    $args
}
