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

#ifndef _DIFSUTILS_H
#define _DIFSUTILS_H 1

#include <misc.h>
#include <auth.h>
#include <X11/fonts/font.h>
#include <X11/fonts/fontstruct.h>

typedef void (*DifsWakeupFunc)(pointer data, int result, unsigned long * pReadmask);
typedef Bool (*DifsWorkFunc) (ClientPtr, pointer);

extern AuthContextPtr GetClientAuthorization (void);
extern Bool ClientIsAsleep (ClientPtr client);
extern Bool ClientSleep (ClientPtr client, Bool (*function) (ClientPtr, pointer), pointer data);
extern Bool QueueWorkProc (DifsWorkFunc function, ClientPtr client, pointer data);
extern Bool RegisterBlockAndWakeupHandlers (BlockHandlerProcPtr blockHandler, DifsWakeupFunc wakeupHandler, pointer blockData);
#if 0
extern Bool XpClientIsBitmapClient (ClientPtr client);
extern Bool XpClientIsPrintClient (ClientPtr client, FontPathElementPtr fpe);
extern FontResolutionPtr GetClientResolutions (int *num);
#endif
#if 0
extern int GetDefaultPointSize (void);
#endif
extern int SetDefaultResolutions (char *str);
#if 0
extern int client_auth_generation (ClientPtr client);
#endif
extern int set_font_authorizations (char **authorizations, int *authlen, ClientPtr client);
extern int strncmpnocase (char *first, char *second, int n);
extern pointer Xalloc (unsigned long m);
extern pointer Xrealloc (pointer n, unsigned long m);
extern void BlockHandler (OSTimePtr pTimeout, pointer pReadmask);
extern void ClientWakeup (ClientPtr client);
extern void CopyISOLatin1Lowered (char *d, char *s, int length);
extern void InitBlockAndWakeupHandlers (void);
extern void NoopDDA (void);
extern void ProcessWorkQueue (void);
extern void RemoveBlockAndWakeupHandlers (BlockHandlerProcPtr blockHandler, DifsWakeupFunc wakeupHandler, pointer blockData);
extern void SetDefaultPointSize (int ps);
extern void WakeupHandler (int result, unsigned long * pReadmask);
extern void Xfree (unsigned long *n);
extern pointer Xcalloc (unsigned long n);

#endif /* _DIFSUTILS_H */
