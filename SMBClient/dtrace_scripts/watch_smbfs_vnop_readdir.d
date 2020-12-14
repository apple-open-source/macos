#!/usr/sbin/dtrace -s
#pragma D option flowindent

BEGIN
{
	printf("Watching smbfs_vnop_readdir - ^C to quit ...\n");
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_readdir:entry
{ 
	printf("proc <%s> offset <%u>", execname, ((struct vnop_readdir_args *) arg0)->a_uio->uio_offset);
	ustack(15); 
}
