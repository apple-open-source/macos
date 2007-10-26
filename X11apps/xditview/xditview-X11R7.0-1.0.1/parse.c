/* $XConsortium: parse.c,v 1.13 94/04/17 20:43:36 keith Exp $ */
/*

Copyright (c) 1991  X Consortium

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
/* $XFree86: xc/programs/xditview/parse.c,v 1.5tsi Exp $ */

/*
 * parse.c
 *
 * parse dvi input
 */

#include <X11/Xos.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <stdio.h>
#include <ctype.h>
#include "DviP.h"

static void ParseDrawFunction(DviWidget dw, char *buf);
static void ParseDeviceControl(DviWidget dw);
static void PutCharacters(DviWidget dw, unsigned char *src, int len);
static void push_env(DviWidget dw);
static void pop_env(DviWidget dw);

#define HorizontalMove(dw, delta)	((dw)->dvi.state->x += (delta))

#ifdef USE_XFT
static int
charWidth (DviWidget dw, XftFont *font, char c)
{
    XGlyphInfo	extents;

    XftTextExtents8 (XtDisplay (dw), font, 
		     (unsigned char *) &c, 1, &extents);
    return extents.xOff;
}
#else
#define charWidth(dw,fi,c) (\
    (fi)->per_char ?\
	(fi)->per_char[(c) - (fi)->min_char_or_byte2].width\
    :\
	(fi)->max_bounds.width\
)
#endif
    
int
ParseInput(dw)
    DviWidget	dw;
{
	int		n, k;
	int		c;
	char		Buffer[BUFSIZ];
	int		NextPage;
	int		prevFont;
	int		otherc;
	unsigned char	tc;

	/*
	 * make sure some state exists
	 */

	if (!dw->dvi.state)
	    push_env (dw);
	for (;;) {
		switch (DviGetC(dw, &c)) {
		case '\n':	
			break;
		case ' ':	/* when input is text */
		case 0:		/* occasional noise creeps in */
			break;
		case '{':	/* push down current environment */
			push_env(dw);
			break;
		case '}':
			pop_env(dw);
			break;
		/*
		 * two motion digits plus a character
		 */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			HorizontalMove(dw, (c-'0')*10 +
					   DviGetC(dw,&otherc)-'0');
			/* fall through */
		case 'c':	/* single ascii character */
			(void) DviGetC(dw,&c);
		    	if (c == ' ')
			    break;
			tc = c;
			PutCharacters (dw, &tc, 1);
			break;
		case 'C':
			GetWord(dw, Buffer, BUFSIZ);
			{
	    	    	    DviCharNameMap	*map;
			    int			i;
			    unsigned char	*ligature;
    	    	    
			    c = -1;
	    	    	    map = QueryFontMap (dw, dw->dvi.state->font_number);
	    	    	    if (map)
			    {
		    	    	c = DviCharIndex (map, Buffer);
				if (c == -1)
				{
				    ligature = DviCharIsLigature (map, Buffer);
				    if (ligature) {
					i = strlen ((char *) ligature);
					PutCharacters (dw, ligature, i);
					break;
   				    }
				}
			    }
			    prevFont = -1;
	    	    	    if (c == -1) {
			    	for (i = 1; (map = QueryFontMap (dw, i)); i++)
				    if (map->special)
				    	if ((c = DviCharIndex (map, Buffer)) != -1) {
					    prevFont = dw->dvi.state->font_number;
					    dw->dvi.state->font_number = i;
					    break;
				    	}
			    }
			    if (c != -1)
			    {
				tc = c;
				PutCharacters (dw, &tc, 1);
			    }
			    if (prevFont != -1)
				dw->dvi.state->font_number = prevFont;
			}
			break;
		case 'D':	/* draw function */
			GetLine(dw, Buffer, BUFSIZ);
			ParseDrawFunction(dw, Buffer);
			break;
		case 's':	/* ignore fractional sizes */
			n = GetNumber(dw);
			if (!dw->dvi.size_scale)
			{
			    static int	guesses[] = { 1, 4, 100, 1000, 1 };
			    int		i;

			    for (i = 0; i < 4; i++)
				if (8 <= n/guesses[i] && n/guesses[i] <= 24)
				{
				    break;
				}
    			    dw->dvi.size_scale = guesses[i];
			}
			dw->dvi.state->font_size = n;
			dw->dvi.state->line_width = n * (dw->dvi.device_resolution / 
							 (720 * dw->dvi.size_scale));
			break;
		case 'f':
			n = GetNumber(dw);
			dw->dvi.state->font_number = n;
			break;
		case 'H':	/* absolute horizontal motion */
			k = GetNumber(dw);
			HorizontalGoto(dw, k);
			break;
		case 'h':	/* relative horizontal motion */
			k = GetNumber(dw);
			HorizontalMove(dw, k);
			break;
		case 'w':	/* word space */
			break;
		case 'V':
			n = GetNumber(dw);
			VerticalGoto(dw, n);
			break;
		case 'v':
			n = GetNumber(dw);
			VerticalMove(dw, n);
			break;
		case 'P':	/* new spread */
			break;
		case 'p':	/* new page */
			(void) GetNumber(dw);
			NextPage = dw->dvi.current_page + 1;
			RememberPagePosition(dw, NextPage);
			FlushCharCache (dw);
			return(NextPage);
		case 'n':	/* end of line */
			GetNumber(dw);
			GetNumber(dw);
			HorizontalGoto(dw, 0);
			break;
		case '#':	/* comment */
		case 'F':	/* file info */
			GetLine(dw, NULL, 0);
			break;
		case 't':	/* text */
			GetLine(dw, Buffer, BUFSIZ);
			PutCharacters (dw, (unsigned char *)Buffer,
				       strlen (Buffer));
			dw->dvi.state->x = ToDevice (dw, dw->dvi.cache.x);
			break;
		case 'x':	/* device control */
			ParseDeviceControl(dw);
			break;
		case EOF:
			dw->dvi.last_page = dw->dvi.current_page;
			FlushCharCache (dw);
			return dw->dvi.current_page;
		default:
			GetLine (dw, Buffer, BUFSIZ);
			fprintf (stderr, "Unknown command %s\n", Buffer);
			break;
		}
	}
}

static void
push_env(dw)
	DviWidget	dw;
{
	DviState	*new;

	new = (DviState *) XtMalloc (sizeof (*new));
	if (dw->dvi.state)
		*new = *(dw->dvi.state);
	else {
		new->font_size = 10 * dw->dvi.size_scale;
		new->font_number = 1;
		new->line_style = 0;
		new->line_width = 10;
		new->x = 0;
		new->y = 0;
	}
	new->next = dw->dvi.state;
	dw->dvi.state = new;
}

static void
pop_env(dw)
	DviWidget	dw;
{
	DviState	*old;

	old = dw->dvi.state;
	dw->dvi.state = old->next;
	XtFree ((char *) old);
}

static void
InitTypesetter (DviWidget dw)
{
	while (dw->dvi.state)
		pop_env (dw);
	dw->dvi.size_scale = dw->dvi.size_scale_set;
	push_env (dw);
	FlushCharCache (dw);
}

static void
SetFont (DviWidget dw)
{
    dw->dvi.cache.font_size = dw->dvi.state->font_size;
    dw->dvi.cache.font_number = dw->dvi.state->font_number;
    dw->dvi.cache.font = QueryFont (dw,
			  dw->dvi.cache.font_number,
			  dw->dvi.cache.font_size);
}

static void
PutCharacters (dw, src, len)
    DviWidget	    dw;
    unsigned char   *src;
    int		    len;
{
    int	    xx, yx;
    int	    fx, fy;
    char    *dst;
    int	    c;

    xx = ToX(dw, dw->dvi.state->x);
    yx = ToX(dw, dw->dvi.state->y);
    fy = FontSizeInPixels (dw, dw->dvi.state->font_size);
    fx = fy * len;
    /*
     * quick and dirty extents calculation:
     */
    if (yx + fy >= dw->dvi.extents.y1 &&
	yx - fy <= dw->dvi.extents.y2 &&
	xx + fx >= dw->dvi.extents.x1 &&
	xx - fx <= dw->dvi.extents.x2)
    {
#ifdef USE_XFT
	XftFont	    *font;
	DviTextItem *text;
#else
	register XFontStruct	*font;
	register XTextItem		*text;
#endif

	if (!dw->dvi.display_enable)
	    return;

	if (yx != dw->dvi.cache.y ||
	    dw->dvi.cache.char_index + len > DVI_CHAR_CACHE_SIZE)
	    FlushCharCache (dw);
	/*
	 * load a new font, if the current block is not empty,
	 * step to the next.
	 */
	if (dw->dvi.cache.font_size != dw->dvi.state->font_size ||
	    dw->dvi.cache.font_number != dw->dvi.state->font_number)
	{
	    SetFont (dw);
	    if (dw->dvi.cache.cache[dw->dvi.cache.index].nchars != 0) {
		++dw->dvi.cache.index;
		if (dw->dvi.cache.index >= dw->dvi.cache.max)
		    FlushCharCache (dw);
		dw->dvi.cache.cache[dw->dvi.cache.index].nchars = 0;
	    }
	}
	if (xx != dw->dvi.cache.x) {
	    if (dw->dvi.cache.cache[dw->dvi.cache.index].nchars != 0) {
		++dw->dvi.cache.index;
		if (dw->dvi.cache.index >= dw->dvi.cache.max)
		    FlushCharCache (dw);
		dw->dvi.cache.cache[dw->dvi.cache.index].nchars = 0;
	    }
	}
	if (!dw->dvi.cache.font)
	    SetFont (dw);
	text = &dw->dvi.cache.cache[dw->dvi.cache.index];
	font = dw->dvi.cache.font;
	dst = &dw->dvi.cache.char_cache[dw->dvi.cache.char_index];
	if (text->nchars == 0) {
	    text->chars = dst;
#ifdef USE_XFT
	    text->x = xx;
#else
	    text->delta = xx - dw->dvi.cache.x;
#endif
#ifdef USE_XFT
	    text->font = font;
#endif
	    if (font != dw->dvi.font) {
#ifndef USE_XFT
		text->font = font->fid;
#endif
		dw->dvi.font = font;
	    }
#ifndef USE_XFT
	    else
		text->font = None;
#endif
	    dw->dvi.cache.x = xx;
	}
	dw->dvi.cache.char_index += len;
	text->nchars += len;
	while (len--)
	{
	    c = *src++;
	    *dst++ = c;
	    if (font)
		dw->dvi.cache.x += charWidth(dw,font,c);
	}
    }
}

static void
ParseDrawFunction(dw, buf)
	DviWidget	dw;
	char		*buf;
{
    int	n, m, n1, m1;

    SetGCForDraw (dw);
    switch (buf[0]) {
    case 'l':				/* draw a line */
	sscanf(buf+1, "%d %d", &n, &m);
	DrawLine(dw, n, m);
	break;
    case 'c':				/* circle */
	sscanf(buf+1, "%d", &n);
	DrawCircle(dw, n);
	break;
    case 'e':				/* ellipse */
	sscanf(buf+1, "%d %d", &m, &n);
	DrawEllipse(dw, m, n);
	break;
    case 'a':				/* arc */
	sscanf(buf+1, "%d %d %d %d", &n, &m, &n1, &m1);
	DrawArc(dw, n, m, n1, m1);
	break;
    case '~':				/* wiggly line */
	DrawSpline(dw, buf+1,1);
	break;
    case 't':				/* line width */
	sscanf(buf+1, "%d", &n);
	dw->dvi.state->line_width = n;
	break;
    case 's':				/* line style */
	sscanf(buf+1, "%d", &n);
	/* XXX */
	break;
    default:
	/* warning("unknown drawing function %s", buf); */
	break;
    }
} 

extern int LastPage, CurrentPage;

static void
ParseDeviceControl(dw)				/* Parse the x commands */
	DviWidget	dw;
{
    char str[20], str1[50];
    int c, n;

    GetWord (dw, str, 20);
    switch (str[0]) {			/* crude for now */
    case 'T':				/* output device */
	GetWord(dw, str, 20);
	break;
    case 'i':				/* initialize */
	InitTypesetter (dw);
	break;
    case 't':				/* trailer */
	break;
    case 'p':				/* pause -- can restart */
	break;
    case 's':				/* stop */
	return;
    case 'r':				/* resolution when prepared */
	SetDeviceResolution (dw, GetNumber (dw));
	break;
    case 'f':				/* font used */
	n = GetNumber(dw);
	GetWord(dw, str, 20);
	GetLine(dw, str1, 50);
	SetFontPosition(dw, n, str, str1);
	break;
    case 'H':				/* char height */
	break;
    case 'S':				/* slant */
	break;
    }
    while (DviGetC(dw,&c) != '\n')		/* skip rest of input line */
	    if (c == EOF)
		    return;
}
