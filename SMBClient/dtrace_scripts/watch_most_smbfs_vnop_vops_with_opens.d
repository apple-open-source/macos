#!/usr/sbin/dtrace -s
#pragma D option flowindent

BEGIN
{
	printf("Watching most smbfs_vnop/vops + opens - ^C to quit ...\n");
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
    //ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_unmount:return
{
    printf("proc <%s> error <%d>", execname, arg1);
}

fbt:com.apple.filesystems.smbfs:smbfs_root:entry
{
    printf("proc <%s> ",
            execname
    );
    //ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_root:return
{
    printf("proc <%s> error <%d>", execname, arg1);
}

fbt:com.apple.filesystems.smbfs:smbfs_vfs_getattr:entry
{
    printf("proc <%s> ",
        execname
    );
    //ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_vfs_getattr:return
{
    printf("proc <%s> error <%d>", execname, arg1);
}

fbt:com.apple.filesystems.smbfs:smbfs_sync:entry
{
    printf("proc <%s> ",
        execname
    );
    //ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_sync:return
{
    printf("proc <%s> error <%d>", execname, arg1);
}

fbt:com.apple.filesystems.smbfs:smbfs_vget:entry
{
    self->smbfs_vop_vget_arg1 = arg1;
    printf("proc <%s> ino <%lld> ",
        execname,
        self->smbfs_vop_vget_arg1
    );
    //ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_vget:return
/self->smbfs_vop_vget_arg1/
{
    printf("proc <%s> ino <%lld> error <%d>", execname, self->smbfs_vop_vget_arg1, arg1);

	self->smbfs_vop_vget_arg1 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_sysctl:entry
{
    printf("proc <%s>  ",
        execname
    );
    //ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_sysctl:return
{
    printf("proc <%s> error <%d>", execname, arg1);
}

fbt:com.apple.filesystems.smbfs:smbfs_sync_callback:entry
{
    /* Save vnop_close_args arg */
    self->vop_sync_arg0 = arg0;
}

fbt:com.apple.filesystems.smbfs:smbfs_sync_callback:return
/self->vop_sync_arg0/
{
    printf("proc <%s> name <%s> <%d> ",
        execname,
        stringof(((vnode_t) self->vop_sync_arg0)->v_name),
        arg1
    );

	self->vop_sync_arg0 = 0;
}








fbt:com.apple.filesystems.smbfs:smbfs_vnop_close:entry
{ 
    self->vnop_close_arg0 = arg0;

    printf("proc <%s> name <%s> a_fflag <0x%x> ",
    	   execname,
    	   stringof(((struct vnop_close_args *) arg0)->a_vp->v_name),
    	   ((struct vnop_close_args *) arg0)->a_fflag
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

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattr:entry
{  		
    self->vnop_getattr_arg0 = arg0;

   printf("proc <%s> name <%s> va_active <x%llx>",
    	   execname,
    	   stringof(((struct vnop_getattr_args *) arg0)->a_vp->v_name),
    	   ((struct vnop_getattr_args *) arg0)->a_vap->va_active
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattr:return
/self->vnop_getattr_arg0/
{
    printf("proc <%s> name <%s> va_active <x%llx> error <%d>",
        execname,
        stringof(((struct vnop_getattr_args *) self->vnop_getattr_arg0)->a_vp->v_name),
        ((struct vnop_getattr_args *) self->vnop_getattr_arg0)->a_vap->va_active,
        arg1
    );

	self->vnop_getattr_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_lookup:entry
{ 
    self->vnop_lookup_arg0 = arg0;

    printf("proc <%s> name <%s> nameiop <0x%x>",
    	   execname,
    	   stringof(((struct vnop_lookup_args *) arg0)->a_cnp->cn_nameptr),
       	   ((struct componentname *) arg0)->cn_nameiop
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_lookup:return
/self->vnop_lookup_arg0/
{
    printf("proc <%s> name <%s> nameiop <0x%x> error <%d>",
        execname,
        stringof(((struct vnop_lookup_args *) self->vnop_lookup_arg0)->a_cnp->cn_nameptr),
       	((struct componentname *) self->vnop_lookup_arg0)->cn_nameiop,
        arg1
    );

	self->vnop_lookup_arg0 = 0;
}

fbt:mach_kernel:cache_lookup:entry
{
	self->cache_lookup_trace = ((vnode_t) arg0)->v_tag == 23 ? 1 : 0;

    self->cache_lookup_arg0 = arg0;
    self->cache_lookup_arg2 = arg2;

    //printf("proc <%s> parDir <%s> name <%s>",
    	//execname,
     	//stringof(((vnode_t) self->cache_lookup_arg0)->v_name),
       	//stringof(((struct componentname *) self->cache_lookup_arg2)->cn_nameptr)
    	//);
	//ustack(15);
}

fbt:mach_kernel:cache_lookup:return
/self->cache_lookup_trace/
{
    printf("proc <%s> parDir <%s> name <%s> error <%d>",
    	execname,
     	stringof(((vnode_t) self->cache_lookup_arg0)->v_name),
       	stringof(((struct componentname *) self->cache_lookup_arg2)->cn_nameptr),
       	arg1
    	);
	//ustack(15);
	
	/* Clear ap value */
	self->cache_lookup_trace = 0;
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

fbt:com.apple.filesystems.smbfs:smbfs_vnop_readdirattr:entry
{
    self->vnop_readdirattr_arg0 = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_readdirattr_args *) arg0)->a_vp->v_name)
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_readdirattr:return
/self->vnop_readdirattr_arg0/
{
    printf("proc <%s> name <%s> error <%d>",
        execname,
        stringof(((struct vnop_readdirattr_args *) self->vnop_readdirattr_arg0)->a_vp->v_name),
        arg1
    );

	self->vnop_readdirattr_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattrlistbulk:entry
{ 
	self->vnop_getattrlistbulk_arg0 = arg0;

    printf("proc <%s> name <%s> va_active <x%llx>",
    	   execname,
    	   stringof(((struct vnop_getattrlistbulk_args *) arg0)->a_vp->v_name),
     	   ((struct vnop_getattrlistbulk_args *) arg0)->a_vap->va_active
          );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattrlistbulk:return
/self->vnop_getattrlistbulk_arg0/
{ 
    printf("proc <%s> name <%s> va_supported <x%llx> act_count <%d> error <%d>",
        execname,
        stringof(((struct vnop_getattrlistbulk_args *) self->vnop_getattrlistbulk_arg0)->a_vp->v_name),
        ((struct vnop_getattrlistbulk_args *) self->vnop_getattrlistbulk_arg0)->a_vap->va_supported,
        *(((struct vnop_getattrlistbulk_args *) self->vnop_getattrlistbulk_arg0)->a_actualcount),
        arg1
    );

	self->vnop_getattrlistbulk_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_setattr:entry
{ 
    self->vnop_setattr_arg0 = arg0;

    printf("proc <%s> name <%s> va_active <x%llx> ",
    	   execname,
    	   stringof(((struct vnop_setattr_args *) arg0)->a_vp->v_name),
            ((struct vnop_setattr_args *) arg0)->a_vap->va_active
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_setattr:return
/self->vnop_setattr_arg0/
{
    printf("proc <%s> name <%s> va_active <x%llx> error <%d>",
        execname,
        stringof(((struct vnop_setattr_args *) self->vnop_setattr_arg0)->a_vp->v_name),
        ((struct vnop_setattr_args *) arg0)->a_vap->va_active,
       arg1
    );

	self->vnop_setattr_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getxattr:entry
{ 
    self->vnop_getattr_arg0 = arg0;

    printf("proc <%s> name <%s> xattr <%s>",
    	   execname,
    	   stringof(((struct vnop_getxattr_args *) arg0)->a_vp->v_name),
    	   stringof(((struct vnop_getxattr_args *) arg0)->a_name)
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getxattr:return
/self->vnop_getattr_arg0/
{
    printf("proc <%s> name <%s> xattr <%s> error <%d>",
        execname,
        stringof(((struct vnop_getxattr_args *) self->vnop_getattr_arg0)->a_vp->v_name),
        stringof(((struct vnop_getxattr_args *) self->vnop_getattr_arg0)->a_name),
        arg1
    );

	self->vnop_getattr_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_setxattr:entry
{ 
    self->vnop_setxattr_arg0 = arg0;

    printf("proc <%s> name <%s> xattr <%s>",
    	   execname,
    	   stringof(((struct vnop_setxattr_args *) arg0)->a_vp->v_name),
    	   stringof(((struct vnop_setxattr_args *) arg0)->a_name)
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_setxattr:return
/self->vnop_setxattr_arg0/
{
    printf("proc <%s> name <%s> xattr <%s> error <%d>",
    execname,
        stringof(((struct vnop_setxattr_args *) self->vnop_setxattr_arg0)->a_vp->v_name),
        stringof(((struct vnop_setxattr_args *) self->vnop_setxattr_arg0)->a_name),
        arg1
    );

	/* Clear ap value */
	self->vnop_setxattr_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_removexattr:entry
{ 
    self->vnop_removexattr_arg0 = arg0;

    printf("proc <%s> name <%s> xattr <%s>",
    	   execname,
    	   stringof(((struct vnop_removexattr_args *) arg0)->a_vp->v_name),
    	   stringof(((struct vnop_removexattr_args *) arg0)->a_name)
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_removexattr:return
/self->vnop_removexattr_arg0/
{
    printf("proc <%s> name <%s> xattr <%s> error <%d>",
        execname,
        stringof(((struct vnop_removexattr_args *) self->vnop_removexattr_arg0)->a_vp->v_name),
        stringof(((struct vnop_removexattr_args *) self->vnop_removexattr_arg0)->a_name),
        arg1
    );

	self->vnop_removexattr_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_listxattr:entry
{ 
    self->vnop_listxattr_arg0 = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_listxattr_args *) arg0)->a_vp->v_name)
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_listxattr:return
/self->vnop_listxattr_arg0/
{
    printf("proc <%s> name <%s> error <%d> ",
        execname,
        stringof(((struct vnop_listxattr_args *) self->vnop_listxattr_arg0)->a_vp->v_name),
        arg1
    );

	self->vnop_listxattr_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getnamedstream:entry
{ 
    self->vnop_getnamedstream_arg0 = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_getnamedstream_args *) arg0)->a_name)
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getnamedstream:return
/self->vnop_getnamedstream_arg0/
{
    printf("proc <%s> name <%s> error <%d> ",
        execname,
        stringof(((struct vnop_getnamedstream_args *) self->vnop_getnamedstream_arg0)->a_name),
        arg1
    );

	self->vnop_getnamedstream_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_makenamedstream:entry
{ 
    self->vnop_makenamedstream_arg0 = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_makenamedstream_args *) arg0)->a_name)
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_makenamedstream:return
/self->vnop_makenamedstream_arg0/
{
    printf("proc <%s> name <%s> error <%d> ",
        execname,
        stringof(((struct vnop_makenamedstream_args *) self->vnop_makenamedstream_arg0)->a_name),
        arg1
    );

	self->vnop_makenamedstream_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_removenamedstream:entry
{ 
    self->vnop_removenamedstream_arg0 = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_removenamedstream_args *) arg0)->a_name)
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_removenamedstream:return
/self->vnop_removenamedstream_arg0/
{
    printf("proc <%s> name <%s> error <%d> ",
        execname,
        stringof(((struct vnop_removenamedstream_args *) self->vnop_removenamedstream_arg0)->a_name),
        arg1
        );

	self->vnop_removenamedstream_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_access:entry
{ 
    self->vnop_access_arg0 = arg0;

	printf("proc <%s> name <%s> action <0x%x>",
		execname, 
		stringof(((struct vnop_access_args *) arg0)->a_vp->v_name),
		((struct vnop_access_args *) arg0)->a_action
	);
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_access:return
/self->vnop_access_arg0/
{
    printf("proc <%s> name <%s> action <0x%x> error <%d>",
        execname,
        stringof(((struct vnop_access_args *) self->vnop_access_arg0)->a_vp->v_name),
        ((struct vnop_access_args *) self->vnop_access_arg0)->a_action,
        arg1
    );

	self->vnop_access_arg0 = 0;
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

fbt:com.apple.filesystems.smbfs:smbfs_vnop_remove:entry
{
    self->vnop_remove_arg0 = arg0;

    printf("proc <%s> parent <%s> name <%s>",
        execname,
        stringof(((struct vnop_remove_args *) arg0)->a_dvp->v_name),
        stringof(((struct vnop_remove_args *) arg0)->a_vp->v_name)
    );
    //ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_remove:return
/self->vnop_remove_arg0/
{
    printf("proc <%s> parent <%s> name <%s> error <%d>",
        execname,
        stringof(((struct vnop_remove_args *) self->vnop_remove_arg0)->a_dvp->v_name),
        stringof(((struct vnop_remove_args *) self->vnop_remove_arg0)->a_vp->v_name),
        arg1
    );

	self->vnop_remove_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_rmdir:entry
{
    self->vnop_rmdir_arg0 = arg0;

    printf("proc <%s> parent <%s> name <%s>",
        execname,
        stringof(((struct vnop_rmdir_args *) arg0)->a_dvp->v_name),
        stringof(((struct vnop_rmdir_args *) arg0)->a_vp->v_name)
    );
    //ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_rmdir:return
/self->vnop_rmdir_arg0/
{
    printf("proc <%s> parent <%s> name <%s> error <%d>",
        execname,
        stringof(((struct vnop_rmdir_args *) self->vnop_rmdir_arg0)->a_dvp->v_name),
        stringof(((struct vnop_rmdir_args *) self->vnop_rmdir_arg0)->a_vp->v_name),
        arg1
    );

	self->vnop_rmdir_arg0 = 0;
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

fbt:com.apple.filesystems.smbfs:smbfs_vnop_mmap:entry
{
	/* Save vnop_mmap_args arg */
	self->vnop_mmap_arg0 = arg0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_mmap:return
/self->vnop_mmap_arg0/
{
	printf("proc <%s> name <%s> error <%d> ",
		execname,
		stringof(((struct vnop_mmap_args *) self->vnop_mmap_arg0)->a_vp->v_name),
		arg1
	);

	self->vnop_mmap_arg0 = 0;

	//stack();
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_mnomap:entry
{
	/* Save vnop_mnomap_args arg */
	self->vnop_mnomap_arg0 = arg0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_mnomap:return
/self->vnop_mnomap_arg0/
{
	printf("proc <%s> name <%s> error <%d> ",
		execname,
		stringof(((struct vnop_mnomap_args *) self->vnop_mnomap_arg0)->a_vp->v_name),
		arg1
	);

	self->vnop_mnomap_arg0 = 0;

	//stack();
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_mmap_check:entry
{
	/* Save vnop_mmap_check_args arg */
	self->vnop_mmap_check_arg0 = arg0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_mmap_check:return
/self->vnop_mmap_check_arg0/
{
	printf("proc <%s> name <%s> error <%d> ",
		execname,
		stringof(((struct vnop_mmap_check_args *) self->vnop_mmap_check_arg0)->a_vp->v_name),
		arg1
	);

	self->vnop_mmap_check_arg0 = 0;

	//stack();
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_strategy:entry
{
	/* Save vnop_strategy_args arg */
	self->vnop_strategy_arg0 = arg0;
	self->bp = ((struct vnop_strategy_args *) self->vnop_strategy_arg0)->a_bp;
	
	printf("proc <%s> name <%s> ",
		execname,
		stringof(((struct buf *) self->bp)->b_vp->v_name)
	);
	
	self->bp = 0;	
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_strategy:return
/self->vnop_strategy_arg0/
{	
	printf("proc <%s> error <%d> ",
		execname,
		arg1
	);

	self->vnop_strategy_arg0 = 0;

	//stack();
}


fbt:com.apple.filesystems.smbfs:smbfs_getattr:entry
{
    self->smbfs_getattr_name = stringof(((vnode_t) arg1)->v_name);
    self->smbfs_getattr_vap = (struct vnode_attr *) arg2;

    printf("proc <%s> name <%s> va_active <x%llx>",
        execname,
        self->smbfs_getattr_name,
        self->smbfs_getattr_vap == NULL ? 0 : self->smbfs_getattr_vap->va_active
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_getattr:return
{
    printf("proc <%s> name <%s> va_active <x%llx> error <%d>",
        execname,
        self->smbfs_getattr_name,
        self->smbfs_getattr_vap == NULL ? 0 : self->smbfs_getattr_vap->va_active,
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_update_cache:entry
{
    self->smbfs_update_cache_vp = (vnode_t) arg1;
    self->smbfs_update_cache_vap = (struct vnode_attr *) arg2;

    printf("proc <%s> name <%s> va_active <x%llx>",
        execname,
        stringof(self->smbfs_update_cache_vp->v_name),
        self->smbfs_update_cache_vap == NULL ? 0 : self->smbfs_update_cache_vap->va_active
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_update_cache:return
{
    printf("proc <%s> name <%s> va_active <x%llx> error <%d>",
        execname,
        stringof(self->smbfs_update_cache_vp->v_name),
        self->smbfs_update_cache_vap == NULL ? 0 : self->smbfs_update_cache_vap->va_active,
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_attr_cachelookup:entry
{
    self->smbfs_attr_cachelookup_vp = (vnode_t) arg1;
    self->smbfs_attr_cachelookup_vap = (struct vnode_attr *) arg2;
    self->smbfs_attr_cachelookup_useCacheData = arg4;

    printf("proc <%s> name <%s> va_active <x%llx> useCache <%d>",
        execname,
        stringof(self->smbfs_attr_cachelookup_vp->v_name),
        self->smbfs_attr_cachelookup_vap == NULL ? 0 : self->smbfs_attr_cachelookup_vap->va_active,
        self->smbfs_attr_cachelookup_useCacheData
        );
}

fbt:com.apple.filesystems.smbfs:smbfs_attr_cachelookup:return
{
    printf("proc <%s> name <%s> va_active <x%llx> useCache <%d> error <%d>",
        execname,
        stringof(self->smbfs_attr_cachelookup_vp->v_name),
        self->smbfs_attr_cachelookup_vap == NULL ? 0 : self->smbfs_attr_cachelookup_vap->va_active,
        self->smbfs_attr_cachelookup_useCacheData,
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_lookup:entry
{
    printf("proc <%s>",
        execname
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_lookup:return
{
    printf("proc <%s> error <%d>",
        execname,
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smb2fs_smb_qstreaminfo:entry
{ 
    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof((struct vnop_close_args *) arg3)
           );
	//stack(15); 
}

fbt:com.apple.filesystems.smbfs:smb2fs_smb_qstreaminfo:return
{ 
    printf("proc <%s> error <%d> ",
    	   execname,
    	   arg1
           );
	//stack(15); 
}

fbt:mach_kernel:open:entry
{
    printf("proc <%s> path <%s> flags <0x%x> mode <0x%x>",
    	execname,
    	copyinstr(((struct open_args *) arg1)->path),
    	((struct open_args *) arg1)->flags,
    	((struct open_args *) arg1)->mode
    	);
	//ustack(15);
}

fbt:mach_kernel:open_extended:entry
{
    printf("proc <%s> path <%s> flags <0x%x> mode <0x%x>",
    execname,
    	copyinstr(((struct open_extended_args *) arg1)->path),
    	((struct open_extended_args *) arg1)->flags,
    	((struct open_extended_args *) arg1)->mode
    	);
    //ustack(15);
}





