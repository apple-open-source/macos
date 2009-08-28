/* $Xorg: rpcauth.c,v 1.4 2001/02/09 02:05:40 xorgcvs Exp $ */
/*

Copyright 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
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
/* $XFree86: xc/programs/xdm/rpcauth.c,v 1.3 2001/01/17 23:45:22 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * rpcauth
 *
 * generate SecureRPC authorization records
 */

# include   <X11/Xos.h>
# include   <rpc/rpc.h>
# include   <rpc/key_prot.h>

# include   "dm.h"
# include   "dm_auth.h"
# include   "dm_error.h"

/*ARGSUSED*/
void
SecureRPCInitAuth (unsigned short name_len, char *name)
{
}

Xauth *
SecureRPCGetAuth (
    unsigned short  namelen,
    char	    *name)
{
    char    key[MAXNETNAMELEN+1];
    Xauth   *new;

    new = (Xauth *) malloc (sizeof *new);
    if (!new)
	return (Xauth *) 0;
    new->family = FamilyWild;
    new->address_length = 0;
    new->address = 0;
    new->number_length = 0;
    new->number = 0;

    getnetname (key);
    Debug ("System netname %s\n", key);
    new->data_length = strlen(key);
    new->data = (char *) malloc (new->data_length);
    if (!new->data)
    {
	free ((char *) new);
	return (Xauth *) 0;
    }
    new->name = (char *) malloc (namelen);
    if (!new->name)
    {
	free ((char *) new->data);
	free ((char *) new);
	return (Xauth *) 0;
    }
    memmove( new->name, name, namelen);
    new->name_length = namelen;
    memmove( new->data, key, new->data_length);
    return new;
}
