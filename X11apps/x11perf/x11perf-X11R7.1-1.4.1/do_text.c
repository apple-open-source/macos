/* $Xorg: do_text.c,v 1.4 2000/08/17 19:54:10 cpqbld Exp $ */
/*****************************************************************************
Copyright 1988, 1989 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************************/
/* $XFree86: xc/programs/x11perf/do_text.c,v 1.7 2001/01/17 23:45:12 dawes Exp $ */

#include "x11perf.h"
#include <stdio.h>

static char **charBuf;
static XFontStruct *font, *bfont;
static int height, ypos;
static XTextItem *items;
static int charsPerLine, totalLines;

#define XPOS 20
#define SEGS 3


int 
InitText(XParms xp, Parms p, int reps)
{
    int		i, j;
    char	ch;
    XGCValues   gcv;

    font = XLoadQueryFont(xp->d, p->font);
    if (font == NULL) {
	printf("Could not load font '%s', benchmark omitted\n", p->font);
	return 0;
    }

    bfont = NULL;
    if (p->bfont != NULL) {
	bfont = XLoadQueryFont(xp->d, p->bfont);
	if (bfont == NULL) {
	    printf("Could not load font '%s', benchmark omitted\n", p->bfont);
	    return 0;
	}
    }

    ypos = XPOS;
    height = (font->max_bounds.ascent + font->max_bounds.descent) + 1;
    if (bfont != NULL) {
	int     h = (bfont->max_bounds.ascent + bfont->max_bounds.descent) + 1;
	if (h > height)
	    height = h;
    }
    gcv.font = font->fid;
    XChangeGC(xp->d, xp->fggc, GCFont, &gcv);
    XChangeGC(xp->d, xp->bggc, GCFont, &gcv);

    charsPerLine = p->objects;
    charsPerLine = (charsPerLine + 3) & ~3;
    p->objects = charsPerLine;

    totalLines = '\177' - ' ' + 1;
    if (totalLines > reps) totalLines = reps;

    charBuf = (char **) malloc(totalLines*sizeof (char *));
    if (p->special)
	items = (XTextItem *) malloc(totalLines*SEGS*sizeof (XTextItem));

    for (i = 0; i != totalLines; i++) {
	charBuf[i] = (char *) malloc (sizeof (char)*charsPerLine);
	ch = i + ' ';
	for (j = 0; j != charsPerLine; j++) {
	    charBuf[i][j] = ch;
	    if (ch == '\177') ch = ' '; else ch++;
	}
	if (p->special) {
	    items[i*SEGS+0].chars = &(charBuf[i][0]);
	    items[i*SEGS+0].nchars = charsPerLine/4;
	    items[i*SEGS+0].delta = 0;
	    items[i*SEGS+0].font = font->fid;
	    items[i*SEGS+1].chars = &(charBuf[i][charsPerLine/4]);
	    items[i*SEGS+1].nchars = charsPerLine/2;
	    items[i*SEGS+1].delta = 3;
	    items[i*SEGS+1].font = bfont->fid;
	    items[i*SEGS+2].chars = &(charBuf[i][3*charsPerLine/4]);
	    items[i*SEGS+2].nchars = charsPerLine/4;
	    items[i*SEGS+2].delta = 3;
	    items[i*SEGS+2].font = font->fid;
	}
    }
    return reps;
}


#define GetRealChar(font, totalChars, ch)				\
{									\
    XCharStruct *pci;							\
    do {								\
	ch--;								\
	if (ch < 0) {							\
	    ch = totalChars-1;						\
	}								\
	if (font->per_char == NULL) break;				\
	pci = &(font->per_char[ch]);					\
    } while ( (pci->lbearing | pci->rbearing | pci->width		\
             | pci->ascent | pci->descent | pci->attributes) == 0);     \
} /* GetRealChar */

int 
InitText16(XParms xp, Parms p, int reps)
{
    register int	i, j;
    register char	*pbuf0, *pbuf1, *pbuf2;
    XGCValues   	gcv;
    int			rows, columns, totalChars, ch;
    int			brows, bcolumns = 0, btotalChars = 0, bch = 0;

    font = XLoadQueryFont(xp->d, p->font);
    if (font == NULL) {
	printf("Could not load font '%s', benchmark omitted\n", p->font);
	return 0;
    }
    rows = font->max_byte1 - font->min_byte1 + 1;
    columns = font->max_char_or_byte2 - font->min_char_or_byte2 + 1;
    totalChars = rows * columns;
    totalLines = rows;
    ch = totalChars;

    bfont = NULL;
    if (p->bfont != NULL) {
	bfont = XLoadQueryFont(xp->d, p->bfont);
	if (bfont == NULL) {
	    printf("Could not load font '%s', benchmark omitted\n", p->bfont);
	    return 0;
	}
	brows = bfont->max_byte1 - bfont->min_byte1 + 1;
	bcolumns = bfont->max_char_or_byte2 - bfont->min_char_or_byte2 + 1;
	btotalChars = brows * bcolumns;
	bch = btotalChars;
	if (brows > totalLines) totalLines = brows;
    }

    ypos = XPOS;
    height = (font->max_bounds.ascent + font->max_bounds.descent) + 1;
    if (bfont != NULL) {
	int     h = (bfont->max_bounds.ascent + bfont->max_bounds.descent) + 1;
	if (h > height)
	    height = h;
    }
    gcv.font = font->fid;
    XChangeGC(xp->d, xp->fggc, GCFont, &gcv);
    XChangeGC(xp->d, xp->bggc, GCFont, &gcv);

    charsPerLine = p->objects;

    if (totalLines > reps) totalLines = reps;

    if (p->special) {
	charsPerLine = (charsPerLine + 3) & ~3;	/* make a multiple of four */
	p->objects = charsPerLine;

	items = (XTextItem *) malloc(totalLines*SEGS*sizeof (XTextItem));

	for (i = 0; i < totalLines; i++) {
	    pbuf0 = items[i*SEGS+0].chars =
			    (char *) malloc (sizeof (char)*charsPerLine/2);
	    items[i*SEGS+0].nchars = charsPerLine/4;
	    items[i*SEGS+0].delta = 0;
	    items[i*SEGS+0].font = font->fid;
	    pbuf1 = items[i*SEGS+1].chars =
			    (char *) malloc (sizeof (char)*charsPerLine);
	    items[i*SEGS+1].nchars = charsPerLine/2;
	    items[i*SEGS+1].delta = 3;
	    items[i*SEGS+1].font = bfont->fid;
	    pbuf2 = items[i*SEGS+2].chars =
			    (char *) malloc (sizeof (char)*charsPerLine/2);
	    items[i*SEGS+2].nchars = charsPerLine/4;
	    items[i*SEGS+2].delta = 3;
	    items[i*SEGS+2].font = font->fid;
	    for (j = 0; j < charsPerLine/4; j++) {
		GetRealChar(font, totalChars, ch);
		*pbuf0++ = ch / columns + font->min_byte1;
		*pbuf0++ = ch % columns + font->min_char_or_byte2;
		GetRealChar(font, totalChars, ch);
		*pbuf2++ = ch / columns + font->min_byte1;
		*pbuf2++ = ch % columns + font->min_char_or_byte2;
	    }
	    for (j = 0; j < charsPerLine/2; j++) {
		GetRealChar(bfont, btotalChars, bch);
		*pbuf1++ = bch / bcolumns + bfont->min_byte1;
		*pbuf1++ = bch % bcolumns + bfont->min_char_or_byte2;
	    }
	}
    } else {
	charBuf = (char **) malloc(totalLines*sizeof (char *));
	for (i = 0; i < totalLines; i++) {
	    pbuf0 = charBuf[i] = (char *) malloc (sizeof (char)*charsPerLine*2);
	    for (j = 0; j < charsPerLine; j++) {
		GetRealChar(font, totalChars, ch);
		*pbuf0++ = ch / columns + font->min_byte1;
		*pbuf0++ = ch % columns + font->min_char_or_byte2;
	    }
	}
    }
    return reps;
}

void 
DoText(XParms xp, Parms p, int reps)
{
    int     i, line, startLine;

    startLine = 0;
    line = 0;
    for (i = 0; i != reps; i++) {
	XDrawString(
	    xp->d, xp->w, xp->fggc, XPOS, ypos, charBuf[line], charsPerLine);
	ypos += height;
	if (ypos > HEIGHT - height) {
	    /* Wraparound to top of window */
	    ypos = XPOS;
	    line = startLine;
	    startLine = (startLine + 1) % totalLines;
	}
	line = (line + 1) % totalLines;
	CheckAbort ();
    }
}

void 
DoText16(XParms xp, Parms p, int reps)
{
    int     i, line, startLine;

    startLine = 0;
    line = 0;
    for (i = 0; i < reps; i++) {
	XDrawString16(
	    xp->d, xp->w, xp->fggc, XPOS, ypos, (XChar2b *)charBuf[line], charsPerLine);
	ypos += height;
	if (ypos > HEIGHT - height) {
	    /* Wraparound to top of window */
	    ypos = XPOS;
	    line = startLine;
	    startLine = (startLine + 1) % totalLines;
	}
	line = (line + 1) % totalLines;
	CheckAbort ();
    }
}

void 
DoPolyText(XParms xp, Parms p, int reps)
{
    int     i, line, startLine;

    startLine = 0;
    line = 0;
    for (i = 0; i != reps; i++) {
	XDrawText(
	    xp->d, xp->w, xp->fggc, XPOS, ypos, &items[line*SEGS], SEGS);
	ypos += height;
	if (ypos > HEIGHT - height) {
	    /* Wraparound to top of window */
	    ypos = XPOS;
	    line = startLine;
	    startLine = (startLine + 1) % totalLines;
	}
	line = (line + 1) % totalLines;
	CheckAbort ();
    }
}

void 
DoPolyText16(XParms xp, Parms p, int reps)
{
    int     i, line, startLine;

    startLine = 0;
    line = 0;
    for (i = 0; i != reps; i++) {
	XDrawText16(
	    xp->d, xp->w, xp->fggc, XPOS, ypos, (XTextItem16 *)&items[line*SEGS], SEGS);
	ypos += height;
	if (ypos > HEIGHT - height) {
	    /* Wraparound to top of window */
	    ypos = XPOS;
	    line = startLine;
	    startLine = (startLine + 1) % totalLines;
	}
	line = (line + 1) % totalLines;
	CheckAbort ();
    }
}

void 
DoImageText(XParms xp, Parms p, int reps)
{
    int     i, line, startLine;

    startLine = 0;
    line = 0;
    for (i = 0; i != reps; i++) {
	XDrawImageString(
	    xp->d, xp->w, xp->fggc, XPOS, ypos, charBuf[line], charsPerLine);
	ypos += height;
	if (ypos > HEIGHT - height) {
	    /* Wraparound to top of window */
	    ypos = XPOS;
	    startLine = (startLine + 17) % totalLines;
	    line = startLine;
	}
	line = (line + 1) % totalLines;
	CheckAbort ();
    }
}

void 
DoImageText16(XParms xp, Parms p, int reps)
{
    int     i, line, startLine;

    startLine = 0;
    line = 0;
    for (i = 0; i != reps; i++) {
	XDrawImageString16(
	    xp->d, xp->w, xp->fggc, XPOS, ypos, (XChar2b *)charBuf[line], charsPerLine);
	ypos += height;
	if (ypos > HEIGHT - height) {
	    /* Wraparound to top of window */
	    ypos = XPOS;
	    startLine = (startLine + 17) % totalLines;
	    line = startLine;
	}
	line = (line + 1) % totalLines;
	CheckAbort ();
    }
}

void 
ClearTextWin(XParms xp, Parms p)
{
    XClearWindow(xp->d, xp->w);
}

void 
EndText(XParms xp, Parms p)
{
    int i;

    if(font==NULL)return;
    for (i = 0; i != totalLines; i++)
	free(charBuf[i]);
    free(charBuf);
    if (p->special)
	free(items);
    XFreeFont(xp->d, font);
    if (bfont != NULL)
	XFreeFont(xp->d, bfont);
}

void 
EndText16(XParms xp, Parms p)
{
    int i;

    if(font==NULL)return;
    if (p->special) {
	for (i = 0; i < totalLines; i++) {
	    free(items[i*SEGS+0].chars);
	    free(items[i*SEGS+1].chars);
	    free(items[i*SEGS+2].chars);
	}
	free(items);
    } else {
	for (i = 0; i < totalLines; i++) {
	    free(charBuf[i]);
	}
	free(charBuf);
    }
    XFreeFont(xp->d, font);
    if(bfont != NULL) {
	XFreeFont(xp->d, bfont);
    }
}

#ifdef XFT
#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>

static XftFont	*aafont;
static XftDraw	*aadraw;
static XftColor	aacolor;

int 
InitAAText(XParms xp, Parms p, int reps)
{
    int			i, j;
    char		ch;
    XRenderColor	color;

    aafont = XftFontOpenName (xp->d, DefaultScreen (xp->d), p->font);
    
    if (aafont == NULL) 
    {
	printf("Could not load font '%s', benchmark omitted\n", 
	       p->font);
	return 0;
    }

    aadraw = XftDrawCreate (xp->d, xp->w, 
			    xp->vinfo.visual, 
			    xp->cmap);

    if (!aadraw) 
    {
	printf ("Cannot create XftDraw object\n");
	XftFontClose (xp->d, aafont);
	return 0;
    }
    color.red = 0;
    color.green = 0;
    color.blue = 0;
    color.alpha = 0xffff;
    if (!XftColorAllocValue (xp->d,
			     xp->vinfo.visual, 
			     xp->cmap,
			     &color, &aacolor))
    {
	printf ("Cannot allocate black\n");
	XftFontClose (xp->d, aafont);
	XftDrawDestroy (aadraw);
	aafont = 0;
	aadraw = 0;
	return 0;
    }
    
    ypos = XPOS;
    height = aafont->height;
    
    charsPerLine = p->objects;
    charsPerLine = (charsPerLine + 3) & ~3;
    p->objects = charsPerLine;

    totalLines = '\177' - ' ' + 1;
    if (totalLines > reps) totalLines = reps;

    charBuf = (char **) malloc(totalLines*sizeof (char *));

    for (i = 0; i != totalLines; i++) {
	charBuf[i] = (char *) malloc (sizeof (char)*charsPerLine);
	ch = i + ' ';
	for (j = 0; j != charsPerLine; j++) {
	    charBuf[i][j] = ch;
	    if (ch == '\177') ch = ' '; else ch++;
	}
    }
    return reps;
}

void 
DoAAText(XParms xp, Parms p, int reps)
{
    int     i, line, startLine;

    startLine = 0;
    line = 0;
    for (i = 0; i != reps; i++) {
	XftDrawString8 (aadraw, &aacolor, aafont, 
		       XPOS, ypos, (unsigned char *) charBuf[line], charsPerLine);
	ypos += height;
	if (ypos > HEIGHT - height) {
	    /* Wraparound to top of window */
	    ypos = XPOS;
	    line = startLine;
	    startLine = (startLine + 1) % totalLines;
	}
	line = (line + 1) % totalLines;
	CheckAbort ();
    }
}

void 
EndAAText(XParms xp, Parms p)
{
    int i;

    if(!aadraw)return;
    for (i = 0; i != totalLines; i++)
	free(charBuf[i]);
    free(charBuf);
    XftDrawDestroy (aadraw);
    XftFontClose (xp->d, aafont);
    XftColorFree (xp->d,
		  xp->vinfo.visual, 
		  xp->cmap,
		  &aacolor);
}

#endif
