/* $Xorg: lbx_zlib.c,v 1.6 2001/02/09 02:04:05 xorgcvs Exp $ */

/*

Copyright 1995, 1998  The Open Group

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
/* $XFree86: xc/lib/lbxutil/lbx_zlib/lbx_zlib.c,v 1.10 2001/12/14 19:56:59 dawes Exp $ */

#ifdef WIN32
#define _WILLWINSOCK_
#endif
#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#if !defined(WIN32) && !defined(Lynx)
#include <sys/param.h>
#endif
#include "lbxbufstr.h"
#include "lbx_zlib.h"
#include "os.h"
#include "lbxzlib.h"

unsigned long stream_out_compressed;
unsigned long stream_out_uncompressed;
unsigned long stream_out_plain;
unsigned long stream_in_compressed;
unsigned long stream_in_uncompressed;
unsigned long stream_in_plain;
#ifdef LBXREQSTATS
unsigned long stream_in_packet_header_bytes = 0;
extern int LbxWhoAmI;
#endif

struct ZlibInfo {
    struct compress_private compress_state;
    struct compress_private decompress_state;
    int			    fd;
    int			    compress_off;
    ZlibBuffer		    inbuf;
    ZlibBuffer		    outbuf;
    unsigned char	    header[ZLIB_PACKET_HDRLEN];
    struct iovec	    iovbuf[2];
};

static int
init_compress(struct compress_private *priv,/* local pointer to private data */
	      int level)	/* compression level */
{
    priv->cp_outputcount = 0;

    priv->cp_in_count = 0;		/* length of input */
    priv->cp_bytes_out = 0;		/* length of compressed output */
    priv->cp_inputbuf = priv->cp_inputbufend = NULL;
    priv->cp_packet = NULL;

    priv->stream.zalloc = (alloc_func) 0;
    priv->stream.zfree = (free_func) 0;
    priv->stream.next_in = NULL;
    priv->stream.next_out = NULL;
    priv->stream.avail_in = priv->stream.avail_out = 0;

    priv->z_err = deflateInit (&(priv->stream), level);

    return (priv->compress_inited = (priv->z_err == Z_OK) ? 1 : 0);
}

static int
init_decompress(struct compress_private *priv)/* local pointer to private data */
{
    priv->cp_outputcount = 0;

    priv->cp_in_count = 0;		/* length of input */
    priv->cp_bytes_out = 0;		/* length of compressed output */
    priv->cp_inputbuf = priv->cp_inputbufend = NULL;
    priv->cp_packet = NULL;

    priv->stream.zalloc = (alloc_func) 0;
    priv->stream.zfree = (free_func) 0;
    priv->stream.next_in = NULL;
    priv->stream.next_out = NULL;
    priv->stream.avail_in = priv->stream.avail_out = 0;

    priv->need_flush_decompress = 0;

    priv->z_err = inflateInit (&(priv->stream));

#ifdef LBXREQSTATS
    priv->req_length = -1;
    priv->req_compbytes_read = 0;
    priv->req_uncompbytes_read = 0;
    priv->x_header_bytes_read = 0;
#endif

    return (priv->decompress_inited = (priv->z_err == Z_OK) ? 1 : 0);
}


static void
do_compress (struct compress_private *priv,
	     int flush)

{
    priv->stream.next_in = priv->cp_inputbuf;
    priv->stream.avail_in = priv->cp_inputbufend - priv->cp_inputbuf;

    priv->stream.next_out = priv->cp_outputbuf;
    priv->stream.avail_out = priv->cp_outputbufend - priv->cp_outputbuf;

    priv->z_err = deflate (&(priv->stream), flush);

    priv->cp_inputbuf = priv->stream.next_in;
    priv->cp_outputbuf = priv->stream.next_out;
}


static void
do_decompress (struct compress_private *priv)

{
    priv->stream.next_in = priv->cp_inputbuf;
    priv->stream.avail_in = priv->cp_inputbufend - priv->cp_inputbuf;

    priv->stream.next_out = priv->cp_outputbuf;
    priv->stream.avail_out = priv->cp_outputbufend - priv->cp_outputbuf;

    priv->z_err = inflate (&(priv->stream), Z_PARTIAL_FLUSH);

    priv->need_flush_decompress = (priv->stream.avail_out == 0);

    priv->cp_inputbuf = priv->stream.next_in;
    priv->cp_outputbuf = priv->stream.next_out;
}

static int
GetNewPacket(struct ZlibInfo *comp)
{
    register struct compress_private *priv = &comp->decompress_state;
    int				     len;
    int				     result;

    if (priv->cp_packet)	{
	/* Free up previous packet in input buffer */
	FreeInput(&comp->inbuf, priv->cp_inputbufend - priv->cp_packet);
	priv->cp_packet = NULL;
    }

    if ((result = GetInputPtr(comp->fd,
			      &comp->inbuf,
			      ZLIB_PACKET_HDRLEN,
			      &priv->cp_packet)) <= 0)
	return result;
    len = ZLIB_GET_DATALEN(priv->cp_packet);
    if ((result = GetInputPtr(comp->fd,
			      &comp->inbuf,
			      len + ZLIB_PACKET_HDRLEN,
			      &priv->cp_packet)) <= 0) {
	priv->cp_packet = NULL;
	return result;
    }

    return len;
}

static int
NewPacketAvail(struct ZlibInfo *comp)
{
    register struct compress_private *priv = &comp->decompress_state;
    char 			     *pkt;
    int				     len;

    if (priv->cp_packet)	{
	/* Free up previous packet in input buffer */
	FreeInput(&comp->inbuf, priv->cp_inputbufend - priv->cp_packet);
	priv->cp_packet = NULL;
    }

    if ((pkt = BYTES_AVAIL(&comp->inbuf, ZLIB_PACKET_HDRLEN))) {
	len = ZLIB_GET_DATALEN(pkt);
	if (BYTES_AVAIL(&comp->inbuf, len + ZLIB_PACKET_HDRLEN))
	    return TRUE;
    }

    return FALSE;
}

static int
PlainWrite(struct ZlibInfo *comp,
	   unsigned char  *buffer,
	   int            buflen)
{
    int		   retval;
    int		   lenleft = buflen;

    if ((retval = ZlibFlush(comp->fd)) == 0) {
	register struct iovec *iov = comp->iovbuf;
	while (lenleft) {
	    int outlen, written;
	    if ((outlen = iov[1].iov_len)) {
		iov[1].iov_base = (caddr_t) buffer;
	    }
	    else {
		outlen = MIN(lenleft, ZLIB_MAX_DATALEN);
		ZLIB_PUT_PKTHDR(comp->header, outlen, FALSE);
		iov[0].iov_base = (caddr_t) comp->header;
		iov[0].iov_len = ZLIB_PACKET_HDRLEN;
		iov[1].iov_base = (caddr_t) buffer;
		iov[1].iov_len = outlen;
		stream_out_uncompressed += ZLIB_PACKET_HDRLEN;
	    }
	    if ((retval = FlushIovBuf(comp->fd, iov)) < 0)
		break;
	    written = outlen - retval;
	    lenleft -= written;
	    buffer += written;
	    stream_out_plain += written;
	    stream_out_uncompressed += written;
	    if (retval)
		break;
	}
	if (lenleft == buflen)
	    return retval;
	else
	    return buflen - lenleft;
    }

    else if (retval > 0) {
	retval = -1;
	errno = EWOULDBLOCK;
    }

    return retval;
}

#define MAX_FDS	256

static struct ZlibInfo	*per_fd[MAX_FDS];

/*
 * Initialize ZLIB compressor
 */
void *
ZlibInit(int fd,
	 int level)		/* compression level */
{
    struct ZlibInfo		     *comp;
    int				     ret1, ret2;

    if ((comp = (struct ZlibInfo *)Xalloc(sizeof(struct ZlibInfo))) == NULL)
	return NULL;

    ret1 = InitZlibBuffer(&comp->inbuf, INBUFFER_SIZE);
    ret2 = InitZlibBuffer(&comp->outbuf, OUTBUFFER_SIZE);
    if (ret1 < 0 || ret2 < 0) {
	ZlibFree(comp);
	return NULL;
    }
    comp->compress_off = FALSE;
    comp->iovbuf[1].iov_len = 0;
    comp->fd = fd;

    if (!init_compress(&comp->compress_state, level) ||
        !init_decompress(&comp->decompress_state)) {
	ZlibFree(comp);
	return NULL;
    }

    per_fd[fd] = comp;

#ifdef LBXREQSTATS
    InitLbxReqStats ();
#endif

    return (void *)comp;
}

void
ZlibFree(struct ZlibInfo *comp)
{
    if (!comp)
	return;
    per_fd[comp->fd] = 0;
    FreeZlibBuffer(&comp->inbuf);
    FreeZlibBuffer(&comp->outbuf);

    if (comp->compress_state.compress_inited)
	deflateEnd (&(comp->compress_state.stream));
    else if (comp->decompress_state.decompress_inited)
	inflateEnd (&(comp->compress_state.stream));

    Xfree(comp);
}

int
ZlibFlush(int fd)
{
    struct ZlibInfo *comp = per_fd[fd];
    struct compress_private *priv = &comp->compress_state;

    if (priv->cp_in_count) {
	int len;
	do_compress (priv, Z_PARTIAL_FLUSH);
	len = priv->cp_outputbuf - (priv->cp_packet + ZLIB_PACKET_HDRLEN);
	ZLIB_PUT_PKTHDR(priv->cp_packet, len, TRUE);
	stream_out_compressed += (len + ZLIB_PACKET_HDRLEN);

	CommitOutBuf(&comp->outbuf, len + ZLIB_PACKET_HDRLEN);
	priv->cp_in_count = 0;
    }

    return FlushOutBuf(comp->fd, &comp->outbuf);
}

int
ZlibStuffInput(int fd,
	       unsigned char *buffer,
	       int buflen)
{
    struct ZlibInfo	    *comp = per_fd[fd];

    if (StuffInput (&comp->inbuf, buffer, buflen) != buflen)
	return 0;
    return 1;
}

void
ZlibCompressOn(int fd)
{
    per_fd[fd]->compress_off = FALSE;
}

void
ZlibCompressOff(int fd)
{
    per_fd[fd]->compress_off = TRUE;
}

int
ZlibWriteV(int		   fd,
	   struct iovec   *iov,
	   int		   iovcnt)
{
    int	i;
    int	total = 0;
    int	this_time;	    

    for (i = 0; i < iovcnt; i++)
    {
	this_time = ZlibWrite(fd, (unsigned char *)iov[i].iov_base,
				iov[i].iov_len);
	if (this_time > 0)
	    total += this_time;
	if (this_time != iov[i].iov_len)
	{
	    if (total)
		return total;
	    return this_time;
	}
    }
    return total;
}

int
ZlibWrite(int		   fd,
	  unsigned char  *buffer,
	  int            buflen)
{
    struct ZlibInfo	    *comp = per_fd[fd];
    struct compress_private *priv = &comp->compress_state;
    int			    len;
    int			    lenleft = buflen;
    unsigned char  	    *p = buffer;

    if (comp->compress_off) {
	return PlainWrite(comp, buffer, buflen);
    }

    while (lenleft) {
	if (priv->cp_in_count == 0) {
	    priv->cp_packet = (unsigned char *) ReserveOutBuf(&comp->outbuf,
		ZLIB_PACKET_HDRLEN + ZLIB_MAX_OUTLEN);
	    if (!priv->cp_packet) {
		errno = EWOULDBLOCK;
		return -1;
	    }
	    priv->cp_outputbuf = priv->cp_packet + ZLIB_PACKET_HDRLEN;
	    priv->cp_outputbufend = priv->cp_outputbuf + ZLIB_MAX_OUTLEN;
	}

	len = MIN(ZLIB_MAX_PLAIN - priv->cp_in_count, lenleft);
	stream_out_plain += len;

	priv->cp_inputbuf = p;
	priv->cp_inputbufend = p + len;
	do_compress(priv, Z_NO_FLUSH);

	p += len;
	lenleft -= len;
	if ((priv->cp_in_count += len) == ZLIB_MAX_PLAIN) {
	    if (ZlibFlush(fd) < 0) {
		if (lenleft == buflen)
		    return -1;
		return buflen - lenleft;
	    }
	}
    }

    return buflen;
}

int
ZlibRead(int		fd,
	 unsigned char  *buffer,
	 int		buflen)
{
    struct ZlibInfo	    *comp = per_fd[fd];
    struct compress_private *priv = &comp->decompress_state;
    unsigned char	    *p = buffer;
    int			    lenleft = buflen;
    int			    len;
    int			    retval = -1;

    /*
     * First check if there is any data Zlib decompressed already but
     * didn't have output buffer space to store it in.
     */

    if (priv->need_flush_decompress)
    {
	priv->cp_outputbuf = p;
	priv->cp_outputbufend = p + lenleft;

	do_decompress (priv);

	lenleft -= (priv->cp_outputbuf - p);
	p = priv->cp_outputbuf;
    }


    /*
     * Need to decompress some more data
     */

    priv->cp_outputbuf = p;
    priv->cp_outputbufend = p + lenleft;
    while (priv->cp_outputbuf != priv->cp_outputbufend) {
	if (priv->cp_inputbuf == priv->cp_inputbufend) {
	    if ((retval = GetNewPacket(comp)) <= 0)
		break;
	    priv->cp_inputbuf = priv->cp_packet + ZLIB_PACKET_HDRLEN;
	    priv->cp_inputbufend = priv->cp_inputbuf + retval;
	    if (ZLIB_COMPRESSED(priv->cp_packet))
		stream_in_compressed += (retval + ZLIB_PACKET_HDRLEN);
	    else
		stream_in_uncompressed += (retval + ZLIB_PACKET_HDRLEN);
#ifdef LBXREQSTATS
	    stream_in_packet_header_bytes += ZLIB_PACKET_HDRLEN;
#endif
	}

	if (ZLIB_COMPRESSED(priv->cp_packet))
#ifdef LBXREQSTATS
	    if (LbxWhoAmI == 1)	      /* only support request stats for now */
		do_decompress_with_stats(priv);
	    else
#endif
	    do_decompress(priv);

	else {
	    len = MIN(priv->cp_inputbufend - priv->cp_inputbuf,
		priv->cp_outputbufend - priv->cp_outputbuf);
	    memmove(priv->cp_outputbuf, priv->cp_inputbuf, len);
	    priv->cp_inputbuf += len;
	    priv->cp_outputbuf += len;
	}
    }

    if ((len = priv->cp_outputbuf - buffer) == 0)
	return retval;
    else {
	stream_in_plain += len;
	return len;
    }
}

int
ZlibInputAvail(int fd)
{
    struct ZlibInfo	    *comp = per_fd[fd];
    struct compress_private *priv = &comp->decompress_state;

    return (
	priv->need_flush_decompress ||
        priv->cp_inputbuf != priv->cp_inputbufend ||
	NewPacketAvail(comp) > 0);
}
