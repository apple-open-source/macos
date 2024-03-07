#!/usr/sbin/dtrace -s
#pragma D option flowindent

BEGIN
{
	printf("Watching all smbfs_vnop/vops - ^C to quit ...\n");
}

fbt:com.apple.filesystems.smbfs:smbfs_mount:entry
{
    printf("proc <%s> ",
            execname
    );
    //ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_mount:return
{
    printf("proc <%s> error <%d>", execname, arg1);
}

fbt:com.apple.filesystems.smbfs:smbfs_unmount:entry
{
    printf("proc <%s> mntflags <%d> ",
            execname,
            arg1
    );
    
    stack();

    ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_unmount:return
{
    printf("proc <%s> error <%d>", execname, arg1);
}





