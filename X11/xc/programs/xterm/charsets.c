/*
 * $XFree86: xc/programs/xterm/charsets.c,v 1.11 2003/12/31 17:12:26 dickey Exp $
 */

/************************************************************

Copyright 1998-2000,2003 by Thomas E. Dickey

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
unsigned xtermCharSetIn(unsigned code, int charset)
{
	if (code >= 128 && code < 256) {
		switch (charset) {
		case 'A':	/* United Kingdom set (or Latin 1)	*/
			if (code == XK_sterling)
				code = 0x23;
			code &= 0x7f;
			break;

#if OPT_XMC_GLITCH
		case '?':
#endif
		case '1':	/* Alternate Character ROM standard characters */
		case '2':	/* Alternate Character ROM special graphics */
		case 'B':	/* ASCII set				*/
			break;

		case '0':	/* special graphics (line drawing)	*/
			break;

		case '4':	/* Dutch */
			switch (code) {
			case XK_sterling:	code = 0x23;	break;
			case XK_threequarters:	code = 0x40;	break;
			case XK_ydiaeresis:	code = 0x5b;	break;
			case XK_onehalf:	code = 0x5c;	break;
	/* N/A		case XK_bar:		code = 0x5d;	break; */
			case XK_diaeresis:	code = 0x7b;	break;
	/* N/A		case XK_f:		code = 0x7c;	break; */
			case XK_onequarter:	code = 0x7d;	break;
			case XK_acute:		code = 0x7e;	break;
			}
			break;

		case 'C':
		case '5':	/* Finnish */
			switch (code) {
			case XK_Adiaeresis:	code = 0x5b;	break;
			case XK_Odiaeresis:	code = 0x5c;	break;
			case XK_Aring:		code = 0x5d;	break;
			case XK_Udiaeresis:	code = 0x5e;	break;
			case XK_eacute:		code = 0x60;	break;
			case XK_adiaeresis:	code = 0x7b;	break;
			case XK_odiaeresis:	code = 0x7c;	break;
			case XK_aring:		code = 0x7d;	break;
			case XK_udiaeresis:	code = 0x7e;	break;
			}
			break;

		case 'R':	/* French */
			switch (code) {
			case XK_sterling:	code = 0x23;	break;
			case XK_agrave:		code = 0x40;	break;
			case XK_degree:		code = 0x5b;	break;
			case XK_ccedilla:	code = 0x5c;	break;
			case XK_section:	code = 0x5d;	break;
			case XK_eacute:		code = 0x7b;	break;
			case XK_ugrave:		code = 0x7c;	break;
			case XK_egrave:		code = 0x7d;	break;
			case XK_diaeresis:	code = 0x7e;	break;
			}
			break;

		case 'Q':	/* French Canadian */
			switch (code) {
			case XK_agrave:		code = 0x40;	break;
			case XK_acircumflex:	code = 0x5b;	break;
			case XK_ccedilla:	code = 0x5c;	break;
			case XK_ecircumflex:	code = 0x5d;	break;
			case XK_icircumflex:	code = 0x5e;	break;
			case XK_ocircumflex:	code = 0x60;	break;
			case XK_eacute:		code = 0x7b;	break;
			case XK_ugrave:		code = 0x7c;	break;
			case XK_egrave:		code = 0x7d;	break;
			case XK_ucircumflex:	code = 0x7e;	break;
			}
			break;

		case 'K':	/* German */
			switch (code) {
			case XK_section:	code = 0x40;	break;
			case XK_Adiaeresis:	code = 0x5b;	break;
			case XK_Odiaeresis:	code = 0x5c;	break;
			case XK_Udiaeresis:	code = 0x5d;	break;
			case XK_adiaeresis:	code = 0x7b;	break;
			case XK_odiaeresis:	code = 0x7c;	break;
			case XK_udiaeresis:	code = 0x7d;	break;
			case XK_ssharp:		code = 0x7e;	break;
			}
			break;

		case 'Y':	/* Italian */
			switch (code) {
			case XK_sterling:	code = 0x23;	break;
			case XK_section:	code = 0x40;	break;
			case XK_degree:		code = 0x5b;	break;
			case XK_ccedilla:	code = 0x5c;	break;
			case XK_eacute:		code = 0x5d;	break;
			case XK_ugrave:		code = 0x60;	break;
			case XK_agrave:		code = 0x7b;	break;
			case XK_ograve:		code = 0x7c;	break;
			case XK_egrave:		code = 0x7d;	break;
			case XK_igrave:		code = 0x7e;	break;
			}
			break;

		case 'E':
		case '6':	/* Norwegian/Danish */
			switch (code) {
			case XK_Adiaeresis:	code = 0x40;	break;
			case XK_AE:		code = 0x5b;	break;
			case XK_Ooblique:	code = 0x5c;	break;
			case XK_Aring:		code = 0x5d;	break;
			case XK_Udiaeresis:	code = 0x5e;	break;
			case XK_adiaeresis:	code = 0x60;	break;
			case XK_ae:		code = 0x7b;	break;
			case XK_oslash:		code = 0x7c;	break;
			case XK_aring:		code = 0x7d;	break;
			case XK_udiaeresis:	code = 0x7e;	break;
			}
			break;

		case 'Z':	/* Spanish */
			switch (code) {
			case XK_sterling:	code = 0x23;	break;
			case XK_section:	code = 0x40;	break;
			case XK_exclamdown:	code = 0x5b;	break;
			case XK_Ntilde:		code = 0x5c;	break;
			case XK_questiondown:	code = 0x5d;	break;
			case XK_degree:		code = 0x7b;	break;
			case XK_ntilde:		code = 0x7c;	break;
			case XK_ccedilla:	code = 0x7d;	break;
			}
			break;

		case 'H':
		case '7':	/* Swedish */
			switch (code) {
			case XK_Eacute:		code = 0x40;	break;
			case XK_Adiaeresis:	code = 0x5b;	break;
			case XK_Odiaeresis:	code = 0x5c;	break;
			case XK_Aring:		code = 0x5d;	break;
			case XK_Udiaeresis:	code = 0x5e;	break;
			case XK_eacute:		code = 0x60;	break;
			case XK_adiaeresis:	code = 0x7b;	break;
			case XK_odiaeresis:	code = 0x7c;	break;
			case XK_aring:		code = 0x7d;	break;
			case XK_udiaeresis:	code = 0x7e;	break;
			}
			break;

		case '=':	/* Swiss */
			switch (code) {
			case XK_ugrave:		code = 0x23;	break;
			case XK_agrave:		code = 0x40;	break;
			case XK_eacute:		code = 0x5b;	break;
			case XK_ccedilla:	code = 0x5c;	break;
			case XK_ecircumflex:	code = 0x5d;	break;
			case XK_icircumflex:	code = 0x5e;	break;
			case XK_egrave:		code = 0x5f;	break;
			case XK_ocircumflex:	code = 0x60;	break;
			case XK_adiaeresis:	code = 0x7b;	break;
			case XK_odiaeresis:	code = 0x7c;	break;
			case XK_udiaeresis:	code = 0x7d;	break;
			case XK_ucircumflex:	code = 0x7e;	break;
			}
			break;

		default:	/* any character sets we don't recognize*/
			break;
		}
		code &= 0x7f;	/* NRC in any case is 7-bit */
	}
	return code;
}

/*
 * Translate a string to the display form.  This assumes the font has the
 * DEC graphic characters in cells 0-31, and otherwise is ISO-8859-1.
 */
int xtermCharSetOut(IChar *buf, IChar *ptr, char leftset)
{
	IChar *s;
	register TScreen *screen = &term->screen;
	int count = 0;
	int rightset = screen->gsets[(int)(screen->curgr)];

	TRACE(("CHARSET GL=%c(G%d) GR=%c(G%d) %s\n",
		leftset,  screen->curss ? screen->curss : screen->curgl,
		rightset, screen->curgr,
		visibleIChar(buf, ptr-buf)));

	for (s = buf; s < ptr; ++s) {
		int eight = CharOf(E2A(*s));
		int seven = eight & 0x7f;
		int cs = (eight >= 128) ? rightset : leftset;
		int chr = eight;

		count++;
		switch (cs) {
		case 'A':	/* United Kingdom set (or Latin 1)	*/
			if ((term->flags & NATIONAL)
			 || (screen->vtXX_level <= 1)) {
				if (chr == 0x23)
					chr = XPOUND;	/* UK pound sign*/
			} else {
				chr = (seven | 0x80);
			}
			break;

#if OPT_XMC_GLITCH
		case '?':
#endif
		case '1':	/* Alternate Character ROM standard characters */
		case '2':	/* Alternate Character ROM special graphics */
		case 'B':	/* ASCII set				*/
			break;

		case '0':	/* special graphics (line drawing)	*/
			if (chr >= 0x5f && chr <= 0x7e) {
#if OPT_WIDE_CHARS
				if (screen->utf8_mode)
				    chr = dec2ucs(chr - 0x5f);
				else
#endif
				    chr = (chr == 0x5f) ? 0x7f : (chr - 0x5f);
			}
			break;

		case '4':	/* Dutch */
			switch (chr = seven) {
			case 0x23:	chr = XK_sterling;	break;
			case 0x40:	chr = XK_threequarters;	break;
			case 0x5b:	chr = XK_ydiaeresis;	break;
			case 0x5c:	chr = XK_onehalf;	break;
			case 0x5d:	chr = XK_bar;		break;
			case 0x7b:	chr = XK_diaeresis;	break;
			case 0x7c:	chr = XK_f;		break;
			case 0x7d:	chr = XK_onequarter;	break;
			case 0x7e:	chr = XK_acute;		break;
			}
			break;

		case 'C':
		case '5':	/* Finnish */
			switch (chr = seven) {
			case 0x5b:	chr = XK_Adiaeresis;	break;
			case 0x5c:	chr = XK_Odiaeresis;	break;
			case 0x5d:	chr = XK_Aring;		break;
			case 0x5e:	chr = XK_Udiaeresis;	break;
			case 0x60:	chr = XK_eacute;	break;
			case 0x7b:	chr = XK_adiaeresis;	break;
			case 0x7c:	chr = XK_odiaeresis;	break;
			case 0x7d:	chr = XK_aring;		break;
			case 0x7e:	chr = XK_udiaeresis;	break;
			}
			break;

		case 'R':	/* French */
			switch (chr = seven) {
			case 0x23:	chr = XK_sterling;	break;
			case 0x40:	chr = XK_agrave;	break;
			case 0x5b:	chr = XK_degree;	break;
			case 0x5c:	chr = XK_ccedilla;	break;
			case 0x5d:	chr = XK_section;	break;
			case 0x7b:	chr = XK_eacute;	break;
			case 0x7c:	chr = XK_ugrave;	break;
			case 0x7d:	chr = XK_egrave;	break;
			case 0x7e:	chr = XK_diaeresis;	break;
			}
			break;

		case 'Q':	/* French Canadian */
			switch (chr = seven) {
			case 0x40:	chr = XK_agrave;	break;
			case 0x5b:	chr = XK_acircumflex;	break;
			case 0x5c:	chr = XK_ccedilla;	break;
			case 0x5d:	chr = XK_ecircumflex;	break;
			case 0x5e:	chr = XK_icircumflex;	break;
			case 0x60:	chr = XK_ocircumflex;	break;
			case 0x7b:	chr = XK_eacute;	break;
			case 0x7c:	chr = XK_ugrave;	break;
			case 0x7d:	chr = XK_egrave;	break;
			case 0x7e:	chr = XK_ucircumflex;	break;
			}
			break;

		case 'K':	/* German */
			switch (chr = seven) {
			case 0x40:	chr = XK_section;	break;
			case 0x5b:	chr = XK_Adiaeresis;	break;
			case 0x5c:	chr = XK_Odiaeresis;	break;
			case 0x5d:	chr = XK_Udiaeresis;	break;
			case 0x7b:	chr = XK_adiaeresis;	break;
			case 0x7c:	chr = XK_odiaeresis;	break;
			case 0x7d:	chr = XK_udiaeresis;	break;
			case 0x7e:	chr = XK_ssharp;	break;
			}
			break;

		case 'Y':	/* Italian */
			switch (chr = seven) {
			case 0x23:	chr = XK_sterling;	break;
			case 0x40:	chr = XK_section;	break;
			case 0x5b:	chr = XK_degree;	break;
			case 0x5c:	chr = XK_ccedilla;	break;
			case 0x5d:	chr = XK_eacute;	break;
			case 0x60:	chr = XK_ugrave;	break;
			case 0x7b:	chr = XK_agrave;	break;
			case 0x7c:	chr = XK_ograve;	break;
			case 0x7d:	chr = XK_egrave;	break;
			case 0x7e:	chr = XK_igrave;	break;
			}
			break;

		case 'E':
		case '6':	/* Norwegian/Danish */
			switch (chr = seven) {
			case 0x40:	chr = XK_Adiaeresis;	break;
			case 0x5b:	chr = XK_AE;		break;
			case 0x5c:	chr = XK_Ooblique;	break;
			case 0x5d:	chr = XK_Aring;		break;
			case 0x5e:	chr = XK_Udiaeresis;	break;
			case 0x60:	chr = XK_adiaeresis;	break;
			case 0x7b:	chr = XK_ae;		break;
			case 0x7c:	chr = XK_oslash;	break;
			case 0x7d:	chr = XK_aring;		break;
			case 0x7e:	chr = XK_udiaeresis;	break;
			}
			break;

		case 'Z':	/* Spanish */
			switch (chr = seven) {
			case 0x23:	chr = XK_sterling;	break;
			case 0x40:	chr = XK_section;	break;
			case 0x5b:	chr = XK_exclamdown;	break;
			case 0x5c:	chr = XK_Ntilde;	break;
			case 0x5d:	chr = XK_questiondown;	break;
			case 0x7b:	chr = XK_degree;	break;
			case 0x7c:	chr = XK_ntilde;	break;
			case 0x7d:	chr = XK_ccedilla;	break;
			}
			break;

		case 'H':
		case '7':	/* Swedish */
			switch (chr = seven) {
			case 0x40:	chr = XK_Eacute;	break;
			case 0x5b:	chr = XK_Adiaeresis;	break;
			case 0x5c:	chr = XK_Odiaeresis;	break;
			case 0x5d:	chr = XK_Aring;		break;
			case 0x5e:	chr = XK_Udiaeresis;	break;
			case 0x60:	chr = XK_eacute;	break;
			case 0x7b:	chr = XK_adiaeresis;	break;
			case 0x7c:	chr = XK_odiaeresis;	break;
			case 0x7d:	chr = XK_aring;		break;
			case 0x7e:	chr = XK_udiaeresis;	break;
			}
			break;

		case '=':	/* Swiss */
			switch (chr = seven) {
			case 0x23:	chr = XK_ugrave;	break;
			case 0x40:	chr = XK_agrave;	break;
			case 0x5b:	chr = XK_eacute;	break;
			case 0x5c:	chr = XK_ccedilla;	break;
			case 0x5d:	chr = XK_ecircumflex;	break;
			case 0x5e:	chr = XK_icircumflex;	break;
			case 0x5f:	chr = XK_egrave;	break;
			case 0x60:	chr = XK_ocircumflex;	break;
			case 0x7b:	chr = XK_adiaeresis;	break;
			case 0x7c:	chr = XK_odiaeresis;	break;
			case 0x7d:	chr = XK_udiaeresis;	break;
			case 0x7e:	chr = XK_ucircumflex;	break;
			}
			break;

		default:	/* any character sets we don't recognize*/
			count --;
			break;
		}
		*s = A2E(chr);
	}
	return count;
}
