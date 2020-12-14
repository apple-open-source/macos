#!/usr/sbin/dtrace -s

BEGIN
{
	printf("Watching smbfs - ^C to quit ...\n");
}

fbt:com.apple.filesystems.smbfs::entry
{ 
	printf("proc <%s>", execname);
}
