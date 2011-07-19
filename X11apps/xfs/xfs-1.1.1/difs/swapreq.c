/*
 * swapped requests
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

#include	"xfs-config.h"

#include	<swapreq.h>

#include	<X11/fonts/FSproto.h>
#include	"clientstr.h"
#include	"globals.h"
#include	"dispatch.h"

/* Size including padding to next 32-bit boundary */
#define PAD_TO_32BIT(e)		(((e) + 3) & ~3)

void
SwapLongs(long *list, unsigned long count)
{
    int         n;
    register char *longs = (char *)list;

    while (count >= 8) {
	swapl(longs + 0, n);
	swapl(longs + 4, n);
	swapl(longs + 8, n);
	swapl(longs + 12, n);
	swapl(longs + 16, n);
	swapl(longs + 20, n);
	swapl(longs + 24, n);
	swapl(longs + 28, n);
	longs += 32;
	count -= 8;
    }
    if (count != 0) {
	do {
	    swapl(longs, n);
	    longs += 4;
	} while (--count != 0);
    }
}

/* Byte swap a list of shorts */

void
SwapShorts(short *list, unsigned long count)
{
    register char *shorts = (char *)list;
    register int n;

    while (count >= 16) {
	swaps(shorts + 0, n);
	swaps(shorts + 2, n);
	swaps(shorts + 4, n);
	swaps(shorts + 6, n);
	swaps(shorts + 8, n);
	swaps(shorts + 10, n);
	swaps(shorts + 12, n);
	swaps(shorts + 14, n);
	swaps(shorts + 16, n);
	swaps(shorts + 18, n);
	swaps(shorts + 20, n);
	swaps(shorts + 22, n);
	swaps(shorts + 24, n);
	swaps(shorts + 26, n);
	swaps(shorts + 28, n);
	swaps(shorts + 30, n);
	shorts += 32;
	count -= 16;
    }
    if (count != 0) {
	do {
	    swaps(shorts, n);
	    shorts += 2;
	} while (--count != 0);
    }
}

/*
 * used for all requests that have nothing but 'length' swapped
 */
int
SProcSimpleRequest(ClientPtr client)
{
    REQUEST(fsReq);
    stuff->length = lswaps(stuff->length);
    return ((*ProcVector[stuff->reqType]) (client));
}

/*
 * used for all requests that have nothing but 'length' & a resource id swapped
 */
int
SProcResourceRequest(ClientPtr client)
{
    REQUEST(fsResourceReq);
    stuff->length = lswaps(stuff->length);
    stuff->id = lswapl(stuff->id);
    return ((*ProcVector[stuff->reqType]) (client));
}

static int
swap_auth(ClientPtr client, pointer data, int num, int length)
{
    unsigned char *p;
    unsigned char t;
    CARD16      namelen,
                datalen;
    int         i;

    if (num == 0) {	/* Nothing to swap */
	return (FSSuccess);
    }

    /* The font service protocol spec states that the lengths should always
     * be in the client's native byte order, and thus need swapping for a
     * byte-swapped client - but set_font_authorizations() in the X server
     * sample implementation has always filled them in in big-endian format,
     * and xfs was swapping when running on a byte-swapped connection, and
     * then assuming the result was big-endian.
     *
     * It also specifies padding each string out to the next 32-bit boundary,
     * but again, set_font_authorizations() in the X server sample
     * implementation has always failed to do so, but byte-pads the total
     * length of data instead.
     *
     * This code determines if the individual string lengths add up to the
     * correct total length when interpreted as generated by the traditional
     * sample implementation, and if so, uses them that way, otherwise it
     * will swap them, to make them big-endian, before passing on to the
     * unswapped code that assumes that they are big-endian.
     */

    /* First determine if data matches the traditional sample
       implementation format */
    for (i = 0, p = data; i < num; i++) {
	if ((p - (unsigned char *)data) > (length - 4))
	    break;

	namelen = (p[0] << 8) + p[1];
	datalen = (p[2] << 8) + p[3];
	p += 4 + namelen + datalen;
    }
    if ((i == num) && (PAD_TO_32BIT(p - (unsigned char *)data) == length))
	return (FSSuccess);

    /* Length didn't match, so we'll try swapping & padding */
    for (i = 0, p = data; i < num; i++) {
	if ((p - (unsigned char *)data) > (length - 4))
	    break;

	namelen = (p[1] << 8) + p[0];
	t = p[0];
	p[0] = p[1];
	p[1] = t;
	p += 2;

	datalen = (p[1] << 8) + p[0];
	t = p[0];
	p[0] = p[1];
	p[1] = t;
	p += 2 + PAD_TO_32BIT(namelen) + PAD_TO_32BIT(datalen);
    }
    if ((i == num) && ((p - (unsigned char *)data) == length))
	return (FSSuccess);

    /* If length didn't match either way, we give up. */
    SendErrToClient(client, FSBadLength, (pointer)&length);
    return (FSBadLength);
}

int
SProcCreateAC(ClientPtr client)
{
    int status;
    int length;

    REQUEST(fsCreateACReq);
    stuff->length = lswaps(stuff->length);
    stuff->acid = lswapl(stuff->acid);
    length = (stuff->length << 2) - sizeof(fsCreateACReq);
    status = swap_auth(client, (pointer) &stuff[1],
		       stuff->num_auths, length);
    if (status != FSSuccess)
	return (status);
    return ((*ProcVector[stuff->reqType]) (client));
}

int
SProcSetResolution(ClientPtr client)
{
    REQUEST(fsSetResolutionReq);
    stuff->length = lswaps(stuff->length);
    stuff->num_resolutions = lswaps(stuff->num_resolutions);
    if ((int)stuff->length - (&stuff[1] - &stuff[0]) !=
	stuff->num_resolutions * sizeof(fsResolution))
	return (FSBadLength);
    SwapShorts((short *) &stuff[1], stuff->num_resolutions);

    return ((*ProcVector[stuff->reqType]) (client));
}


int
SProcQueryExtension(ClientPtr client)
{
    REQUEST(fsQueryExtensionReq);
    stuff->length = lswaps(stuff->length);
    return ((*ProcVector[FS_QueryExtension]) (client));
}

int
SProcListCatalogues(ClientPtr client)
{
    REQUEST(fsListCataloguesReq);
    stuff->length = lswaps(stuff->length);
    stuff->maxNames = lswapl(stuff->maxNames);
    stuff->nbytes = lswaps(stuff->nbytes);
    return ((*ProcVector[FS_ListCatalogues]) (client));
}

int
SProcListFonts(ClientPtr client)
{
    REQUEST(fsListFontsReq);
    stuff->length = lswaps(stuff->length);
    stuff->maxNames = lswapl(stuff->maxNames);
    stuff->nbytes = lswaps(stuff->nbytes);
    return ((*ProcVector[FS_ListFonts]) (client));
}

int
SProcListFontsWithXInfo(ClientPtr client)
{
    REQUEST(fsListFontsWithXInfoReq);
    stuff->length = lswaps(stuff->length);
    stuff->maxNames = lswapl(stuff->maxNames);
    stuff->nbytes = lswaps(stuff->nbytes);
    return ((*ProcVector[FS_ListFontsWithXInfo]) (client));
}

int
SProcOpenBitmapFont(ClientPtr client)
{
    REQUEST(fsOpenBitmapFontReq);
    stuff->length = lswaps(stuff->length);
    stuff->fid = lswapl(stuff->fid);
    stuff->format_hint = lswapl(stuff->format_hint);
    stuff->format_mask = lswapl(stuff->format_mask);
    return ((*ProcVector[FS_OpenBitmapFont]) (client));
}

int
SProcQueryXExtents(ClientPtr client)
{
    REQUEST(fsQueryXExtents8Req); /* 8 and 16 are the same here */
    stuff->length = lswaps(stuff->length);
    stuff->fid = lswapl(stuff->fid);
    stuff->num_ranges = lswapl(stuff->num_ranges);

    return ((*ProcVector[stuff->reqType]) (client));
}

int
SProcQueryXBitmaps(ClientPtr client)
{
    REQUEST(fsQueryXBitmaps8Req); /* 8 and 16 are the same here */
    stuff->length = lswaps(stuff->length);
    stuff->fid = lswapl(stuff->fid);
    stuff->format = lswapl(stuff->format);
    stuff->num_ranges = lswapl(stuff->num_ranges);

    return ((*ProcVector[stuff->reqType]) (client));
}

int
SwapConnClientPrefix(ClientPtr client, fsConnClientPrefix *pCCP)
{
#if 0
    REQUEST(fsFakeReq);
#endif
	
    pCCP->major_version = lswaps(pCCP->major_version);
    pCCP->minor_version = lswaps(pCCP->minor_version);
    pCCP->auth_len = lswaps(pCCP->auth_len);
    return (swap_auth(client, (pointer) &pCCP[1],
		      pCCP->num_auths, pCCP->auth_len));
}
