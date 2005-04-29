/* $Xorg: FSOpenServ.c,v 1.4 2001/02/09 02:03:25 xorgcvs Exp $ */

/* @(#)FSOpenServ.c	4.1	91/05/02
 * Copyright 1990 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software 
 * and its documentation for any purpose is hereby granted without fee, 
 * provided that the above copyright notice appear in all copies and 
 * that both that copyright notice and this permission notice appear 
 * in supporting documentation, and that the names of Network Computing 
 * Devices or Digital not be used in advertising or publicity pertaining 
 * to distribution of the software without specific, written prior 
 * permission. Network Computing Devices or Digital make no representations 
 * about the suitability of this software for any purpose.  It is provided 
 * "as is" without express or implied warranty.
 *
 * NETWORK COMPUTING DEVICES AND  DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES 
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, 
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, 
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS 
 * SOFTWARE.
 */
/* $XFree86: xc/lib/FS/FSOpenServ.c,v 1.9 2003/12/22 17:48:02 tsi Exp $ */

/*

Copyright 1987, 1994, 1998  The Open Group

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
 * does initial handshake w/ font server
 */

#include	<stdio.h>
#include	"FSlibint.h"
#include 	<X11/Xtrans.h>

int         _FSdebug = 0;

static fsReq _dummy_request = {
    0, 0, 0
};

static void OutOfMemory ( FSServer *svr, char *setup );

FSServer   *_FSHeadOfServerList = NULL;

void _FSFreeServerStructure(svr)
    FSServer   *svr;
{
    if (svr->server_name)
	FSfree(svr->server_name);
    if (svr->vendor)
	FSfree(svr->vendor);

    if (svr->buffer)
	FSfree(svr->buffer);

    FSfree((char *) svr);
}

static
void OutOfMemory(svr, setup)
    FSServer   *svr;
    char       *setup;
{

    _FSDisconnectServer(svr->trans_conn);
    _FSFreeServerStructure(svr);
    FSfree(setup);
    errno = ENOMEM;
}

/*
 * connects to a server, makes a FSServer object and returns a pointer
 * to it
 */

FSServer   *
FSOpenServer(server)
    char       *server;
{
    FSServer   *svr;
    int         i;
    int         endian;
    fsConnClientPrefix client;
    fsConnSetup prefix;
    char       *setup;
    fsConnSetupAccept conn;
    char       *auth_data;
    char       *alt_data,
               *ad;
    AlternateServer *alts;
    int         altlen;
    char       *vendor_string;
    unsigned long        setuplength;

    if (server == NULL || *server == '\0') {
	if ((server = getenv("FONTSERVER")) == NULL) {
	    return (FSServer *) NULL;
	}
    }

    if ((svr = (FSServer *) FScalloc(1, sizeof(FSServer))) == NULL) {
	errno = ENOMEM;
	return (FSServer *) NULL;
    }
    if ((svr->trans_conn = _FSConnectServer(server)) == NULL) {
	FSfree((char *) svr);
	return (FSServer *) NULL;
    }

    svr->fd = _FSTransGetConnectionNumber (svr->trans_conn);

    endian = 1;
    if (*(char *) &endian)
	client.byteOrder = 'l';
    else
	client.byteOrder = 'B';
    client.major_version = FS_PROTOCOL;
    client.minor_version = FS_PROTOCOL_MINOR;
/* XXX -- fix this when we have some auths */
    client.num_auths = 0;
    client.auth_len = 0;
    _FSSendClientPrefix(svr, &client);

/* see if connection was accepted */
    _FSRead(svr, (char *) &prefix, (long) SIZEOF(fsConnSetup));

    setuplength = prefix.alternate_len << 2;
    if (setuplength > (SIZE_MAX>>2)
	|| (alt_data = (char *)
	 (setup = FSmalloc((unsigned) setuplength))) == NULL) {
	errno = ENOMEM;
	FSfree((char *) svr);
	return (FSServer *) NULL;
    }
    _FSRead(svr, (char *) alt_data, setuplength);
    ad = alt_data;

#if SIZE_MAX <= UINT_MAX
    if (prefix.num_alternates > SIZE_MAX / sizeof(AlternateServer)) {
	errno = ENOMEM;
	FSfree((char *) alt_data);
	FSfree((char *) svr);
	return (FSServer *) 0;
    }
#endif

    alts = (AlternateServer *)
	FSmalloc(sizeof(AlternateServer) * prefix.num_alternates);
    if (!alts) {
	errno = ENOMEM;
	FSfree((char *) alt_data);
	FSfree((char *) svr);
	return (FSServer *) 0;
    }
    for (i = 0; i < prefix.num_alternates; i++) {
	alts[i].subset = (Bool) *ad++;
	altlen = (int) *ad++;
	alts[i].name = (char *) FSmalloc(altlen + 1);
	if (!alts[i].name) {
	    while (--i) {
		FSfree((char *) alts[i].name);
	    }
	    FSfree((char *) alts);
	    FSfree((char *) alt_data);
	    FSfree((char *) svr);
	    errno = ENOMEM;
	    return (FSServer *) 0;
	}
	bcopy(ad, alts[i].name, altlen);
	alts[i].name[altlen] = '\0';
	ad += altlen + ((4 - (altlen + 2)) & 3);
    }
    FSfree((char *) alt_data);

    svr->alternate_servers = alts;
    svr->num_alternates = prefix.num_alternates;

    setuplength = prefix.auth_len << 2;
    if (setuplength > (SIZE_MAX>>2) 
	|| (auth_data = (char *)
	 (setup = FSmalloc((unsigned) setuplength))) == NULL) {
	errno = ENOMEM;
	FSfree((char *) alts);
	FSfree((char *) svr);
	return (FSServer *) NULL;
    }
    _FSRead(svr, (char *) auth_data, setuplength);

    if (prefix.status != AuthSuccess) {
	fprintf(stderr, "%s: connection to \"%s\" refused by server\r\n%s: ",
		"FSlib", server, "FSlib");
	FSfree((char *) alts);
	FSfree((char *) svr);
	FSfree(setup);
	return (FSServer *) NULL;
    }
    /* get rest */
    _FSRead(svr, (char *) &conn, (long) SIZEOF(fsConnSetupAccept));

    if ((vendor_string = (char *)
	 FSmalloc((unsigned) conn.vendor_len + 1)) == NULL) {
	errno = ENOMEM;
	FSfree((char *) auth_data);
	FSfree((char *) alts);
	FSfree((char *) svr);
	return (FSServer *) NULL;
    }
    _FSReadPad(svr, (char *) vendor_string, conn.vendor_len);

    /* move the data into the FSServer struct */
    svr->next = (FSServer *) NULL;
    svr->proto_version = prefix.major_version;
    svr->release = conn.release_number;
    svr->max_request_size = conn.max_request_len;

    svr->event_vec[FS_Error] = _FSUnknownWireEvent;
    svr->event_vec[FS_Reply] = _FSUnknownWireEvent;
    svr->wire_vec[FS_Error] = _FSUnknownNativeEvent;
    svr->wire_vec[FS_Reply] = _FSUnknownNativeEvent;
    for (i = FSLASTEvent; i < 128; i++) {
	svr->event_vec[i] = _FSUnknownWireEvent;
	svr->wire_vec[i] = _FSUnknownNativeEvent;
    }
    svr->resource_id = 1;

    svr->vendor = vendor_string;
    svr->vendor[conn.vendor_len] = '\0';

    svr->vnumber = FS_PROTOCOL;
    svr->request = 0;
    svr->last_request_read = 0;
    svr->last_req = (char *) &_dummy_request;

    if ((svr->server_name = FSmalloc((unsigned) (strlen(server) + 1)))
	    == NULL) {
	OutOfMemory(svr, setup);
	return (FSServer *) NULL;
    }
    (void) strcpy(svr->server_name, server);

    /* setup the output buffers */
    if ((svr->bufptr = svr->buffer = FSmalloc(BUFSIZE)) == NULL) {
	OutOfMemory(svr, setup);
	return (FSServer *) NULL;
    }
    svr->bufmax = svr->buffer + BUFSIZE;

    /* set up input event queue */
    svr->head = svr->tail = NULL;
    svr->qlen = 0;

    FSfree(setup);

    (void) FSSynchronize(svr, _FSdebug);

    svr->next = _FSHeadOfServerList;
    _FSHeadOfServerList = svr;

    return (svr);
}
