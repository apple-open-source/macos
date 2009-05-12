/* $XTermId: charsets.c,v 1.41 2009/01/25 23:39:12 tom Exp $ */

/************************************************************

Copyright 1998-2008,2009 by Thomas E. Dickey

                        All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
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
IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization.

********************************************************/

#include <xterm.h>
#include <data.h>
#include <fontutils.h>

#include <X11/keysym.h>

/*
 * This module performs translation as needed to support the DEC VT220 national
 * replacement character sets.  We assume that xterm's font is based on the ISO
 * 8859-1 (Latin 1) character set, which is almost the same as the DEC
 * multinational character set.  Glyph positions 0-31 have to be the DEC
 * graphic characters, though.
 *
 * References:
 *	"VT220 Programmer Pocket Guide" EK-VT220-HR-002 (2nd ed., 1984), which
 *		contains character charts for the national character sets.
 *	"VT330/VT340 Programmer Reference Manual Volume 1: Text Programming"
 *		EK-VT3XX-TP-001 (1st ed, 1987), which contains a table (2-1)
 *		listing the glyphs which are mapped from the multinational
 *		character set to the national character set.
 *
 * The latter reference, though easier to read, has a few errors and omissions.
 */

/*
 * Translate an input keysym to the corresponding NRC keysym.
 */
unsigned
xtermCharSetIn(unsigned code, int charset)
{
#define MAP(to, from) case from: code = to; break

    if (code >= 128 && code < 256) {
	switch (charset) {
	case 'A':		/* United Kingdom set (or Latin 1)      */
	    if (code == XK_sterling)
		code = 0x23;
	    code &= 0x7f;
	    break;

#if OPT_XMC_GLITCH
	case '?':
#endif
	case '1':		/* Alternate Character ROM standard characters */
	case '2':		/* Alternate Character ROM special graphics */
	case 'B':		/* ASCII set                            */
	    break;

	case '0':		/* special graphics (line drawing)      */
	    break;

	case '4':		/* Dutch */
	    switch (code) {
		MAP(0x23, XK_sterling);
		MAP(0x40, XK_threequarters);
		MAP(0x5b, XK_ydiaeresis);
		MAP(0x5c, XK_onehalf);
		MAP(0x5d, XK_bar);	/* glyph is not ISO-8859-1 */
		MAP(0x7b, XK_diaeresis);
		MAP(0x7c, XK_f);	/* glyph is not ISO-8859-1 */
		MAP(0x7d, XK_onequarter);
		MAP(0x7e, XK_acute);
	    }
	    break;

	case 'C':
	case '5':		/* Finnish */
	    switch (code) {
		MAP(0x5b, XK_Adiaeresis);
		MAP(0x5c, XK_Odiaeresis);
		MAP(0x5d, XK_Aring);
		MAP(0x5e, XK_Udiaeresis);
		MAP(0x60, XK_eacute);
		MAP(0x7b, XK_adiaeresis);
		MAP(0x7c, XK_odiaeresis);
		MAP(0x7d, XK_aring);
		MAP(0x7e, XK_udiaeresis);
	    }
	    break;

	case 'R':		/* French */
	    switch (code) {
		MAP(0x23, XK_sterling);
		MAP(0x40, XK_agrave);
		MAP(0x5b, XK_degree);
		MAP(0x5c, XK_ccedilla);
		MAP(0x5d, XK_section);
		MAP(0x7b, XK_eacute);
		MAP(0x7c, XK_ugrave);
		MAP(0x7d, XK_egrave);
		MAP(0x7e, XK_diaeresis);
	    }
	    break;

	case 'Q':		/* French Canadian */
	    switch (code) {
		MAP(0x40, XK_agrave);
		MAP(0x5b, XK_acircumflex);
		MAP(0x5c, XK_ccedilla);
		MAP(0x5d, XK_ecircumflex);
		MAP(0x5e, XK_icircumflex);
		MAP(0x60, XK_ocircumflex);
		MAP(0x7b, XK_eacute);
		MAP(0x7c, XK_ugrave);
		MAP(0x7d, XK_egrave);
		MAP(0x7e, XK_ucircumflex);
	    }
	    break;

	case 'K':		/* German */
	    switch (code) {
		MAP(0x40, XK_section);
		MAP(0x5b, XK_Adiaeresis);
		MAP(0x5c, XK_Odiaeresis);
		MAP(0x5d, XK_Udiaeresis);
		MAP(0x7b, XK_adiaeresis);
		MAP(0x7c, XK_odiaeresis);
		MAP(0x7d, XK_udiaeresis);
		MAP(0x7e, XK_ssharp);
	    }
	    break;

	case 'Y':		/* Italian */
	    switch (code) {
		MAP(0x23, XK_sterling);
		MAP(0x40, XK_section);
		MAP(0x5b, XK_degree);
		MAP(0x5c, XK_ccedilla);
		MAP(0x5d, XK_eacute);
		MAP(0x60, XK_ugrave);
		MAP(0x7b, XK_agrave);
		MAP(0x7c, XK_ograve);
		MAP(0x7d, XK_egrave);
		MAP(0x7e, XK_igrave);
	    }
	    break;

	case 'E':
	case '6':		/* Norwegian/Danish */
	    switch (code) {
		MAP(0x40, XK_Adiaeresis);
		MAP(0x5b, XK_AE);
		MAP(0x5c, XK_Ooblique);
		MAP(0x5d, XK_Aring);
		MAP(0x5e, XK_Udiaeresis);
		MAP(0x60, XK_adiaeresis);
		MAP(0x7b, XK_ae);
		MAP(0x7c, XK_oslash);
		MAP(0x7d, XK_aring);
		MAP(0x7e, XK_udiaeresis);
	    }
	    break;

	case 'Z':		/* Spanish */
	    switch (code) {
		MAP(0x23, XK_sterling);
		MAP(0x40, XK_section);
		MAP(0x5b, XK_exclamdown);
		MAP(0x5c, XK_Ntilde);
		MAP(0x5d, XK_questiondown);
		MAP(0x7b, XK_degree);
		MAP(0x7c, XK_ntilde);
		MAP(0x7d, XK_ccedilla);
	    }
	    break;

	case 'H':
	case '7':		/* Swedish */
	    switch (code) {
		MAP(0x40, XK_Eacute);
		MAP(0x5b, XK_Adiaeresis);
		MAP(0x5c, XK_Odiaeresis);
		MAP(0x5d, XK_Aring);
		MAP(0x5e, XK_Udiaeresis);
		MAP(0x60, XK_eacute);
		MAP(0x7b, XK_adiaeresis);
		MAP(0x7c, XK_odiaeresis);
		MAP(0x7d, XK_aring);
		MAP(0x7e, XK_udiaeresis);
	    }
	    break;

	case '=':		/* Swiss */
	    switch (code) {
		MAP(0x23, XK_ugrave);
		MAP(0x40, XK_agrave);
		MAP(0x5b, XK_eacute);
		MAP(0x5c, XK_ccedilla);
		MAP(0x5d, XK_ecircumflex);
		MAP(0x5e, XK_icircumflex);
		MAP(0x5f, XK_egrave);
		MAP(0x60, XK_ocircumflex);
		MAP(0x7b, XK_adiaeresis);
		MAP(0x7c, XK_odiaeresis);
		MAP(0x7d, XK_udiaeresis);
		MAP(0x7e, XK_ucircumflex);
	    }
	    break;

	default:		/* any character sets we don't recognize */
	    break;
	}
	code &= 0x7f;		/* NRC in any case is 7-bit */
    }
    return code;
#undef MAP
}

/*
 * Translate a string to the display form.  This assumes the font has the
 * DEC graphic characters in cells 0-31, and otherwise is ISO-8859-1.
 */
int
xtermCharSetOut(XtermWidget xw, IChar * buf, IChar * ptr, int leftset)
{
    IChar *s;
    TScreen *screen = TScreenOf(xw);
    int count = 0;
    int rightset = screen->gsets[(int) (screen->curgr)];

#define MAP(from, to) case from: chr = to; break

    TRACE(("CHARSET GL=%c(G%d) GR=%c(G%d) SS%d\n\t%s\n",
	   leftset, screen->curgl,
	   rightset, screen->curgr,
	   screen->curss,
	   visibleIChar(buf, (unsigned) (ptr - buf))));

    for (s = buf; s < ptr; ++s) {
	int eight = CharOf(E2A(*s));
	int seven = eight & 0x7f;
	int cs = (eight >= 128) ? rightset : leftset;
	int chr = eight;

	count++;
#if OPT_WIDE_CHARS
	/*
	 * This is only partly right - prevent inadvertant remapping of
	 * the replacement character and other non-8bit codes into bogus
	 * 8bit codes.
	 */
	if (screen->utf8_mode) {
	    if (*s > 255)
		continue;
	}
#endif
	switch (cs) {
	case 'A':		/* United Kingdom set (or Latin 1)      */
	    if ((xw->flags & NATIONAL)
		|| (screen->vtXX_level <= 1)) {
		if (chr == 0x23) {
#if OPT_WIDE_CHARS
		    chr = (screen->utf8_mode
			   ? 0xa3
			   : XTERM_POUND);
#else
		    chr = XTERM_POUND;
#endif
		}
	    } else {
		chr = (seven | 0x80);
	    }
	    break;

#if OPT_XMC_GLITCH
	case '?':
#endif
	case '1':		/* Alternate Character ROM standard characters */
	case '2':		/* Alternate Character ROM special graphics */
	case 'B':		/* ASCII set                            */
	    break;

	case '0':		/* special graphics (line drawing)      */
	    if (seven > 0x5f && seven <= 0x7e) {
#if OPT_WIDE_CHARS
		if (screen->utf8_mode)
		    chr = (int) dec2ucs((unsigned) (seven - 0x5f));
		else
#endif
		    chr = seven - 0x5f;
	    } else {
		chr = seven;
	    }
	    break;

	case '4':		/* Dutch */
	    switch (chr = seven) {
		MAP(0x23, XK_sterling);
		MAP(0x40, XK_threequarters);
		MAP(0x5b, XK_ydiaeresis);
		MAP(0x5c, XK_onehalf);
		MAP(0x5d, XK_bar);
		MAP(0x7b, XK_diaeresis);
		MAP(0x7c, XK_f);
		MAP(0x7d, XK_onequarter);
		MAP(0x7e, XK_acute);
	    }
	    break;

	case 'C':
	case '5':		/* Finnish */
	    switch (chr = seven) {
		MAP(0x5b, XK_Adiaeresis);
		MAP(0x5c, XK_Odiaeresis);
		MAP(0x5d, XK_Aring);
		MAP(0x5e, XK_Udiaeresis);
		MAP(0x60, XK_eacute);
		MAP(0x7b, XK_adiaeresis);
		MAP(0x7c, XK_odiaeresis);
		MAP(0x7d, XK_aring);
		MAP(0x7e, XK_udiaeresis);
	    }
	    break;

	case 'R':		/* French */
	    switch (chr = seven) {
		MAP(0x23, XK_sterling);
		MAP(0x40, XK_agrave);
		MAP(0x5b, XK_degree);
		MAP(0x5c, XK_ccedilla);
		MAP(0x5d, XK_section);
		MAP(0x7b, XK_eacute);
		MAP(0x7c, XK_ugrave);
		MAP(0x7d, XK_egrave);
		MAP(0x7e, XK_diaeresis);
	    }
	    break;

	case 'Q':		/* French Canadian */
	    switch (chr = seven) {
		MAP(0x40, XK_agrave);
		MAP(0x5b, XK_acircumflex);
		MAP(0x5c, XK_ccedilla);
		MAP(0x5d, XK_ecircumflex);
		MAP(0x5e, XK_icircumflex);
		MAP(0x60, XK_ocircumflex);
		MAP(0x7b, XK_eacute);
		MAP(0x7c, XK_ugrave);
		MAP(0x7d, XK_egrave);
		MAP(0x7e, XK_ucircumflex);
	    }
	    break;

	case 'K':		/* German */
	    switch (chr = seven) {
		MAP(0x40, XK_section);
		MAP(0x5b, XK_Adiaeresis);
		MAP(0x5c, XK_Odiaeresis);
		MAP(0x5d, XK_Udiaeresis);
		MAP(0x7b, XK_adiaeresis);
		MAP(0x7c, XK_odiaeresis);
		MAP(0x7d, XK_udiaeresis);
		MAP(0x7e, XK_ssharp);
	    }
	    break;

	case 'Y':		/* Italian */
	    switch (chr = seven) {
		MAP(0x23, XK_sterling);
		MAP(0x40, XK_section);
		MAP(0x5b, XK_degree);
		MAP(0x5c, XK_ccedilla);
		MAP(0x5d, XK_eacute);
		MAP(0x60, XK_ugrave);
		MAP(0x7b, XK_agrave);
		MAP(0x7c, XK_ograve);
		MAP(0x7d, XK_egrave);
		MAP(0x7e, XK_igrave);
	    }
	    break;

	case 'E':
	case '6':		/* Norwegian/Danish */
	    switch (chr = seven) {
		MAP(0x40, XK_Adiaeresis);
		MAP(0x5b, XK_AE);
		MAP(0x5c, XK_Ooblique);
		MAP(0x5d, XK_Aring);
		MAP(0x5e, XK_Udiaeresis);
		MAP(0x60, XK_adiaeresis);
		MAP(0x7b, XK_ae);
		MAP(0x7c, XK_oslash);
		MAP(0x7d, XK_aring);
		MAP(0x7e, XK_udiaeresis);
	    }
	    break;

	case 'Z':		/* Spanish */
	    switch (chr = seven) {
		MAP(0x23, XK_sterling);
		MAP(0x40, XK_section);
		MAP(0x5b, XK_exclamdown);
		MAP(0x5c, XK_Ntilde);
		MAP(0x5d, XK_questiondown);
		MAP(0x7b, XK_degree);
		MAP(0x7c, XK_ntilde);
		MAP(0x7d, XK_ccedilla);
	    }
	    break;

	case 'H':
	case '7':		/* Swedish */
	    switch (chr = seven) {
		MAP(0x40, XK_Eacute);
		MAP(0x5b, XK_Adiaeresis);
		MAP(0x5c, XK_Odiaeresis);
		MAP(0x5d, XK_Aring);
		MAP(0x5e, XK_Udiaeresis);
		MAP(0x60, XK_eacute);
		MAP(0x7b, XK_adiaeresis);
		MAP(0x7c, XK_odiaeresis);
		MAP(0x7d, XK_aring);
		MAP(0x7e, XK_udiaeresis);
	    }
	    break;

	case '=':		/* Swiss */
	    switch (chr = seven) {
		MAP(0x23, XK_ugrave);
		MAP(0x40, XK_agrave);
		MAP(0x5b, XK_eacute);
		MAP(0x5c, XK_ccedilla);
		MAP(0x5d, XK_ecircumflex);
		MAP(0x5e, XK_icircumflex);
		MAP(0x5f, XK_egrave);
		MAP(0x60, XK_ocircumflex);
		MAP(0x7b, XK_adiaeresis);
		MAP(0x7c, XK_odiaeresis);
		MAP(0x7d, XK_udiaeresis);
		MAP(0x7e, XK_ucircumflex);
	    }
	    break;

	default:		/* any character sets we don't recognize */
	    count--;
	    break;
	}
	/*
	 * The state machine already treated DEL as a nonprinting and
	 * nonspacing character.  If we have DEL now, simply render
	 * it as a blank.
	 */
	if (chr == ANSI_DEL)
	    chr = ' ';
	*s = (IChar) A2E(chr);
    }
    TRACE(("%d\t%s\n",
	   count,
	   visibleIChar(buf, (unsigned) (ptr - buf))));
    return count;
#undef MAP
}
