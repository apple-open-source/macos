#!/usr/sbin/dtrace -s
/*#pragma D option flowindent*/

BEGIN
{
	printf("Watching smbfs_vnop_getattrlistbulkv and smbfs_vnop_readdir - ^C to quit ...\n");
}

/* 
 * Watch the two main dir enumerators 
 */
fbt:com.apple.filesystems.smbfs:smbfs_vnop_readdir:entry
{ 
	/* Save vnop_readdir_args arg */
	self->ap = arg0;
	
	printf("proc <%s> dir <%s> offset <%u>", 
	       execname, 
	       stringof(((struct vnop_readdir_args *) arg0)->a_vp->v_name), 
	       ((struct vnop_readdir_args *) arg0)->a_uio->uio_offset);

	/* Print out the user backtrace */
	ustack();
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_readdir:return
/self->ap/
{ 
	printf("proc <%s> dir <%s> offset <%u> error <%d> eof <%d> actual_cnt <%d> \n", 
           execname,
           stringof(((struct vnop_readdir_args *) self->ap)->a_vp->v_name),
           ((struct vnop_readdir_args *) self->ap)->a_uio->uio_offset,
           arg1,
           *(((struct vnop_readdir_args *) self->ap)->a_eofflag),
           *(((struct vnop_readdir_args *) self->ap)->a_numdirent));
			
	/* Clear ap value */
	self->ap = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattrlistbulk:entry
{ 
	/* Save vnop_getattrlistbulk_args arg */
	self->ap = arg0;

	printf("proc <%s> dir <%s> offset <%u>", 
	       execname, 
	       stringof(((struct vnop_getattrlistbulk_args *) arg0)->a_vp->v_name), 
	       ((struct vnop_getattrlistbulk_args *) arg0)->a_uio->uio_offset);

	/* Print out the user backtrace */
	ustack();
}

fbt:com.apple.filesystems.smbfs:smbfs_fetch_new_entries:entry
{ 
	/* Save args */
	self->dvp = arg1;
	self->offset = arg3;
	self->is_overflow = arg4;
	
	printf("proc <%s> dir <%s> offset <%u> is_overflow <%d>", 
	       execname, 
	       stringof(((struct vnode *) arg1)->v_name), 
	       arg3,
	       arg4);
}

fbt:com.apple.filesystems.smbfs:smbfs_fetch_new_entries:return
/self->dvp/
{ 
	printf("proc <%s> dir <%s> offset <%u> is_overflow <%d> error <%d> \n", 
			execname, 
			stringof(((struct vnode *) self->dvp)->v_name), 
			self->offset, 
			self->is_overflow, 
			arg1);

	/* Clear arg values */
	self->dvp = 0;
	self->offset = 0;
	self->is_overflow = 0;
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
