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
 * difs/swaprep.c -- font server reply swapping
 */


#ifndef _SWAPREP_H_
#define _SWAPREP_H_ 1

#include	<misc.h>

extern void CopySwap16Write (ClientPtr client, int size, short *pbuf);
extern void CopySwap32Write (ClientPtr client, int size, long *pbuf);
extern void SCreateACReply (ClientPtr client, int size, void * data);
extern void SErrorEvent (fsError *error, fsError *perror);
extern void SGenericReply (ClientPtr client, int size, void * data);
extern void SGetEventMaskReply (ClientPtr client, int size, void * data);
extern void SGetResolutionReply (ClientPtr client, int size, void * data);
extern void SListCataloguesReply (ClientPtr client, int size, void * data);
extern void SListExtensionsReply (ClientPtr client, int size, void * data);
extern void SListFontsReply (ClientPtr client, int size, void * data);
extern void SListFontsWithXInfoReply (ClientPtr client, int size, void * data);
extern void SOpenBitmapFontReply (ClientPtr client, int size, void * data);
extern void SQueryExtensionReply (ClientPtr client, int size, void * data);
extern void SQueryXBitmapsReply (ClientPtr client, int size, void * data);
extern void SQueryXExtentsReply (ClientPtr client, int size, void * data);
extern void SQueryXInfoReply (ClientPtr client, int size, void * data);
extern void Swap16Write (ClientPtr client, int size, short *pbuf);
extern void Swap32Write (ClientPtr client, int size, long *pbuf);
extern void SwapExtents (fsXCharInfo *extents, int num);
extern void SwapPropInfo (fsPropInfo *pi);
extern void WriteSConnSetup (ClientPtr client, fsConnSetup *pcsp);
extern void WriteSConnectionInfo (ClientPtr client, unsigned long size, char *pInfo);

#endif /* _SWAPREP_H_ */
