/* $Xorg: fontinfo.c,v 1.4 2001/02/09 02:05:42 xorgcvs Exp $ */
/*
 * font data query
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
/* $XFree86: xc/programs/xfs/difs/fontinfo.c,v 1.11 2002/10/15 01:45:02 dawes Exp $ */

#include        "FS.h"
#include        "FSproto.h"
#include        <stdio.h>
#include        <X11/Xos.h>
#include        "clientstr.h"
#include        "difsfnst.h"
#include        "fontstruct.h"
#include        "closestr.h"
#include        "globals.h"
#include	"difs.h"
#include	"dispatch.h"
#include	<swapreq.h>
#include	<swaprep.h>

void
CopyCharInfo(
    CharInfoPtr ci,
    fsXCharInfo *dst)
{
    xCharInfo  *src = &ci->metrics;

    dst->ascent = src->ascent;
    dst->descent = src->descent;
    dst->left = src->leftSideBearing;
    dst->right = src->rightSideBearing;
    dst->width = src->characterWidth;
    dst->attributes = src->attributes;
}


int
convert_props(
    FontInfoPtr pinfo,
    fsPropInfo **props)
{
    int i;
    int data_len, cur_off;
    char *str;
    pointer ptr, off_ptr, string_base;
    fsPropOffset local_offset;

    /*
     * compute the size of the property data
     */
    data_len = 0;
    for (i = 0; i < pinfo->nprops; i++)
    {
	data_len += strlen(NameForAtom(pinfo->props[i].name));
	if (NULL != pinfo->isStringProp && pinfo->isStringProp[i])
	    data_len += strlen(NameForAtom(pinfo->props[i].value));
    }

    /*
     * allocate the single chunk that the difs layer requires
     */
    ptr = (pointer) fsalloc(SIZEOF(fsPropInfo)
			    + SIZEOF(fsPropOffset) * pinfo->nprops
			    + data_len);
    if (!ptr)
	return AllocError;
    string_base = (char *)ptr + SIZEOF(fsPropInfo) + SIZEOF(fsPropOffset) * pinfo->nprops;
  
    /*
     * copy in the header
     */
    ((fsPropInfo *)ptr)->num_offsets = pinfo->nprops;
    ((fsPropInfo *)ptr)->data_len = data_len;

    /*
     * compute the offsets and copy the string data
     */
    off_ptr = (char *)ptr + SIZEOF(fsPropInfo);
    cur_off = 0;
    for (i = 0; i < pinfo->nprops; i++)
    {
	local_offset.name.position = cur_off;
	str = NameForAtom(pinfo->props[i].name);
	local_offset.name.length = strlen(str);
	memmove( (char *)string_base+cur_off, str, local_offset.name.length);
	cur_off += local_offset.name.length;
	if (NULL != pinfo->isStringProp && pinfo->isStringProp[i])
	{
	    local_offset.value.position = cur_off;
	    str = NameForAtom(pinfo->props[i].value);
	    local_offset.value.length = strlen(str);
	    memmove( (char *)string_base+cur_off, str, local_offset.value.length);
	    cur_off += local_offset.value.length;
	    local_offset.type = PropTypeString;
	} else {
	    local_offset.value.position = pinfo->props[i].value;
	    local_offset.value.length = 0; /* protocol says must be zero */
	    local_offset.type = PropTypeSigned;
	}
	memmove( off_ptr, &local_offset, SIZEOF(fsPropOffset));
	off_ptr = (char *)off_ptr + SIZEOF(fsPropOffset);
    }

    assert(off_ptr == string_base);
    assert(cur_off == data_len);

    *props = (fsPropInfo *) ptr;
    return Successful;
}


/*
 * does the real work of turning a list of range (or chars) into
 * a list of ranges
 */
static fsRange *
build_range(
    Bool        type,
    pointer     src,
    int         item_size,
    int        *num,
    Bool       *all,
    FontInfoPtr	pfi)
{
    fsRange    *new = (fsRange *) 0,
               *np;
    unsigned long src_num;
    unsigned long i;

    if (type) {			/* range flag is set, deal with data as a list
				 * of char2bs */
	char *rp = (char *) src;

	src_num = *num;
	if (src_num == 0) {
	    *all = TRUE;
	    return new;
	}

	np = new = (fsRange *) fsalloc(sizeof(fsRange) * (src_num + 1) / 2);
	if (!np)
	    return np;
	/* Build a new range */
	for (i = 1; i < src_num; i += 2)
	{
	    np->min_char_high = (item_size == 1) ? 0 : *rp++;
	    np->min_char_low  = *rp++;
	    np->max_char_high = (item_size == 1) ? 0 : *rp++;
	    np->max_char_low  = *rp++;
	    np++;
	}

	/* If src_num is odd, we need to determine the final range
	   by examining the fontinfo */
	if (i == src_num)
	{
	    np->min_char_high = (item_size == 1) ? 0 : *rp++;
	    np->min_char_low  = *rp++;
	    np->max_char_high = pfi->lastRow;
	    np->max_char_low  = pfi->lastCol;
	    np++;
	}
	*num = np - new;
	return new;
    } else {			/* deal with data as a list of characters */
	unsigned char      *pp = src;

	src_num = *num;
	np = new = (fsRange *) fsalloc(SIZEOF(fsRange) * src_num);
	if (!np)
	    return np;

	/* Build a range, with coalescence, from the list of chars */

	for (i = 0; i < src_num; i++) {
	    if (item_size == 1) {
		np->min_char_low = *pp;
		np->min_char_high = 0;
	    } else {
		np->min_char_low = ((fsChar2b *) pp)->low;
		np->min_char_high = ((fsChar2b *) pp)->high;
	    }
	    np->max_char_high = np->min_char_high;
	    np->max_char_low = np->min_char_low;
	    /* Can we coalesce? */
	    if (np > new &&
		np->max_char_high == np[-1].max_char_high &&
		np->max_char_low == np[-1].max_char_low + 1)
		np[-1].max_char_low++;		/* Yes */
	    else
		np++;				/* No */
	    pp += item_size;
	}
	*num = np - new;
	return new;
    }
}

/*
 * provide backward compatibility with version 1, which had
 * the bytes of char2b backwards
 */
static void
swap_char2b (fsChar2b *values, int number)
{
    fsChar2b temp;
    int i;

    for (i = 0; i < number; i++) {
	temp.low = ((fsChar2b_version1 *)values)->low;
	temp.high = ((fsChar2b_version1 *)values)->high;
	*values++ = temp;
    }
}

#define pPtr ((QEclosurePtr) data)

static Bool
do_query_extents(ClientPtr client, pointer data)
{
    int         err;
    unsigned long lendata,
                num_extents;
    fsXCharInfo *extents;
    fsQueryXExtents8Reply reply;

    err = GetExtents (pPtr->client, pPtr->pfont,
		     pPtr->flags, pPtr->nranges, pPtr->range, &num_extents, &extents);
    if (err == Suspended) {
	if (!pPtr->slept) {
	    pPtr->pfont->unload_glyphs = 0;  /* Not a safe call for this font */
	    pPtr->slept = TRUE;
	    ClientSleep(client, do_query_extents, (pointer) pPtr);
	}
	return TRUE;
    }
    if (err != Successful) {
	SendErrToClient(pPtr->client, FontToFSError(err), (pointer) 0);
	goto finish;
    }
    reply.type = FS_Reply;
    reply.sequenceNumber = pPtr->client->sequence;
    reply.num_extents = num_extents;
    lendata = SIZEOF(fsXCharInfo) * num_extents;
    reply.length = (SIZEOF(fsQueryXExtents8Reply) + lendata) >> 2;
    if (client->swapped)
	SwapExtents(extents, num_extents);
    WriteReplyToClient(pPtr->client, SIZEOF(fsQueryXExtents8Reply), &reply);
    (void) WriteToClient(pPtr->client, lendata, (char *) extents);
    fsfree((char *) extents);
finish:
    if (pPtr->slept)
	ClientWakeup(pPtr->client);
    if (pPtr->pfont->unload_glyphs)  /* For rasterizers that want to save memory */
	(*pPtr->pfont->unload_glyphs)(pPtr->pfont);
    fsfree(pPtr->range);
    fsfree(pPtr);
    return TRUE;
}

int
QueryExtents(
    ClientPtr   client,
    ClientFontPtr cfp,
    int         item_size,
    int         nranges,
    Bool        range_flag,
    pointer     range_data)
{
    QEclosurePtr c;
    fsRange    *fixed_range;
    Bool        all_glyphs = FALSE;

    if (item_size == 2  &&  client->major_version == 1)
	swap_char2b ((fsChar2b *)range_data, nranges);

    fixed_range = build_range(range_flag, range_data, item_size,
			      &nranges, &all_glyphs, &cfp->font->info);

    if (!fixed_range && !all_glyphs) {
	SendErrToClient(client, FSBadRange, 0);
	return FSBadRange;
    }
    c = (QEclosurePtr) fsalloc(sizeof(QEclosureRec));
    if (!c)
	return FSBadAlloc;
    c->client = client;
    c->slept = FALSE;
    c->pfont = cfp->font;
    c->flags = (all_glyphs) ? LoadAll : 0;
    c->flags |= (item_size == 1) ? EightBitFont : SixteenBitFont;
    c->nranges = nranges;
    c->range = fixed_range;

    (void) do_query_extents(client, (pointer) c);
    return FSSuccess;
}

#undef pPtr
#define pPtr ((QBclosurePtr) data)

static Bool
do_query_bitmaps(ClientPtr client, pointer data)
{
    int         err;
    unsigned long num_glyphs;
    int data_size;
    fsOffset32   *offsets;
    pointer     glyph_data;
    fsQueryXBitmaps8Reply reply;
    int		freedata;

    err = GetBitmaps (pPtr->client, pPtr->pfont, pPtr->format,
				    pPtr->flags, pPtr->nranges, pPtr->range,
			     &data_size, &num_glyphs, &offsets, &glyph_data, &freedata);

    if (err == Suspended) {
	if (!pPtr->slept) {
	    pPtr->pfont->unload_glyphs = 0;  /* Not a safe call for this font */
	    pPtr->slept = TRUE;
	    ClientSleep(client, do_query_bitmaps, (pointer) pPtr);
	}
	return TRUE;
    }
    if (err != Successful) {
	SendErrToClient(pPtr->client, FontToFSError(err), (pointer) 0);
	goto finish;
    }
    reply.type = FS_Reply;
    reply.sequenceNumber = pPtr->client->sequence;
    reply.replies_hint = 0;
    reply.num_chars = num_glyphs;
    reply.nbytes = data_size;
    reply.length = (SIZEOF(fsQueryXBitmaps8Reply) + data_size +
		    (SIZEOF(fsOffset32) * num_glyphs) + 3) >> 2;

    WriteReplyToClient(pPtr->client, SIZEOF(fsQueryXBitmaps8Reply), &reply);
    if (client->swapped)
	SwapLongs((long *)offsets, num_glyphs * 2);
    (void) WriteToClient(pPtr->client, (num_glyphs * SIZEOF(fsOffset32)),
			 (char *) offsets);
    (void) WriteToClient(pPtr->client, data_size, (char *) glyph_data);
    fsfree((char *) offsets);
    if (freedata)
	fsfree((char *) glyph_data);
finish:
    if (pPtr->slept)
	ClientWakeup(pPtr->client);
    if (pPtr->pfont->unload_glyphs)  /* For rasterizers that want to save memory */
	(*pPtr->pfont->unload_glyphs)(pPtr->pfont);
    fsfree(pPtr->range);
    fsfree(pPtr);
    return TRUE;
}

int
QueryBitmaps(
    ClientPtr   client,
    ClientFontPtr cfp,
    int         item_size,
    fsBitmapFormat format,
    int         nranges,
    Bool        range_flag,
    pointer     range_data)
{
    QBclosurePtr c;
    fsRange    *fixed_range;
    Bool        all_glyphs = FALSE;

    if (item_size == 2  &&  client->major_version == 1)
	swap_char2b ((fsChar2b *)range_data, nranges);

    fixed_range = build_range(range_flag, range_data, item_size,
			      &nranges, &all_glyphs, &cfp->font->info);

    if (!fixed_range && !all_glyphs) {
	SendErrToClient(client, FSBadRange, 0);
	return FSBadRange;
    }
    c = (QBclosurePtr) fsalloc(sizeof(QBclosureRec));
    if (!c)
	return FSBadAlloc;
    c->client = client;
    c->slept = FALSE;
    c->pfont = cfp->font;
    c->flags = (all_glyphs) ? LoadAll : 0;
    c->nranges = nranges;
    c->range = fixed_range;
    c->format = format;

    (void) do_query_bitmaps(client, (pointer) c);
    return FSSuccess;
}
