#!/usr/sbin/dtrace -s

BEGIN
{
	printf("Watching smbfs_vnop_xattrs - ^C to quit ...\n");
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
