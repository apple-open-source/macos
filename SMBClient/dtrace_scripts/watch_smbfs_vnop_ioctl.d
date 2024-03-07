#!/usr/sbin/dtrace -s
#pragma D option flowindent

BEGIN
{
	printf("Watching smbfs_vnop_ioctl - ^C to quit ...\n");
}

fbt:mach_kernel:fsctl:entry
{ 
    self->vnop_ioctl_arg0 = arg1;

    printf("proc <%s> path <%s> ",
    	   execname,
    	   copyinstr(((struct fsctl_args *) arg1)->path)
           );
	//ustack(15); 
}

fbt:mach_kernel:fsctl:return
/self->vnop_ioctl_arg0/
{
    printf("proc <%s> path <%s> error <%d>",
        execname,
        copyinstr(((struct fsctl_args *) self->vnop_ioctl_arg0)->path),
        arg1
    );

	self->vnop_ioctl_arg0 = 0;
}

fbt:mach_kernel:mac_mount_check_fsctl:entry
{ 

    printf("proc <%s> ",
    	   execname
           );
	//ustack(15); 
}

fbt:mach_kernel:mac_mount_check_fsctl:return
{
    printf("proc <%s> error <%d>",
        execname,
        arg1
    );
}

fbt:mach_kernel:namei:entry
{ 

    printf("proc <%s> ",
    	   execname
           );
	//ustack(15); 
}

fbt:mach_kernel:namei:return
{
    printf("proc <%s> error <%d>",
        execname,
        arg1
    );
}

fbt:mach_kernel:mac_vnode_check_lookup_preflight:entry
{ 

    printf("proc <%s> ",
    	   execname
           );
	//ustack(15); 
}

fbt:mach_kernel:mac_vnode_check_lookup_preflight:return
{
    printf("proc <%s> error <%d>",
        execname,
        arg1
    );

}
