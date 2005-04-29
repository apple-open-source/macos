/* $Xorg: atom.c,v 1.4 2001/02/09 02:05:42 xorgcvs Exp $ */
/*
 * font server atom manipulations
 */
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
 * @(#)atom.c	4.1	5/2/91
 *
 */
/* $XFree86: xc/programs/xfs/difs/atom.c,v 3.5 2003/05/27 22:27:05 tsi Exp $ */

#include "misc.h"
#include "fsresource.h"
#include "difs.h"

#define InitialTableSize 100
#define	FSA_LAST_PREDEFINED	0 /* only None is predefined */

typedef struct _Node {
    struct _Node *left,
               *right;
    Atom        a;
    unsigned int fingerPrint;
    char       *string;
}           NodeRec, *NodePtr;

static Atom lastAtom = None;
static NodePtr atomRoot = (NodePtr) NULL;
static unsigned long tableLength;
static NodePtr *nodeTable;

Atom
MakeAtom(char *string, unsigned int len, Bool makeit)
{
    register NodePtr *np;
    unsigned    i;
    int         comp;
    register unsigned int fp = 0;

    np = &atomRoot;
    for (i = 0; i < (len + 1) / 2; i++) {
	fp = fp * 27 + string[i];
	fp = fp * 27 + string[len - 1 - i];
    }
    while (*np != (NodePtr) NULL) {
	if (fp < (*np)->fingerPrint)
	    np = &((*np)->left);
	else if (fp > (*np)->fingerPrint)
	    np = &((*np)->right);
	else {			/* now start testing the strings */
	    comp = strncmp(string, (*np)->string, (int) len);
	    if ((comp < 0) || ((comp == 0) && (len < strlen((*np)->string))))
		np = &((*np)->left);
	    else if (comp > 0)
		np = &((*np)->right);
	    else
		return (*np)->a;
	}
    }
    if (makeit) {
	register NodePtr nd;

	nd = (NodePtr) fsalloc(sizeof(NodeRec));
	if (!nd)
	    return BAD_RESOURCE;
#if FSA_LAST_PREDEFINED > 0
	if (lastAtom < FSA_LAST_PREDEFINED) {
	    nd->string = string;
	} else
#endif
	{
	    nd->string = (char *) fsalloc(len + 1);
	    if (!nd->string) {
		fsfree(nd);
		return BAD_RESOURCE;
	    }
	    strncpy(nd->string, string, (int) len);
	    nd->string[len] = 0;
	}
	if ((lastAtom + 1) >= tableLength) {
	    NodePtr    *table;

	    table = (NodePtr *) fsrealloc(nodeTable,
					tableLength * (2 * sizeof(NodePtr)));
	    if (!table) {
		if (nd->string != string)
		    fsfree(nd->string);
		fsfree(nd);
		return BAD_RESOURCE;
	    }
	    tableLength <<= 1;
	    nodeTable = table;
	}
	*np = nd;
	nd->left = nd->right = (NodePtr) NULL;
	nd->fingerPrint = fp;
	nd->a = (++lastAtom);
	*(nodeTable + lastAtom) = nd;
	return nd->a;
    } else
	return None;
}

int
ValidAtom(Atom atom)
{
    return (atom != None) && (atom <= lastAtom);
}

char       *
NameForAtom(Atom atom)
{
    NodePtr     node;

    if (atom > lastAtom)
	return 0;
    if ((node = nodeTable[atom]) == (NodePtr) NULL)
	return 0;
    return node->string;
}

static void
atom_error(void)
{
    FatalError("initializing atoms\n");
}

static void
free_atom(NodePtr patom)
{
    if (patom->left)
	free_atom(patom->left);
    if (patom->right)
	free_atom(patom->right);
    if (patom->a > FSA_LAST_PREDEFINED)
	fsfree(patom->string);
    fsfree(patom);
}

static void
free_all_atoms(void)
{
    if (atomRoot == (NodePtr) NULL)
	return;
    free_atom(atomRoot);
    atomRoot = (NodePtr) NULL;
    fsfree(nodeTable);
    nodeTable = (NodePtr *) NULL;
    lastAtom = None;
}

void
InitAtoms(void)
{
    free_all_atoms();
    tableLength = InitialTableSize;
    nodeTable = (NodePtr *) fsalloc(InitialTableSize * sizeof(NodePtr));
    if (!nodeTable)
	atom_error();
    nodeTable[None] = (NodePtr) NULL;
    lastAtom = FSA_LAST_PREDEFINED;
}
