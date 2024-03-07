#!/usr/sbin/dtrace -s
/*#pragma D option flowindent*/

BEGIN
{
	printf("Watching smbfs_vnop_allocate - ^C to quit ...\n");
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_allocate:entry
{ 
    self->vnop_allocate_arg0 = arg0;

    printf("proc <%s> name <%s> ",
    	   execname,
    	   stringof(((struct vnop_allocate_args *) arg0)->a_vp->v_name)
           );

	stack();

	ustack(15); 
}

