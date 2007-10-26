#!/usr/sbin/dtrace -s
/*
 * newproc.d - snoop new processes as they are executed. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

proc:::exec-success { trace(stringof(curpsinfo->pr_psargs)); }

