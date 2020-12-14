#!/usr/sbin/dtrace -s

BEGIN
{
	printf("Watching smbfs_vnop_xattrs - ^C to quit ...\n");
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_setxattr:entry
{ 
	/* Save vnop_setxattr_args arg */
	self->ap = arg0;

	printf("proc <%s> name <%s> xattr <%s> options <0x%x>",
		execname,
		stringof(((struct vnop_setxattr_args *) arg0)->a_vp->v_name),
		stringof(((struct vnop_setxattr_args *) arg0)->a_name),
		((struct vnop_setxattr_args *) arg0)->a_options);
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_setxattr:return
/self->ap/
{
	printf("proc <%s> name <%s> xattr <%s> error <%d> \n",
		execname,
		stringof(((struct vnop_setxattr_args *) self->ap)->a_vp->v_name),
		stringof(((struct vnop_setxattr_args *) self->ap)->a_name),
		arg1);

	/* Clear arg values */
	self->ap = 0;
}


fbt:com.apple.filesystems.smbfs:smbfs_vnop_listxattr:entry
{
	/* Save smbfs_vnop_listxattr arg */
	self->ap = arg0;

	printf("proc <%s> name <%s> size <%d>",
		execname,
		stringof(((struct vnop_listxattr_args *) arg0)->a_vp->v_name),
		*((struct vnop_listxattr_args *) arg0)->a_size);
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_listxattr:return
/self->ap/
{
	printf("proc <%s> name <%s> error <%d> \n",
		execname,
		stringof(((struct vnop_listxattr_args *) self->ap)->a_vp->v_name),
		arg1);

	/* Clear arg values */
	self->ap = 0;
}


fbt:com.apple.filesystems.smbfs:smbfs_vnop_removexattr:entry
{
	/* Save smbfs_vnop_removexattr arg */
	self->ap = arg0;

	printf("proc <%s> name <%s> xattr <%s> options <0x%x>",
		execname,
		stringof(((struct vnop_removexattr_args *) arg0)->a_vp->v_name),
		stringof(((struct vnop_removexattr_args *) arg0)->a_name),
		((struct vnop_removexattr_args *) arg0)->a_options);
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_removexattr:return
/self->ap/
{
	printf("proc <%s> name <%s> xattr <%s> error <%d> \n",
		execname,
		stringof(((struct vnop_removexattr_args *) self->ap)->a_vp->v_name),
		stringof(((struct vnop_removexattr_args *) self->ap)->a_name),
		arg1);

	/* Clear arg values */
	self->ap = 0;
}


fbt:com.apple.filesystems.smbfs:smbfs_vnop_getxattr:entry
{
	/* Save smbfs_vnop_getxattr arg */
	self->ap = arg0;

	printf("proc <%s> name <%s> xattr <%s> options <0x%x> size <%d>",
		execname,
		stringof(((struct vnop_getxattr_args *) arg0)->a_vp->v_name),
		stringof(((struct vnop_getxattr_args *) arg0)->a_name),
		((struct vnop_getxattr_args *) arg0)->a_options,
		*((struct vnop_getxattr_args *) arg0)->a_size);
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getxattr:return
/self->ap/
{
	printf("proc <%s> name <%s> xattr <%s> error <%d> \n",
		execname,
		stringof(((struct vnop_getxattr_args *) self->ap)->a_vp->v_name),
		stringof(((struct vnop_getxattr_args *) self->ap)->a_name),
		arg1);

	/* Clear arg values */
	self->ap = 0;
}
