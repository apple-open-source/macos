/* $Xorg: atomcache.c,v 1.4 2001/02/09 02:05:31 xorgcvs Exp $ */
/*

Copyright 1998  The Open Group

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
/*
 * Copyright 1994 Network Computing Devices, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name Network Computing Devices, Inc. not be
 * used in advertising or publicity pertaining to distribution of this 
 * software without specific, written prior permission.
 * 
 * THIS SOFTWARE IS PROVIDED `AS-IS'.  NETWORK COMPUTING DEVICES, INC.,
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT
 * LIMITATION ALL IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, OR NONINFRINGEMENT.  IN NO EVENT SHALL NETWORK
 * COMPUTING DEVICES, INC., BE LIABLE FOR ANY DAMAGES WHATSOEVER, INCLUDING
 * SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS OF USE, DATA,
 * OR PROFITS, EVEN IF ADVISED OF THE POSSIBILITY THEREOF, AND REGARDLESS OF
 * WHETHER IN AN ACTION IN CONTRACT, TORT OR NEGLIGENCE, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 */
/* $XFree86: xc/programs/lbxproxy/di/atomcache.c,v 1.7 2002/10/15 02:16:26 dawes Exp $ */

/*
 * atom cache for LBX
 *
 * real close to standard atom code, but with a slight twist.
 * LBX doesn't want to invent the Atom associated with a string,
 * but insert it itself.  only LbxMakeAtom() ends up being modified.
 *
 * the end result is a (probably) sparse array of atoms.
 *
 * when a client does a GetAtomName(), we use NameForAtom()
 * - if it works, great, return the value.
 * - if its doesn't, send it on to the server, and when the answer
 * 	comes back, call LbxMakeAtom()
 *
 * when a client does InternAtom(), LbxMakeAtom() is called.
 * if its already there (or only-if-exists is true) we return
 * the appropriate value. if not, we send it on to the server,
 * and call LbxMakeAtom() with the answer.
 */


#include "misc.h"
#include "util.h"
#include "wire.h"
#include "atomcache.h"

static int
Hash(string, len)
    char       *string;
    int		len;
{
    int         h;

    h = 0;
    while (len--)
	h = (h << 3) ^ *string++;
    if (h < 0)
	return -h;
    return h;
}

static Bool
ResizeHashTable(server)
    XServerPtr	server;
{
    int         newHashSize;
    int         newHashMask;
    AtomListPtr *newHashTable;
    int         i;
    int         h;
    int         newRehash;
    int         r;

    if (server->hashSize == 0)
	newHashSize = 1024;
    else
	newHashSize = server->hashSize * 2;
    newHashTable = (AtomListPtr *) xalloc(newHashSize * sizeof(AtomListPtr));
    if (!newHashTable)
	return FALSE;
    bzero((char *) newHashTable, newHashSize * sizeof(AtomListPtr));
    newHashMask = newHashSize - 1;
    newRehash = (newHashMask - 2);
    for (i = 0; i < server->hashSize; i++) {
	if (server->hashTable[i]) {
	    h = (server->hashTable[i]->hash) & newHashMask;
	    if (newHashTable[h]) {
		r = server->hashTable[i]->hash % newRehash | 1;
		do {
		    h += r;
		    if (h >= newHashSize)
			h -= newHashSize;
		} while (newHashTable[h]);
	    }
	    newHashTable[h] = server->hashTable[i];
	}
    }
    xfree(server->hashTable);
    server->hashTable = newHashTable;
    server->hashSize = newHashSize;
    server->hashMask = newHashMask;
    server->rehash = newRehash;
    return TRUE;
}

static Bool
ResizeReverseMap(server, atom)
    XServerPtr	server;
    Atom	atom;
{
    int oldMapSize = 0;

    /* has the map already been initialized? */
    if (server->reverseMapSize <= 0)
	server->reverseMapSize = 1000;
    else
	/* keep track of the map size before we resize it */
	oldMapSize = server->reverseMapSize;

    /* grow the map until it is big enough */
    while (server->reverseMapSize < atom)
	server->reverseMapSize *= 2;

    /* resize the map */
    server->reverseMap = (AtomListPtr *) xrealloc(server->reverseMap,
		server->reverseMapSize * sizeof(AtomListPtr));

    if (!server->reverseMap)
	/* memory allocation problem */
	return FALSE;

    /* zero out the new portion of the map */
    bzero((char *) (server->reverseMap + oldMapSize),
	  ((server->reverseMapSize - oldMapSize) * sizeof(AtomListPtr)));

    return TRUE;
}

Atom
LbxMakeAtom(server, string, len, atom, makeit)
    XServerPtr	server;
    char       *string;
    Atom        atom;
    unsigned    len;
    int         makeit;
{
    AtomListPtr a;
    int         hash;
    int         h = 0;
    int         r;

    hash = Hash(string, len);
    if (server->hashTable) {
	h = hash & server->hashMask;
	if (server->hashTable[h]) {
	    if (server->hashTable[h]->hash == hash && 
		server->hashTable[h]->len == len &&
		!strncmp(server->hashTable[h]->name, string, len)) {
		    return server->hashTable[h]->atom;
	    }
	    r = (hash % server->rehash) | 1;
	    for (;;) {
		h += r;
		if (h >= server->hashSize)
		    h -= server->hashSize;
		if (!server->hashTable[h])
		    break;
		if (server->hashTable[h]->hash == hash && 
		    server->hashTable[h]->len == len &&
		    !strncmp(server->hashTable[h]->name, string, len)) {
			return server->hashTable[h]->atom;
		}
	    }
	}
    }
    if (!makeit)
	return None;
    a = (AtomListPtr) xalloc(sizeof(AtomListRec) + len + 1);
    a->name = (char *) (a + 1);
    a->len = len;
    strncpy(a->name, string, len);
    a->name[len] = '\0';
    a->atom = atom;
    if (atom > server->lastAtom)
	server->lastAtom = atom;
    a->hash = hash;
    if (server->hashUsed >= server->hashSize / 2) {
	ResizeHashTable(server);
	h = hash & server->hashMask;
	if (server->hashTable[h]) {
	    r = (hash % server->rehash) | 1;
	    do {
		h += r;
		if (h >= server->hashSize)
		    h -= server->hashSize;
	    } while (server->hashTable[h]);
	}
    }
    server->hashTable[h] = a;
    server->hashUsed++;
    a->flags = 0;
    for (r = 0; r < server->atom_control_count; r++) {
	if (a->len == server->atom_control[r].len &&
	    !strncmp(a->name, server->atom_control[r].name, a->len)) {
	    a->flags = server->atom_control[r].flags;
	    break;
	}
    }
    if (server->reverseMapSize <= a->atom)
	ResizeReverseMap(server, a->atom);
    server->reverseMap[a->atom] = a;
    return a->atom;
}

char *
NameForAtom(server, atom)
    XServerPtr	server;
    Atom        atom;
{
    if (atom != None && atom <= server->lastAtom && server->reverseMap[atom])
	return server->reverseMap[atom]->name;
    return 0;
}

unsigned
FlagsForAtom(server, atom)
    XServerPtr	server;
    Atom        atom;
{
    if (atom != None && atom <= server->lastAtom && server->reverseMap[atom])
	return server->reverseMap[atom]->flags;
    return 0;
}

void
FreeAtoms()
{
    int         i;

    for (i=0; i < lbxMaxServers; i++) {
	if (servers[i]) {
	    if (servers[i]->reverseMap) {
		for (i = 0; i <= servers[i]->lastAtom; i++)
		    xfree(servers[i]->reverseMap[i]);
		xfree(servers[i]->reverseMap);
	    }
	    xfree(servers[i]->hashTable);
	    servers[i]->reverseMapSize = 0;
	    servers[i]->reverseMap = NULL;
	    servers[i]->hashTable = NULL;
	    servers[i]->lastAtom = 0;
	    servers[i]->hashSize = 0;
	    servers[i]->hashUsed = 0;
	}
    }
}
