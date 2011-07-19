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
 * xdmauth
 *
 * generate authorization data for XDM-AUTHORIZATION-1 as per XDMCP spec
 */

#include "dm.h"
#include "dm_auth.h"
#include "dm_error.h"

#ifdef HASXDMAUTH

static char	auth_name[256];
static int	auth_name_len;

static void
XdmPrintDataHex (char *s, char *a, int l)
{
    int	i;

    Debug ("%s", s);
    for (i = 0; i < l; i++)
	Debug (" %02x", a[i] & 0xff);
    Debug ("\n");
}

# ifdef XDMCP
static void
XdmPrintArray8Hex (char *s, ARRAY8Ptr a)
{
    XdmPrintDataHex (s, (char *) a->data, a->length);
}
# endif

void
XdmInitAuth (unsigned short name_len, char *name)
{
    if (name_len > 256)
	name_len = 256;
    auth_name_len = name_len;
    memmove( auth_name, name, name_len);
}

/*
 * Generate authorization for XDM-AUTHORIZATION-1
 *
 * When being used with XDMCP, 8 bytes are generated for the session key
 * (sigma), as the random number (rho) is already shared between xdm and
 * the server.  Otherwise, we'll prepend a random number to pass in the file
 * between xdm and the server (16 bytes total)
 */

static Xauth *
XdmGetAuthHelper (unsigned short namelen, char *name, int includeRho)
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
    if (includeRho)
	new->data_length = 16;
    else
	new->data_length = 8;

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
    memmove( (char *)new->name, name, namelen);
    new->name_length = namelen;
    if (!GenerateAuthData ((char *)new->data, new->data_length))
    {
	free ((char *) new->name);
	free ((char *) new->data);
	free ((char *) new);
	return (Xauth *) 0;
    }
    /*
     * set the first byte of the session key to zero as it
     * is a DES key and only uses 56 bits
     */
    ((char *)new->data)[new->data_length - 8] = '\0';
    XdmPrintDataHex ("Local server auth", (char *)new->data, new->data_length);
    return new;
}

Xauth *
XdmGetAuth (unsigned short namelen, char *name)
{
    return XdmGetAuthHelper (namelen, name, TRUE);
}

# ifdef XDMCP

void
XdmGetXdmcpAuth (struct protoDisplay *pdpy,
    unsigned short authorizationNameLen, char *authorizationName)
{
    Xauth   *fileauth, *xdmcpauth;

    if (pdpy->fileAuthorization && pdpy->xdmcpAuthorization)
	return;
    xdmcpauth = XdmGetAuthHelper (authorizationNameLen, authorizationName, FALSE);
    if (!xdmcpauth)
	return;
    fileauth = (Xauth *) malloc (sizeof (Xauth));
    if (!fileauth)
    {
	XauDisposeAuth(xdmcpauth);
	return;
    }
    /* build the file auth from the XDMCP auth */
    *fileauth = *xdmcpauth;
    fileauth->name = malloc (xdmcpauth->name_length);
    fileauth->data = malloc (16);
    fileauth->data_length = 16;
    if (!fileauth->name || !fileauth->data)
    {
	XauDisposeAuth (xdmcpauth);
	if (fileauth->name)
	    free ((char *) fileauth->name);
	if (fileauth->data)
	    free ((char *) fileauth->data);
	free ((char *) fileauth);
	return;
    }
    /*
     * for the file authorization, prepend the random number (rho)
     * which is simply the number we've been passing back and
     * forth via XDMCP
     */
    memmove( fileauth->name, xdmcpauth->name, xdmcpauth->name_length);
    memmove( fileauth->data, pdpy->authenticationData.data, 8);
    memmove( fileauth->data + 8, xdmcpauth->data, 8);
    XdmPrintDataHex ("Accept packet auth", xdmcpauth->data, xdmcpauth->data_length);
    XdmPrintDataHex ("Auth file auth", fileauth->data, fileauth->data_length);
    /* encrypt the session key for its trip back to the server */
    XdmcpWrap ((unsigned char *)xdmcpauth->data, (unsigned char *)&pdpy->key,
	       (unsigned char *)xdmcpauth->data, 8);
    pdpy->fileAuthorization = fileauth;
    pdpy->xdmcpAuthorization = xdmcpauth;
}

#  define atox(c)	('0' <= c && c <= '9' ? c - '0' : \
		 'a' <= c && c <= 'f' ? c - 'a' + 10 : \
		 'A' <= c && c <= 'F' ? c - 'A' + 10 : -1)

static int
HexToBinary(char *key)
{
    char    *out, *in;
    int	    top, bottom;

    in = key + 2;
    out= key;
    while (in[0] && in[1])
    {
	top = atox(in[0]);
	if (top == -1)
	    return 0;
	bottom = atox(in[1]);
	if (bottom == -1)
	    return 0;
	*out++ = (top << 4) | bottom;
	in += 2;
    }
    if (in[0])
	return 0;
    *out++ = '\0';
    return 1;
}

/*
 * Search the Keys file for the entry matching this display.  This
 * routine accepts either plain ascii strings for keys, or hex-encoded numbers
 */

static int
XdmGetKey(struct protoDisplay *pdpy, ARRAY8Ptr displayID)
{
    FILE    *keys;
    char    line[1024], id[1024], key[1024];
    int	    keylen;

    Debug ("Lookup key for %*.*s\n", displayID->length, displayID->length, displayID->data);
    keys = fopen (keyFile, "r");
    if (!keys)
	return FALSE;
    while (fgets (line, sizeof (line) -  1, keys))
    {
	if (line[0] == '#' || sscanf (line, "%s %s", id, key) != 2)
	    continue;
	bzero(line, sizeof(line));
	Debug ("Key entry for \"%s\" %d bytes\n", id, strlen(key));
	if (strlen (id) == displayID->length &&
	    !strncmp (id, (char *)displayID->data, displayID->length))
	{
	    if (!strncmp (key, "0x", 2) || !strncmp (key, "0X", 2))
		if (!HexToBinary (key))
		    break;
	    keylen = strlen (key);
	    while (keylen < 7)
		key[keylen++] = '\0';
	    pdpy->key.data[0] = '\0';
	    memmove( pdpy->key.data + 1, key, 7);
	    bzero(key, sizeof(key));
	    fclose (keys);
	    return TRUE;
	}
    }
    bzero(line, sizeof(line));
    bzero(key, sizeof(key));
    fclose (keys);
    return FALSE;
}

/*ARGSUSED*/
int
XdmCheckAuthentication(struct protoDisplay *pdpy, ARRAY8Ptr displayID,
    ARRAY8Ptr authenticationName, ARRAY8Ptr authenticationData)
{
    XdmAuthKeyPtr   incoming;

    if (!XdmGetKey (pdpy, displayID))
	return FALSE;
    if (authenticationData->length != 8)
	return FALSE;
    XdmcpUnwrap (authenticationData->data, (unsigned char *)&pdpy->key,
		  authenticationData->data, 8);
    XdmPrintArray8Hex ("Request packet auth", authenticationData);
    if (!XdmcpCopyARRAY8(authenticationData, &pdpy->authenticationData))
	return FALSE;
    incoming = (XdmAuthKeyPtr) authenticationData->data;
    XdmcpIncrementKey (incoming);
    XdmcpWrap (authenticationData->data, (unsigned char *)&pdpy->key,
		  authenticationData->data, 8);
    return TRUE;
}

# endif /* XDMCP */
#endif /* HASXDMAUTH (covering the entire file) */
