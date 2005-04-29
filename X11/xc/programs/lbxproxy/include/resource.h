/* $Xorg: resource.h,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */

/*

Copyright 1995, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/lbxproxy/include/resource.h,v 1.6 2003/11/17 22:20:49 dawes Exp $ */

#ifndef RESOURCE_H
#define RESOURCE_H 1

#define RT_COLORMAP	((RESTYPE)1)
#define RT_CMAPENTRY	((RESTYPE)2)
#define RT_LASTPREDEF	((RESTYPE)2)
#define RT_NONE		((RESTYPE)0)

#define RC_NEVERRETAIN	((RESTYPE)1<<29)
#define RC_LASTPREDEF	RC_NEVERRETAIN
#define RC_ANY		(~(RESTYPE)0)

typedef unsigned long RESTYPE;

typedef struct _Resource {
    struct _Resource	*next;
    XID			id;
    RESTYPE		type;
    pointer		value;
} ResourceRec, *ResourcePtr;
#define NullResource ((ResourcePtr)NULL)

typedef struct _ClientResource {
    ResourcePtr *resources;
    int		elements;
    int		buckets;
    int		hashsize;	/* log(2)(buckets) */
    XID		fakeID;
    XID		endFakeID;
} ClientResourceRec;


/* bits and fields within a resource id */
#define PROXY_BIT		0x40000000		/* use illegal bit */

typedef int (*DeleteType)(
    ClientPtr /*client*/,
    pointer /*value*/,
    XID /*id*/
);

extern Bool InitDeleteFuncs(
    void
);

extern Bool InitClientResources(
    ClientPtr /*client*/
);

extern void FinishInitClientResources(
    ClientPtr /*client*/,
    XID /*ridBase*/,
    XID /*ridMask*/
);

extern XID FakeClientID(
    int /*client*/
);

extern Bool AddResource(
    ClientPtr /*client*/,
    XID /*id*/,
    RESTYPE /*type*/,
    pointer /*value*/
);

extern void FreeResource(
    ClientPtr /*client*/,
    XID /*id*/,
    RESTYPE /*skipDeleteFuncType*/
);

extern void FreeClientResources(
    ClientPtr /*client*/
);

extern void FreeAllResources(
    void
);

extern pointer LookupIDByType(
    ClientPtr /*client*/,
    XID /*id*/,
    RESTYPE /*rtype*/
);

#endif
