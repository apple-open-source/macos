#!/usr/sbin/dtrace -s
#pragma D option flowindent

BEGIN
{
	printf("Watching all smbfs_vnop/vops - ^C to quit ...\n");
}

fbt:mach_kernel:unmount:entry
{ 
   printf("proc <%s> ",
            execname
    );
    
    /* Print out the kernel backtrace */
    stack();
}

fbt:mach_kernel:unmount:return
{ 
    printf("proc <%s> error <%d>", execname, arg1);

    /* Print out the kernel backtrace */
    stack();
}


fbt:mach_kernel:mac_mount_check_umount:entry
{ 
   printf("proc <%s> ",
            execname
    );

    /* Print out the kernel backtrace */
    stack();
}

fbt:mach_kernel:mac_mount_check_umount:return
{ 
    printf("proc <%s> error <%d>", execname, arg1);

    /* Print out the kernel backtrace */
    stack();
}


fbt:mach_kernel:safedounmount:entry
{ 
   printf("proc <%s> ",
            execname
    );

    /* Print out the kernel backtrace */
    stack();
}

fbt:mach_kernel:safedounmount:return
{ 
    printf("proc <%s> error <%d>", execname, arg1);

    /* Print out the kernel backtrace */
    stack();
}


fbt:mach_kernel:dounmount:entry
{ 
   printf("proc <%s> ",
            execname
    );

    /* Print out the kernel backtrace */
    stack();
}

fbt:mach_kernel:dounmount:return
{ 
    printf("proc <%s> error <%d>", execname, arg1);

    /* Print out the kernel backtrace */
    stack();
}


fbt:mach_kernel:vflush:entry
{ 
   printf("proc <%s> ",
            execname
    );

    /* Print out the kernel backtrace */
    stack();
}

fbt:mach_kernel:vflush:return
{ 
    printf("proc <%s> error <%d>", execname, arg1);

    /* Print out the kernel backtrace */
    stack();
}







fbt:mach_kernel:VFS_SYNC:entry
{ 
   printf("proc <%s> ",
            execname
    );

    /* Print out the kernel backtrace */
    stack();
}



fbt:mach_kernel:VFS_UNMOUNT:entry
{ 
   printf("proc <%s> ",
            execname
    );

    /* Print out the kernel backtrace */
    stack();
}

