/* $Xorg: lbx_zlib_io.c,v 1.3 2000/08/17 19:46:41 cpqbld Exp $ */

/*
 * Copyright 1993 Network Computing Devices
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of NCD. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  NCD. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NCD. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NCD.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Dale Tonogai, Network Computing Devices
 */
/* $XFree86: xc/lib/lbxutil/lbx_zlib/lbx_zlib_io.c,v 1.12 2001/07/25 15:04:57 dawes Exp $ */

#ifdef WIN32
#define _WILLWINSOCK_
#endif
#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <errno.h>
#if !defined(WIN32) && !defined(Lynx)
#include <sys/param.h>
#endif
#include "lbxbufstr.h"
#include "lbx_zlib.h"
#include "os.h"

#include <stddef.h>


/*
 * The following is taken from the xtrans code, almost as is,
 * it would be nice to share it...
 */
#if defined(WIN32) || defined(__sxg__) || (defined(SCO) && !defined(SVR4) && !defined(SCO325))
static int
writev(int fildes, const struct iovec *iov, int iovcnt)
{
    int i, len, total;
    char *base;

    ESET(0);
    for (i = 0, total = 0;  i < iovcnt;  i++, iov++) {
	len = iov->iov_len;
	base = iov->iov_base;
	while (len > 0) {
	    register int nbytes;
	    nbytes = write(fildes, base, len);
	    if (nbytes < 0 && total == 0)  return -1;
	    if (nbytes <= 0)  return total;
	    ESET(0);
	    len   -= nbytes;
	    total += nbytes;
	    base  += nbytes;
	}
    }
    return total;
}
#endif

int
InitZlibBuffer(b, size)
    ZlibBufferPtr b;
    int size;
{
    if ((b->bufbase = (char *)Xalloc(size)) == NULL)
	return -1;
    b->bufend = b->bufbase + size;
    b->bufptr = b->bufbase;
    b->bufcnt = 0;
    return 0;
}

void
FreeZlibBuffer(b)
    ZlibBufferPtr b;
{
    if (b->bufbase) {
	Xfree(b->bufbase);
	b->bufbase = NULL;
    }
}

/*
 * Returns:
 *	1 if desired amount of data available in input buffer
 *	0 if eof
 *     -1 if error
 */
int
GetInputPtr(fd, inbuf, reqlen, ppkt)
    int		 fd;
    ZlibBufferPtr inbuf;
    int		 reqlen;
    unsigned char **ppkt;
{
    int		 readbytes;
    int		 gotbytes;

    if (inbuf->bufcnt == 0)
	inbuf->bufptr = inbuf->bufbase;

    if (reqlen <= inbuf->bufcnt) {
	*ppkt = (unsigned char *)inbuf->bufptr;
	return 1;
    }

    if (reqlen > inbuf->bufend - inbuf->bufptr) {
	memmove(inbuf->bufbase, inbuf->bufptr, inbuf->bufcnt);
	inbuf->bufptr = inbuf->bufbase;
    }
    readbytes = (inbuf->bufend - inbuf->bufptr) - inbuf->bufcnt;
    gotbytes = read(fd, inbuf->bufptr + inbuf->bufcnt, readbytes);
    if (gotbytes > 0) {
	if (reqlen <= (inbuf->bufcnt += gotbytes)) {
	    *ppkt = (unsigned char *)inbuf->bufptr;
	    return 1;
	}
    }
    else
	return gotbytes;

    errno = EWOULDBLOCK;
    return -1;
}

/*
 * When ZLIB is started, we may well have read some data off of the
 * wire somewhere.  This sticks those bytes ahead of anything we might
 * read in the future
 */

int
StuffInput(inbuf, pkt, reqlen)
    ZlibBufferPtr inbuf;
    unsigned char *pkt;
    int		 reqlen;
{
    int		 readbytes;
    char	 *last;
    
    last = inbuf->bufptr + inbuf->bufcnt;
    if (reqlen > inbuf->bufend - last)
    {
	memmove(inbuf->bufbase, inbuf->bufptr, inbuf->bufcnt);
	inbuf->bufptr = inbuf->bufbase;
	last = inbuf->bufptr + inbuf->bufcnt;
    }
    readbytes = MIN(reqlen, inbuf->bufend - last);
    memmove(last, pkt, readbytes);
    inbuf->bufcnt += readbytes;
    return readbytes;
}

void
FreeInput(inbuf, len)
    ZlibBufferPtr inbuf;
    int		 len;
{
    inbuf->bufptr += len;
    if ((inbuf->bufcnt -= len) == 0)
        inbuf->bufptr = inbuf->bufbase;
}

/*
 * Reserve outlen bytes in the output buffer.
 */
char *
ReserveOutBuf(outbuf, outlen)
    ZlibBufferPtr outbuf;
    int		 outlen;
{
    int		 left;

    left = (outbuf->bufend - outbuf->bufptr) - outbuf->bufcnt;
    if (left < outlen)
	return NULL;
    else
	return outbuf->bufptr + outbuf->bufcnt;
}

/*
 * Commit previously reserved space as real output
 */
void
CommitOutBuf(outbuf, outlen)
    ZlibBufferPtr outbuf;
    int		 outlen;
{
    outbuf->bufcnt += outlen;
}

/*
 * Write out as much as possible from the output buffer.
 * Returns: >= 0 - amount left in buffer
 *	    <  0 - write error
 */
int
FlushOutBuf(fd, outbuf)
    int		 fd;
    ZlibBufferPtr outbuf;
{
    int		 bytes;

    if (outbuf->bufcnt == 0)
	return 0;
    bytes = write(fd, outbuf->bufptr, outbuf->bufcnt);
    if (bytes > 0) {
	outbuf->bufptr += bytes;
	if ((outbuf->bufcnt -= bytes) == 0)
	    outbuf->bufptr = outbuf->bufbase;
	return outbuf->bufcnt;
    }
    else if (bytes == 0) {
	errno = EWOULDBLOCK;
	bytes = -1;
    }
    return bytes;
}

/*
 * Write out as much as possible from the iovec array (no more than
 * two entries allowed).
 * Returns: >= 0 - amount left in iovec[1]
 *	    <  0 - write error
 */
int
FlushIovBuf(fd, iovbuf)
    int		 fd;
    struct iovec *iovbuf;
{
    int		 bytes;
    int		 niov = 2;
    struct iovec *iov = iovbuf;

    if (iov[0].iov_len == 0) {
	++iov;
	--niov;
    }
    bytes = writev(fd, iov, niov);
    if (bytes > 0) {
	int i;
	int len;
	for (i = 0; i < niov; i++) {
	    len = MIN(bytes, iov[i].iov_len);
	    iov[i].iov_len -= len;
	/* 
	 * An explicit cast is necessary because silly SGI changed 
	 * iov_base from a caddr_t to a void* in IRIX 6.x, and strictly 
	 * speaking ANSI/ISO C doesn't allow the use of a cast in an 
	 * lvalue, i.e. such as: '((char*)(iov[i].iov_base)) += len;'
	 */
	    iov[i].iov_base = ((char*)(iov[i].iov_base)) + len;
	    if ((bytes -= len) == 0)
		break;
	}
	return iovbuf[1].iov_len;
    }
    else if (bytes == 0) {
	errno = EWOULDBLOCK;
	bytes = -1;
    }
    return bytes;
}
