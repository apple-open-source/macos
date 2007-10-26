/* $XConsortium: resource.h,v 1.4 94/04/17 19:56:00 dpw Exp $ */
/* 
Copyright (c) 1987  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
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
 * AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES, DIGITAL OR MIT BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * @(#)resource.h	4.1	91/05/02
 *
 */
/* $XFree86: xc/programs/xfs/include/fsresource.h,v 1.3 1998/10/25 12:48:04 dawes Exp $ */

#ifndef _RESOURCE_H_
#define _RESOURCE_H_

#include	"misc.h"

typedef unsigned long RESTYPE;

#define	RC_VANILLA	((RESTYPE)0)
#define	RC_CACHED	((RESTYPE)1<<31)
#define	RC_LASTPREDEF	RC_CACHED
#define	RC_ANY		(~(RESTYPE)0)

#define	RT_FONT		((RESTYPE)1)
#define	RT_AUTHCONT	((RESTYPE)2)
#define	RT_LASTPREDEF	RT_AUTHCONT
#define	RT_NONE		((RESTYPE)0)

#define	CLIENTOFFSET		22
#define	RESOURCE_ID_MASK	0x3FFFFF
#define	CLIENT_BITS(id)		((id) & 0x1fc00000)
#define	CLIENT_ID(id)		((int)(CLIENT_BITS(id) >> CLIENTOFFSET))
#define	SERVER_BIT		0x20000000

#define	INVALID			(0)

#define	BAD_RESOURCE		0xe0000000

#ifdef NOTYET
extern Bool ChangeResourceValue(int cid, FSID id, RESTYPE rtype, pointer value);
extern FSID FakeClientID(int client);
extern RESTYPE CreateNewResourceClass(void);
extern RESTYPE CreateNewResourceType(DeleteType deleteFunc);
extern pointer LookupIDByClass(FSID id, RESTYPE classes);
extern void FreeResourceByType(int cid, FSID id, RESTYPE type, Bool skipFree);
#endif /* NOTYET */

extern Bool AddResource(int cid, FSID id, RESTYPE type, pointer value);
extern Bool InitClientResources(ClientPtr client);
extern FSID FakeClientID(int client);
extern pointer LookupIDByType(int cid, FSID id, RESTYPE rtype);
extern void FreeAllResources(void);
extern void FreeClientResources(ClientPtr client);
extern void FreeResource(int cid, FSID id, RESTYPE skipDeleteFuncType);
int NoneDeleteFunc (void *ptr, FSID id);

#endif				/* _RESOURCE_H_ */
