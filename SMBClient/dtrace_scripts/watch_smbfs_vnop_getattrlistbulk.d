#!/usr/sbin/dtrace -s
#pragma D option flowindent

BEGIN
{
	printf("Watching smbfs_vnop_getattrlistbulk - ^C to quit ...\n");
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattrlistbulk:entry
{ 
	printf("proc <%s> dir <%s> offset <%lld> va_active <0x%llx>",
            execname,
            stringof(((struct vnop_getattrlistbulk_args *) arg0)->a_vp->v_name),
            ((struct vnop_getattrlistbulk_args *) arg0)->a_uio->uio_offset,
            ((struct vnop_getattrlistbulk_args *) arg0)->a_vap->va_active);
	ustack(); 
}
