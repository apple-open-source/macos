#!/usr/sbin/dtrace -s
/*
 * readdist.d - read distribution by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

sysinfo:::readch { @dist[execname] = quantize(arg0); }
