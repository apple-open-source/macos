/* $Xorg: initfonts.c,v 1.4 2001/02/09 02:05:42 xorgcvs Exp $ */
/*
Copyright 1987, 1998  The Open Group

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
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices,
 * or Digital not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Network Computing Devices, or Digital
 * make no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES, OR DIGITAL BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * %W%	%E%
 *
 */
/* $XFree86: xc/programs/xfs/difs/initfonts.c,v 1.6 2001/08/01 00:45:04 tsi Exp $ */

#include        <X11/fonts/font.h>
#include	"difs.h"
#ifdef FONTCACHE
#include        "misc.h"
#include        <X11/extensions/fontcacheP.h>

#define CACHE_HI_MARK	(2048 * 1024)
#define CACHE_LOW_MARK	(((2048 * 1024) / 4) * 3)
#define CACHE_BALANCE	70
#endif

FontPatternCachePtr fontPatternCache;
#ifdef FONTCACHE
FontCacheSettings cacheSettings = { -1, -1, -1 };
#endif

void
InitFonts(void)
{
#ifdef FONTCACHE
    long himark, lowmark;
    long balance;
    FontCacheSettings cs;
#endif

    if (fontPatternCache)
	FreeFontPatternCache(fontPatternCache);
    fontPatternCache = MakeFontPatternCache();

    ResetFontPrivateIndex();

#ifdef FONTCACHE
    /* check cache control parameters */
    if (cacheSettings.himark == -1) {
	himark = CACHE_HI_MARK;
	if (cacheSettings.lowmark == -1) {
	    lowmark = CACHE_LOW_MARK;
	} else {
	    lowmark = cacheSettings.lowmark;
	}
    } else {
	himark = cacheSettings.himark;
	if (cacheSettings.lowmark == -1) {
	    lowmark = (himark / 4) * 3;
	} else {
	    lowmark = cacheSettings.lowmark;
	}
    }
    if (cacheSettings.balance == -1) {
	balance = CACHE_BALANCE;
    } else {
	balance = cacheSettings.balance;
    }

    if (himark <= 0 || lowmark <= 0) {
	FatalError("illegal cache parameter setting\n");
    }
    if (himark <= lowmark) {
	FatalError("illegal cache parameter setting\n");
    }
    if (!(10 <= balance && balance <= 90)) {
	FatalError("illegal cache parameter setting\n");
    }

    /* set cache control parameters */
    cs.himark = himark;
    cs.lowmark = lowmark;
    cs.balance = balance;
    if (FontCacheChangeSettings(&cs) == 0) {
	FatalError("couldn't init renderer font cache\n");
    }
#endif

#ifdef FONT_PCF
    FontFileRegisterFpeFunctions();

#endif

#ifdef FONT_FS
    fs_register_fpe_functions();
#endif
}
