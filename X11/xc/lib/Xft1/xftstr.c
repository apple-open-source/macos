/*
 * $XFree86: xc/lib/Xft1/xftstr.c,v 1.1.1.1 2002/02/15 01:26:15 keithp Exp $
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "xftint.h"

char *
_XftSaveString (const char *s)
{
    char    *r;

    if (!s)
	return 0;
    r = (char *) malloc (strlen (s) + 1);
    if (!r)
	return 0;
    strcpy (r, s);
    return r;
}

const char *
_XftGetInt(const char *ptr, int *val)
{
    if (*ptr == '*') {
	*val = -1;
	ptr++;
    } else
	for (*val = 0; *ptr >= '0' && *ptr <= '9';)
	    *val = *val * 10 + *ptr++ - '0';
    if (*ptr == '-')
	return ptr;
    return (char *) 0;
}

char *
_XftSplitStr (const char *field, char *save)
{
    char    *s = save;
    char    c;

    while (*field)
    {
	if (*field == '-')
	    break;
	c = *field++;
	*save++ = c;
    }
    *save = 0;
    return s;
}

char *
_XftDownStr (const char *field, char *save)
{
    char    *s = save;
    char    c;

    while (*field)
    {
	c = *field++;
	*save++ = c;
    }
    *save = 0;
    return s;
}

const char *
_XftSplitField (const char *field, char *save)
{
    char    c;

    while (*field)
    {
	if (*field == '-' || *field == '=')
	    break;
	c = *field++;
	*save++ = c;
    }
    *save = 0;
    return field;
}

const char *
_XftSplitValue (const char *field, char *save)
{
    char    c;

    while (*field)
    {
	if (*field == '-' || *field == ',')
	    break;
	c = *field++;
	*save++ = c;
    }
    *save = 0;
    if (*field)
	field++;
    return field;
}

int
_XftMatchSymbolic (XftSymbolic *s, int n, const char *name, int def)
{
    while (n--)
    {
	if (!_XftStrCmpIgnoreCase (s->name, name))
	    return s->value;
	s++;
    }
    return def;
}

int
_XftStrCmpIgnoreCase (const char *s1, const char *s2)
{
    char    c1, c2;
    
    for (;;) 
    {
	c1 = *s1++;
	c2 = *s2++;
	if (!c1 || !c2)
	    break;
	if (isupper (c1))
	    c1 = tolower (c1);
	if (isupper (c2))
	    c2 = tolower (c2);
	if (c1 != c2)
	    break;
    }
    return (int) c2 - (int) c1;
}

int
XftUtf8ToUcs4 (XftChar8    *src_orig,
	       XftChar32   *dst,
	       int	    len)
{
    XftChar8	*src = src_orig;
    XftChar8	s;
    int		extra;
    XftChar32	result;

    if (len == 0)
	return 0;
    
    s = *src++;
    len--;
    
    if (!(s & 0x80))
    {
	result = s;
	extra = 0;
    } 
    else if (!(s & 0x40))
    {
	return -1;
    }
    else if (!(s & 0x20))
    {
	result = s & 0x1f;
	extra = 1;
    }
    else if (!(s & 0x10))
    {
	result = s & 0xf;
	extra = 2;
    }
    else if (!(s & 0x08))
    {
	result = s & 0x07;
	extra = 3;
    }
    else if (!(s & 0x04))
    {
	result = s & 0x03;
	extra = 4;
    }
    else if ( ! (s & 0x02))
    {
	result = s & 0x01;
	extra = 5;
    }
    else
    {
	return -1;
    }
    if (extra > len)
	return -1;
    
    while (extra--)
    {
	result <<= 6;
	s = *src++;
	
	if ((s & 0xc0) != 0x80)
	    return -1;
	
	result |= s & 0x3f;
    }
    *dst = result;
    return src - src_orig;
}

Bool
XftUtf8Len (XftChar8	*string,
	    int		len,
	    int		*nchar,
	    int		*wchar)
{
    int		n;
    int		clen;
    int		width = 1;
    XftChar32	c;
    
    n = 0;
    while (len)
    {
	clen = XftUtf8ToUcs4 (string, &c, len);
	if (clen <= 0)	/* malformed UTF8 string */
	    return False;
	if (c >= 0x10000)
	    width = 4;
	else if (c >= 0x100)
	{
	    if (width == 1)
		width = 2;
	}
	string += clen;
	len -= clen;
	n++;
    }
    *nchar = n;
    *wchar = width;
    return True;
}
