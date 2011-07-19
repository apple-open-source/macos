/*
 
Copyright 1990, 1998  The Open Group

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

 * Copyright 1990 Network Computing Devices;
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

#include	<stdio.h>
#include	<X11/Xosdefs.h>
#include	<stdlib.h>
#include	<string.h>
#include	"fstobdf.h"

unsigned long        pointSize;
unsigned long        yResolution;

static char *warning[] =
{
    "COMMENT  ",
    "COMMENT  WARNING:  This bdf file was generated from a font server using",
    "COMMENT  fstobdf.  The resulting font is subject to the same copyright,",
    "COMMENT  license, and trademark restrictions as the original font.  The",
    "COMMENT  authors and distributors of fstobdf disclaim all liability for",
    "COMMENT  misuse of the program or its output.",
    "COMMENT  ",
    NULL
};

static char *
FindStringProperty(char *propName, 
		   int *propLength, 
		   FSPropInfo *propInfo, 
		   FSPropOffset *propOffsets, 
		   unsigned char *propData)
{
    FSPropOffset *propOffset;
    int         length;
    int         i;

    propOffset = &propOffsets[0];
    length = strlen(propName);
    for (i = propInfo->num_offsets; i--; propOffset++) {
	if (propOffset->type == PropTypeString) {

#ifdef DEBUG
	    char        pname[256];

	    memmove( pname, propData + propOffset->name.position, 
		  propOffset->name.length);
	    pname[propOffset->name.length] = '\0';
	    fprintf(stderr, "prop name: %s (len %d)\n",
		    pname, propOffset->name.length);
#endif

	    if ((propOffset->name.length == length) &&
		    !strncmp((char*)propData + propOffset->name.position, propName, length)) {
		*propLength = propOffset->value.length;
		return (char *)(propData + propOffset->value.position);
	    }
	}
    }
    *propLength = 0;
    return (NULL);
}

static int
FindNumberProperty(char *propName, 
		   unsigned long *propValue, 
		   FSPropInfo *propInfo, 
		   FSPropOffset *propOffsets, 
		   unsigned char *propData)
{
    FSPropOffset *propOffset;
    int         i;
    int         length;

    propOffset = &propOffsets[0];
    length = strlen(propName);
    for (i = propInfo->num_offsets; i--; propOffset++) {
	if ((propOffset->type == PropTypeSigned) ||
		(propOffset->type == PropTypeUnsigned)) {
	    if ((propOffset->name.length == length) &&
		    !strncmp((char*)propData + propOffset->name.position, propName, length)) {
		*propValue = propOffset->value.position;
		return (propOffset->type);
	    }
	}
    }
    return (-1);
}

/*
 * EmitHeader - print STARTFONT, COMMENT lines, FONT, SIZE, and
 * FONTBOUNDINGBOX lines
 */
Bool
EmitHeader(FILE *outFile, 
	   FSXFontInfoHeader *fontHeader, 
	   FSPropInfo *propInfo, 
	   FSPropOffset *propOffsets, 
	   unsigned char *propData)
{
    int         len;
    int         type;
    char       *cp;
    char      **cpp;
    unsigned long xResolution;

    fprintf(outFile, "STARTFONT 2.1\n");

    /*
     * find COPYRIGHT message and print it first, followed by warning
     */
    cp = FindStringProperty("COPYRIGHT", &len, propInfo, propOffsets,
			    propData);
    if (cp) {
	fprintf(outFile, "COMMENT  \nCOMMENT  ");
	fwrite(cp, 1, len, outFile);
	fputc('\n', outFile);
    }
    for (cpp = warning; *cpp; cpp++)
	fprintf(outFile, "%s\n", *cpp);

    /*
     * FONT name
     */
    cp = FindStringProperty("FONT", &len, propInfo, propOffsets, propData);
    if (cp) {
	fprintf(outFile, "FONT ");
	fwrite(cp, 1, len, outFile);
	fputc('\n', outFile);
    } else {
	fprintf(stderr, "unable to find FONT property\n");
	return (False);
    }

    /*
     * SIZE point xres yres
     * 
     * Get XLFD values if possible, else fake it
     */
    type = FindNumberProperty("RESOLUTION_X", &xResolution, propInfo,
			      propOffsets, propData);
    if ((type != PropTypeUnsigned) && (type != PropTypeSigned))
	xResolution = 72;

    type = FindNumberProperty("RESOLUTION_Y", &yResolution, propInfo,
			      propOffsets, propData);
    if ((type != PropTypeUnsigned) && (type != PropTypeSigned))
	yResolution = 72;

    type = FindNumberProperty("POINT_SIZE", &pointSize, propInfo,
			      propOffsets, propData);
    if ((type == PropTypeUnsigned) || (type == PropTypeSigned))
	pointSize = (pointSize + 5) / 10;
    else
	pointSize = ((fontHeader->font_ascent + fontHeader->font_descent)
		     * 72) / yResolution;

    fprintf(outFile, "SIZE %lu %lu %lu\n", pointSize, xResolution, 
	    yResolution);

    /*
     * FONTBOUNDINGBOX width height xoff yoff
     * 
     */
    fprintf(outFile, "FONTBOUNDINGBOX %d %d %d %d\n",
	    fontHeader->max_bounds.right - fontHeader->min_bounds.left,
	    fontHeader->max_bounds.ascent + fontHeader->max_bounds.descent,
	    fontHeader->min_bounds.left,
	    -fontHeader->max_bounds.descent);
    return (True);
}
