/*
 * Copyright © 2002 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "xcursorint.h"
#include <X11/Xlibint.h>
#include <X11/Xatom.h>
#include <stdlib.h>

static XcursorBool
_XcursorFontIsCursor (Display *dpy, Font font)
{
    XcursorFontInfo	*fi;
    XcursorDisplayInfo	*info;
    XcursorBool		ret;
    XFontStruct		*fs;
    int			n;
    Atom		cursor;

    if (!dpy || !font)
        return XcursorFalse;

    if (font == dpy->cursor_font)
	return XcursorTrue;

    info = _XcursorGetDisplayInfo (dpy);
    if (!info)
	return XcursorFalse;
    LockDisplay (dpy);
    for (fi = info->fonts; fi; fi = fi->next)
	if (fi->font == font)
	{
	    ret = fi->is_cursor_font;
	    UnlockDisplay (dpy);
	    return ret;
	}
    UnlockDisplay (dpy);
    ret = XcursorFalse;
    fs = XQueryFont (dpy, font);
    if (fs)
    {
	cursor = XInternAtom (dpy, "cursor", False);
	for (n = 0; n < fs->n_properties; n++)
	    if (fs->properties[n].name == XA_FONT)
	    {
		ret = (fs->properties[n].card32 == cursor);
		break;
	    }
    }
    fi = malloc (sizeof (XcursorFontInfo));
    if (fi)
    {
	fi->font = font;
	fi->is_cursor_font = ret;
	LockDisplay (dpy);
	fi->next = info->fonts;
	info->fonts = fi;
	UnlockDisplay (dpy);
    }
    return ret;
}

Cursor
XcursorTryShapeCursor (Display	    *dpy,
		       Font	    source_font,
		       Font	    mask_font,
		       unsigned int source_char,
		       unsigned int mask_char,
		       XColor _Xconst *foreground,
		       XColor _Xconst *background)
{
    Cursor  cursor = None;

    if (!dpy || !source_font || !mask_font || !foreground || !background)
        return 0;
    
    if (!XcursorSupportsARGB (dpy) && !XcursorGetThemeCore (dpy))
	return None;
    
    if (source_font == mask_font && 
	_XcursorFontIsCursor (dpy, source_font) &&
	source_char + 1 == mask_char)
    {
	int		size = XcursorGetDefaultSize (dpy);
	char		*theme = XcursorGetTheme (dpy);
	XcursorImages   *images = XcursorShapeLoadImages (source_char, theme, size);

	if (images)
	{
	    cursor = XcursorImagesLoadCursor (dpy, images);
	    XcursorImagesDestroy (images);
	}
    }
    return cursor;
}

void
XcursorNoticeCreateBitmap (Display	*dpy,
			   Pixmap	pid,
			   unsigned int width,
			   unsigned int height)
{
    XcursorDisplayInfo	*info;
    unsigned long	oldest;
    unsigned long	now;
    int			i;
    int			replace = 0;
    XcursorBitmapInfo	*bmi;

    if (!dpy)
        return;

    if (!XcursorSupportsARGB (dpy) && !XcursorGetThemeCore (dpy))
	return;
    
    if (width > MAX_BITMAP_CURSOR_SIZE || height > MAX_BITMAP_CURSOR_SIZE)
	return;
    
    info = _XcursorGetDisplayInfo (dpy);
    if (!info)
	return;
    
    LockDisplay (dpy);
    replace = 0;
    now = dpy->request;
    oldest = now;
    for (i = 0; i < NUM_BITMAPS; i++)
    {
	if (!info->bitmaps[i].bitmap)
	{
	    replace = i;
	    break;
	}
	if ((long) (now - info->bitmaps[i].sequence) >
	    (long) (now - oldest))
	{
	    replace = i;
	    oldest = info->bitmaps[i].sequence;
	}
    }
    bmi = &info->bitmaps[replace];
    bmi->bitmap = pid;
    bmi->sequence = now;
    bmi->width = width;
    bmi->height = height;
    bmi->has_image = False;
    UnlockDisplay (dpy);
}

static XcursorBitmapInfo *
_XcursorGetBitmap (Display *dpy, Pixmap bitmap)
{
    XcursorDisplayInfo	*info;
    int			i;

    if (!dpy || !bitmap)
        return NULL;
    
    info = _XcursorGetDisplayInfo (dpy);

    if (!info)
	return NULL;
    LockDisplay (dpy);
    for (i = 0; i < NUM_BITMAPS; i++)
	if (info->bitmaps[i].bitmap == bitmap)
	{
	    info->bitmaps[i].sequence = dpy->request;
	    UnlockDisplay (dpy);
	    return &info->bitmaps[i];
	}
    UnlockDisplay (dpy);
    return NULL;
}

static Bool
_XcursorClientLSB (void)
{
    int	    v = 1;
    return *((char *) &v) == 1;
}

/* stolen from Xlib */
static unsigned char const _reverse_byte[0x100] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};
    
#define RotByte(t,i)    (((t) << (i)) | ((t) >> (8 - (i))))

void
XcursorImageHash (XImage	  *image,
		  unsigned char   hash[XCURSOR_BITMAP_HASH_SIZE])
{
    int		    i;
    int		    x, y;
    unsigned char   *line;
    unsigned char   t;
    int		    low_addr;
    Bool	    bit_swap;

    if (!image)
        return;

    for (i = 0; i < XCURSOR_BITMAP_HASH_SIZE; i++)
	hash[i] = 0;
    /*
     * Flip byte order on MSB machines where the bitmap_unit isn't
     * in bytes
     */
    low_addr = 0;
    if (image->bitmap_unit != 8)
    {
	if (!_XcursorClientLSB())
	    switch (image->bitmap_unit) {
	    case 16:
		low_addr = 1;
		break;
	    case 32:
		low_addr = 3;
		break;
	    }
    }
    /*
     * Flip bit order on MSB images
     */
    bit_swap = (image->bitmap_bit_order != LSBFirst);
    
    line = (unsigned char *) image->data;
    i = 0;
    /*
     * Compute the hash.  Yes, it might be nice to use
     * a stronger hash function, but MD5 and SHA1 are both
     * a bit to expensive in time and space for this,
     * and cursors are generally small enough that a weak
     * hash is sufficient to distinguish among them.
     */
    for (y = 0; y < image->height; y++)
    {
	for (x = 0; x < image->bytes_per_line; x++)
	{
	    t = line[x^low_addr];
	    if (bit_swap)
		t = _reverse_byte[t];
	    if (t)
		hash[(i++) & (XCURSOR_BITMAP_HASH_SIZE - 1)] ^= RotByte (t, y & 7);
	}
	line += image->bytes_per_line;
    }
}

static Bool
_XcursorLogDiscover (void)
{
    static Bool	been_here;
    static Bool	log;

    if (!been_here)
    {
	been_here = True;

	if (getenv ("XCURSOR_DISCOVER"))
	    log = True;
    }
    return log;
}
    
void
XcursorNoticePutBitmap (Display	    *dpy,
			Drawable    draw,
			XImage	    *image)
{
    XcursorBitmapInfo	*bmi;

    if (!dpy || !image)
        return;
    
    if (!XcursorSupportsARGB (dpy) && !XcursorGetThemeCore (dpy))
	return;
    
    if (image->width > MAX_BITMAP_CURSOR_SIZE || 
	image->height > MAX_BITMAP_CURSOR_SIZE)
	return;
    
    bmi = _XcursorGetBitmap (dpy, (Pixmap) draw);
    if (!bmi)
	return;
    /*
     * Make sure the image fills the bitmap
     */
    if (image->width != bmi->width || image->height != bmi->height)
    {
	bmi->bitmap = 0;
	return;
    }
    /*
     * If multiple images are placed in the same bitmap,
     * assume it's not going to be a cursor
     */
    if (bmi->has_image)
    {
	bmi->bitmap = 0;
	return;
    }
    /*
     * Make sure the image is valid
     */
    if (image->bytes_per_line & ((image->bitmap_unit >> 3) - 1))
    {
	bmi->bitmap = 0;
	return;
    }
    /*
     * Hash the image
     */
    XcursorImageHash (image, bmi->hash);
    /*
     * Display the hash value and the image if
     * requested so that users can find out what
     * cursor name is associated with each image
     */
    if (_XcursorLogDiscover())
    {
	int x, y;
	int i;
	XImage	t = *image;

	XInitImage (&t);
	
	printf ("Cursor image name: ");
	for (i = 0; i < XCURSOR_BITMAP_HASH_SIZE; i++)
	    printf ("%02x", bmi->hash[i]);
	printf ("\n");
	for (y = 0; y < image->height; y++)
	{
	    for (x = 0; x < image->width; x++)
		putchar (XGetPixel (&t, x, y) ? '*' : ' ');
	    putchar ('\n');
	}
    }
    bmi->has_image = True;
}

Cursor
XcursorTryShapeBitmapCursor (Display		*dpy,
			     Pixmap		source,
			     Pixmap		mask,
			     XColor		*foreground,
			     XColor		*background,
			     unsigned int	x,
			     unsigned int	y)
{
    XcursorBitmapInfo	*bmi;
    char		name[8 * XCURSOR_BITMAP_HASH_SIZE];
    int			i;
    Cursor		cursor;

    if (!dpy || !foreground || !background)
        return 0;

    if (!XcursorSupportsARGB (dpy) && !XcursorGetThemeCore (dpy))
	return None;
    
    bmi = _XcursorGetBitmap (dpy, source);
    if (!bmi || !bmi->has_image)
	return None;
    for (i = 0; i < XCURSOR_BITMAP_HASH_SIZE; i++)
	sprintf (name + 2 * i, "%02x", bmi->hash[i]);
    cursor = XcursorLibraryLoadCursor (dpy, name);
    if (_XcursorLogDiscover())
	printf ("Cursor hash %s returns 0x%x\n", name, (unsigned int) cursor);
    return cursor;
}
