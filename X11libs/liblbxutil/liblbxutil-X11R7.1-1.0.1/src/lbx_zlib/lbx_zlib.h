/* $Xorg: lbx_zlib.h,v 1.4 2001/02/09 02:04:05 xorgcvs Exp $ */

/*

Copyright 1996  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

/*
 * Copyright 1988, 1989, 1990, 1994 Network Computing Devices, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name Network Computing Devices, Inc. not be
 * used in advertising or publicity pertaining to distribution of this 
 * software without specific, written prior permission.
 * 
 * THIS SOFTWARE IS PROVIDED `AS-IS'.  NETWORK COMPUTING DEVICES, INC.,
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT
 * LIMITATION ALL IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, OR NONINFRINGEMENT.  IN NO EVENT SHALL NETWORK
 * COMPUTING DEVICES, INC., BE LIABLE FOR ANY DAMAGES WHATSOEVER, INCLUDING
 * SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS OF USE, DATA,
 * OR PROFITS, EVEN IF ADVISED OF THE POSSIBILITY THEREOF, AND REGARDLESS OF
 * WHETHER IN AN ACTION IN CONTRACT, TORT OR NEGLIGENCE, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 */
/* $XFree86: xc/lib/lbxutil/lbx_zlib/lbx_zlib.h,v 1.9 2001/08/27 19:01:07 dawes Exp $ */

#include "zlib.h"
#include <X11/Xfuncproto.h>

struct compress_private {
    z_stream	stream;
    char	compress_inited;
    char	decompress_inited;
    int         z_err;		/* error code for last stream operation */
    unsigned char *cp_inputbuf;
    unsigned char *cp_inputbufend;
    unsigned char *cp_outputbuf;
    unsigned char *cp_outputbufend;
    unsigned char *cp_packet;
    int cp_outputcount;
    long int cp_in_count;		/* length of input */
    long int cp_bytes_out;		/* length of compressed output */
    int need_flush_decompress;
#ifdef LBXREQSTATS
    int x_req_code;
    int lbx_req_code;
    int x_header_bytes_read;
    char x_header_buf[4];
    int req_length;
    int req_compbytes_read;
    int req_uncompbytes_read;
#endif
};

#ifndef MIN
#define MIN(_a, _b) ( ((_a) < (_b)) ? (_a) : (_b))
#endif


/*
 * The following is taken from the xtrans code, it would be nice to share it
 */
#if defined(WIN32) || (defined(USG) && !defined(CRAY) && !defined(umips) && !defined(MOTOROLA) && !defined(uniosu) && !defined(__sxg__))

struct iovec {
    caddr_t iov_base;
    int iov_len;
};

#else
#ifndef Lynx
#include <sys/uio.h>
#else
#include <uio.h>
#endif
#endif

#ifdef WIN32
#define BOOL wBOOL
#undef Status
#define Status wStatus
#include <winsock.h>
#undef Status
#define Status int
#undef BOOL
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif


/* lbx_zlib_io.c */
extern int GetInputPtr ( int fd, ZlibBufferPtr inbuf, int reqlen, 
			 unsigned char **ppkt );
extern int StuffInput ( ZlibBufferPtr inbuf, unsigned char *pkt, int reqlen );
extern void FreeInput ( ZlibBufferPtr inbuf, int len );
extern void CommitOutBuf ( ZlibBufferPtr outbuf, int outlen );
extern int FlushOutBuf ( int fd, ZlibBufferPtr outbuf );
extern int FlushIovBuf ( int fd, struct iovec *iovbuf );

/* Copied from xc/programs/Xserver/include/xorg/os.h */
#ifndef _HAVE_XALLOC_DECLS
#define _HAVE_XALLOC_DECLS
#include <X11/Xdefs.h>

extern pointer Xalloc(unsigned long /*amount*/);
extern pointer Xcalloc(unsigned long /*amount*/);
extern pointer Xrealloc(pointer /*ptr*/, unsigned long /*amount*/);
extern void Xfree(pointer /*ptr*/);
#endif
