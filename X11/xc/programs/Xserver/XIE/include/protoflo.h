/*
 * $XFree86: xc/programs/Xserver/XIE/include/protoflo.h,v 1.1 1998/10/25 07:11:47 dawes Exp $
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
 * dixie/request/protoflo.c
 */

#ifndef _XIE_PROTOFLO_H_
#define _XIE_PROTOFLO_H_ 1

/*
 *  Core X Includes
 */
#define NEED_EVENTS
#include <X.h>
/*
 *  XIE Includes
 */
#include <XIE.h>
#include <XIEproto.h>
/*
 *  more X server includes.
 */
#include <dixstruct.h>
#include <flostr.h>
#include <photospc.h>

/*
 *  Xie protocol procedures called from the dispatcher
 */
extern int ProcAbort (ClientPtr client);
extern int ProcAwait (ClientPtr client);
extern int ProcCreatePhotoflo (ClientPtr client);
extern int ProcCreatePhotospace (ClientPtr client);
extern int ProcDestroyPhotoflo (ClientPtr client);
extern int ProcDestroyPhotospace (ClientPtr client);
extern int ProcExecuteImmediate (ClientPtr client);
extern int ProcExecutePhotoflo (ClientPtr client);
extern int ProcGetClientData (ClientPtr client);
extern int ProcModifyPhotoflo (ClientPtr client);
extern int ProcPutClientData (ClientPtr client);
extern int ProcQueryPhotoflo (ClientPtr client);
extern int ProcRedefinePhotoflo (ClientPtr client);
extern int SProcAbort (ClientPtr client);
extern int SProcAwait (ClientPtr client);
extern int SProcCreatePhotoflo (ClientPtr client);
extern int SProcCreatePhotospace (ClientPtr client);
extern int SProcDestroyPhotoflo (ClientPtr client);
extern int SProcDestroyPhotospace (ClientPtr client);
extern int SProcExecuteImmediate (ClientPtr client);
extern int SProcExecutePhotoflo (ClientPtr client);
extern int SProcGetClientData (ClientPtr client);
extern int SProcModifyPhotoflo (ClientPtr client);
extern int SProcPutClientData (ClientPtr client);
extern int SProcQueryPhotoflo (ClientPtr client);
extern int SProcRedefinePhotoflo (ClientPtr client);

/*
 *  routines referenced by other modules
 */
extern int DeletePhotoflo (floDefPtr flo, xieTypPhotoflo id);
extern int DeletePhotospace (photospacePtr space, xieTypPhotospace id);

#endif /* _XIE_PROTOFLO_H_ */
