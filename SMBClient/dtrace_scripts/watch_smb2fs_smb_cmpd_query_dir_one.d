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
}

fbt:com.apple.filesystems.smbfs:smb2fs_smb_cmpd_query_dir_one:return
{ 
	printf("proc <%s> error <%d>", 
	       execname, arg1);
}


fbt:com.apple.filesystems.smbfs:smbfs_vnop_lookup:entry
{ 
	printf("proc <%s> cnp name <%s>", 
	       execname,
	       stringof(((struct vnop_lookup_args *) arg0)->a_cnp->cn_nameptr));
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_lookup:return
{ 
	printf("proc <%s> error <%d>", 
	       execname,
	       arg1);
}

