# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: test082.tcl,v 1.1.1.1 2003/02/15 04:56:17 zarzycki Exp $
#
# TEST	test082
# TEST	Test of DB_PREV_NODUP (uses test074).
proc test082 { method {dir -prevnodup} {nitems 100} {tnum 82} args} {
	source ./include.tcl

	eval {test074 $method $dir $nitems $tnum} $args
}
