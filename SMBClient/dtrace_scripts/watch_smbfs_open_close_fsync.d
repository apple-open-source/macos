#!/usr/sbin/dtrace -s
#pragma D option flowindent

BEGIN
{
	printf("Watching smb opens/closes/fsync - ^C to quit ...\n");
}


fbt:com.apple.filesystems.smbfs:smbfs_vnop_open:entry
{ 
    self->vnop_open_arg0 = arg0;

    printf("proc <%s> name <%s> a_mode <0x%x>",
    	   execname,
    	   stringof(((struct vnop_open_args *) arg0)->a_vp->v_name),
     	   ((struct vnop_open_args *) arg0)->a_mode
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_open:return
/self->vnop_open_arg0/
{
    printf("proc <%s> name <%s> a_mode <0x%x> error <%d>",
        execname,
        stringof(((struct vnop_open_args *) self->vnop_open_arg0)->a_vp->v_name),
        ((struct vnop_open_args *) self->vnop_open_arg0)->a_mode,
        arg1
    );

	self->vnop_open_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_compound_open:entry
{
	/* Save vnop_compound_open_args arg */
	self->vnop_cmpd_open_arg0 = arg0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_compound_open:return
/self->vnop_cmpd_open_arg0/
{ 
    printf("proc <%s> par_name <%s> name <%s> a_fmode <0x%x> vap <%p> va_active <x%llx> va_mode <0x%x> error <%d> ",
    	   execname,
    	   stringof(((struct vnop_compound_open_args *) self->vnop_cmpd_open_arg0)->a_dvp->v_name),
    	   stringof(((struct vnop_compound_open_args *) self->vnop_cmpd_open_arg0)->a_cnp->cn_nameptr),
    	   ((struct vnop_compound_open_args *) self->vnop_cmpd_open_arg0)->a_fmode,
           ((struct vnop_compound_open_args *) self->vnop_cmpd_open_arg0)->a_vap,
           ((struct vnop_compound_open_args *) self->vnop_cmpd_open_arg0)->a_vap == NULL ? 0 : ((struct vnop_compound_open_args *) self->vnop_cmpd_open_arg0)->a_vap->va_active,
           ((struct vnop_compound_open_args *) self->vnop_cmpd_open_arg0)->a_vap == NULL ? 0 : ((struct vnop_compound_open_args *) self->vnop_cmpd_open_arg0)->a_vap->va_mode,
    	   arg1
           );
			
	self->vnop_cmpd_open_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_close:entry
{ 
    self->vnop_close_arg0 = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_close_args *) arg0)->a_vp->v_name)
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_close:return
/self->vnop_close_arg0/
{
    printf("proc <%s> name <%s> error <%d>",
        execname,
        stringof(((struct vnop_close_args *) self->vnop_close_arg0)->a_vp->v_name),
        arg1
    );

	self->vnop_close_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_create:entry
{ 
    self->vnop_create_arg0 = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_create_args *) arg0)->a_cnp->cn_nameptr)
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_create:return
/self->vnop_create_arg0/
{
    printf("proc <%s> name <%s> error <%d>",
        execname,
        stringof(((struct vnop_create_args *) self->vnop_create_arg0)->a_cnp->cn_nameptr),
        arg1
    );

	self->vnop_create_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_set_create_vap:entry
{
    printf("proc <%s> dir/file <%s> vap <%p> va_active <x%llx>",
            execname,
            stringof(((struct vnode *) arg2)->v_name),
            ((struct vnode_attr *) arg1),
            ((struct vnode_attr *) arg1)->va_active
    );
	//stack();
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_fsync:entry
{
	/* Save vnop_fsync_args arg */
	self->vnop_fsync_arg0 = arg0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_fsync:return
/self->vnop_fsync_arg0/
{
	printf("proc <%s> name <%s> error <%d> ",
		execname,
		stringof(((struct vnop_fsync_args *) self->vnop_fsync_arg0)->a_vp->v_name),
		arg1
	);

	self->vnop_fsync_arg0 = 0;

	//stack();
}
