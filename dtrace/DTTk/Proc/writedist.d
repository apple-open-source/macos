#!/usr/sbin/dtrace -s
/*
 * writedist.d - write distribution by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

sysinfo:::writech { @dist[execname] = quantize(arg0); }
