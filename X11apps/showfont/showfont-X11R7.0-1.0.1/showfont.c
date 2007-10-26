/* $XConsortium: showfont.c,v 1.13 94/04/17 20:44:07 gildea Exp $ */
/*
 * Copyright 1990 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation and the
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices or Digital
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 *
 * NETWORK COMPUTING DEVICES AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL NETWORK COMPUTING DEVICES
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */
/*

Copyright (c) 1987  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/
/* $XFree86: xc/programs/showfont/showfont.c,v 1.3 2000/02/18 12:20:17 tsi Exp $ */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<X11/fonts/FSlib.h>

/*
 * the equivalent of showsnf
 */

#define	GLWIDTHBYTESPADDED(bits,nbytes) \
	((nbytes) == 1 ? (((bits)+7)>>3)        /* pad to 1 byte */ \
	:(nbytes) == 2 ? ((((bits)+15)>>3)&~1)  /* pad to 2 bytes */ \
	:(nbytes) == 4 ? ((((bits)+31)>>3)&~3)  /* pad to 4 bytes */ \
	:(nbytes) == 8 ? ((((bits)+63)>>3)&~7)  /* pad to 8 bytes */ \
	: 0)

int         byteorder = MSBFirst; /* -LSB or -MSB */
int         bitorder = MSBFirst; /* -lsb or -msb */
int         bitmap_pad = 0;	/* -bitmap_pad: ImageRect bitmap format */
int         scan_pad = 8;	/* -pad: ScanlinePad */
int         scan_unit = 8;	/* -unit: ScanlineUnit */
int         first_ch = 0;	/* -start: first character*/
int         end_ch = ~0;	/* -end: end character */
const char *ProgramName;
Bool	    no_props = False;	/* -noprops: don't show font properties */
Bool        extents_only = False; /* -extents_only */

FSServer   *svr;

/* set from bitmap_pad to ImageRectMin, ImageMaxWidth, or ImageMax */
int	    bitmap_format;	

static      FSBitmapFormat
make_format(void)
{
    FSBitmapFormat format;

    format = 0;
    /* set up format */
    switch (scan_pad) {
    case 8:
	format |= BitmapFormatScanlinePad8;
	break;
    case 16:
	format |= BitmapFormatScanlinePad16;
	break;
    case 32:
	format |= BitmapFormatScanlinePad32;
	break;
    case 64:
	format |= BitmapFormatScanlinePad64;
	break;
    default:
	fprintf(stderr, "bogus scanline pad value: %d\n", scan_pad);
	break;
    }
    switch (scan_unit) {
    case 8:
	format |= BitmapFormatScanlineUnit8;
	break;
    case 16:
	format |= BitmapFormatScanlineUnit16;
	break;
    case 32:
	format |= BitmapFormatScanlineUnit32;
	break;
    case 64:
	format |= BitmapFormatScanlineUnit64;
	break;
    default:
	fprintf(stderr, "bogus scanline unit value: %d\n", scan_unit);
	break;
    }
    switch (bitmap_pad) {
    case 0:
	bitmap_format = BitmapFormatImageRectMin;
	break;
    case 1:
	bitmap_format = BitmapFormatImageRectMaxWidth;
	break;
    case 2:
	bitmap_format = BitmapFormatImageRectMax;
	break;
    default:
	fprintf(stderr, "bogus bitmap pad value: %d\n", bitmap_pad);
	break;
    }
    format |= bitmap_format;

    format |= (bitorder == MSBFirst) ? BitmapFormatBitOrderMSB :
	BitmapFormatBitOrderLSB;
    format |= (byteorder == MSBFirst) ? BitmapFormatByteOrderMSB :
	BitmapFormatByteOrderLSB;

    return format;
}

static void
show_char_info(FSXCharInfo *ci)
{
    printf("Left: %-3d    Right: %-3d    Ascent: %-3d    Descent: %-3d    Width: %d\n",
	   ci->left, ci->right, ci->ascent, ci->descent, ci->width);
}

static void
show_glyphs(
    Font        fid,
    FSXFontInfoHeader *hdr,
    Bool        show_all,
    FSChar2b    first,
    FSChar2b    last)
{
    FSXCharInfo *extents;
    int         err,
                ch,
                start,
                end;
    int         offset = 0;
    unsigned char *glyphs;
    FSOffset   *offsets;
    int         scanpad;
    int         r,
                b;
    FSBitmapFormat format;
    FSChar2b    chars[2];
    int         num_chars;

    if (show_all) {
	num_chars = 0;
    } else {
	chars[0] = first;
	chars[1] = last;
	num_chars = 2;
    }
    FSQueryXExtents16(svr, fid, True, chars, num_chars, &extents);

    if (!extents_only) {
	format = make_format();
	err = FSQueryXBitmaps16(svr, fid, format, True, chars, num_chars,
				&offsets, &glyphs);

	if (err != FSSuccess) {
	    fprintf(stderr, "QueryGlyphs failed\n");
	    exit(1);
	}
    }
    start = first.low + (first.high << 8);
    end = last.low + (last.high << 8);

    scanpad = scan_pad >> 3;

    for (ch = 0; ch <= (end - start); ch++) {
	int         bottom,
	            bpr,
	            charwidth;

	printf("char #%d", ch + start);
	if ((ch + start >= 0) && (ch + start <= 127) && isprint(ch + start))
	    printf(" '%c'\n", (char) (ch + start));
	else
	    printf(" 0x%04x\n", ch + start);
	show_char_info(&extents[ch]);
	if (extents_only)
	    continue;
	if (offset != offsets[ch].position)
	    fprintf(stderr, "offset mismatch: expected %d, got %d\n",
		    offset, offsets[ch].position);
	switch (bitmap_format) {
	case BitmapFormatImageRectMin:
	    bottom = extents[ch].descent + extents[ch].ascent;
	    charwidth = extents[ch].right - extents[ch].left;
	    break;
	case BitmapFormatImageRectMaxWidth:
	    bottom = extents[ch].descent + extents[ch].ascent;
	    charwidth = hdr->max_bounds.right - hdr->min_bounds.left;
	    break;
	case BitmapFormatImageRectMax:
	    bottom = hdr->max_bounds.ascent +
		hdr->max_bounds.descent;
	    charwidth = hdr->max_bounds.right - hdr->min_bounds.left;
	    break;
	default:
	    bottom = 0;
	    charwidth = 0;
	}

	if (extents[ch].left == 0 &&
	    extents[ch].right == 0 &&
	    extents[ch].width == 0 &&
	    extents[ch].ascent == 0 &&
	    extents[ch].descent == 0)
	{
	    printf ("Nonexistent character\n");
	    continue;
	}
	bpr = GLWIDTHBYTESPADDED(charwidth, scanpad);
	if (offsets[ch].length != bottom * bpr) {
	    fprintf (stderr, "length mismatch: expected %d (%dx%d), got %d\n",
			 bottom * bpr, bpr, bottom, offsets[ch].length);
	}
	offset = offsets[ch].position;
	for (r = 0; r < bottom; r++) {
	    unsigned char *row = glyphs + offset;

	    for (b = 0; b < charwidth; b++) {
		putchar((row[b >> 3] &
			 (1 << (7 - (b & 7)))) ? '#' : '-');
	    }
	    putchar('\n');
	    offset += bpr;
	}
    }
    FSFree((char *) extents);
    if (!extents_only) {
	FSFree((char *) offsets);
	FSFree((char *) glyphs);
    }
}

static void
show_props(
    FSPropInfo *pi,
    FSPropOffset *po,
    unsigned char *pd)
{
    int         i;
    char        buf[512];
    int         num_props;

    num_props = pi->num_offsets;
    for (i = 0; i < num_props; i++, po++) {
	strncpy(buf, (char *) (pd + po->name.position), po->name.length);
	buf[po->name.length] = '\0';
	printf("%s\t", buf);
	switch (po->type) {
	case PropTypeString:
	    strncpy(buf, (char *)(pd + po->value.position), po->value.length);
	    buf[po->value.length] = '\0';
	    printf("%s\n", buf);
	    break;
	case PropTypeUnsigned:
	    printf("%lu\n", (unsigned long) po->value.position);
	    break;
	case PropTypeSigned:
	    printf("%ld\n", (long) po->value.position);
	    break;
	default:
	    fprintf(stderr, "bogus property\n");
	    break;
	}
    }
}

static void
show_info(
    Font        fid,
    FSXFontInfoHeader *hdr,
    FSChar2b   *first,
    FSChar2b   *last)
{
    FSPropInfo  pi;
    FSPropOffset *po;
    unsigned char *pd;

    FSQueryXInfo(svr, fid, hdr, &pi, &po, &pd);
    printf("Direction: %s\n", (hdr->draw_direction == LeftToRightDrawDirection)
	   ? "Left to Right" : "Right to Left");
    *first = hdr->char_range.min_char;
    *last = hdr->char_range.max_char;
    printf("Range:	%d to %d\n",
	   first->low + (first->high << 8),
	   last->low + (last->high << 8));
    if (hdr->flags & FontInfoAllCharsExist)
	printf("All chars exist\n");
    printf("Default char: %d\n",
	   hdr->default_char.low + (hdr->default_char.high << 8));
    printf("Min bounds: \n");
    show_char_info(&hdr->min_bounds);
    printf("Max bounds: \n");
    show_char_info(&hdr->max_bounds);
    printf("Font Ascent: %d  Font Descent: %d\n",
	   hdr->font_ascent, hdr->font_descent);

    if (!no_props)
	show_props(&pi, po, pd);
    FSFree((char *) po);
    FSFree((char *) pd);
}

static void
usage(void)
{
    printf("%s: [-server servername] [-extents_only] [-noprops] [-lsb] [-msb] [-LSB] [-MSB] [-unit #] [-pad #] [-bitmap_pad value] [-start first_char] [-end last_char] -fn fontname\n", ProgramName);
    exit(0);
}

int
main(int argc, char **argv)
{
    char *servername = "localhost:7100"; /* -server: font server name */
    char *fontname = NULL; /* -fn: font name */
    int         i;
    Font        fid,
                dummy;
    FSBitmapFormat format;
    FSBitmapFormatMask fmask;
    FSChar2b    first,
                last;
    FSXFontInfoHeader hdr;
    Bool        show_all = True;

    ProgramName = argv[0];

    for (i = 1; i < argc; i++) {
	if (!strncmp(argv[i], "-se", 3)) {
	    if (++i < argc)
		servername = argv[i];
	    else
		usage();
	} else if (!strncmp(argv[i], "-ext", 4)) {
	    extents_only = True;
	} else if (!strncmp(argv[i], "-noprops", 7)) {
	    no_props = True;
	} else if (!strncmp(argv[i], "-lsb", 4)) {
	    bitorder = LSBFirst;
	} else if (!strncmp(argv[i], "-msb", 4)) {
	    bitorder = MSBFirst;
	} else if (!strncmp(argv[i], "-LSB", 4)) {
	    byteorder = LSBFirst;
	} else if (!strncmp(argv[i], "-MSB", 4)) {
	    byteorder = MSBFirst;
	} else if (!strncmp(argv[i], "-p", 2)) {
	    if (++i < argc)
		scan_pad = atoi(argv[i]);
	    else
		usage();
	} else if (!strncmp(argv[i], "-u", 2)) {
	    if (++i < argc)
		scan_unit = atoi(argv[i]);
	    else
		usage();
	} else if (!strncmp(argv[i], "-b", 2)) {
	    if (++i < argc)
		bitmap_pad = atoi(argv[i]);
	    else
		usage();
	} else if (!strncmp(argv[i], "-st", 3)) {
	    if (++i < argc)
		first_ch = atoi(argv[i]);
	    else
		usage();
	} else if (!strncmp(argv[i], "-e", 2)) {
	    if (++i < argc)
		end_ch = atoi(argv[i]);
	    else
		usage();
	} else if (!strncmp(argv[i], "-f", 2)) {
	    if (++i < argc)
		fontname = argv[i];
	    else
		usage();
	} else
	    usage();
    }
    if (fontname == NULL)
	usage();

    if (first_ch != 0 && end_ch != ~0 && end_ch < first_ch) {
	fprintf(stderr,
		"bad character range -- end (%d) is less than start (%d)\n",
		end_ch, first_ch);
	exit(1);
    }
    if ((svr = FSOpenServer(servername)) == NULL) {
	if(FSServerName(servername) != NULL)
		fprintf(stderr, "can't open server \"%s\"\n", FSServerName(servername));
	else
		fprintf(stderr, "can't open server \"\"\n");
	exit(1);
    }
    format = make_format();
    fmask = (BitmapFormatMaskByte | BitmapFormatMaskBit |
	     BitmapFormatMaskImageRectangle | BitmapFormatMaskScanLinePad |
	     BitmapFormatMaskScanLineUnit);
    fid = FSOpenBitmapFont(svr, format, fmask, fontname, &dummy);
    if (fid) {
	printf("opened font %s\n", fontname);
	show_info(fid, &hdr, &first, &last);
	if (first_ch != 0 &&
		((unsigned)first_ch >= (first.low + (first.high << 8)))) {
	    first.low = first_ch & 0xff;
	    first.high = first_ch >> 8;
	    show_all = False;
	}
	if (end_ch != ~0 &&
		((unsigned)end_ch <= (last.low + (last.high << 8)))) {
	    last.low = end_ch & 0xff;
	    last.high = end_ch >> 8;
	    show_all = False;
	}
	/* make sure the range is legal */
	if ((first.high > last.high) || (first.high == last.high &&
					 first.low > last.low)) {
	    last = first;
	    fprintf(stderr,
		    "adjusting range -- specifed first char is after end\n");
	}
	show_glyphs(fid, &hdr, show_all, first, last);
	FSCloseFont(svr, fid);
    } else {
	fprintf(stderr, "couldn't get font %s\n", fontname);
	FSCloseServer(svr);
	exit(1);
    }
    FSCloseServer(svr);
    exit(0);
}
