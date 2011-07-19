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

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * mitauth
 *
 * generate authorization keys
 * for MIT-MAGIC-COOKIE-1 type authorization
 */

#include   <X11/Xos.h>

#include   "dm.h"
#include   "dm_auth.h"

#define AUTH_DATA_LEN	16	/* bytes of authorization data */
static char	auth_name[256];

void
MitInitAuth (unsigned short name_len, char *name)
{
    if (name_len > 256)
	name_len = 256;
    memmove( auth_name, name, name_len);
}

Xauth *
MitGetAuth (unsigned short namelen, char *name)
{
    Xauth   *new;
    new = (Xauth *) malloc (sizeof (Xauth));

    if (!new)
	return (Xauth *) 0;
    new->family = FamilyWild;
    new->address_length = 0;
    new->address = NULL;
    new->number_length = 0;
    new->number = NULL;

    new->data = (char *) malloc (AUTH_DATA_LEN);
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
    memmove( (char *)new->name, name, namelen);
    new->name_length = namelen;
    if (!GenerateAuthData (new->data, AUTH_DATA_LEN))
    {
	free((char *) new->name);
	free((char *) new->data);
	free((char *) new);
	return (Xauth *) 0;
    }
    new->data_length = AUTH_DATA_LEN;
    return new;
}
