#!/usr/sbin/dtrace -s
/*
 * readbytes.d - read bytes by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

sysinfo:::readch { @bytes[execname] = sum(arg0); }
