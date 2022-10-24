/*
 * ASSERTION:
 *
 * Trace at least one entry/return of mach_trap provider.
 *
 */

#pragma D option quiet
#pragma D option statusrate=100ms

int i;
BEGIN
{
	i = 0;
}

mach_trap:::entry
{
	i++;
}

mach_trap:::return
/ i > 0 /
{
	exit(0);
}
