#!/usr/sbin/dtrace -s
#pragma D option flowindent

BEGIN
{
	printf("Watching smbfs_vnop_readdir - ^C to quit ...\n");
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_readdir:entry
{
    self->vnop_readdir_arg0 = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_readdir_args *) arg0)->a_vp->v_name)
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_readdir:return
/self->vnop_readdir_arg0/
{
    printf("proc <%s> name <%s> error <%d> ",
        execname,
        stringof(((struct vnop_readdir_args *) self->vnop_readdir_arg0)->a_vp->v_name),
        arg1
    );

	self->vnop_readdir_arg0 = 0;
}

