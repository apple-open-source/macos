#!/usr/sbin/dtrace -s
/*#pragma D option flowindent*/

BEGIN
{
	printf("Watching smb2fs_smb_cmpd_query_dir_one - ^C to quit ...\n");
}

fbt:com.apple.filesystems.smbfs:smb2fs_smb_cmpd_query_dir_one:entry
{ 
	printf("proc <%s>", 
	       execname);

	/* Print out the kernel backtrace */
	stack(); 

	/* Print out the user stack */    
	ustack(15);
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

fbt:com.apple.filesystems.smbfs:smbfs_lookup:entry
{
    printf("proc <%s>",
        execname
    );
    
	/* Print out the kernel backtrace */
	stack(); 
}

fbt:com.apple.filesystems.smbfs:smbfs_smb_query_info:entry
{
    printf("proc <%s>",
        execname
    );
    
	/* Print out the kernel backtrace */
	stack(); 
}
