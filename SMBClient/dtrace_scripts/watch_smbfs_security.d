#!/usr/sbin/dtrace -s

fbt:com.apple.filesystems.smbfs:smbfs_setsecurity:entry
{ 
        printf("proc <%s> name <%s> va_active <0x%x> va_acl <%p> ", 
               execname, 
               stringof(((struct vnode *) arg1)->v_name),                           
               ((struct vnode_attr *) arg2)->va_active,
               ((struct vnode_attr *) arg2)->va_acl
              );

        /* Print out the user backtrace */
        ustack();
}
