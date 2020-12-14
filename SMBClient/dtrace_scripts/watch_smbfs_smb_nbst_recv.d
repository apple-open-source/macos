#!/usr/sbin/dtrace -s

fbt:com.apple.filesystems.smbfs:smb_nbst_recv:entry
{
	printf("proc <%s>", execname);

	/* Print out the kernel backtrace */
	stack(); 
}
