/* $Xorg: swaprep.c,v 1.4 2001/02/09 02:05:43 xorgcvs Exp $ */
/*
 * font server reply swapping
 */
/*
 
Copyright 1990, 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

 * Copyright 1990, 1991 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation 
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices, or Digital
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES,
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */
/* $XFree86: xc/programs/xfs/difs/swaprep.c,v 1.4 2001/01/17 23:45:29 dawes Exp $ */

#include	<swaprep.h>
#include	<swapreq.h>

#include	<X11/fonts/FSproto.h>
#include	"clientstr.h"

static void SwapConnSetupAccept(fsConnSetupAccept *pConnSetup, fsConnSetupAccept *pConnSetupT);

void
Swap32Write(
    ClientPtr   client,
    int         size,
    long       *pbuf)
{
    int         n,
                i;

    size >>= 2;
    for (i = 0; i < size; i++) {
	swapl(&pbuf[i], n);
    }
    (void) WriteToClient(client, size << 2, (char *) pbuf);
}

void
Swap16Write(
    ClientPtr   client,
    int         size,
    short      *pbuf)
{
    int         n,
                i;

    size >>= 1;
    for (i = 0; i < size; i++) {
	swaps(&pbuf[i], n);
    }
    (void) WriteToClient(client, size << 1, (char *) pbuf);
}

void
CopySwap32Write(
    ClientPtr   client,
    int         size,
    long       *pbuf)
{
    int         bufsize = size;
    long       *pbufT;
    long       *from,
               *to,
               *fromLast,
               *toLast;
    long        tmpbuf[1];

    while (!(pbufT = (long *) ALLOCATE_LOCAL(bufsize))) {
	bufsize >>= 1;
	if (bufsize == 4) {
	    pbufT = tmpbuf;
	    break;
	}
    }
    /* convert lengths from # of bytes to # of longs */
    size >>= 2;
    bufsize >>= 2;

    from = pbuf;
    fromLast = from + size;
    while (from < fromLast) {
	int         nbytes;

	to = pbufT;
	toLast = to + min(bufsize, fromLast - from);
	nbytes = (toLast - to) << 2;
	while (to < toLast) {
	    /*
	     * can't write "cpswapl(*from++, *to++)" because cpswapl is a
	     * macro that evaulates its args more than once
	     */
	    cpswapl(*from, *to);
	    from++;
	    to++;
	}
	(void) WriteToClient(client, nbytes, (char *) pbufT);
    }

    if (pbufT != tmpbuf)
	DEALLOCATE_LOCAL((char *) pbufT);
}

void
CopySwap16Write(
    ClientPtr   client,
    int         size,
    short      *pbuf)
{
    int         bufsize = size;
    short      *pbufT;
    register short *from,
               *to,
               *fromLast,
               *toLast;
    short       tmpbuf[2];

    /* Allocate as big a buffer as we can... */
    while (!(pbufT = (short *) ALLOCATE_LOCAL(bufsize))) {
	bufsize >>= 1;
	if (bufsize == 4) {
	    pbufT = tmpbuf;
	    break;
	}
    }

    /* convert lengths from # of bytes to # of shorts */
    size >>= 1;
    bufsize >>= 1;

    from = pbuf;
    fromLast = from + size;
    while (from < fromLast) {
	int         nbytes;

	to = pbufT;
	toLast = to + min(bufsize, fromLast - from);
	nbytes = (toLast - to) << 1;
	while (to < toLast) {
	    /*
	     * can't write "cpswaps(*from++, *to++)" because cpswaps is a
	     * macro that evaulates its args more than once
	     */
	    cpswaps(*from, *to);
	    from++;
	    to++;
	}
	(void) WriteToClient(client, nbytes, (char *) pbufT);
    }

    if (pbufT != tmpbuf)
	DEALLOCATE_LOCAL((char *) pbufT);
}


#undef  pRep
#define pRep ((fsGenericReply *)data)

void
SGenericReply(
    ClientPtr   client,
    int         size,
    void *	data)
{
    pRep->sequenceNumber = lswaps(pRep->sequenceNumber);
    pRep->length = lswapl(pRep->length);
    (void) WriteToClient(client, size, (char *) pRep);
}


#undef  pRep
#define pRep ((fsListExtensionsReply *)data)

void
SListExtensionsReply(
    ClientPtr   client,
    int         size,
    void *	data)
{
    pRep->sequenceNumber = lswaps(pRep->sequenceNumber);
    pRep->length = lswapl(pRep->length);
    (void) WriteToClient(client, size, (char *) pRep);
}

#undef  pRep
#define pRep ((fsQueryExtensionReply *)data)

void
SQueryExtensionReply(
    ClientPtr   client,
    int         size,
    void *	data)
{
    pRep->sequenceNumber = lswaps(pRep->sequenceNumber);
    pRep->length = lswapl(pRep->length);
    pRep->major_version = lswaps(pRep->major_version);
    pRep->minor_version = lswaps(pRep->minor_version);
    (void) WriteToClient(client, size, (char *) pRep);
}

#undef  pRep
#define pRep ((fsListCataloguesReply *)data)

void
SListCataloguesReply(
    ClientPtr   client,
    int         size,
    void *	data)
{
    pRep->sequenceNumber = lswaps(pRep->sequenceNumber);
    pRep->length = lswapl(pRep->length);
    pRep->num_replies = lswapl(pRep->num_replies);
    pRep->num_catalogues = lswapl(pRep->num_catalogues);
    (void) WriteToClient(client, size, (char *) pRep);
}

#undef  pRep
#define pRep ((fsCreateACReply *)data)

void
SCreateACReply(
    ClientPtr   client,
    int         size,
    void *	data)
{
    pRep->sequenceNumber = lswaps(pRep->sequenceNumber);
    pRep->length = lswapl(pRep->length);
    pRep->status = lswaps(pRep->status);
    (void) WriteToClient(client, size, (char *) pRep);
}

#undef  pRep
#define pRep ((fsGetEventMaskReply *)data)

void
SGetEventMaskReply(
    ClientPtr   client,
    int         size,
    void *	data)
{
    pRep->sequenceNumber = lswaps(pRep->sequenceNumber);
    pRep->length = lswapl(pRep->length);
    pRep->event_mask = lswapl(pRep->event_mask);
    (void) WriteToClient(client, size, (char *) pRep);
}

#undef  pRep
#define pRep ((fsGetResolutionReply *)data)

void
SGetResolutionReply(
    ClientPtr   client,
    int         size,
    void *	data)
{
    pRep->sequenceNumber = lswaps(pRep->sequenceNumber);
    pRep->length = lswapl(pRep->length);
    (void) WriteToClient(client, size, (char *) pRep);
}

#undef  pRep
#define pRep ((fsListFontsReply *)data)

void
SListFontsReply(
    ClientPtr   client,
    int         size,
    void *	data)
{
    pRep->sequenceNumber = lswaps(pRep->sequenceNumber);
    pRep->length = lswapl(pRep->length);
    pRep->following = lswapl(pRep->following);
    pRep->nFonts = lswapl(pRep->nFonts);
    (void) WriteToClient(client, size, (char *) pRep);
}

#define SwapXFontInfoHeader(reply) \
    reply->font_header_flags = lswapl(reply->font_header_flags); \
 \
    reply->font_header_min_bounds_left = lswaps(reply->font_header_min_bounds_left); \
    reply->font_header_min_bounds_right = lswaps(reply->font_header_min_bounds_right); \
    reply->font_header_min_bounds_width = lswaps(reply->font_header_min_bounds_width); \
    reply->font_header_min_bounds_ascent = lswaps(reply->font_header_min_bounds_ascent); \
    reply->font_header_min_bounds_descent = lswaps(reply->font_header_min_bounds_descent); \
    reply->font_header_min_bounds_attributes = lswaps(reply->font_header_min_bounds_attributes); \
 \
    reply->font_header_max_bounds_left = lswaps(reply->font_header_max_bounds_left); \
    reply->font_header_max_bounds_right = lswaps(reply->font_header_max_bounds_right); \
    reply->font_header_max_bounds_width = lswaps(reply->font_header_max_bounds_width); \
    reply->font_header_max_bounds_ascent = lswaps(reply->font_header_max_bounds_ascent); \
    reply->font_header_max_bounds_descent = lswaps(reply->font_header_max_bounds_descent); \
    reply->font_header_max_bounds_attributes = lswaps(reply->font_header_max_bounds_attributes); \
 \
    reply->font_header_font_ascent = lswaps(reply->font_header_font_ascent); \
    reply->font_header_font_descent = lswaps(reply->font_header_font_descent)

#undef  pRep
#define pRep ((fsListFontsWithXInfoReply *)data)

void
SListFontsWithXInfoReply(
    ClientPtr   client,
    int         size,
    void *	data)
{
    pRep->sequenceNumber = lswaps(pRep->sequenceNumber);
    pRep->length = lswapl(pRep->length);
    if (size > SIZEOF(fsGenericReply)) { 	/* not last in series? */
	pRep->nReplies = lswapl(pRep->nReplies);
	SwapXFontInfoHeader(pRep);
    }
    (void) WriteToClient(client, size, (char *) pRep);
}

#undef  pRep
#define pRep ((fsOpenBitmapFontReply *)data)

void
SOpenBitmapFontReply(
    ClientPtr   client,
    int         size,
    void *	data)
{
    pRep->sequenceNumber = lswaps(pRep->sequenceNumber);
    pRep->length = lswapl(pRep->length);
    pRep->otherid = lswapl(pRep->otherid);

    (void) WriteToClient(client, size, (char *) pRep);
}

#undef  pRep
#define pRep ((fsQueryXInfoReply *)data)

void
SQueryXInfoReply(
    ClientPtr   client,
    int         size,
    void *	data)
{
    pRep->sequenceNumber = lswaps(pRep->sequenceNumber);
    pRep->length = lswapl(pRep->length);
    SwapXFontInfoHeader(pRep);
    (void) WriteToClient(client, size, (char *) pRep);
}

#undef  pRep
#define pRep ((fsQueryXExtents8Reply *)data)

void
SQueryXExtentsReply(
    ClientPtr   client,
    int         size,
    void *	data) /* QueryXExtents16Reply is the same */
{
    pRep->sequenceNumber = lswaps(pRep->sequenceNumber);
    pRep->length = lswapl(pRep->length);
    pRep->num_extents = lswapl(pRep->num_extents);
    (void) WriteToClient(client, size, (char *) pRep);
}

#undef  pRep
#define pRep ((fsQueryXBitmaps8Reply *)data)

void
SQueryXBitmapsReply(
    ClientPtr   client,
    int         size,
    void *	data) /* QueryXBitmaps16Reply is the same */
{
    pRep->sequenceNumber = lswaps(pRep->sequenceNumber);
    pRep->length = lswapl(pRep->length);
    pRep->replies_hint = lswapl(pRep->replies_hint);
    pRep->num_chars = lswapl(pRep->num_chars);
    pRep->nbytes = lswapl(pRep->nbytes);
    (void) WriteToClient(client, size, (char *) pRep);
}

void
SErrorEvent(fsError *error, fsError *perror)
{
    *perror = *error;
    perror->sequenceNumber = lswaps(perror->sequenceNumber);
    perror->length = lswapl(perror->length);
    perror->timestamp = lswapl(perror->timestamp);
}

void
WriteSConnectionInfo(
    ClientPtr   client,
    unsigned long size,
    char       *pInfo)
{
    char       *pInfoT,
               *pInfoTBase;
    fsConnSetupAccept *pConnSetup = (fsConnSetupAccept *) pInfo;
    int         i;

    pInfoT = pInfoTBase = (char *) ALLOCATE_LOCAL(size);
    if (!pInfoT) {
	client->noClientException = -2;
	return;
    }
    SwapConnSetupAccept(pConnSetup, (fsConnSetupAccept *) pInfoT);
    pInfoT += SIZEOF(fsConnSetup);
    pInfo += SIZEOF(fsConnSetup);

    i = (pConnSetup->vendor_len + 3) & ~3;
    memmove( pInfoT, pInfo, i);

    (void) WriteToClient(client, (int) size, (char *) pInfoTBase);
    DEALLOCATE_LOCAL(pInfoTBase);
}

static void
SwapConnSetupAccept(fsConnSetupAccept *pConnSetup, fsConnSetupAccept *pConnSetupT)
{
    pConnSetupT->length = lswapl(pConnSetup->length);
    pConnSetupT->max_request_len = lswaps(pConnSetup->max_request_len);
    pConnSetupT->vendor_len = lswaps(pConnSetup->vendor_len);
    pConnSetupT->release_number = lswapl(pConnSetup->release_number);
}

void
WriteSConnSetup(ClientPtr client, fsConnSetup *pcsp)
{
    fsConnSetup cspT;

    cspT.status = lswaps(pcsp->status);
    cspT.major_version = lswaps(pcsp->major_version);
    cspT.minor_version = lswaps(pcsp->minor_version);
    cspT.num_alternates = pcsp->num_alternates;
    cspT.auth_index = pcsp->auth_index;
    cspT.alternate_len = lswaps(pcsp->alternate_len);
    cspT.auth_len = lswaps(pcsp->auth_len);
    (void) WriteToClient(client, SIZEOF(fsConnSetup), (char *) &cspT);
}

static void
SwapPropOffset(char *po)
{
    int i, n;

    for (i=0; i<4; i++)
    {
	swapl(po, n);
	po += 4;
    }
}

void
SwapPropInfo(fsPropInfo *pi)
{
    int i;
    char *po;

    po = (char *) pi + SIZEOF(fsPropInfo);
    for (i = 0; i < pi->num_offsets; i++)
    {
	SwapPropOffset(po);
	po += SIZEOF(fsPropOffset);
    }
 
    pi->num_offsets = lswapl(pi->num_offsets);
    pi->data_len = lswapl(pi->data_len);
}

void
SwapExtents(fsXCharInfo *extents, int num)
{
    SwapShorts((short *)extents, num * (SIZEOF(fsXCharInfo) / 2));
}
