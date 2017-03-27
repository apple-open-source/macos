#!/usr/sbin/dtrace -s
/*
 * minfbyproc.d - minor faults by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

vminfo:::as_fault { @mem[execname] = sum(arg0); }
