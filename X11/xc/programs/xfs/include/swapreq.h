/*
 * $XFree86: xc/programs/xfs/include/swapreq.h,v 1.2 2001/06/25 20:40:18 paulo Exp $
 */

/************************************************************

Copyright 1998 by Thomas E. Dickey <dickey@clark.net>

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the above listed
copyright holder(s) not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.

THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD
TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE
LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

********************************************************/

/*
 * difs/swapreq.h -- swapped requests
 */


#ifndef _SWAPREQ_H_
#define _SWAPREQ_H_ 1

#include <misc.h>

extern int SProcCreateAC(ClientPtr client);
extern int SProcListCatalogues(ClientPtr client);
extern int SProcListFonts(ClientPtr client);
extern int SProcListFontsWithXInfo(ClientPtr client);
extern int SProcOpenBitmapFont(ClientPtr client);
extern int SProcQueryExtension(ClientPtr client);
extern int SProcQueryXBitmaps(ClientPtr client);
extern int SProcQueryXExtents(ClientPtr client);
extern int SProcResourceRequest(ClientPtr client);
extern int SProcSetResolution(ClientPtr client);
extern int SProcSimpleRequest(ClientPtr client);
extern int SwapConnClientPrefix(ClientPtr client, fsConnClientPrefix *pCCP);
extern void SwapLongs(long *list, unsigned long count);
extern void SwapShorts(short *list, unsigned long count);

#endif /* _SWAPREQ_H_ */
