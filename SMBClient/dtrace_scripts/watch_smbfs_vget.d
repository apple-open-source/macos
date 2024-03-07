#!/usr/sbin/dtrace -s
/*#pragma D option flowindent*/

BEGIN
{
	printf("Watching smbfs_vget - ^C to quit ...\n");
}

fbt:com.apple.filesystems.smbfs:smbfs_vget:entry
{
	printf("proc <%s> inode <%u>",
	       execname, 
	       arg1);

	/* Print out the user backtrace */
	ustack();
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_rename:entry
{
    self->vnop_rename_arg0 = arg0;

    printf("proc <%s> curr_name <%s> new_name <%s>",
        execname,
        stringof(((struct vnop_rename_args *) arg0)->a_fvp->v_name),
        stringof(((struct vnop_rename_args *) arg0)->a_tcnp->cn_nameptr)
    );
	//ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_rename:return
/self->vnop_rename_arg0/
{
    printf("proc <%s> curr_name <%s> new_name <%s> error <%d>",
        execname,
        stringof(((struct vnop_rename_args *) self->vnop_rename_arg0)->a_fvp->v_name),
        stringof(((struct vnop_rename_args *) self->vnop_rename_arg0)->a_tcnp->cn_nameptr),
        arg1
    );

	self->vnop_rename_arg0 = 0;
}
