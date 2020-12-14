#!/usr/sbin/dtrace -s
#pragma D option flowindent

BEGIN
{
	printf("Watching most smbfs_vnop/vops - ^C to quit ...\n");
}

fbt:com.apple.filesystems.smbfs:smbfs_mount:entry
{
    printf("proc <%s> ",
            execname
    );
    ustack(15);
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
    ustack(15);
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
    ustack(15);
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
    ustack(15);
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
    ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_sync:return
{
    printf("proc <%s> error <%d>", execname, arg1);
}

fbt:com.apple.filesystems.smbfs:smbfs_vget:entry
{
    self->ap = arg1;
    printf("proc <%s> ino <%lld> ",
        execname,
        self->ap
    );
    ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_vget:return
/self->ap/
{
    printf("proc <%s> ino <%lld> error <%d>", execname, self->ap, arg1);
}

fbt:com.apple.filesystems.smbfs:smbfs_sysctl:entry
{
    printf("proc <%s>  ",
        execname
    );
    ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_sysctl:return
{
    printf("proc <%s> error <%d>", execname, arg1);
}

fbt:com.apple.filesystems.smbfs:smbfs_sync_callback:entry
{
    /* Save vnop_close_args arg */
    self->ap = arg0;
}

fbt:com.apple.filesystems.smbfs:smbfs_sync_callback:return
/self->ap/
{
    printf("proc <%s> name <%s> <%d> ",
        execname,
        stringof(((vnode_t) self->ap)->v_name),
        arg1
    );
}








fbt:com.apple.filesystems.smbfs:smbfs_vnop_close:entry
{ 
    self->ap = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_close_args *) arg0)->a_vp->v_name)
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_close:return
/self->ap/
{
    printf("proc <%s> name <%s> error <%d>",
        execname,
        stringof(((struct vnop_close_args *) self->ap)->a_vp->v_name),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_create:entry
{ 
    self->ap = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_create_args *) arg0)->a_cnp->cn_nameptr)
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_create:return
/self->ap/
{
    printf("proc <%s> name <%s> error <%d>",
        execname,
        stringof(((struct vnop_create_args *) self->ap)->a_cnp->cn_nameptr),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattr:entry
{  		
    self->ap = arg0;

   printf("proc <%s> name <%s> va_active <x%llx>",
    	   execname,
    	   stringof(((struct vnop_getattr_args *) arg0)->a_vp->v_name),
    	   ((struct vnop_getattr_args *) arg0)->a_vap->va_active
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattr:return
/self->ap/
{
    printf("proc <%s> name <%s> va_active <x%llx> error <%d>",
        execname,
        stringof(((struct vnop_getattr_args *) self->ap)->a_vp->v_name),
        ((struct vnop_getattr_args *) self->ap)->a_vap->va_active,
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_lookup:entry
{ 
    self->ap = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_lookup_args *) arg0)->a_cnp->cn_nameptr)
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_lookup:return
/self->ap/
{
    printf("proc <%s> name <%s> error <%d>",
        execname,
        stringof(((struct vnop_lookup_args *) self->ap)->a_cnp->cn_nameptr),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_open:entry
{ 
    self->ap = arg0;

    printf("proc <%s> name <%s> a_mode <0x%x>",
    	   execname,
    	   stringof(((struct vnop_open_args *) arg0)->a_vp->v_name),
     	   ((struct vnop_open_args *) arg0)->a_mode
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_open:return
/self->ap/
{
    printf("proc <%s> name <%s> a_mode <0x%x> error <%d>",
        execname,
        stringof(((struct vnop_open_args *) self->ap)->a_vp->v_name),
        ((struct vnop_open_args *) self->ap)->a_mode,
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_compound_open:entry
{
    self->ap = arg0;
    self->name = stringof(((struct vnop_compound_open_args *) arg0)->a_cnp->cn_nameptr);
    self->fmode = ((struct vnop_compound_open_args *) arg0)->a_fmode;
    self->va_active = ((struct vnop_compound_open_args *) arg0)->a_vap->va_active;
    self->va_mode = ((struct vnop_compound_open_args *) arg0)->a_vap->va_mode;

    printf("proc <%s> name <%s> fmode <x%x> va_active <0x%x> va_mode <0x%x>",
        execname,
        self->name,
        self->fmode,
        self->va_active,
        self->va_mode
    );
	ustack(15); 
    /* stack(15); */

}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_compound_open:return
/self->ap/
{
    /* Note that cn_nameptr has been freed by this time */
    printf("proc <%s> name <%s> fmode <x%x> va_active <0x%x> va_mode <0x%x> error <%d>",
        execname,
        self->name,
        self->fmode,
        self->va_active,
        self->va_mode,
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_readdir:entry
{
    self->ap = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_readdir_args *) arg0)->a_vp->v_name)
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_readdir:return
/self->ap/
{
    printf("proc <%s> name <%s> error <%d> ",
        execname,
        stringof(((struct vnop_readdir_args *) self->ap)->a_vp->v_name),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_readdirattr:entry
{
    self->ap = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_readdirattr_args *) arg0)->a_vp->v_name)
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_readdirattr:return
/self->ap/
{
    printf("proc <%s> name <%s> error <%d>",
        execname,
        stringof(((struct vnop_readdirattr_args *)  self->ap)->a_vp->v_name),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattrlistbulk:entry
{ 
	self->ap = arg0;

    printf("proc <%s> name <%s> va_active <x%llx>",
    	   execname,
    	   stringof(((struct vnop_getattrlistbulk_args *) arg0)->a_vp->v_name),
     	   ((struct vnop_getattrlistbulk_args *) arg0)->a_vap->va_active
          );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattrlistbulk:return
/self->ap/
{ 
    printf("proc <%s> name <%s> va_supported <x%llx> act_count <%d> error <%d>",
        execname,
        stringof(((struct vnop_getattrlistbulk_args *) self->ap)->a_vp->v_name),
        ((struct vnop_getattrlistbulk_args *) self->ap)->a_vap->va_supported,
        *(((struct vnop_getattrlistbulk_args *) self->ap)->a_actualcount),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_setattr:entry
{ 
    self->ap = arg0;

    printf("proc <%s> name <%s> va_active <x%llx> ",
    	   execname,
    	   stringof(((struct vnop_setattr_args *) arg0)->a_vp->v_name),
            ((struct vnop_setattr_args *) arg0)->a_vap->va_active
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_setattr:return
/self->ap/
{
    printf("proc <%s> name <%s> va_active <x%llx> error <%d>",
        execname,
        stringof(((struct vnop_setattr_args *) self->ap)->a_vp->v_name),
        ((struct vnop_setattr_args *) arg0)->a_vap->va_active,
       arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getxattr:entry
{ 
    self->ap = arg0;

    printf("proc <%s> name <%s> xattr <%s>",
    	   execname,
    	   stringof(((struct vnop_getxattr_args *) arg0)->a_vp->v_name),
    	   stringof(((struct vnop_getxattr_args *) arg0)->a_name)
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getxattr:return
/self->ap/
{
    printf("proc <%s> name <%s> xattr <%s> error <%d>",
        execname,
        stringof(((struct vnop_getxattr_args *) self->ap)->a_vp->v_name),
        stringof(((struct vnop_getxattr_args *) self->ap)->a_name),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_setxattr:entry
{ 
    self->ap = arg0;

    printf("proc <%s> name <%s> xattr <%s>",
    	   execname,
    	   stringof(((struct vnop_setxattr_args *) arg0)->a_vp->v_name),
    	   stringof(((struct vnop_setxattr_args *) arg0)->a_name)
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_setxattr:return
/self->ap/
{
    printf("proc <%s> name <%s> xattr <%s> error <%d>",
    execname,
        stringof(((struct vnop_setxattr_args *) self->ap)->a_vp->v_name),
        stringof(((struct vnop_setxattr_args *) self->ap)->a_name),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_removexattr:entry
{ 
    self->ap = arg0;

    printf("proc <%s> name <%s> xattr <%s>",
    	   execname,
    	   stringof(((struct vnop_removexattr_args *) arg0)->a_vp->v_name),
    	   stringof(((struct vnop_removexattr_args *) arg0)->a_name)
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_removexattr:return
/self->ap/
{
    printf("proc <%s> name <%s> xattr <%s> error <%d>",
        execname,
        stringof(((struct vnop_removexattr_args *) self->ap)->a_vp->v_name),
        stringof(((struct vnop_removexattr_args *) self->ap)->a_name),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_listxattr:entry
{ 
    self->ap = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_listxattr_args *) arg0)->a_vp->v_name)
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_listxattr:return
/self->ap/
{
    printf("proc <%s> name <%s> error <%d> ",
        execname,
        stringof(((struct vnop_listxattr_args *) self->ap)->a_vp->v_name),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getnamedstream:entry
{ 
    self->ap = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_getnamedstream_args *) arg0)->a_name)
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getnamedstream:return
/self->ap/
{
    printf("proc <%s> name <%s> error <%d> ",
        execname,
        stringof(((struct vnop_getnamedstream_args *) self->ap)->a_name),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_makenamedstream:entry
{ 
    self->ap = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_makenamedstream_args *) arg0)->a_name)
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_makenamedstream:return
/self->ap/
{
    printf("proc <%s> name <%s> error <%d> ",
        execname,
        stringof(((struct vnop_makenamedstream_args *) self->ap)->a_name),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_removenamedstream:entry
{ 
    self->ap = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_removenamedstream_args *) arg0)->a_name)
           );
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_removenamedstream:return
/self->ap/
{
    printf("proc <%s> name <%s> error <%d> ",
        execname,
        stringof(((struct vnop_removenamedstream_args *) self->ap)->a_name),
        arg1
        );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_access:entry
{ 
    self->ap = arg0;

	printf("proc <%s> name <%s> action <0x%x>",
		execname, 
		stringof(((struct vnop_access_args *) arg0)->a_vp->v_name),
		((struct vnop_access_args *) arg0)->a_action
	);
	ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_access:return
/self->ap/
{
    printf("proc <%s> name <%s> action <0x%x> error <%d>",
        execname,
        stringof(((struct vnop_access_args *) self->ap)->a_vp->v_name),
        ((struct vnop_access_args *) self->ap)->a_action,
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_rename:entry
{
    self->ap = arg0;

    printf("proc <%s> curr_name <%s> new_name <%s>",
        execname,
        stringof(((struct vnop_rename_args *) arg0)->a_fvp->v_name),
        stringof(((struct vnop_rename_args *) arg0)->a_tcnp->cn_nameptr)
    );
ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_rename:return
/self->ap/
{
    printf("proc <%s> curr_name <%s> new_name <%s> error <%d>",
        execname,
        stringof(((struct vnop_rename_args *) arg0)->a_fvp->v_name),
        stringof(((struct vnop_rename_args *) arg0)->a_tcnp->cn_nameptr),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_remove:entry
{
    self->ap = arg0;

    printf("proc <%s> parent <%s> name <%s>",
        execname,
        stringof(((struct vnop_remove_args *) self->ap)->a_dvp->v_name),
        stringof(((struct vnop_remove_args *) self->ap)->a_vp->v_name)
    );
    ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_remove:return
/self->ap/
{
    printf("proc <%s> parent <%s> name <%s> error <%d>",
        execname,
        stringof(((struct vnop_remove_args *) self->ap)->a_dvp->v_name),
        stringof(((struct vnop_remove_args *) self->ap)->a_vp->v_name),
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_rmdir:entry
{
    self->ap = arg0;

    printf("proc <%s> parent <%s> name <%s>",
        execname,
        stringof(((struct vnop_rmdir_args *) self->ap)->a_dvp->v_name),
        stringof(((struct vnop_rmdir_args *) self->ap)->a_vp->v_name)
    );
    ustack(15);
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_rmdir:return
/self->ap/
{
    printf("proc <%s> parent <%s> name <%s> error <%d>",
        execname,
        stringof(((struct vnop_rmdir_args *) self->ap)->a_dvp->v_name),
        stringof(((struct vnop_rmdir_args *) self->ap)->a_vp->v_name),
        arg1
    );
}





fbt:com.apple.filesystems.smbfs:smbfs_getattr:entry
{
    self->name = stringof(((vnode_t) arg1)->v_name);
    self->vap = (struct vnode_attr *) arg2;

    printf("proc <%s> name <%s> va_active <x%llx>",
        execname,
        self->name,
        self->vap->va_active
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_getattr:return
{
    printf("proc <%s> name <%s> va_active <x%llx> error <%d>",
        execname,
        self->name,
        self->vap->va_active,
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_update_cache:entry
{
    self->vp = (vnode_t) arg1;
    self->vap = (struct vnode_attr *) arg2;

    printf("proc <%s> name <%s> va_active <x%llx>",
        execname,
        stringof(self->vp->v_name),
        self->vap == NULL ? 0 : self->vap->va_active
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_update_cache:return
{
    printf("proc <%s> name <%s> va_active <x%llx> error <%d>",
        execname,
        stringof(self->vp->v_name),
        self->vap == NULL ? 0 : self->vap->va_active,
        arg1
    );
}

fbt:com.apple.filesystems.smbfs:smbfs_attr_cachelookup:entry
{
    self->vp = (vnode_t) arg1;
    self->vap = (struct vnode_attr *) arg2;
    self->useCacheData = arg4;

    printf("proc <%s> name <%s> va_active <x%llx> useCache <%d>",
        execname,
        stringof(self->vp->v_name),
        self->vap == NULL ? 0 : self->vap->va_active,
        self->useCacheData
        );
}

fbt:com.apple.filesystems.smbfs:smbfs_attr_cachelookup:return
{
    printf("proc <%s> name <%s> va_active <x%llx> useCache <%d> error <%d>",
        execname,
        stringof(self->vp->v_name),
        self->vap == NULL ? 0 : self->vap->va_active,
        self->useCacheData,
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

fbt:com.apple.filesystems.smbfs:smbfs_smb_qstreaminfo:entry
{ 
    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof((struct vnop_close_args *) arg3)
           );
	stack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_smb_qstreaminfo:return
{ 
    printf("proc <%s> error <%d> ",
    	   execname,
    	   arg1
           );
	stack(15); 
}

fbt:mach_kernel:open:entry
{
    printf("proc <%s> path <%s> flags <0x%x> mode <0x%x>",
    execname,
    copyinstr(((struct open_args *) arg1)->path),
    ((struct open_args *) arg1)->flags,
    ((struct open_args *) arg1)->mode
    );
ustack(15);
}

fbt:mach_kernel:open_extended:entry
{
    printf("proc <%s> path <%s> flags <0x%x> mode <0x%x>",
    execname,
    copyinstr(((struct open_extended_args *) arg1)->path),
    ((struct open_extended_args *) arg1)->flags,
    ((struct open_extended_args *) arg1)->mode
    );
    ustack(15);
}





