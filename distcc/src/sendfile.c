/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003 by Martin Pool <mbp@samba.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


                        /* "I've always wanted to use sendfile(), but
                         * never had a reason until now"
                         *              -- mbp                        */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include <sys/stat.h>
#ifdef HAVE_SYS_SENDFILE_H
#  include <sys/sendfile.h>
#endif /* !HAVE_SYS_SENDFILE_H */
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "util.h"
#include "exitcode.h"


#ifdef HAVE_SENDFILE
/* If you don't have it, just use dcc_pump_readwrite */

/**
 * Map FreeBSD and Linux into something like the Linux interface.
 *
 * Our sockets are never non-blocking, so that seems to me to say that
 * the kernel will never return EAGAIN -- we will always either send
 * the whole thing or get an error.  Is that really true?
 *
 * How nice to have the function parameters reversed between platforms
 * in a way that will not give a compiler warning.
 *
 * @param offset offset in input to start writing; updated on return
 * to reflect the number of bytes sent.
 *
 * @return number of bytes sent; -1 otherwise, with errno set.  (As
 * for Linux)
 *
 * @sa http://www.freebsd.org/cgi/man.cgi?query=sendfile&sektion=2&apropos=0&manpath=FreeBSD+5.0-current
 **/


#if defined(__FreeBSD__)
static ssize_t sys_sendfile(int ofd, int ifd, off_t *offset, size_t size)
{
    off_t sent_bytes;
    int ret;
    
    /* According to the manual, this can never partially complete on a
     * socket open for blocking IO. */
    ret = sendfile(ifd, ofd, *offset, size, 0, &sent_bytes, 0);
    if (ret == -1) {
        /* http://cvs.apache.org/viewcvs.cgi/apr/network_io/unix/sendrecv.c?rev=1.95&content-type=text/vnd.viewcvs-markup */
        if (errno == EAGAIN) {
            *offset += sent_bytes;
            return sent_bytes;
        } else {
            return -1;
        }
    } else if (ret == 0) {
        *offset += size;
        return size;
    } else {
        rs_log_error("don't know how to handle return %d from BSD sendfile",
                     ret);
        return -1;
    }
}
#elif defined(linux)
static ssize_t sys_sendfile(int ofd, int ifd, off_t *offset, size_t size)
{
    return sendfile(ofd, ifd, offset, size);
}
#elif defined(__hpux) || defined(__hpux__)
/* HP cc in ANSI mode defines __hpux; gcc defines __hpux__ */
static ssize_t sys_sendfile(int ofd, int ifd, off_t *offset, size_t size)
{
    ssize_t ret;
    
    ret = sendfile(ofd, ifd, *offset, size, NULL, 0);
    if (ret == -1) {
        return -1;
    } else if (ret > 0) {
        *offset += ret;
        return ret;
    } else {
        rs_log_error("don't know how to handle return %ld from HP-UX sendfile",
                     (long) ret);
        return -1;
    }
}
#else
#warning "Please write a sendfile implementation for this system"
static ssize_t sys_sendfile(int ofd, int ifd, off_t *offset, size_t size)
{
    rs_log_warning("no sendfile implementation on this platform");
    errno = ENOSYS;
    return -1;
}
#endif /* !(__FreeBSD__) && !def(linux) */


/**
 * Transmit the body of a file using sendfile().
 *
 * If the sendfile() call fails in a way that makes us think that
 * regular IO might work, then we try that instead.  For example, the
 * /tmp filesystem may not support sendfile().
 *
 * @param ofd Output fd
 * @param ifd Input file (must allow mmap)
 **/
int dcc_pump_sendfile(int ofd, int ifd, size_t size)
{
    ssize_t sent;
    off_t offset = 0;

    while (size) {
        /* Handle possibility of partial transmission, e.g. if
         * sendfile() is interrupted by a signal.  size is decremented
         * as we go. */

        sent = sys_sendfile(ofd, ifd, &offset, size);
        if (sent == -1) {
            if ((errno == ENOSYS || errno == EINVAL) && offset == 0) {
                /* The offset==0 tests is because we may be part way through
                 * the file.  We can't just naively go back to read/write
                 * because sendfile() does not update the file pointer: we
                 * would need to lseek() first.  That case is not handled at
                 * the moment because it's unlikely that sendfile() would
                 * suddenly be unsupported while we're using it.  A failure
                 * halfway through probably indicates a genuine error.*/

                rs_log_info("decided to use read/write rather than sendfile");
                return dcc_pump_readwrite(ofd, ifd, size);
            } else {
                rs_log_error("sendfile failed: %s", strerror(errno));
                return -1;
            }
        } else if (sent == 0) {
            rs_log_error("sendfile returned 0? can't cope");
            return -1;
        } else if (sent != (ssize_t) size) {
            /* offset is automatically updated by sendfile. */
            size -= sent;
            rs_log_notice("sendfile: partial transmission of %ld bytes; retrying %ld @%ld",
                          (long) sent, (long) size, (long) offset);
        } else {
            /* normal case, everything was sent. */
            break;
        }
    }
    return 0;
}
#endif /* def HAVE_SENDFILE */

