#!/usr/sbin/dtrace -s

fbt:com.apple.filesystems.smbfs:smb2_smb_tree_connect:entry
{
    printf("proc <%s> server_name <%s> ",
    	   execname,
    	   stringof(arg2)
           );
			
	stack(15);
			
	ustack(15); 

}

fbt:com.apple.filesystems.smbfs:smb2fs_smb_query_network_interface_info:entry
{ 
    printf("proc <%s> ",
    	   execname
           );
			
	stack(15);
			
	ustack(15); 
}





fbt:com.apple.filesystems.smbfs:smb_smb_treedisconnect:entry
{ 
    printf("proc <%s> ",
    	   execname
           );
			
	stack(15);
			
	ustack(15); 
}

