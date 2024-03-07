#!/usr/sbin/dtrace -s
/*#pragma D option flowindent*/

BEGIN
{
	printf("Watching smbfs_vnop_getattrlistbulk and smbfs_vnop_readdir - ^C to quit ...\n");
}

/* 
 * Watch the two main dir enumerators 
 */
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

fbt:com.apple.filesystems.smbfs:smbfs_fetch_new_entries:entry
{ 
	/* Save args */
	self->smbfs_fetch_new_entries_dvp = arg1;
	self->smbfs_fetch_new_entries_dvpoffset = arg3;
	self->smbfs_fetch_new_entries_dvpis_overflow = arg4;
	
	printf("proc <%s> dir <%s> offset <%u> is_overflow <%d>", 
	       execname, 
	       stringof(((struct vnode *) arg1)->v_name), 
	       arg3,
	       arg4);
}

fbt:com.apple.filesystems.smbfs:smbfs_fetch_new_entries:return
/self->smbfs_fetch_new_entries_dvpdvp/
{ 
	printf("proc <%s> dir <%s> offset <%u> is_overflow <%d> error <%d> \n", 
			execname, 
			stringof(((struct vnode *) self->smbfs_fetch_new_entries_dvpdvp)->v_name), 
			self->smbfs_fetch_new_entries_dvpoffset, 
			self->smbfs_fetch_new_entries_dvpis_overflow, 
			arg1);

	/* Clear arg values */
	self->smbfs_fetch_new_entries_dvpdvp = 0;
	self->smbfs_fetch_new_entries_dvpoffset = 0;
	self->smbfs_fetch_new_entries_dvpis_overflow = 0;
}

/* 
 * Dir enumeration caching significant events 
 */
fbt:com.apple.filesystems.smbfs:smb_dir_cache_remove:entry
{ 
	printf("proc <%s> dir <%s> cache <%s> reason <%s>", 
	       execname, 
	       stringof(((struct vnode *) arg0)->v_name), 
	       stringof(arg2), 
	       stringof(arg3));
	       
	/* Print out the kernel backtrace */
	stack(); 
}

fbt:com.apple.filesystems.smbfs:smbfs_closedirlookup:entry
{ 
	printf("proc <%s> reason <%s>", execname, stringof(arg1));

	/* Print out the kernel backtrace */
	stack(); 
}

fbt:com.apple.filesystems.smbfs:smbfs_handle_lease_break:entry
{ 
	printf("proc <%s> dir <%s> curr_lease_state <0x%x> new_lease_state <0x%x> \n", 
	       execname, 
	       stringof(((struct vnode *) arg1)->v_name), 
	       arg7, 
	       arg8);
}

/* 
 * Change Notify functions (aka Finder Open Window Notifies) 
 */
fbt:com.apple.filesystems.smbfs:smbfs_start_change_notify:entry
{ 
	printf("Monitoring - dir <%s> \n", 
	       stringof(((struct vnode *) arg1)->v_name));
}

/* 
 * kQueue monitoring of dirs that are open in Finder 
 */
fbt:com.apple.filesystems.smbfs:smbfs_notified_vnode:entry
{ 
	printf("Update Event - dir <%s> \n", 
	       stringof(((struct vnode *) arg0)->v_name));
}


fbt:com.apple.filesystems.smbfs:smbfs_stop_change_notify:entry
{ 
	printf("End Monitoring - dir <%s> \n", 
           stringof(((struct vnode *) arg1)->v_name));
}

/* 
 * Global dir caching events 
 */
fbt:com.apple.filesystems.smbfs:smb_global_dir_cache_add_entry:entry
{ 
	printf("proc <%s> dir <%s>", 
	       execname, 
	       stringof(((struct vnode *) arg0)->v_name));
}

fbt:com.apple.filesystems.smbfs:smb_global_dir_cache_low_memory:entry
{ 
	printf("Low Memory Event - proc <%s> free_all <%d> \n", 
	       execname, 
	       arg0);
}

fbt:com.apple.filesystems.smbfs:smb_global_dir_cache_prune:entry
{ 
	printf("Low Memory Event - proc <%s> free_all <%d> \n", 
	       execname, 
	       arg0);
}

fbt:com.apple.filesystems.smbfs:smb_global_dir_cache_remove_one:entry
{ 
	printf("proc <%s> dir <%s>", 
	       execname, 
	       stringof(((struct vnode *) arg0)->v_name));
}
