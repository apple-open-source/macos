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

#ifndef _DISPATCH_H_
#define _DISPATCH_H_ 1

#include <misc.h>
extern ClientPtr currentClient;

extern void Dispatch (void);
extern int ProcInitialConnection (ClientPtr client);
extern int ProcEstablishConnection (ClientPtr client);
#ifdef DEBUG
#define SendErrToClient(client, error, data)	\
	fprintf(stderr, "SendErrToClient %s %d\n", __FILE__, __LINE__),\
	DoSendErrToClient(client, error, data)
#else
#define SendErrToClient DoSendErrToClient
#endif
extern void DoSendErrToClient (ClientPtr client, int error, pointer data);
extern int ProcBadRequest (ClientPtr client);
extern int ProcNoop (ClientPtr client);
extern int ProcListCatalogues (ClientPtr client);
extern int ProcSetCatalogues (ClientPtr client);
extern int ProcGetCatalogues (ClientPtr client);
extern int ProcCreateAC (ClientPtr client);
extern int DeleteAuthCont  (pointer value, FSID id);
extern int ProcFreeAC (ClientPtr client);
extern int ProcSetAuthorization (ClientPtr client);
extern int ProcSetResolution (ClientPtr client);
extern int ProcGetResolution (ClientPtr client);
extern int ProcListFonts (ClientPtr client);
extern int ProcListFontsWithXInfo (ClientPtr client);
extern int ProcOpenBitmapFont (ClientPtr client);
extern int ProcQueryXInfo (ClientPtr client);
extern int ProcQueryXExtents (ClientPtr client);
extern int ProcQueryXBitmaps (ClientPtr client);
extern int ProcCloseFont (ClientPtr client);
extern void InitProcVectors (void);
extern void InitClient (ClientPtr client, int i, pointer ospriv);
extern ClientPtr NextAvailableClient (pointer ospriv);
extern void MarkClientException (ClientPtr client);

#endif /* _DISPATCH_H_ */
