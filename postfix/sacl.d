#!/usr/sbin/dtrace -s

/* Trace Postfix SACL checks. */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("sacl-cached is the time to query the sacl-cache\n");
	printf("sacl-resolve is the time to convert username to GUID\n");
	printf("sacl-finish is the time to query membership in the SACL\n");
	printf("%5s %-16s %-32s %6s %14s\n", "PID", "Probe Name", "User Name", "Result", "Elapsed nsec");
}

::sacl_check:sacl-start
{
	self->start = timestamp;
}

::sacl_check:sacl-cached,
::sacl_check:sacl-resolve,
::sacl_check:sacl-finish
{
	printf("%5d %-16s %-32s %6d %14d\n", pid, probename, copyinstr(arg0), arg1, timestamp - self->start);
	self->start = timestamp;
}
