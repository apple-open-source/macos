/* $Xorg: pm.c,v 1.5 2001/02/09 02:05:31 xorgcvs Exp $ */

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
/* $XFree86: xc/programs/lbxproxy/di/pm.c,v 1.9 2002/09/16 18:06:20 eich Exp $ */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/ICE/ICElib.h>
#include <X11/ICE/ICEmsg.h>
#include <X11/ICE/ICEproto.h>
#include <X11/PM/PM.h>
#include <X11/PM/PMproto.h>

/* 
 * Because ICElib.h had a #define for Bool and because misc.h 
 * has a typedef for Bool, _BOOL_ALREADY_DEFINED_ is defined so 
 * that misc.h does not cause a conflict for Bool.
 */
#define _BOOL_ALREADY_DEFINED_
#include "wire.h"
#include "pmP.h"
#include "pm.h"

extern char *display_name;

/*
 * Local constants
 */
#define ERROR_STRING_SIZE 256

/*
 * Static definitions
 */
static void PMprocessMessages ();
static Status _ConnectToProxyManager ();

static int PMopcode;
static int PMversionCount = 1;
static IcePoVersionRec	PMversions[] =
	{{PM_MAJOR_VERSION, PM_MINOR_VERSION, PMprocessMessages}};
static int gotFirstGetProxyAddr = 0;

/*
 * Public variables
 */
IceConn		PM_iceConn;
int 		proxy_manager_fd = -1;
Bool		proxyMngr;

/*
 * The following comment and ICE I/O error handler code were taken
 * from the X Session Manager.  What's good enough for the XSM
 * is good enough for the LBX proxy ...
 *
 *     The real way to handle IO errors is to check the return status
 *     of IceProcessMessages.  xsm properly does this.
 *    
 *     Unfortunately, a design flaw exists in the ICE library in which
 *     a default IO error handler is invoked if no IO error handler is
 *     installed.  This default handler exits.  We must avoid this.
 *    
 *     To get around this problem, we install an IO error handler that
 *     does a little magic.  Since a previous IO handler might have been
 *     installed, when we install our IO error handler, we do a little
 *     trick to get both the previous IO error handler and the default
 *     IO error handler.  When our IO error handler is called, if the
 *     previous handler is not the default handler, we call it.  This
 *     way, everyone's IO error handler gets called except the stupid
 *     default one which does an exit!
 *
 */
static IceIOErrorHandler prev_handler;

static void
MyIoErrorHandler (ice_conn)
    IceConn ice_conn;
{
    if (prev_handler)
        (*prev_handler) (ice_conn);
    fprintf (stderr, "Received an ICE I/O error from the Proxy Manager\n");
 
    proxy_manager_fd = -1;
}

static void
InstallIOErrorHandler ()
{
    IceIOErrorHandler default_handler;

    prev_handler = IceSetIOErrorHandler (NULL);
    default_handler = IceSetIOErrorHandler (MyIoErrorHandler);
    if (prev_handler == default_handler)
        prev_handler = NULL;
}


Bool
CheckForProxyManager ()
{
    if (getenv ("PROXY_MANAGER"))
	return 1;
    else
	return 0;
}

void
ConnectToProxyManager ()
{
    char    *proxyManagerAddr;
    char    errorString[ERROR_STRING_SIZE];

    proxyManagerAddr = (char *) getenv ("PROXY_MANAGER");

    if (proxyManagerAddr)
    {
	InstallIOErrorHandler ();

	if (!_ConnectToProxyManager (proxyManagerAddr, errorString))
	{
	    fprintf (stderr, "%s\n", errorString);
	    exit (1);
	}
    }
}


static Status
_ConnectToProxyManager (pmAddr, errorString)
    char *pmAddr;
    char *errorString;
{
    IceProtocolSetupStatus	setupstat;
    char			*vendor = NULL;
    char			*release = NULL;
    pmStartProxyMsg		*pMsg;
    char 			*pData;
    int				len;
    int				majorVersion, minorVersion;
    char			iceError[ERROR_STRING_SIZE];

    /*
     * Register support for PROXY_MANAGEMENT.
     */
    if ((PMopcode = IceRegisterForProtocolSetup (
	PM_PROTOCOL_NAME,
	"The Open Group, Inc.", "1.0",
	PMversionCount, PMversions,
	0, /* authcount */
	NULL, /* authnames */ 
        NULL, /* authprocs */
	NULL  /* IceIOErrorProc */ )) < 0)
    {
	strcpy (errorString,
	    "Could not register PROXY_MANAGEMENT protocol with ICE");
	return 0;
    }


    if ((PM_iceConn = IceOpenConnection (
	pmAddr,	NULL, 0, 0, sizeof(iceError), iceError)) == NULL)
    {
	snprintf (errorString, sizeof(errorString),
	    "Could not open ICE connection to proxy manager: %s", iceError);
	return 0;
    }

    setupstat = IceProtocolSetup (PM_iceConn, PMopcode, NULL,
	False /* mustAuthenticate */,
	&majorVersion, &minorVersion,
	&vendor, &release, ERROR_STRING_SIZE, errorString);

    if (setupstat != IceProtocolSetupSuccess)
    {
	IceCloseConnection (PM_iceConn);
	snprintf (errorString,sizeof(errorString),
	    "Could not initialize proxy management protocol: %s",
	    iceError);
	return 0;
    }


    /*
     * Now send the StartProxy message.
     */

    len = STRING_BYTES ("LBX");

    IceGetHeaderExtra (PM_iceConn, PMopcode, PM_StartProxy,
	SIZEOF (pmStartProxyMsg), WORD64COUNT (len),
	pmStartProxyMsg, pMsg, pData);

    STORE_STRING (pData, "LBX");

    IceFlush (PM_iceConn);

    while (!gotFirstGetProxyAddr)
    {
	/* wait for a GetProxyAddr request before continuing... */
	int status = IceProcessMessages( PM_iceConn, NULL, NULL );
	if (status == IceProcessMessagesIOError)
	{
	    fprintf( stderr, "IO error occured connecting to proxy manager");
	    return FALSE;
	}
    }

    proxy_manager_fd = IceConnectionNumber (PM_iceConn);

    ListenToProxyManager();

    return 1;
}


void
SendGetProxyAddrReply (
    IceConn requestor_iceConn,
    int status,
    char *addr,
    char *error)

{
    int len = STRING_BYTES (addr) + STRING_BYTES (error);
    pmGetProxyAddrReplyMsg *pReply;
    char *pData;

    IceGetHeaderExtra (requestor_iceConn,
	PMopcode, PM_GetProxyAddrReply,
	SIZEOF (pmGetProxyAddrReplyMsg), WORD64COUNT (len),
	pmGetProxyAddrReplyMsg, pReply, pData);

    pReply->status = status;

    STORE_STRING (pData, addr);
    STORE_STRING (pData, error);

    IceFlush (requestor_iceConn);
}


static int
casecmp (str1, str2)
    char *str1, *str2;
{
    char buf1[512],buf2[512];
    char c, *s;
    register int n;

    for (n=0, s = buf1; (c = *str1++); n++) {
	if (isupper(c))
	    c = tolower(c);
	if (n>510)
	    break;
	*s++ = c;
    }
    *s = '\0';
    for (n=0, s = buf2; (c = *str2++); n++) {
	if (isupper(c))
	    c = tolower(c);
	if (n>510)
	    break;
	*s++ = c;
    }
    *s = '\0';
    return (strcmp(buf1, buf2));
}


/* ARGSUSED */
static void
PMprocessMessages (iceConn, clientData, opcode, length, 
		   swap, replyWait, replyReadyRet)
    IceConn		 iceConn;
    IcePointer       clientData;
    int		 opcode;
    unsigned long	 length;
    Bool		 swap;
    IceReplyWaitInfo *replyWait;
    Bool		 *replyReadyRet;
{
    switch (opcode)
    {
    case PM_GetProxyAddr:
    {
	pmGetProxyAddrMsg *pMsg;
	char *pData, *pStart;
	char *serviceName = NULL, *serverAddress = NULL;
	char *hostAddress = NULL, *startOptions = NULL;
	char *authName = NULL, *authData = NULL;
	int authLen;
	char * colon;
	char * tmpAddress = NULL;
	
	CHECK_AT_LEAST_SIZE (iceConn, PMopcode, opcode,
	    length, SIZEOF (pmGetProxyAddrMsg), IceFatalToProtocol);

	IceReadCompleteMessage (iceConn, SIZEOF (pmGetProxyAddrMsg),
	    pmGetProxyAddrMsg, pMsg, pStart);

	if (!IceValidIO (iceConn))
	{
	    IceDisposeCompleteMessage (iceConn, pStart);
	    return;
	}

	authLen = swap ? lswaps (pMsg->authLen) : pMsg->authLen;

	pData = pStart;

	SKIP_STRING (pData, swap);	/* proxy-service */
	SKIP_STRING (pData, swap);	/* server-address */
	SKIP_STRING (pData, swap);	/* host-address */
	SKIP_STRING (pData, swap);	/* start-options */
	if (authLen > 0)
	{
	    SKIP_STRING (pData, swap);		    /* auth-name */
	    pData += (authLen +  PAD64 (authLen));  /* auth-data */
	}

	CHECK_COMPLETE_SIZE (iceConn, PMopcode, opcode,
	    length, pData - pStart + SIZEOF (pmGetProxyAddrMsg),
	    pStart, IceFatalToProtocol);

	pData = pStart;

	EXTRACT_STRING (pData, swap, serviceName);
	EXTRACT_STRING (pData, swap, serverAddress);
	EXTRACT_STRING (pData, swap, hostAddress);
	EXTRACT_STRING (pData, swap, startOptions);
	if (authLen > 0)
	{
	    EXTRACT_STRING (pData, swap, authName);
	    authData = (char *) malloc (authLen);
	    memcpy (authData, pData, authLen);
	}

	/*
	 * Convert the display name (serverAddress) into a FQDN
	 * to consolidate servers.  So that for example, requests
	 * for displays foo:0, foo.bar:0 and foo.bar.com:0 will
	 * be set to the same server.
	 *
	 * If gethostbyname fails, try to connect anyhow because
	 * the display name could be something like :0, local:0
	 * or unix:0.
	 */
	colon = strchr (serverAddress, ':');
	if (colon)
	{
	    struct hostent *hostent;

	    *colon = '\0';
	    hostent = gethostbyname (serverAddress);
	    *colon = ':';

	    if (hostent && hostent->h_name) {
		tmpAddress = (char *) malloc (strlen (hostent->h_name) + 
					      strlen (colon) + 1);
		(void) sprintf (tmpAddress, "%s%s", hostent->h_name, colon);
	        serverAddress = tmpAddress;
	    }
	}
	display_name = serverAddress;

	if (casecmp (serviceName, "LBX") != 0)
	{
	    SendGetProxyAddrReply (iceConn, PM_Unable,
		NULL, "Incorrect proxy service, should be LBX");
	}
	else
	{
	    if (!gotFirstGetProxyAddr)
	    {
		gotFirstGetProxyAddr = 1;
		if (authLen > 0)
		    XSetAuthorization (authName, strlen (authName),
		        authData, authLen);
		/*
		 * Connect to this server and send a GetProxyAddrReply msg.
		 */
		if (!ConnectToServer (display_name)) {
		    FatalError ("could not connect to '%s'", 
				display_name);
		}
	    }
	    else
	    {
                /*
		 * First check to see if a server for this serverAddress
		 * already exists.
		 */
		int		i;
		int		found = 0;

		for (i=0; i < lbxMaxServers; i++)
		{
		    if (servers[i] && servers[i]->display_name &&
		       casecmp (serverAddress, servers[i]->display_name) == 0)
		    {
		        SendGetProxyAddrReply (iceConn, 
					       PM_Success, 
					       servers[i]->proxy_name, 
					       NULL);
			found = 1;
			break;
		    }
		}
		if (!found)
		{
		    /*
		     * Go ahead and try to connect to the new server.  If
		     * there is an error, the connection code will send
		     * a GetProxyAddrReply message.
		     */
		    if (authLen > 0)
			XSetAuthorization (authName, strlen (authName),
			    authData, authLen);

		    (void) ConnectToServer (display_name);
		}
	    }
	}

	IceDisposeCompleteMessage (iceConn, pStart);

	if (serviceName)
	    free (serviceName);
	if (hostAddress)
	    free (hostAddress);
	if (startOptions)
	    free (startOptions);
	if (authName)
	    free (authName);
	if (authData)
	    free (authData);
	if (tmpAddress)
	    free (tmpAddress);

	break;
    }

    default:
    {
	_IceErrorBadMinor (iceConn, PMopcode, opcode, IceCanContinue);
	_IceReadSkip (iceConn, length << 3);
	break;
    }
    }
}

void
HandleProxyManagerConnection ()
{
    /*
     * If an IO error occurs, the IO error handler will output
     * an error message.
     */
    (void) IceProcessMessages (PM_iceConn, NULL, NULL);
}
