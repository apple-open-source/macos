/* $Xorg: XDpyName.c,v 1.5 2001/02/09 02:05:58 xorgcvs Exp $ */
/*

Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT
SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABIL-
ITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization from
The Open Group.

*/

#ifdef XP_UNIX
#include "RxPlugin.h"		/* for PluginGlobal */
#endif
#include "RxI.h"

#include <X11/Xlib.h>
#include <X11/Xos.h>

#include <stdlib.h>
#include <stdio.h>

#include <X11/Xfuncs.h>
#include <X11/Xmd.h>
#include <X11/ICE/ICElib.h>
#include <X11/ICE/ICEmsg.h>
#include <X11/ICE/ICEproto.h>
#include <X11/PM/PM.h>
#include <X11/PM/PMproto.h>

#define DEFAULT_PROXY_MANAGER ":6500"

/*
 * Pad to a 64 bit boundary
 */

#define PAD64(_bytes) ((8 - ((unsigned int) (_bytes) % 8)) % 8)

#define PADDED_BYTES64(_bytes) (_bytes + PAD64 (_bytes))


/*
 * Number of 8 byte units in _bytes.
 */

#define WORD64COUNT(_bytes) (((unsigned int) ((_bytes) + 7)) >> 3)


/*
 * Compute the number of bytes for a STRING representation
 */

#define STRING_BYTES(_str) (2 + (_str ? strlen (_str) : 0) + \
		     PAD64 (2 + (_str ? strlen (_str) : 0)))



#define SKIP_STRING(_pBuf, _swap) \
{ \
    CARD16 _len; \
    EXTRACT_CARD16 (_pBuf, _swap, _len); \
    _pBuf += _len; \
    if (PAD64 (2 + _len)) \
        _pBuf += PAD64 (2 + _len); \
}

/*
 * STORE macros
 */

#define STORE_CARD16(_pBuf, _val) \
{ \
    *((CARD16 *) _pBuf) = _val; \
    _pBuf += 2; \
}

#define STORE_STRING(_pBuf, _string) \
{ \
    int _len = _string ? strlen (_string) : 0; \
    STORE_CARD16 (_pBuf, _len); \
    if (_len) { \
        memcpy (_pBuf, _string, _len); \
        _pBuf += _len; \
    } \
    if (PAD64 (2 + _len)) \
        _pBuf += PAD64 (2 + _len); \
}


/*
 * EXTRACT macros
 */

#define EXTRACT_CARD16(_pBuf, _swap, _val) \
{ \
    _val = *((CARD16 *) _pBuf); \
    _pBuf += 2; \
    if (_swap) \
        _val = lswaps (_val); \
}

#define EXTRACT_STRING(_pBuf, _swap, _string) \
{ \
    CARD16 _len; \
    EXTRACT_CARD16 (_pBuf, _swap, _len); \
    _string = (char *) malloc (_len + 1); \
    memcpy (_string, _pBuf, _len); \
    _string[_len] = '\0'; \
    _pBuf += _len; \
    if (PAD64 (2 + _len)) \
        _pBuf += PAD64 (2 + _len); \
}


/*
 * Byte swapping
 */

/* byte swap a long literal */
#define lswapl(_val) ((((_val) & 0xff) << 24) |\
		   (((_val) & 0xff00) << 8) |\
		   (((_val) & 0xff0000) >> 8) |\
		   (((_val) >> 24) & 0xff))

/* byte swap a short literal */
#define lswaps(_val) ((((_val) & 0xff) << 8) | (((_val) >> 8) & 0xff))


#define CHECK_AT_LEAST_SIZE(_iceConn, _majorOp, _minorOp, _expected_len, _actual_len, _severity) \
    if ((((_actual_len) - SIZEOF (iceMsg)) >> 3) > _expected_len) \
    { \
       _IceErrorBadLength (_iceConn, _majorOp, _minorOp, _severity); \
       return; \
    }


#define CHECK_COMPLETE_SIZE(_iceConn, _majorOp, _minorOp, _expected_len, _actual_len, _pStart, _severity) \
    if (((PADDED_BYTES64((_actual_len)) - SIZEOF (iceMsg)) >> 3) \
        != _expected_len) \
    { \
       _IceErrorBadLength (_iceConn, _majorOp, _minorOp, _severity); \
       IceDisposeCompleteMessage (iceConn, _pStart); \
       return; \
    }

static void PMprocessMessages ();

#ifdef XP_UNIX
#define PMOPCODE RxGlobal.pm_opcode
#define ICECONN  RxGlobal.ice_conn
#else
static int pm_opcode;
#define PMOPCODE pm_opcode
#define ICECONN  ice_conn
#endif

static int PMversionCount = 1;
static IcePoVersionRec	PMversions[] =
                {{PM_MAJOR_VERSION, PM_MINOR_VERSION, PMprocessMessages}};

typedef struct {
    int		status;
    char	*addr;
    char	*error;
} GetProxyAddrReply;

static int findproxy (proxyname, manager, server, name)
    char*			proxyname;
    char*			manager;
    char*			server;
    char*			name;
{
#ifndef XP_UNIX
    IceConn			ice_conn;
#endif
    IceProtocolSetupStatus	setupstat;
    char			*vendor = NULL;
    char			*release = NULL;
    pmGetProxyAddrMsg		*pMsg;
    char 			*pData;
    int				len;
    IceReplyWaitInfo		replyWait;
    GetProxyAddrReply		reply;
    int				majorVersion, minorVersion;
    Bool			gotReply, ioErrorOccured;
    char			errorString[255];

    /*
     * Register support for PROXY_MANAGEMENT.
     */

    if (PMOPCODE == 0) {
	if ((PMOPCODE = IceRegisterForProtocolSetup (
	    PM_PROTOCOL_NAME,
	    "XC", "1.0",
	    PMversionCount, PMversions,
	    0, /* authcount */
	    NULL, /* authnames */ 
            NULL, /* authprocs */
	    NULL  /* IceIOErrorProc */ )) < 0) {
	    fprintf (stderr,
		"Could not register PROXY_MANAGEMENT protocol with ICE\n");
	    return 0;
	}

	if ((ICECONN = IceOpenConnection (
	    manager, NULL, 0, 0, 256, errorString)) == NULL) {
	    fprintf (stderr,
		"Could not open ICE connection to proxy manager\n   (%s)\n",
		errorString);
	    return 0;
	}

	setupstat = IceProtocolSetup (ICECONN, PMOPCODE, NULL,
	    False /* mustAuthenticate */,
	    &majorVersion, &minorVersion,
	    &vendor, &release, 256, errorString);

	if (setupstat != IceProtocolSetupSuccess) {
	    IceCloseConnection (ICECONN);
	    fprintf (stderr,
		"Could not initialize proxy management protocol\n   (%s)\n",
		errorString);
	    fprintf (stderr, "%d\n", setupstat);
	    return 0;
	}
    }

    /*
     * Now send the GetProxyAddr request.
     */

    len = STRING_BYTES (name) + STRING_BYTES (server) +
	STRING_BYTES ("") + STRING_BYTES ("");

    IceGetHeaderExtra (ICECONN, PMOPCODE, PM_GetProxyAddr,
	SIZEOF (pmGetProxyAddrMsg), WORD64COUNT (len),
	pmGetProxyAddrMsg, pMsg, pData);

    pMsg->authLen = 0;

    STORE_STRING (pData, name);
    STORE_STRING (pData, server);
    STORE_STRING (pData, ""); /* host address, if there was any */
    STORE_STRING (pData, ""); /* start options, if there were any */

    IceFlush (ICECONN);

    replyWait.sequence_of_request = IceLastSentSequenceNumber (ICECONN);
    replyWait.major_opcode_of_request = PMOPCODE;
    replyWait.minor_opcode_of_request = PM_GetProxyAddr;
    replyWait.reply = (IcePointer) &reply;

    gotReply = False;
    ioErrorOccured = False;

    while (!gotReply && !ioErrorOccured) {
	ioErrorOccured = (IceProcessMessages (
	    ICECONN, &replyWait, &gotReply) == IceProcessMessagesIOError);

	if (ioErrorOccured) {
	    fprintf (stderr, "IO error occured\n");
	    IceCloseConnection (ICECONN);
	    return 0;
	} else if (gotReply) {
	    if (reply.status == PM_Success) {
		strcpy (proxyname, reply.addr);
	    } else {
		fprintf (stderr, "Error from proxy manager: %s\n",
		    reply.error);
		return 0;
	    }
	}
    }
    return 1;
}



static void
PMprocessMessages (iceConn, clientData, opcode,
    length, swap, replyWait, replyReadyRet)

IceConn		 iceConn;
IcePointer       clientData;
int		 opcode;
unsigned long	 length;
Bool		 swap;
IceReplyWaitInfo *replyWait;
Bool		 *replyReadyRet;

{
    if (replyWait)
	*replyReadyRet = False;

    switch (opcode) {
    case PM_GetProxyAddrReply:

	if (!replyWait ||
	    replyWait->minor_opcode_of_request != PM_GetProxyAddr) {
	    _IceReadSkip (iceConn, length << 3);

	    _IceErrorBadState (iceConn, PMOPCODE,
		PM_GetProxyAddrReply, IceFatalToProtocol);
	} else {
	    pmGetProxyAddrReplyMsg 	*pMsg;
	    char			*pData, *pStart;
	    GetProxyAddrReply 	*reply = 
		(GetProxyAddrReply *) (replyWait->reply);

	    CHECK_AT_LEAST_SIZE (iceConn, PMOPCODE, opcode,
		length, SIZEOF (pmGetProxyAddrReplyMsg), IceFatalToProtocol);

	    IceReadCompleteMessage (iceConn, SIZEOF (pmGetProxyAddrReplyMsg),
		pmGetProxyAddrReplyMsg, pMsg, pStart);

	    if (!IceValidIO (iceConn)) {
		IceDisposeCompleteMessage (iceConn, pStart);
		return;
	    }

	    pData = pStart;

	    SKIP_STRING (pData, swap);		/* proxy-address */
	    SKIP_STRING (pData, swap);		/* failure-reason */

	    CHECK_COMPLETE_SIZE (iceConn, PMOPCODE, opcode,
		length, pData - pStart + SIZEOF (pmGetProxyAddrReplyMsg),
		pStart, IceFatalToProtocol);

	    pData = pStart;

	    EXTRACT_STRING (pData, swap, reply->addr);
	    EXTRACT_STRING (pData, swap, reply->error);

	    reply->status = pMsg->status;
	    *replyReadyRet = True;

	    IceDisposeCompleteMessage (iceConn, pStart);
	}
	break;

    default: 
	_IceErrorBadMinor (iceConn, PMOPCODE, opcode, IceCanContinue);
	_IceReadSkip (iceConn, length << 3);
	break;
    }
}



/* This function returns the X Print Display name and the printer name.
 * Both are copies which the caller is responsible to free.
 */
char *
GetXPrintDisplayName(char **printer_return)
{
    char *display_name, *pdpy_name;
    char *ptr, *printer_name;

    display_name = getenv("XPRINTER");

    if (display_name != NULL) {
	/* if name is of the form "xprint:display" change it to "display"
	 * Note: this is quite unlikely to happen for now but it might in the
	 * future... I guess this is what some call forward compatibility ?-)
	 */
	if (strncmp(display_name, "xprint:", 7) == 0)
	    pdpy_name = display_name + 7;
	else
	    pdpy_name = display_name;

	ptr = strchr(pdpy_name, '@');
	if (ptr != NULL) {
	    /* server specified, store printer and remove it from
	     * display name */
	    printer_name = (char *)Malloc(ptr - pdpy_name + 1);
	    if (printer_name != NULL) {
		strncpy(printer_name, pdpy_name, ptr - pdpy_name);
		printer_name[ptr - pdpy_name] = '\0';
	    }
	    ptr++;
	    pdpy_name = (char *)Malloc(strlen(ptr) + 1);
	    if (pdpy_name != NULL)
		strcpy(pdpy_name, ptr);
	} else {
	    printer_name = (char *)Malloc(strlen(pdpy_name) + 1);
	    if (printer_name != NULL)
		strcpy(printer_name, pdpy_name);
	    pdpy_name = NULL;
	}
    } else {
	pdpy_name = NULL;
	/* look for printer name in other variables */
	printer_name = getenv("PDPRINTER");
	if (printer_name == NULL) {
	    printer_name = getenv("LPDEST");
	    if (printer_name == NULL)
		printer_name = getenv("PRINTER");
	}
	/* make a copy */
	if (printer_name != NULL) {
	    ptr = printer_name;
	    printer_name = (char *)Malloc(strlen(ptr) + 1);
	    if (printer_name != NULL)
		strcpy(printer_name, ptr);
	}
    }

    /* if no server was specified in XPRINTER, look at XPSERVERLIST */
    if (pdpy_name == NULL) {
	char *servers_list = getenv("XPSERVERLIST");
	if (servers_list != NULL && servers_list[0] != '\0') {
	    /* use the first one from the server list */
	    ptr = strchr(servers_list, ' ');
	    if (ptr != NULL) {
		pdpy_name = (char *) Malloc(ptr - servers_list + 1);
		if (pdpy_name != NULL) {
		    strncpy(pdpy_name, servers_list, ptr - servers_list);
		    pdpy_name[ptr - servers_list] = '\0';
		}
	    } else {
		pdpy_name = (char *) Malloc(strlen(servers_list) + 1);
		if (pdpy_name != NULL)
		    strcpy(pdpy_name, servers_list);
	    }
	}
    }
    *printer_return = printer_name;
    return pdpy_name;
}

#define MAXLEN 256

char *
GetXFwpDisplayName(char *dpy_name)
{
#if 0
    char *fwp_dpy_name, *ptr, *limit;
    FILE *in;
    int c, status;
#else
    char *fwp_dpy_name;
#endif
    char buf[MAXLEN];
    char *proxy_mngr;

    /* first, let's figure out where the proxy manager should be */
    proxy_mngr = getenv("PROXY_MANAGER");
    if (proxy_mngr == NULL)
	proxy_mngr = DEFAULT_PROXY_MANAGER;

#if 0
    /* now let's see if we can find a firewall proxy */
    sprintf(buf,
	    "xfindproxy -manager %s -server %s -name xfwp 2>/dev/null",
	    proxy_mngr, dpy_name);
    in = popen(buf, "r");
    if (in != NULL) {
	ptr = buf;
	limit = buf + MAXLEN - 1;
	/* read in first line */
	while ((c = fgetc(in)) != EOF && c != '\n' && ptr < limit)
	    *ptr++ = c;
	status = pclose(in);

	(void) fprintf (stderr, "buf: %s\n", buf);
	(void) fprintf (stderr, "status: %d\n", status);
	(void) fprintf (stderr, "ptr: %p, buf: %p\n", ptr, buf);
	if (status == -1 || ptr == buf) {
	    perror ("too bad");
	    /* xfindproxy failed, consider we do not have a firewall */
	    fwp_dpy_name = NULL;
	} else {
	    /* make sure we have a NULL terminated string */
	    *ptr = '\0';
	    fwp_dpy_name = (char *) Malloc(ptr - buf + 1);
	    if (fwp_dpy_name != NULL)
		strcpy(fwp_dpy_name, buf);
	}
    } else {
	/* failed to run xfindproxy, consider that we don't have a firewall */
	fwp_dpy_name = NULL;
    }
#else
    fwp_dpy_name = NULL;
    if (findproxy (buf, proxy_mngr, dpy_name, "xfwp")) {
	fwp_dpy_name = (char *) Malloc (strlen (buf) + 1);
	if (fwp_dpy_name != NULL)
	    strcpy (fwp_dpy_name, buf);
    }
#endif

    return fwp_dpy_name;
}
