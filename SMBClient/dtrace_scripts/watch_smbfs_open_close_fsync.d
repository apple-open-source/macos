#!/usr/sbin/dtrace -s

fbt:com.apple.filesystems.smbfs:smbfs_vnop_open:entry
{
	/* Save vnop_open_args arg */
	self->ap = arg0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_open:return
/self->ap/
{ 
    printf("proc <%s> name <%s> a_mode <0x%x> error <%d> ",
    	   execname,
    	   stringof(((struct vnop_open_args *) self->ap)->a_vp->v_name),
    	   ((struct vnop_open_args *) self->ap)->a_mode,
    	   arg1
           );
			
	/* Clear ap value */
	self->ap = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_close:entry
{
	/* Save vnop_close_args arg */
	self->ap = arg0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_close:return
/self->ap/
{ 
    printf("proc <%s> name <%s> a_fflag <0x%x> error <%d> ",
    	   execname,
    	   stringof(((struct vnop_close_args *) self->ap)->a_vp->v_name),
    	   ((struct vnop_close_args *) self->ap)->a_fflag,
    	   arg1
           );
			
	/* Clear ap value */
	self->ap = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_create:entry
{
	/* Save vnop_create_args arg */
	self->ap = arg0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_create:return
/self->ap/
{ 
    printf("proc <%s> par_name <%s> name <%s> error <%d> ",
    	   execname,
    	   stringof(((struct vnop_create_args *) self->ap)->a_dvp->v_name),
    	   stringof(((struct vnop_create_args *) self->ap)->a_cnp->cn_nameptr),
    	   arg1
           );
			
	/* Clear ap value */
	self->ap = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_compound_open:entry
{
	/* Save vnop_create_args arg */
	self->ap = arg0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_compound_open:return
/self->ap/
{ 
    printf("proc <%s> par_name <%s> name <%s> a_fmode <0x%x> vap <%p> va_active <x%llx> error <%d> ",
    	   execname,
    	   stringof(((struct vnop_compound_open_args *) self->ap)->a_dvp->v_name),
    	   stringof(((struct vnop_compound_open_args *) self->ap)->a_cnp->cn_nameptr),
    	   ((struct vnop_compound_open_args *) self->ap)->a_fmode,
           ((struct vnop_compound_open_args *) self->ap)->a_vap,
           ((struct vnop_compound_open_args *) self->ap)->a_vap->va_active,
    	   arg1
           );
			
	/* Clear ap value */
	self->ap = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_set_create_vap:entry
{
    printf("proc <%s> dir/file <%s> vap <%p> va_active <x%llx>",
            execname,
            stringof(((struct vnode *) arg2)->v_name),
            ((struct vnode_attr *) arg1),
            ((struct vnode_attr *) arg1)->va_active
    );
stack();
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_fsync:entry
{
/* Save vnop_fsync_args arg */
self->ap = arg0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_fsync:return
/self->ap/
{
printf("proc <%s> name <%s> error <%d> ",
execname,
stringof(((struct vnop_fsync_args *) self->ap)->a_vp->v_name),
arg1
);

/* Clear ap value */
self->ap = 0;

stack();
}
