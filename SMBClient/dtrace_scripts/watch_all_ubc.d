#!/usr/sbin/dtrace -s

BEGIN
{
	printf("Watching ubc - ^C to quit ...\n");
}

fbt:mach_kernel:ubc*:entry
{ 
    /* Print out the kernel backtrace */
    stack();
}

