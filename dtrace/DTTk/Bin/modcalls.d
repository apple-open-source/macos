#!/usr/sbin/dtrace -s
/*
 * modcalls.d - kernel function calls by module. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 09-Jan-2006  Brendan Gregg   Created this.
 */

fbt:::entry { @calls[probemod] = count(); }
