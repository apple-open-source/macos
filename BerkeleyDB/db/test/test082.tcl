# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: test082.tcl,v 1.2 2004/03/30 01:24:09 jtownsen Exp $
#
# TEST	test082
# TEST	Test of DB_PREV_NODUP (uses test074).
proc test082 { method {dir -prevnodup} {nitems 100} {tnum "082"} args} {
	source ./include.tcl

	eval {test074 $method $dir $nitems $tnum} $args
}
