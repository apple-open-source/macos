#! /usr/sbin/dtrace -s

/* samba-file-io.d: report file I/O performed by samba.
 *
 * Copyright (C) 2006 Apple Computer Inc. All rights reserved.
 */

#pragma D option quiet

BEGIN
{
    progname = "smbd";
    printf("%5s %20s %20s %20s\n",
	    "FD", "Function", "IO Size (bytes)", "Elapsed nsec");
}

syscall::read:entry,
syscall::pread:entry,
syscall::read_nocancel:entry,
syscall::pread_nocancel:entry,
syscall::write:entry,
syscall::pwrite:entry,
syscall::write_nocancel:entry,
syscall::pwrite_nocancel:entry
/ execname == progname /
{
    self->fd = arg0;
    /* arg1 is the buffer pointer */
    self->size = arg2;
    /* arg3 is the offset for p* variants */

    self->stamp = timestamp;
}

syscall::readv:entry,
syscall::writev:entry,
syscall::readv_nocancel:entry,
syscall::writev_nocancel:entry
/ execname == progname /
{
    /* FIXME */
    trace(arg0);
}

syscall::aio_read:entry,
syscall::aio_write:entry
/ execname == progname /
{
    /* FIXME */
    trace(arg0);
}

syscall::sendfile:entry
/ execname == progname /
{
    self->fd = arg0;
    /* arg1 is the socket fd */
    /* arg2 is the file offset */
    self->size = *(int64_t *)copyin(arg3, 8);
    /* arg4 is the header/footer iovec */
    /* arg5 is the flags */

    self->stamp = timestamp;
}

syscall::read:return,
syscall::pread:return,
syscall::read_nocancel:return,
syscall::pread_nocancel:return,
syscall::write:return,
syscall::pwrite:return,
syscall::write_nocancel:return,
syscall::pwrite_nocancel:return,
syscall::sendfile:return
/ execname == progname /
{
    elapsed = timestamp - self->stamp;
    printf("%5d %20s %20u %20u\n",
	    self->fd, probefunc, self->size, elapsed);
}

