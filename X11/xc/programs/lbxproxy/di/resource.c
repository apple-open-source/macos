/* $Xorg: resource.c,v 1.4 2000/08/17 19:53:56 cpqbld Exp $ */
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

*/

/************************************************************

Copyright 1988, 1989 Network Computing Devices, Inc.  All rights reserved.

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
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

********************************************************/


#include <stdio.h>
#include "misc.h"
#include "os.h"
#include "resource.h" 
#include "lbx.h" 
#include "colormap.h"
#include "wire.h"

static void RebuildTable();

#define INITBUCKETS 64
#define INITHASHSIZE 6
#define MAXHASHSIZE 11

#define NullResource ((ResourcePtr)NULL)

#define TypeMask (RC_LASTPREDEF - 1)

static DeleteType *DeleteFuncs = (DeleteType *)NULL;

/*
 * NOTE for LBX Multi Display
 *
 * The index field for each client in the global "clients" array
 * represents that client's position in the "clients" array.
 *
 * Each server has its own clientTable array and the client's
 * index in the "clients" array is identical to the client's
 * index in its server's clientTable array.   This facilitates
 * the storage and retrieval of a client's resources.
 */

Bool
InitDeleteFuncs()
{
    if (DeleteFuncs)
	xfree(DeleteFuncs);
    DeleteFuncs = (DeleteType *) xalloc((RT_LASTPREDEF + 1) *
					sizeof(DeleteType));
    if (!DeleteFuncs)
	return FALSE;
    DeleteFuncs[RT_COLORMAP & TypeMask] = DestroyColormap;
    DeleteFuncs[RT_CMAPENTRY & TypeMask] = FreeClientPixels;
    return TRUE;
}

/*****************
 * InitClientResources
 *    When a new client is created, call this to allocate space
 *    in resource table
 *****************/

Bool
InitClientResources(client)
    ClientPtr client;
{
    register int i, j;
 
    if (!client || !client->server)
	/*
	 * This can occur during a reset
	 */
	return TRUE;

    client->server->clientTable[i = client->index].resources =
	(ResourcePtr *)xalloc(INITBUCKETS*sizeof(ResourcePtr));
    if (!client->server->clientTable[i].resources)
	return FALSE;

    client->server->clientTable[i].buckets = INITBUCKETS;
    client->server->clientTable[i].elements = 0;
    client->server->clientTable[i].hashsize = INITHASHSIZE;

    for (j=0; j<INITBUCKETS; j++) 
    {
        client->server->clientTable[i].resources[j] = NullResource;
    }
    return TRUE;
}

void
FinishInitClientResources(client, ridBase, ridMask)
    ClientPtr client;
    XID ridBase, ridMask;
{
    client->ridBase = ridBase;
    client->ridMask = ridMask;
    client->server->clientTable[client->index].endFakeID = 
		(ridBase | ridMask) + 1;
    client->server->clientTable[client->index].fakeID = ridBase | PROXY_BIT;
}

static int
Hash(client, id)
    int client;
    register XID id;
{
    id &= clients[client]->ridMask;
    switch (clients[client]->server->clientTable[client].hashsize)
    {
	case 6:
	    return ((int)(0x03F & (id ^ (id>>6) ^ (id>>12))));
	case 7:
	    return ((int)(0x07F & (id ^ (id>>7) ^ (id>>13))));
	case 8:
	    return ((int)(0x0FF & (id ^ (id>>8) ^ (id>>16))));
	case 9:
	    return ((int)(0x1FF & (id ^ (id>>9))));
	case 10:
	    return ((int)(0x3FF & (id ^ (id>>10))));
	case 11:
	    return ((int)(0x7FF & (id ^ (id>>11))));
    }
    return -1;
}

static XID
AvailableID(client, id, maxid, goodid)
    register int client;
    register XID id, maxid, goodid;
{
    register ResourcePtr res;

    if ((goodid >= id) && (goodid <= maxid))
	return goodid;
    for (; id <= maxid; id++)
    {
	res = clients[client]->server->clientTable[client].resources[Hash(client, id)];
	while (res && (res->id != id))
	    res = res->next;
	if (!res)
	    return id;
    }
    return 0;
}

/*
 * return the next usable fake client ID.
 *
 * normally this is just the next one in line, but if we've used the last
 * in the range, we need to find a new range of safe IDs to avoid
 * over-running another client.
 */

XID
FakeClientID(client)
    register int client;	/* Index of a client for the global clients 
				 * array and the server's clientTable */
{
    register XID id, maxid;
    register ResourcePtr *resp;
    register ResourcePtr res;
    register int i;
    XID goodid = 0;

    id = clients[client]->server->clientTable[client].fakeID++;
    if (id != clients[client]->server->clientTable[client].endFakeID)
	return id;

    id = clients[client]->ridBase | PROXY_BIT;
    maxid = id | clients[client]->ridMask;

    for (resp = clients[client]->server->clientTable[client].resources, 
	 i    = clients[client]->server->clientTable[client].buckets;
	 --i >= 0;)
    {
	for (res = *resp++; res; res = res->next)
	{
	    if ((res->id < id) || (res->id > maxid))
		continue;
	    if (((res->id - id) >= (maxid - res->id)) ?
		 (goodid = AvailableID(client, id, res->id - 1, goodid)) :
		!(goodid = AvailableID(client, res->id + 1, maxid, goodid)))
		maxid = res->id - 1;
	    else
		id = res->id + 1;
	}
    }

    clients[client]->server->clientTable[client].fakeID = id + 1;
    clients[client]->server->clientTable[client].endFakeID = maxid + 1;

    return id;
}

Bool
AddResource(pclient, id, type, value)
    ClientPtr pclient;
    XID id;
    RESTYPE type;
    pointer value;
{
    int client;
    register ClientResourceRec *rrec;
    register ResourcePtr res, *head;
    	
    client = pclient->index;

    rrec = &clients[client]->server->clientTable[client];
    if (!rrec->buckets)
    {
	ErrorF("AddResource(%x, %x, %x), client=%d \n",
		id, type, (unsigned long)value, client);
        FatalError("client not in use\n");
    }
    if ((rrec->elements >= 4*rrec->buckets) &&
	(rrec->hashsize < MAXHASHSIZE))
	RebuildTable(client);
    head = &rrec->resources[Hash(client, id)];
    res = (ResourcePtr)xalloc(sizeof(ResourceRec));
    if (!res)
    {
	(*DeleteFuncs[type & TypeMask])(pclient, value, id);
	return FALSE;
    }
    res->next = *head;
    res->id = id;
    res->type = type;
    res->value = value;
    *head = res;
    rrec->elements++;
    return TRUE;
}

static void
RebuildTable(client)
    int client;
{
    register int j;
    register ResourcePtr res, next;
    ResourcePtr **tails, *resources;
    register ResourcePtr **tptr, *rptr;

    /*
     * For now, preserve insertion order, since some ddx layers depend
     * on resources being free in the opposite order they are added.
     */

    j = 2 * clients[client]->server->clientTable[client].buckets;
    tails = (ResourcePtr **)ALLOCATE_LOCAL(j * sizeof(ResourcePtr *));
    if (!tails)
	return;
    resources = (ResourcePtr *)xalloc(j * sizeof(ResourcePtr));
    if (!resources)
    {
	DEALLOCATE_LOCAL(tails);
	return;
    }
    for (rptr = resources, tptr = tails; --j >= 0; rptr++, tptr++)
    {
	*rptr = NullResource;
	*tptr = rptr;
    }
    clients[client]->server->clientTable[client].hashsize++;
    for (j = clients[client]->server->clientTable[client].buckets,
	 rptr = clients[client]->server->clientTable[client].resources;
	 --j >= 0;
	 rptr++)
    {
	for (res = *rptr; res; res = next)
	{
	    next = res->next;
	    res->next = NullResource;
	    tptr = &tails[Hash(client, res->id)];
	    **tptr = res;
	    *tptr = &res->next;
	}
    }
    DEALLOCATE_LOCAL(tails);
    clients[client]->server->clientTable[client].buckets *= 2;
    xfree(clients[client]->server->clientTable[client].resources);
    clients[client]->server->clientTable[client].resources = resources;
}

void
FreeResource(client, id, skipDeleteFuncType)
    ClientPtr client;
    XID id;
    RESTYPE skipDeleteFuncType;
{
    int		cid;
    register    ResourcePtr res;
    register	ResourcePtr *prev, *head;
    register	int *eltptr;
    int		elements;
    Bool	gotOne = FALSE;

    cid = client->index;

    if (clients[cid]->server->clientTable[cid].buckets)
    {
	head =   &clients[cid]->server->clientTable[cid].resources[Hash(cid, id)];
	eltptr = &clients[cid]->server->clientTable[cid].elements;

	prev = head;
	while ((res = *prev))
	{
	    if (res->id == id)
	    {
		RESTYPE rtype = res->type;
		*prev = res->next;
		elements = --*eltptr;
		if (rtype != skipDeleteFuncType)
		    (*DeleteFuncs[rtype & TypeMask])(client, res->value, res->id);
		xfree(res);
		if (*eltptr != elements)
		    prev = head; /* prev may no longer be valid */
		gotOne = TRUE;
	    }
	    else
		prev = &res->next;
        }
    }
    if (!gotOne)
	FatalError("Freeing resource id=%X which isn't there", id);
}

void
FreeClientResources(client)
    ClientPtr client;
{
    register ResourcePtr *resources;
    register ResourcePtr this;
    int j;

    /* This routine shouldn't be called with a null client, but just in
	case ... */

    if (!client)
	return;

    /*
     * For a multi-display proxy, must not re-free the clientTable
     * for a server.
     */
    if (!client->server)
	return;

    resources = client->server->clientTable[client->index].resources;
    for (j=0; j < client->server->clientTable[client->index].buckets; j++) 
    {
        /* It may seem silly to update the head of this resource list as
	we delete the members, since the entire list will be deleted any way, 
	but there are some resource deletion functions "FreeClientPixels" for 
	one which do a LookupID on another resource id (a Colormap id in this
	case), so the resource list must be kept valid up to the point that
	it is deleted, so every time we delete a resource, we must update the
	head, just like in FreeResource. I hope that this doesn't slow down
	mass deletion appreciably. PRH */

	ResourcePtr *head;

	head = &resources[j];

        for (this = *head; this; this = *head)
	{
	    RESTYPE rtype = this->type;
	    *head = this->next;
	    (*DeleteFuncs[rtype & TypeMask])(client, this->value, this->id);
	    xfree(this);	    
	}
    }
    xfree(client->server->clientTable[client->index].resources);
    client->server->clientTable[client->index].buckets = 0;
    if (client->server->lastLbxClientIndexLookup == client)
	client->server->lastLbxClientIndexLookup = NULL;
}

void
FreeAllResources()
{
    int	j;

    for (j = currentMaxClients; --j >= 0; ) 
    {
	if (clients[j]->server->clientTable[j].buckets)
	    FreeClientResources(clients[j]);
    }
}

/*
 *  LookupIDByType returns the object with the given id and type, else NULL.
 */ 
pointer
LookupIDByType(pclient, id, rtype)
    ClientPtr pclient;
    XID id;
    RESTYPE rtype;
{
    register    ResourcePtr res;
    int i, j;
    XServerPtr pserver = pclient->server;

    for (i = 1; i < currentMaxClients; i++) {
	if (pserver->clientTable[i].buckets) {
	    for (j = 0; j < INITBUCKETS; j++) {
		if (pserver->clientTable[i].resources[j]) {
	            res = pserver->clientTable[i].resources[Hash(j, id)];
		    for (; res; res = res->next)
			if ((res->id == id) && (res->type == rtype))
			    return res->value;
		}
	    }
	}
    }

    return (pointer)NULL;
}
