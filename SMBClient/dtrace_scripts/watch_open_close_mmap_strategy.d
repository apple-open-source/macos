#!/usr/sbin/dtrace -s
#pragma D option flowindent

BEGIN
{
	printf("Watching open, close, mmap, mnomap, mmap_check, strategy - ^C to quit ...\n");
}

fbt:com.apple.filesystems.smbfs:smbfs_mount:entry
{
    printf("proc <%s> ",
            execname
    );
    //ustack(15);
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



