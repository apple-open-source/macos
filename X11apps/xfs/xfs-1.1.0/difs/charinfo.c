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
/*
 * Defines GetExtents() and GetBitmaps(), which are
 * called from routines in fontinfo.c.
 * This file was once on the other side of
 * the font library interface as util/fsfuncs.c.
 */

#include "xfs-config.h"

#include <X11/Xos.h>
#include "misc.h"
#include <X11/fonts/fontstruct.h>
#include <X11/fonts/fontutil.h>

/* Don't conflict with macros/prototypes in difsutils.h */
#define _HAVE_XALLOC_DECLS
#include <X11/fonts/fontmisc.h>

#include "clientstr.h"
#define FSMD_H
#include <X11/fonts/FSproto.h>
#include "difs.h"

#define GLWIDTHBYTESPADDED(bits,nbytes) \
	((nbytes) == 1 ? (((bits)+7)>>3)        /* pad to 1 byte */ \
	:(nbytes) == 2 ? ((((bits)+15)>>3)&~1)  /* pad to 2 bytes */ \
	:(nbytes) == 4 ? ((((bits)+31)>>3)&~3)  /* pad to 4 bytes */ \
	:(nbytes) == 8 ? ((((bits)+63)>>3)&~7)  /* pad to 8 bytes */ \
	: 0)

#define GLYPH_SIZE(ch, nbytes)          \
	GLWIDTHBYTESPADDED((ch)->metrics.rightSideBearing - \
			(ch)->metrics.leftSideBearing, (nbytes))

#define n2dChars(pfi)   (((pfi)->lastRow - (pfi)->firstRow + 1) * \
                         ((pfi)->lastCol - (pfi)->firstCol + 1))

#if 0
static CharInfoRec  junkDefault;
#endif

typedef int (*MetricsFunc)(FontPtr, unsigned long, unsigned char *, 
			   FontEncoding, unsigned long *, CharInfoPtr *);

static int
getCharInfos (
    FontPtr	pfont,
    int		num_ranges,
    fsRange	*range,
    Bool	ink_metrics,
    int		*nump,		/* return */
    CharInfoPtr	**retp)		/* return */
{
    CharInfoPtr	*xchars, *xci;
    int		nchars;
    FontInfoPtr pinfo = &pfont->info;
    unsigned int r, c;
    unsigned char   ch[2];
    int         firstCol = pinfo->firstCol;
    int         firstRow = pinfo->firstRow;
    int         lastRow = pinfo->lastRow;
    int         lastCol = pinfo->lastCol;
    fsRange	local_range, *rp;
    int		i;
    FontEncoding    encoding;
    int		err;
    unsigned long   glyphCount;
    unsigned short  defaultCh;
    CharInfoPtr	    defaultPtr;
    MetricsFunc	    metrics_func;
    
    /*
     * compute nchars
     */
    if (num_ranges == 0) {
	if (lastRow)
	    nchars = n2dChars(pinfo);
	else
	    nchars = lastCol - firstCol + 1;
	local_range.min_char_low = firstCol;
	local_range.min_char_high = firstRow;
	local_range.max_char_low = lastCol;
	local_range.max_char_high = lastRow;
	range = &local_range;
	num_ranges = 1;
    } else {
	nchars = 0;
	for (i = 0, rp = range; i < num_ranges; i++, rp++) {
	    if (rp->min_char_high > rp->max_char_high ||
		rp->min_char_low > rp->max_char_low)
		return BadCharRange;
	    nchars += (rp->max_char_high - rp->min_char_high + 1) *
		      (rp->max_char_low - rp->min_char_low + 1);
	}
    }

    xchars = (CharInfoPtr *) fsalloc (sizeof (CharInfoPtr) * nchars);
    if (!xchars)
	return AllocError;
    bzero (xchars, sizeof (CharInfoPtr) * nchars);

    if (ink_metrics)
	metrics_func = (MetricsFunc)pfont->get_metrics;
    else
	metrics_func = pfont->get_glyphs;

    xci = xchars;
    encoding = Linear16Bit;
    if (lastRow)
	encoding = TwoD16Bit;
    defaultCh = pinfo->defaultCh;
    ch[0] = defaultCh >> 8;
    ch[1] = defaultCh & 0xff;
    /* get the default character */
    (*metrics_func) (pfont, 1, ch, encoding,
			  &glyphCount, &defaultPtr);
    if (glyphCount != 1)
	defaultPtr = NULL;
    
    /* for each range, get each character individually, undoing the
     default character substitution so we get zero metrics for
     non-existent characters. */
    for (i = 0, rp = range; i < num_ranges; i++, rp++) {
	for (r = rp->min_char_high; r <= rp->max_char_high; r++)
	{
	    for (c = rp->min_char_low; c <= rp->max_char_low; c++) {
		ch[0] = r;
		ch[1] = c;
		err = (*metrics_func) (pfont, 1, ch, encoding,
					    &glyphCount, xci);
		if (err != Successful)
		{
		    fsfree (xchars);
		    return err;
		}
#if 0
		if (glyphCount != 1 || 
		   (*xci == defaultPtr && defaultCh != ((r<<8)+c)))
		    *xci = &junkDefault;
#endif
		xci++;
	    }
	}
    }
    *retp = xchars;
    *nump = nchars;
    return Successful;
}

int
GetExtents(
    ClientPtr   client,
    FontPtr     pfont,
    Mask        flags,
    unsigned long num_ranges,
    fsRange    *range,
    unsigned long *num_extents,	/* return */
    fsXCharInfo **data)		/* return */
{
    unsigned long size;
    fsXCharInfo *ci;
    fsXCharInfo cilocal;
    char *pci;
    CharInfoPtr	*xchars, *xchars_cur;
    CharInfoPtr xci;
    int		nchars;
    int		err;
    
    if (flags & LoadAll)
	num_ranges = 0;
    err = getCharInfos (pfont, num_ranges, range,
			client->major_version > 1 ? TRUE : FALSE,
			&nchars, &xchars);
    if (err != Successful)
	return err;
    
    size = SIZEOF(fsXCharInfo) * nchars;
    pci = (char *) fsalloc(size);
    if (!pci) {
	fsfree (xchars);
	return AllocError;
    }

    ci = (fsXCharInfo *) pci;
    *num_extents = nchars;
    
    /* pack the data */
    xchars_cur = xchars;
    while (nchars--) {
	xci = *xchars_cur++;
	cilocal.ascent = xci->metrics.ascent;
	cilocal.descent = xci->metrics.descent;
	cilocal.left = xci->metrics.leftSideBearing;
	cilocal.right = xci->metrics.rightSideBearing;
	cilocal.width = xci->metrics.characterWidth;
	cilocal.attributes = xci->metrics.attributes;
	memcpy(pci, &cilocal, SIZEOF(fsXCharInfo));
	pci += SIZEOF(fsXCharInfo);
    }
    
    fsfree (xchars);
    
    *data = ci;
    
    return Successful;
}

static int
packGlyphs (
    ClientPtr   client,
    FontPtr     pfont,
    int         format,
    Mask        flags,
    unsigned long num_ranges,
    fsRange    *range,
    int        *tsize,
    unsigned long *num_glyphs,
    fsOffset32  **offsets,
    pointer     *data,
    int		*freeData)
{
    int         i;
    fsOffset32	*lengths, *l;
    unsigned long size = 0;
    pointer     gdata;
    unsigned char *gd;
    int         bitorder, byteorder, scanlinepad, scanlineunit, mappad;
    int		height = 0, dstbpr = 0, charsize = 0;
    int		dst_off = 0, src_off;
    Bool	contiguous, reformat;
    int		nchars;
    int         src_glyph_pad = pfont->glyph;
    int         src_bit_order = pfont->bit;
    int         src_byte_order = pfont->byte;
    int         err;
    int		max_ascent = 0, max_descent = 0;
    int		min_left = 0, max_right;
    int		srcbpr;
    int		lshift = 0, rshift = 0, dst_left_bytes = 0, src_left_bytes = 0;
    unsigned char   *srcp;
    unsigned char   *dstp;
    unsigned char   bits1, bits2;
    int		    width;
    int		    src_extra;
    int		    dst_extra;
    int		    r, w;
    CharInfoPtr	*bitChars, *bitCharsFree, bitc;
    CharInfoPtr	*inkChars, *inkCharsFree = NULL, inkc;
    FontInfoPtr	pinfo = &pfont->info;
    xCharInfo	*bitm, *inkm;
    
    err = CheckFSFormat(format, (fsBitmapFormatMask) ~ 0,
			&bitorder, &byteorder, &scanlineunit, &scanlinepad, &mappad);
    
    if (err != Successful)
	return err;
    
    if (flags & LoadAll)
	num_ranges = 0;
    
    err = getCharInfos (pfont, num_ranges, range, FALSE, &nchars, &bitCharsFree);
    
    if (err != Successful)
	return err;
    
    /* compute dstbpr for padded out fonts */
    reformat = bitorder != src_bit_order || byteorder != src_byte_order;

    /* we need the ink metrics when shrink-wrapping a TE font (sigh),
     * but only for protocol version > 1 */
    if (mappad != BitmapFormatImageRectMax &&
	pinfo->inkMetrics &&
	client->major_version > 1)
    {
	err = getCharInfos (pfont, num_ranges, range, TRUE, &nchars, &inkCharsFree);
	if (err != Successful)
	{
	    fsfree (bitCharsFree);
	    return err;
	}
	reformat = TRUE;
    }

    /* get space for glyph offsets */
    lengths = (fsOffset32 *) fsalloc(SIZEOF(fsOffset32) * nchars);
    if (!lengths) {
	fsfree (bitCharsFree);
	fsfree (inkCharsFree);
	return AllocError;
    }
    
    switch (mappad)
    {
    case BitmapFormatImageRectMax:
	max_ascent = FONT_MAX_ASCENT(pinfo);
	max_descent = FONT_MAX_DESCENT(pinfo);
	height = max_ascent + max_descent;
	/* do font ascent and font descent match bitmap bounds ? */
	if (height != pinfo->minbounds.ascent + pinfo->minbounds.descent)
	    reformat = TRUE;
	/* fall through */
    case BitmapFormatImageRectMaxWidth:
	min_left = FONT_MIN_LEFT(pinfo);
	max_right = FONT_MAX_RIGHT(pinfo);
	if (min_left != pinfo->maxbounds.leftSideBearing)
	    reformat = TRUE;
	if (max_right != pinfo->maxbounds.rightSideBearing)
	    reformat = TRUE;
	dstbpr = GLWIDTHBYTESPADDED(max_right - min_left, scanlinepad);
	break;
    case BitmapFormatImageRectMin:
	break;
    }
    if (mappad == BitmapFormatImageRectMax)
	charsize = dstbpr * height;
    size = 0;
    gdata = NULL;
    contiguous = TRUE;
    l = lengths;
    inkChars = inkCharsFree;
    bitChars = bitCharsFree;
    for (i = 0; i < nchars; i++)
    {
    	inkc = bitc = *bitChars++;
	/* when ink metrics != bitmap metrics, use ink metrics */
	if (inkChars)
	    inkc = *inkChars++;
    	l->position = size;
	/*
         * Do not repad characters with no bits except for those
         * with non-zero width.
         */
        if (bitc && (bitc->bits || bitc->metrics.characterWidth)) {
	    if (!gdata)
		gdata = (pointer) bitc->bits;
	    if ((char *) gdata + size != bitc->bits)
		contiguous = FALSE;
	    if (mappad == BitmapFormatImageRectMin)
		dstbpr = GLYPH_SIZE(inkc, scanlinepad);
	    if (dstbpr != GLYPH_SIZE(bitc, src_glyph_pad)) reformat = TRUE;
	    if (mappad != BitmapFormatImageRectMax)
	    {
		height = inkc->metrics.ascent + inkc->metrics.descent;
		charsize = height * dstbpr;
	    }
	    l->length = charsize;
	    size += charsize;
	}
	else
	    l->length = 0;
	l++;
    }
    if (contiguous && !reformat)
    {
	*num_glyphs = nchars;
	*freeData = FALSE;
	*data = gdata;
	*tsize = size;
	*offsets = lengths;
	fsfree (bitCharsFree);
	fsfree (inkCharsFree);
	return Successful;
    }
    if (size)
    {
	gdata = (pointer) fsalloc(size);
	if (!gdata) {
	    fsfree (bitCharsFree);
	    fsfree (inkCharsFree);
	    fsfree (lengths);
	    return AllocError;
	}
	bzero ((char *) gdata, size);
    }
    else
	gdata = NULL;
    
    *freeData = TRUE;
    l = lengths;
    gd = gdata;
    
    /* finally do the work */
    bitChars = bitCharsFree;
    inkChars = inkCharsFree;
    for (i = 0; i < nchars; i++, l++) 
    {
	inkc = bitc = *bitChars++;
	if (inkChars)
	    inkc = *inkChars++;

	/* ignore missing chars */
	if (l->length == 0)
	    continue;
	
	bitm = &bitc->metrics;
	inkm = &inkc->metrics;

	/* start address for the destination of bits for this char */

	dstp = gd;

	if (mappad == BitmapFormatImageRectMax)
	    height = max_ascent + max_descent;
	else
	    height = inkm->ascent + inkm->descent;

	/* adjust destination and calculate shift offsets */
	switch (mappad) {
	case BitmapFormatImageRectMax:
	    /* leave the first padded rows blank */
	    if (max_ascent > inkm->ascent)
	    {
		height -= (max_ascent - inkm->ascent);
		dstp += dstbpr * (max_ascent - inkm->ascent);
	    }
	    if (max_descent > inkm->descent)
	    {
		height -= (max_descent - inkm->descent);
	    }
	    /* fall thru */
	case BitmapFormatImageRectMaxWidth:
	    dst_off = inkm->leftSideBearing - min_left;
	    if (dst_off < 0) dst_off = 0;
	    break;
	case BitmapFormatImageRectMin:
	    dst_off = 0;
	    dstbpr = GLYPH_SIZE(inkc, scanlinepad);
	    break;
	}

	srcbpr = GLYPH_SIZE (bitc, src_glyph_pad);
	srcp = (unsigned char *) bitc->bits;

	/* adjust source */
	src_off = 0;
	if (inkm != bitm)
	{
	    srcp += (bitm->ascent - inkm->ascent) * srcbpr;
	    src_off = inkm->leftSideBearing - bitm->leftSideBearing;
	}

	dst_left_bytes = dst_off >> 3;
	dst_off &= 7;
	src_left_bytes = src_off >> 3;
	src_off &= 7;

	/* minimum of source/dest bytes per row */
	width = srcbpr - src_left_bytes;
	if (width > dstbpr - dst_left_bytes)
	    width = dstbpr - dst_left_bytes;
	/* extra bytes in source and dest for padding */
	src_extra = srcbpr - width - src_left_bytes;
	dst_extra = dstbpr - width - dst_left_bytes;
	
#define MSBBitLeft(b,c)	((b) << (c))
#define MSBBitRight(b,c)	((b) >> (c))
#define LSBBitLeft(b,c)	((b) >> (c))
#define LSBBitRight(b,c)	((b) << (c))

	if (dst_off == src_off)
	{
	    if (srcbpr == dstbpr && src_left_bytes == dst_left_bytes)
	    {
		r = height * srcbpr;
		memmove( dstp, srcp, r);
		dstp += r;
	    }
	    else
	    {
		for (r =  height; r; r--)
		{
		    dstp += dst_left_bytes;
		    srcp += src_left_bytes;
		    for (w = width; w; w--)
			*dstp++ = *srcp++;
		    dstp += dst_extra;
		    srcp += src_extra;
		}
	    }
	}
	else
	{
	    if (dst_off > src_off)
	    {
	    	rshift = dst_off - src_off;
	    	lshift = 8 - rshift;
	    }
	    else
	    {
	    	lshift = src_off - dst_off;
	    	rshift = 8 - lshift;
		/* run the loop one fewer time if necessary */
		if (src_extra <= dst_extra)
		{
		    dst_extra++;
		    width--;
		}
		else
		    src_extra--;
	    }
	    
	    for (r = inkm->ascent + inkm->descent; r; r--)
	    {
		dstp += dst_left_bytes;
		srcp += src_left_bytes;
		bits2 = 0;
		/* fetch first part of source when necessary */
		if (dst_off < src_off)
		    bits2 = *srcp++;
		/*
 		 * XXX I bet this does not work when
		 * src_bit_order != src_byte_order && scanlineunit > 1
		 */
		for (w = width; w; w--)
		{
		    bits1 = *srcp++;
		    if (src_bit_order == MSBFirst)
		    {
			*dstp++ = MSBBitRight(bits1, rshift) |
				 MSBBitLeft (bits2, lshift);
		    }
		    else
		    {
			*dstp++ = LSBBitRight(bits1, rshift) |
				 LSBBitLeft (bits2, lshift);
		    }
		    bits2 = bits1;
		}
		/* get the last few bits if we have a place to store them */
		if (dst_extra > 0)
		{
		    if (src_bit_order == MSBFirst)
			*dstp = MSBBitLeft (bits2, lshift);
		    else
			*dstp = LSBBitLeft (bits2, lshift);
		}
		dstp += dst_extra;
		srcp += src_extra;
	    }
	}
	/* skip the amount we just filled in */
	gd += l->length;
    }
    
    
    /* now do the bit, byte, word swapping */
    if (bitorder != src_bit_order)
	BitOrderInvert(gdata, size);
    if (byteorder != src_byte_order) 
    {
	if (scanlineunit == 2)
	    TwoByteSwap(gdata, size);
	else if (scanlineunit == 4)
	    FourByteSwap(gdata, size);
    }
    fsfree (bitCharsFree);
    fsfree (inkCharsFree);
    *num_glyphs = nchars;
    *data = gdata;
    *tsize = size;
    *offsets = lengths;
    
    return Successful;
}

/* ARGSUSED */
int
GetBitmaps(
    ClientPtr   client,
    FontPtr     pfont,
    fsBitmapFormat format,
    Mask        flags,
    unsigned long num_ranges,
    fsRange    *range,
    int        *size,
    unsigned long *num_glyphs,
    fsOffset32  **offsets,
    pointer    *data,
    int		*freeData)
{
    int err;

    assert(pfont);

    *size = 0;
    *data = (pointer) 0;

    err = LoadGlyphRanges(client, pfont, TRUE, num_ranges * 2, 0, 
			  (fsChar2b *)range);

    if (err != Successful)
	return err;

    return packGlyphs (client, pfont, format, flags,
			      num_ranges, range, size, num_glyphs,
			      offsets, data, freeData);
}
