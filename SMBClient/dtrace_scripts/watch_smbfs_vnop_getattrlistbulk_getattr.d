#!/usr/sbin/dtrace -s
#pragma D option flowindent

BEGIN
{
	printf("Watching smbfs_vnop_getattrlistbulk/vnop_getattr - ^C to quit ...\n");
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattrlistbulk:entry
{ 
	self->vnop_getattrlistbulk_arg0 = arg0;

    printf("proc <%s> name <%s> va_active <x%llx>",
    	   execname,
    	   stringof(((struct vnop_getattrlistbulk_args *) arg0)->a_vp->v_name),
     	   ((struct vnop_getattrlistbulk_args *) arg0)->a_vap->va_active
          );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattrlistbulk:return
/self->vnop_getattrlistbulk_arg0/
{ 
    printf("proc <%s> name <%s> va_supported <x%llx> act_count <%d> error <%d>",
        execname,
        stringof(((struct vnop_getattrlistbulk_args *) self->vnop_getattrlistbulk_arg0)->a_vp->v_name),
        ((struct vnop_getattrlistbulk_args *) self->vnop_getattrlistbulk_arg0)->a_vap->va_supported,
        *(((struct vnop_getattrlistbulk_args *) self->vnop_getattrlistbulk_arg0)->a_actualcount),
        arg1
    );

	self->vnop_getattrlistbulk_arg0 = 0;
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattr:entry
{  		
    self->vnop_getattr_arg0 = arg0;

   printf("proc <%s> name <%s> va_active <x%llx>",
    	   execname,
    	   stringof(((struct vnop_getattr_args *) arg0)->a_vp->v_name),
    	   ((struct vnop_getattr_args *) arg0)->a_vap->va_active
           );
	//ustack(15); 
}

fbt:com.apple.filesystems.smbfs:smbfs_vnop_getattr:return
/self->vnop_getattr_arg0/
{
    printf("proc <%s> name <%s> va_active <x%llx> error <%d>",
        execname,
        stringof(((struct vnop_getattr_args *) self->vnop_getattr_arg0)->a_vp->v_name),
        ((struct vnop_getattr_args *) self->vnop_getattr_arg0)->a_vap->va_active,
        arg1
    );

	self->vnop_getattr_arg0 = 0;
}
