#!/usr/sbin/dtrace -s

fbt:com.apple.filesystems.smbfs:smbfs_trigger_get_mount_args:entry
{
	printf("proc <%s>  \n", execname);

	/* Print out the kernel backtrace */
	stack(); 

	/* Print out the user backtrace */
	ustack();
}

