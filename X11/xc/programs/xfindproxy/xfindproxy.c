/* $Xorg: xfindproxy.c,v 1.4 2001/02/09 02:05:42 xorgcvs Exp $ */

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
/* $XFree86: xc/programs/xfindproxy/xfindproxy.c,v 1.8 2001/12/14 20:01:32 dawes Exp $ */


#include <stdio.h>
#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <X11/Xmd.h>
#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/ICE/ICElib.h>
#include <X11/ICE/ICEmsg.h>
#include <X11/ICE/ICEproto.h>
#include <X11/PM/PM.h>
#include <X11/PM/PMproto.h>
#include "xfindproxy.h"

#include <stdlib.h>
#include <ctype.h>

static void PMprocessMessages(IceConn iceConn, IcePointer clientData, 
			      int opcode, unsigned long length, Bool swap, 
			      IceReplyWaitInfo *replyWait, 
			      Bool *replyReadyRet);
static void _XtProcessIceMsgProc(XtPointer client_data, int *source, 
				 XtInputId *id);
static void _XtIceWatchProc(IceConn ice_conn, IcePointer client_data, 
			    Bool opening, IcePointer *watch_data);
static Status InitWatchProcs(XtAppContext appContext);

int PMopcode;

int PMversionCount = 1;
IcePoVersionRec	PMversions[] =
                {{PM_MAJOR_VERSION, PM_MINOR_VERSION, PMprocessMessages}};

XtAppContext	appContext;

typedef struct {
    int		status;
    char	*addr;
    char	*error;
} GetProxyAddrReply;


static int 
cvthexkey(char *hexstr, char **ptrp)	/* turn hex key string into octets */
{
    int i;
    int len = 0;
    char *retval, *s;
    unsigned char *us;
    char c;
    char savec = '\0';

    /* count */
    for (s = hexstr; *s; s++) {
	if (!isascii(*s)) return -1;
	if (isspace(*s)) continue;
	if (!isxdigit(*s)) return -1;
	len++;
    }

    /* if odd then there was an error */
    if ((len & 1) == 1) return -1;


    /* now we know that the input is good */
    len >>= 1;
    retval = (char *) malloc (len);
    if (!retval)
	return -1;

    for (us = (unsigned char *) retval, i = len; i > 0; hexstr++) {
	c = *hexstr;
	if (isspace(c)) continue;	 /* already know it is ascii */
	if (isupper(c))
	    c = tolower(c);
	if (savec) {
#define atoh(c) ((c) - (((c) >= '0' && (c) <= '9') ? '0' : ('a'-10)))
	    *us = (unsigned char)((atoh(savec) << 4) + atoh(c));
#undef atoh
	    savec = 0;		/* ready for next character */
	    us++;
	    i--;
	} else {
	    savec = c;
	}
    }
    *ptrp = retval;
    return len;
}

int
main(int argc, char *argv[])
{
    IceConn			iceConn;
    IceProtocolSetupStatus	setupstat;
    char			*vendor = NULL;
    char			*release = NULL;
    pmGetProxyAddrMsg		*pMsg;
    char 			*pData;
    int				len, i;
    IceReplyWaitInfo		replyWait;
    GetProxyAddrReply		reply;
    int				majorVersion, minorVersion;
    Bool			gotReply, ioErrorOccured;
    char			errorString[255];
    char			*serviceName = NULL, *serverAddress = NULL;
    char			*hostAddress = NULL, *startOptions = NULL;
    char			*managerAddress = NULL;
    Bool			haveAuth = 0;
    char			authName[40];
    char			authData[128];
    char			*authDataBinary = NULL;
    int				authLen = 0;

    for (i = 1; i < argc; i++)
    {
	if (argv[i][0] == '-')
	{
	    switch (argv[i][1])
	    {
	    case 'a':					/* -auth */
		haveAuth = 1;
		continue;

	    case 'm':					/* -manager */
		if (++i >= argc) goto usage;
		managerAddress = (char *) XtNewString (argv[i]);
		continue;

	    case 's':					/* -server */
		if (++i >= argc) goto usage;
		serverAddress = (char *) XtNewString (argv[i]);
		continue;

	    case 'n':					/* -name */
		if (++i >= argc) goto usage;
		serviceName = XtNewString (argv[i]);
		continue;

	    case 'h':					/* -host */
		if (++i >= argc) goto usage;
		hostAddress = XtNewString (argv[i]);
		continue;

	    case 'o':					/* -options */
		if (++i >= argc) goto usage;
		startOptions = XtNewString (argv[i]);
		continue;
	    }
	}

    usage:
	fprintf (stderr,
	 "usage: xfindproxy -server serverAddr -name serviceName [-manager managerAddr] [-auth] [-host hostAddr] [-options opts]\n-manager can be omitted only if PROXY_MANAGER is in the environment\n");
	exit (1);
    }

    if (serviceName == NULL || serverAddress == NULL)
	goto usage;

    if (managerAddress == NULL) {
	managerAddress = getenv("PROXY_MANAGER");
	if (managerAddress == NULL) {
	    fprintf (stderr, "Error: -manager option must be specified when PROXY_MANAGER is not in the environment\n");
	    exit (1);
	}
    }

    /*
     * Register support for PROXY_MANAGEMENT.
     */

    if ((PMopcode = IceRegisterForProtocolSetup (
	PM_PROTOCOL_NAME,
	"XC", "1.0",
	PMversionCount, PMversions,
	0, /* authcount */
	NULL, /* authnames */ 
        NULL, /* authprocs */
	NULL  /* IceIOErrorProc */ )) < 0)
    {
	fprintf (stderr,
	    "Could not register PROXY_MANAGEMENT protocol with ICE");
	exit (1);
    }


    appContext = XtCreateApplicationContext ();

    InitWatchProcs (appContext);

    if ((iceConn = IceOpenConnection (
	managerAddress, NULL, 0, 0, 256, errorString)) == NULL)
    {
	fprintf (stderr,
	    "Could not open ICE connection to proxy manager: %s", errorString);
	exit (1);
    }

    setupstat = IceProtocolSetup (iceConn, PMopcode, NULL,
	False /* mustAuthenticate */,
	&majorVersion, &minorVersion,
	&vendor, &release, 256, errorString);

    if (setupstat != IceProtocolSetupSuccess)
    {
	IceCloseConnection (iceConn);
	fprintf (stderr,
	    "Could not initialize proxy management protocol: %s\n",
	    errorString);
	exit (1);
    }


    /*
     * If auth data is supplied, read it from stdin.
     */

    if (haveAuth)
    {
	fgets (authName, sizeof (authName), stdin);
	fgets (authData, sizeof (authData), stdin);

	for (i = 0; i < strlen (authName); i++)
	    if (authName[i] == '\n')
	    {
		authName[i] = '\0';
		break;
	    }
	for (i = 0; i < strlen (authData); i++)
	    if (authData[i] == '\n')
	    {
		authData[i] = '\0';
		break;
	    }

	/*
	 * Convert the hex auth data to binary.
	 */

	authLen = cvthexkey (authData, &authDataBinary);

	if (authLen == -1)
	{
	    fprintf (stderr, "Could not convert hex auth data to binary\n");
	    exit (1);
	}
    }


    /*
     * Now send the GetProxyAddr request.
     */

    len = STRING_BYTES (serviceName) +
	  STRING_BYTES (serverAddress) +
	  STRING_BYTES (hostAddress) +
	  STRING_BYTES (startOptions) +
	  (authLen > 0 ? (STRING_BYTES (authName) + authLen) : 0);

    IceGetHeaderExtra (iceConn, PMopcode, PM_GetProxyAddr,
	SIZEOF (pmGetProxyAddrMsg), WORD64COUNT (len),
	pmGetProxyAddrMsg, pMsg, pData);

    pMsg->authLen = authLen;

    STORE_STRING (pData, serviceName);
    STORE_STRING (pData, serverAddress);
    STORE_STRING (pData, hostAddress);
    STORE_STRING (pData, startOptions);
    if (authLen > 0)
    {
	STORE_STRING (pData, authName);
	memcpy (pData, authDataBinary, authLen);
    }

    IceFlush (iceConn);

    replyWait.sequence_of_request = IceLastSentSequenceNumber (iceConn);
    replyWait.major_opcode_of_request = PMopcode;
    replyWait.minor_opcode_of_request = PM_GetProxyAddr;
    replyWait.reply = (IcePointer) &reply;

    gotReply = False;
    ioErrorOccured = False;

    while (!gotReply && !ioErrorOccured)
    {
	ioErrorOccured = (IceProcessMessages (
	    iceConn, &replyWait, &gotReply) == IceProcessMessagesIOError);

	if (ioErrorOccured)
	{
	    fprintf (stderr, "IO error occured\n");
	    exit (1);
	}
	else if (gotReply)
	{
	    if (reply.status == PM_Success)
	    {
		fprintf (stdout, "%s\n", reply.addr);
		exit (0);
	    }
	    else
	    {
		fprintf (stderr, "Error from proxy manager: %s\n",
		    reply.error);
		exit (1);
	    }
	}
    }
    /*NOTREACHED*/
    exit(0);
}



static void
PMprocessMessages(IceConn iceConn, IcePointer clientData, int opcode,
		  unsigned long length, Bool swap, 
		  IceReplyWaitInfo *replyWait, Bool *replyReadyRet)
{
    if (replyWait)
	*replyReadyRet = False;

    switch (opcode)
    {
    case PM_GetProxyAddrReply:

	if (!replyWait ||
	    replyWait->minor_opcode_of_request != PM_GetProxyAddr)
	{
	    _IceReadSkip (iceConn, length << 3);

	    _IceErrorBadState (iceConn, PMopcode,
		PM_GetProxyAddrReply, IceFatalToProtocol);
	}
        else
	{
	    pmGetProxyAddrReplyMsg 	*pMsg;
	    char			*pData, *pStart;
	    GetProxyAddrReply 	*reply = 
		(GetProxyAddrReply *) (replyWait->reply);

	    CHECK_AT_LEAST_SIZE (iceConn, PMopcode, opcode,
		length, SIZEOF (pmGetProxyAddrReplyMsg), IceFatalToProtocol);

	    IceReadCompleteMessage (iceConn, SIZEOF (pmGetProxyAddrReplyMsg),
		pmGetProxyAddrReplyMsg, pMsg, pStart);

	    if (!IceValidIO (iceConn))
	    {
		IceDisposeCompleteMessage (iceConn, pStart);
		return;
	    }

	    pData = pStart;

	    SKIP_STRING (pData, swap);		/* proxy-address */
	    SKIP_STRING (pData, swap);		/* failure-reason */

	    CHECK_COMPLETE_SIZE (iceConn, PMopcode, opcode,
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
    {
	_IceErrorBadMinor (iceConn, PMopcode, opcode, IceCanContinue);
	_IceReadSkip (iceConn, length << 3);
	break;
    }
    }
}


static void
_XtProcessIceMsgProc(XtPointer client_data, int *source, XtInputId *id)
{
    IceConn			ice_conn = (IceConn) client_data;
    IceProcessMessagesStatus	status;

    status = IceProcessMessages (ice_conn, NULL, NULL);

    if (status == IceProcessMessagesIOError)
    {
	fprintf (stderr, "IO error occured\n");
	exit (1);
    }
}


static void
_XtIceWatchProc(IceConn ice_conn, IcePointer client_data, 
		Bool opening, IcePointer *watch_data)
{
    if (opening)
    {
	XtAppContext appContext = (XtAppContext) client_data;

	*watch_data = (IcePointer) XtAppAddInput (
	    appContext,
	    IceConnectionNumber (ice_conn),
            (XtPointer) XtInputReadMask,
	    _XtProcessIceMsgProc,
	    (XtPointer) ice_conn);
    }
    else
    {
	XtRemoveInput ((XtInputId) *watch_data);
    }
}


static Status
InitWatchProcs(XtAppContext appContext)
{
    return (IceAddConnectionWatch (_XtIceWatchProc, (IcePointer) appContext));
}
