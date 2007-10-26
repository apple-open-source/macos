#! /usr/sbin/dtrace -s

/* Copyright (C) 2007 Apple Inc. All rights reserved.  */

BEGIN
{
    progname = "smbd";
}

syscall::chflags:entry
/ execname == progname /
{
    self->trace = 1;
    self->path = copyinstr(arg0);
    self->flags = *(u_int*)copyin(arg1, sizeof(u_int));
}

syscall::chflags:return
/ self->trace == 1 /
{
    printf("chflags %s %d => %d %d\n",
	    self->path, self->flags, (int)arg0, errno);
    self->trace = 0;
    self->flags = 0;
    self->path = 0;
}

