#!/usr/sbin/dtrace -s
/*
 * writebytes.d - write bytes by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

sysinfo:::writech { @bytes[execname] = sum(arg0); }
