# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: test100.tcl,v 1.2 2004/03/30 01:24:09 jtownsen Exp $
#
# TEST	test100
# TEST	Test for functionality near the end of the queue
# TEST	using test025 (DB_APPEND).
proc test100 { method {nentries 10000} {tnum "100"} args} {
	if { [is_queueext $method ] == 0 } {
		puts "Skipping test$tnum for $method."
		return;
	}
	eval {test025 $method $nentries 4294967000 $tnum} $args
}
