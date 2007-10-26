#! /usr/sbin/dtrace -C -s

/* Copyright (C) 2007 Apple Inc. All rights reserved.  */


/*
NB - this does't work right unless you trace :entry and :return.
#pragma D option flowindent
*/

                        
inline int CRF_NOMEMBERD = 0x00000001;
inline int KAUTH_UID_NONE = (~(uid_t)0 - 100);
inline int KAUTH_GID_NONE = (~(gid_t)0 - 100);


#define PRINT_GID_ELEMENT(count) \
fbt::initgroups:entry \
/ execname == progname && ((struct initgroups_args *)arg1)->gidsetsize > count / \
{ \
    this->initargs = (struct initgroups_args *)arg1; \
    this->gids = copyin(this->initargs->gidset, \
	    this->initargs->gidsetsize * sizeof(gid_t)); \
    printf("gidset[%d]=%d", count, ((gid_t *)this->gids)[count]); \
}

#define PRINT_CRED_INFO(cred) \
    printf("cr_uid=%d cr_groups[0]=%d cr_gmuid=%d%s cr_flags=%#x%s", \
	    (int)cred->cr_uid, \
	    (int)cred->cr_groups[0], \
	    (int)cred->cr_gmuid, \
	    (int)cred->cr_gmuid == KAUTH_GID_NONE ? " (KAUTH_GID_NONE)" : "", \
	    (int)cred->cr_flags, \
	    cred->cr_flags & CRF_NOMEMBERD ? "(CRF_NOMEMBERD)" : "")


BEGIN
{
    progname = "smbd";
}

syscall::setuid:entry,
syscall::seteuid:entry
/ execname == progname /
{
    printf("uid=%d", (int)arg0);
}

syscall::setgid:entry,
syscall::setegid:entry
/ execname == progname /
{
    printf("gid=%d", (int)arg0);
    ustack(2);
}

syscall::setreuid:entry,
syscall::setregid:entry
/ execname == progname /
{
    printf("real=%d effective=%d", (int)arg0, (int)arg1);
    ustack(2);
}

fbt::open_nocancel:entry
/ execname == progname /
{
    /* arg0 is proc_t, which gives us our credential. */
    self->proc = (struct proc *)arg0;
    self->cred = self->proc->p_ucred;
    self->path = copyinstr(((struct open_args *)arg1)->path);

    PRINT_CRED_INFO(self->cred);
}

fbt::seteuid:entry,
fbt::setegid:entry,
fbt::initgroups:entry
/ execname == progname /
{
    /* arg0 is proc_t, which gives us our credential. */
    self->proc = (struct proc *)arg0;
    self->cred = self->proc->p_ucred;

    PRINT_CRED_INFO(self->cred);
}

fbt::open_nocancel:return,
fbt::seteuid:return,
fbt::setegid:return,
fbt::initgroups:return
/ execname == progname /
{
    this->cred = self->proc->p_ucred;

    printf("process credential %s\n",
	    this->cred == self->cred ? "unchanged" : "CHANGED");
    PRINT_CRED_INFO(this->cred);

    self->cred = 0;
    self->proc = 0;
}

syscall::open:return
/ execname == progname && self->path != 0 /
{
    printf("%s: result=%d errno=%d", self->path, errno, arg0);
    self->path = 0;
}

fbt::initgroups:entry
/ execname == progname /
{
    this->args = (struct initgroups_args *)arg1;
    printf("gidsetsize=%u gidset=%#x gmuid=%d",
	    this->args->gidsetsize,
	    this->args->gidset,
	    this->args->gmuid);
}

PRINT_GID_ELEMENT(0)
PRINT_GID_ELEMENT(1)
PRINT_GID_ELEMENT(2)
PRINT_GID_ELEMENT(3)
PRINT_GID_ELEMENT(4)
PRINT_GID_ELEMENT(5)
PRINT_GID_ELEMENT(6)
PRINT_GID_ELEMENT(7)
PRINT_GID_ELEMENT(8)
PRINT_GID_ELEMENT(9)
PRINT_GID_ELEMENT(10)
PRINT_GID_ELEMENT(11)
PRINT_GID_ELEMENT(12)
PRINT_GID_ELEMENT(13)
PRINT_GID_ELEMENT(14)
PRINT_GID_ELEMENT(15)
PRINT_GID_ELEMENT(16)
PRINT_GID_ELEMENT(17)
PRINT_GID_ELEMENT(18)
PRINT_GID_ELEMENT(19)
PRINT_GID_ELEMENT(20)

fbt::kauth_cred_ismember_gid:entry
/ execname == progname /
{
    /* Return value is struct vfs_context */
    this->cred = (struct ucred *)arg0;
    PRINT_CRED_INFO(this->cred);
}

syscall::settid:entry
/ execname == progname /
{
    printf("uid=%d gid=%d", (int)arg0, (int)arg1);
}

syscall::initgroups:entry,
syscall::setgroups:entry
/ execname == progname /
{
    printf("ngroups=%d group=%x gmuid=%d",
	    (int)arg0, (int)arg1, (int)arg2);
}

