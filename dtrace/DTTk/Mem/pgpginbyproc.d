#!/usr/sbin/dtrace -s
/*
 * pgpginbyproc.d - pages paged in by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

vminfo:::pgpgin { @pg[execname] = sum(arg0); }
