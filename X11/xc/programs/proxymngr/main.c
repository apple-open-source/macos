/* $Xorg: main.c,v 1.6 2001/02/09 02:05:34 xorgcvs Exp $ */

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
/* $XFree86: xc/programs/proxymngr/main.c,v 1.7 2001/12/19 21:37:35 dawes Exp $ */

#include <stdlib.h>
#include "pmint.h"
#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/ICE/ICEmsg.h>
#include <X11/ICE/ICEproto.h>
#include <X11/PM/PMproto.h>
#include <X11/PM/PM.h>
#include "pmdb.h"
#include "config.h"
#include <assert.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

static int PMprotocolSetupProc ( IceConn iceConn, int majorVersion, 
				 int minorVersion, char *vendor, 
				 char *release, IcePointer *clientDataRet, 
				 char **failureReasonRet );
static void SendGetProxyAddr ( PMconn *pmConn, char *serviceName, 
			       char *serverAddress, char *hostAddress, 
			       char *startOptions, int authLen, 
			       char *authName, char *authData );

static int PMAcceptorOpcode;
static int PMOriginatorOpcode;

int PMversionCount = 1;
IcePaVersionRec	PMReplyVersions[] = {{PM_MAJOR_VERSION, PM_MINOR_VERSION,
				      PMReplyProcessMessages}};
IcePoVersionRec	PMSetupVersions[] = {{PM_MAJOR_VERSION, PM_MINOR_VERSION,
				      PMSetupProcessMessages}};

char *PM_VENDOR_STRING = "The X.Org Group";
char *PM_VENDOR_RELEASE = "Release 6.6";

int verbose = 0;

XtAppContext	appContext;

#define PM_PORT "6500"

char *configFile = NULL;

void
Usage ()
{
    fprintf (stderr, "Usage: proxymngr [-config file] [-verbose]\n");
    exit (1);
}

void 
SetCloseOnExec (fd)
    int	fd;
{
    int ret;
#ifdef F_SETFD
#ifdef FD_CLOEXEC
    ret = fcntl (fd, F_SETFD, FD_CLOEXEC);
#else
    ret = fcntl (fd, F_SETFD, 1);
#endif /* FD_CLOEXEC */
#endif /* F_SETFD */
}

/*
 * Main program
 */

int
main (int argc, char *argv[])
{
    IceListenObj *listenObjs;
    int		numTransports, i;
    char	errormsg[256];
    char	*networkIds, *p;

    for (i = 1; i < argc; i++)
    {
	if (strcmp (argv[i], "-config") == 0)
	{
	    if (++i < argc)
	        configFile = argv[i];
	    else
		Usage ();
	}
	else if (strcmp(argv[i], "-verbose") == 0)
	{
	    verbose = 1;
	}
	else
	    Usage ();
    }

    if (!configFile)
	configFile = CONFIG_FILE;

    if (verbose)
	fprintf (stderr, "config file = %s\n", configFile);

    /*
     * Install an IO error handler.
     */
    InstallIOErrorHandler ();

    /*
     * Register support for PROXY_MANAGEMENT.
     */

    /* For Managed proxies, the proxy does the Setup */
    if ((PMAcceptorOpcode = IceRegisterForProtocolReply (
	PM_PROTOCOL_NAME, PM_VENDOR_STRING, PM_VENDOR_RELEASE,
	PMversionCount, PMReplyVersions,
	0, /* authcount */
	NULL, /* authnames */ 
        NULL, /* authprocs */
        HostBasedAuthProc,
	PMprotocolSetupProc,
	NULL, /* protocolActivateProc */
	NULL  /* IceIOErrorProc */ )) < 0)
    {
	fprintf (stderr,
	    "Could not register PROXY_MANAGEMENT protocol reply with ICE");
	exit (1);
    }

    /* For Unmanaged proxies, we do the Setup
     * ICElib doesn't specify that the same opCode will be returned
     * so don't bet on it.
     */
    if ((PMOriginatorOpcode = IceRegisterForProtocolSetup (
	PM_PROTOCOL_NAME, PM_VENDOR_STRING, PM_VENDOR_RELEASE,
	PMversionCount, PMSetupVersions,
	0, /* authcount */
	NULL, /* authnames */ 
        NULL, /* authprocs */
	NULL  /* IceIOErrorProc */ )) < 0)
    {
	fprintf (stderr,
	    "Could not register PROXY_MANAGEMENT protocol setup with ICE");
	exit (1);
    }


    if (!IceListenForWellKnownConnections (
	PM_PORT, &numTransports, &listenObjs, 256, errormsg))
    {
	fprintf (stderr, "%s\n", errormsg);
	exit (1);
    }

    networkIds = IceComposeNetworkIdList (numTransports, listenObjs);
    p = (char *) malloc(sizeof ("PROXY_MANAGER") + strlen(networkIds) + 2);
    sprintf (p, "PROXY_MANAGER=%s", networkIds);
    putenv (p);
    printf ("%s\n", p);
    free (networkIds);

    appContext = XtCreateApplicationContext ();

    InitWatchProcs (appContext);

    for (i = 0; i < numTransports; i++)
    {
	XtAppAddInput (appContext,
	    IceGetListenConnectionNumber (listenObjs[i]),
	    (XtPointer) XtInputReadMask,
	    NewConnectionXtProc, (XtPointer) listenObjs[i]);

	IceSetHostBasedAuthProc (listenObjs[i], HostBasedAuthProc);

        SetCloseOnExec (IceGetListenConnectionNumber (listenObjs[i]));
    }

    /*
     * Main loop
     */
    XtAppMainLoop (appContext);
    exit (0);
}


/*
 * Xt callback invoked when a client attempts to connect.
 */

/* ARGSUSED */
void
NewConnectionXtProc (client_data, source, id)

XtPointer	client_data;
int 		*source;
XtInputId	*id;

{
    IceConn 	ice_conn;
    char	*connstr;
    IceAcceptStatus status;

    ice_conn = IceAcceptConnection((IceListenObj) client_data, &status);
    if (! ice_conn) {
	if (verbose)
	    printf ("IceAcceptConnection failed\n");
    } else {
	IceConnectStatus cstatus;

        /*
	 * Mark this fd to be closed upon exec
	 */
        SetCloseOnExec (IceConnectionNumber (ice_conn));

	while ((cstatus = IceConnectionStatus (ice_conn))==IceConnectPending) {
	    XtAppProcessEvent (appContext, XtIMAll);
	}

	if (cstatus == IceConnectAccepted) {
	    if (verbose) {
		printf ("ICE Connection opened by client, IceConn fd = %d, ",
			IceConnectionNumber (ice_conn));
		connstr = IceConnectionString (ice_conn);
		printf ("Accept at networkId %s\n", connstr);
		free (connstr);
		printf ("\n");
	    }
	} else {
	    if (verbose)
	    {
		if (cstatus == IceConnectIOError)
		    printf ("IO error opening ICE Connection!\n");
		else
		    printf ("ICE Connection rejected!\n");
	    }

	    IceCloseConnection (ice_conn);
	}
    }
}


/*
 * See ConnectToProxy() if you change any of the pmConn structure
 */
static Status
PMprotocolSetupProc (iceConn,
    majorVersion, minorVersion, vendor, release,
    clientDataRet, failureReasonRet)

IceConn    iceConn;
int	   majorVersion;
int	   minorVersion;
char  	   *vendor;
char 	   *release;
IcePointer *clientDataRet;
char	   **failureReasonRet;

{
    /*
     * Allocate new pmConn.
     */

    static char standardError[] = "Could not allocate memory for new client";
    PMconn *pmConn;

    if ((pmConn = (PMconn *) malloc (sizeof (PMconn))) == NULL)
    {
	if (verbose)
	    fprintf (stderr, "%s\n", standardError);

	*failureReasonRet = standardError;
	return (0);
    }

    pmConn->iceConn = iceConn;
    pmConn->pmOpcode = PMAcceptorOpcode;
    pmConn->proto_major_version = majorVersion;
    pmConn->proto_minor_version = minorVersion;
    pmConn->vendor = vendor;
    pmConn->release = release;

    *clientDataRet = (IcePointer) pmConn;

    return (1);
}


static void
SendGetProxyAddr (
    PMconn *pmConn,
    char *serviceName,
    char *serverAddress,
    char *hostAddress,
    char *startOptions,
    int authLen,
    char *authName,
    char *authData)

{
    IceConn iceConn = pmConn->iceConn;
    pmGetProxyAddrMsg *pMsg;
    char *pData;
    int len;

    if (verbose) {
	printf ("Sending GetProxyAddr to proxy %d, serviceName = %s, serverAddr = %s\n",
		IceConnectionNumber(iceConn), serviceName, serverAddress);
	printf ("  hostAddr = %s, options = %s, authLen = %d\n",
		hostAddress ? hostAddress : "",
		startOptions ? startOptions : "",
		authLen);
	if (authLen > 0)
	    printf ("  authName = %s\n", authName);
    }

    len = STRING_BYTES (serviceName) +
	  STRING_BYTES (serverAddress) +
	  STRING_BYTES (hostAddress) +
	  STRING_BYTES (startOptions) +
	  (authLen > 0 ? (STRING_BYTES (authName) + authLen) : 0);

    IceGetHeaderExtra (iceConn, pmConn->pmOpcode, PM_GetProxyAddr,
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
	memcpy (pData, authData, authLen);
    }
	
    IceFlush (iceConn);
}


void
SendGetProxyAddrReply (
    PMconn *requestor,
    int status,
    char *addr,
    char *error)

{
    int len = STRING_BYTES (addr) + STRING_BYTES (error);
    pmGetProxyAddrReplyMsg *pReply;
    char *pData;

    if (verbose) {
	fputs ("Replying with ", stderr);
	fputs (status == PM_Success ? "Success: " :
	       status == PM_Failure ? "Failure: " :
	       status == PM_Unable  ? "Unable: "  :
	       "?unknown status", stderr);
	fputs (status == PM_Success ? addr : error, stderr);
	fputc ('\n', stderr);
    }

    IceGetHeaderExtra (requestor->iceConn,
	requestor->pmOpcode, PM_GetProxyAddrReply,
	SIZEOF (pmGetProxyAddrReplyMsg), WORD64COUNT (len),
	pmGetProxyAddrReplyMsg, pReply, pData);

    pReply->status = status;

    STORE_STRING (pData, addr);
    STORE_STRING (pData, error);

    IceFlush (requestor->iceConn);
}



void
PMReplyProcessMessages (iceConn, clientData, opcode, length, swap)

IceConn		 iceConn;
IcePointer       clientData;
int		 opcode;
unsigned long	 length;
Bool		 swap;

{
    PMconn *pmConn = (PMconn *) clientData;

    assert(pmConn->iceConn == iceConn);

    switch (opcode)
    {
    case PM_GetProxyAddr:
    {
	pmGetProxyAddrMsg 	*pMsg;
	char 			*pData, *pStart;
	char			*serviceName = NULL, *serverAddress = NULL;
	char			*hostAddress = NULL, *startOptions = NULL;
	char			*authName = NULL, *authData = NULL;
	int			authLen;

	CHECK_AT_LEAST_SIZE (iceConn, pmConn->pmOpcode, opcode,
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

	CHECK_COMPLETE_SIZE (iceConn, pmConn->pmOpcode, opcode,
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

	if (serverAddress)
	{  
	    /*
	     * Assume that if serverAddress is something like :0 or :0.0
	     * then the request is for a server on the client's host.
	     *
	     * However, the proxy handling this request may be on a 
	     * different host than the client or the client host, 
	     * proxy host and the server host may all be different, 
	     * thus a serverAddress of :0 or :0.0 is not useful.  
	     * Therefore, change serverAddrees to use the client's 
	     * hostname.
	     */
	    char		*tmpName;

	    tmpName = strchr (serverAddress, ':');

	    if (tmpName && ((serverAddress[0] == ':') || 
			    (!strncmp (serverAddress, "unix:", 5))))
	    {
		struct sockaddr_in	serverSock;
		int 			retVal;
		int 			addrLen = sizeof(serverSock);

		retVal = getpeername(IceConnectionNumber(iceConn),
				     (struct sockaddr *) &serverSock,
				     (void *) &addrLen);
		if (!retVal) 
		{
		    struct hostent *hostent;

		    hostent = gethostbyname (inet_ntoa(serverSock.sin_addr));

		    if (hostent && hostent->h_name) 
		    {
			int		len;
			char		* pch = strdup (tmpName);

			len = strlen(hostent->h_name) + strlen(tmpName) + 1;
			serverAddress = (char *) realloc (serverAddress, len);
			sprintf (serverAddress, "%s%s", hostent->h_name, pch);
			free (pch);
		    }
		}
	    }
	}

	if (verbose) {
	    printf ("Got GetProxyAddr, serviceName = %s, serverAddr = %s\n",
		    serviceName, serverAddress);
	    printf ("  hostAddr = %s, options = %s, authLen = %d\n",
		    hostAddress, startOptions, authLen);
	    if (authLen > 0)
		printf ("  authName = %s\n", authName);
	}

	IceDisposeCompleteMessage (iceConn, pStart);

	ForwardRequest (pmConn, serviceName, serverAddress, hostAddress,
			startOptions, authLen, authName, authData);

	if (serviceName)
	    free (serviceName);
	if (serverAddress)
	    free (serverAddress);
	if (hostAddress)
	    free (hostAddress);
	if (startOptions)
	    free (startOptions);
	if (authName)
	    free (authName);
	if (authData)
	    free (authData);

	break;
    }

    case PM_StartProxy:
    {
	pmStartProxyMsg *pMsg;
	char 		*pData, *pStart;
	char		*serviceName = NULL;
	char		*serverAddress;
	char		*hostAddress;
	char		*startOptions;
	int		authLen;
	char		*authName;
	char		*authData;

	CHECK_AT_LEAST_SIZE (iceConn, pmConn->pmOpcode, opcode,
	    length, SIZEOF (pmStartProxyMsg), IceFatalToProtocol);

	IceReadCompleteMessage (iceConn, SIZEOF (pmStartProxyMsg),
	    pmStartProxyMsg, pMsg, pStart);

	if (!IceValidIO (iceConn))
	{
	    IceDisposeCompleteMessage (iceConn, pStart);
	    return;
	}

	pData = pStart;

	SKIP_STRING (pData, swap);	/* proxy-service */

	CHECK_COMPLETE_SIZE (iceConn, pmConn->pmOpcode, opcode,
	   length, pData - pStart + SIZEOF (pmStartProxyMsg),
	   pStart, IceFatalToProtocol);

	pData = pStart;

	EXTRACT_STRING (pData, swap, serviceName);

	assert(serviceName);

	if (verbose)
	    printf ("Got StartProxy on fd %d, serviceName = %s\n",
		    IceConnectionNumber(iceConn), serviceName);

	IceDisposeCompleteMessage (iceConn, pStart);

	if (! ActivateProxyService (serviceName, pmConn)) {
	    fputs ("Configuration error: received unexpected StartProxy for service ", stderr);
	    fputs (serviceName, stderr);
	    fputc ('\n', stderr);
	    IceCloseConnection (iceConn);
	}
	else {

	    /*
	     * Now send the GetProxyAddr message to the proxy.
	     */
	    if (PeekRequestorQueue(pmConn,
				   NULL, NULL, NULL,
				   &serverAddress, &hostAddress, &startOptions,
				   &authLen, &authName, &authData)) {
		SendGetProxyAddr(pmConn,
				 serviceName, serverAddress,
				 hostAddress, startOptions,
				 authLen, authName, authData);
	    }
	    else if (verbose) {
		    fputs ("Received StartProxy for service ", stderr);
		    fputs (serviceName, stderr);
		    fputs (" but no waiting GetproxyAddr requests\n", stderr);
	    }
	}

	free (serviceName);

	break;
    }

    case PM_GetProxyAddrReply:

    {
	pmGetProxyAddrReplyMsg 	*pMsg;
	char			*pData, *pStart;
	char			*addr = NULL, *error = NULL;

	CHECK_AT_LEAST_SIZE (iceConn, pmConn->pmOpcode, opcode,
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

	CHECK_COMPLETE_SIZE (iceConn, pmConn->pmOpcode, opcode,
	    length, pData - pStart + SIZEOF (pmGetProxyAddrReplyMsg),
	    pStart, IceFatalToProtocol);

	pData = pStart;

	EXTRACT_STRING (pData, swap, addr);
	EXTRACT_STRING (pData, swap, error);

	if (verbose) {
	    printf ("Got GetProxyAddrReply from proxy %d, status = %d, ",
		    IceConnectionNumber(iceConn), pMsg->status);
	    if (pMsg->status == PM_Success)
		printf ("addr = %s\n", addr);
	    else
		printf ("error = %s\n", error);
	}

	{ /* Ignore any unsolicited replies so we don't get further confused */
	    running_proxy *proxy = ProxyForPMconn(pmConn);

	    if (!proxy || !proxy->requests)
	    {
		if (verbose)
		    fprintf (stderr, "Received unsolicited GetProxyAddrReply from proxy %d; ignoring it.\n",
			     IceConnectionNumber(iceConn));

		IceDisposeCompleteMessage (iceConn, pStart);
		break;
	    }
	}

	switch (pMsg->status) {

	case PM_Success:
	{
	    /*
	     * Now send the GetProxyAddr reply to xfindproxy.
	     */

	    SendGetProxyAddrReply (
		PopRequestorQueue (pmConn, True, True /* free proxy list */),
		PM_Success /* status */, addr, NULL);

	    break;
	}

	case PM_Unable:
	{
	    running_proxy_list *proxyList;
	    char *serviceName, *serverAddress, *hostAddress, *startOptions;
	    PMconn *requestor;
	    int authLen;
	    char *authName;
	    char *authData;

	    {
		running_proxy *proxy = ProxyForPMconn(pmConn);
		if (proxy)
		    proxy->refused_service = True;
		else
		    fputs("Internal error: received GetProxyAddrReply from an unknown proxy\n", stderr);
	    }

	    if (! PeekRequestorQueue (pmConn, &requestor,
				      &proxyList, &serviceName, &serverAddress,
				      &hostAddress, &startOptions,
				      &authLen, &authName, &authData)) {
		if (verbose)
		    fputs("Received GetProxyAddrReply from a proxy with no requests\n", stderr);

		proxyList = NULL;
		serviceName = "?unknown service--internal error";
	    }

	    if (proxyList && (proxyList->current < proxyList->count - 1))
	    {
		/*
		 * Ask the next running proxy if it can service this request.
		 */
		running_proxy *nextProxy;

		proxyList->current++;
		nextProxy = proxyList->list[proxyList->current];

		if (nextProxy->pmConn != NULL) {
		    /* send only if the proxy has started */
		    SendGetProxyAddr (nextProxy->pmConn, serviceName,
				      serverAddress, hostAddress, startOptions,
				      authLen, authName, authData);
		}

		PushRequestorQueue (nextProxy, requestor, proxyList,
	            serviceName, serverAddress, hostAddress, startOptions,
		    authLen, authName, authData);

		PopRequestorQueue (pmConn, False, False);
	    }
	    else
	    {
		/*
		 * Start a new proxy.
		 */

		running_proxy *runningProxy = NULL;
		char *startCommand;
		char *proxyAddress;
		Bool managed;

		if (!GetConfig (configFile, serviceName, &managed,
	            &startCommand, &proxyAddress))
		{
		    SendGetProxyAddrReply (requestor, PM_Failure,
		        NULL, "Could not read proxy manager config file");
		}
		else
		{
		    runningProxy = StartNewProxy (serviceName, startCommand);

		    if (runningProxy)
		    {
			PushRequestorQueue (runningProxy,
			    requestor, proxyList,
			    serviceName, serverAddress,
			    hostAddress, startOptions,
			    authLen, authName, authData);
		    }
		    else
		    {
			SendGetProxyAddrReply (pmConn, PM_Failure,
		            NULL, "Can't start new proxy");
		    }
		}

		if (startCommand)
		    free (startCommand);
		if (proxyAddress)
		    free (proxyAddress);

		PopRequestorQueue (pmConn, False,
		    runningProxy ? False : True /* free proxy list */);
	    }
	    break;
	}

	default:
	    if (verbose && pMsg->status != PM_Unable)
		fprintf(stderr,
			"Error: proxy returned unrecognized status: %d\n",
			pMsg->status);
	    /* FALLTHROUGH */

	case PM_Failure:
	    SendGetProxyAddrReply (
		PopRequestorQueue (pmConn, True, True /* free proxy list */),
		pMsg->status, NULL, error);
	}

	IceDisposeCompleteMessage (iceConn, pStart);

	/* see if there was more work queued for this proxy */
	{
	    char *serviceName, *serverAddress, *hostAddress, *startOptions;
	    int authLen;
	    char *authName, *authData;

	    if (PeekRequestorQueue(pmConn,
				   NULL, NULL, &serviceName,
				   &serverAddress, &hostAddress, &startOptions,
				   &authLen, &authName, &authData)) {
		SendGetProxyAddr(pmConn,
				 serviceName, serverAddress,
				 hostAddress, startOptions,
				 authLen, authName, authData);
	    }
	}

	if (addr)
	    free (addr);
	if (error)
	    free (error);

	break;
    }

    case PM_Error:
    {
	iceErrorMsg *pMsg;
	char *pStart;

	CHECK_AT_LEAST_SIZE (iceConn, pmConn->pmOpcode, PM_Error, length,
			     sizeof(iceErrorMsg), IceFatalToProtocol);

	IceReadCompleteMessage (iceConn, SIZEOF (iceErrorMsg),
				iceErrorMsg, pMsg, pStart);

	if (!IceValidIO (iceConn))
	{
	    IceDisposeCompleteMessage (iceConn, pStart);
	    return;
	}

	if (swap)
	{
	    pMsg->errorClass = lswaps (pMsg->errorClass);
	    pMsg->offendingSequenceNum = lswapl (pMsg->offendingSequenceNum);
	}

	fprintf(stderr, "Received ICE Error: class=0x%x\n  offending minor opcode=%d, severity=%d, sequence=%d\n",
		pMsg->errorClass, pMsg->offendingMinorOpcode, pMsg->severity,
		(int)pMsg->offendingSequenceNum);

	IceDisposeCompleteMessage (iceConn, pStart);

	break;
    }

    default:
    {
	_IceErrorBadMinor (iceConn, pmConn->pmOpcode, opcode, IceCanContinue);
	_IceReadSkip (iceConn, length << 3);
	break;
    }
    }
}

void
PMSetupProcessMessages (iceConn, clientData, opcode, length, swap,
			replyWait, replyReadyRet)

IceConn		 iceConn;
IcePointer       clientData;
int		 opcode;
unsigned long	 length;
Bool		 swap;
IceReplyWaitInfo *replyWait;
Bool		 *replyReadyRet;

{
    assert (replyWait == NULL);

    PMReplyProcessMessages (iceConn, clientData, opcode, length, swap);
}


void
ForwardRequest( requestor, serviceName, serverAddress, hostAddress,
		startOptions, authLen, authName, authData )
    PMconn *requestor;
    char *serviceName, *serverAddress, *hostAddress, *startOptions;
    int authLen;
    char *authName, *authData;
{
    running_proxy_list	*proxyList;
    running_proxy	*runningProxy = NULL;
    int			pushRequest = 0;

    if ((proxyList = GetRunningProxyList (
	serviceName, serverAddress)) != NULL)
    {
	while (proxyList->current < proxyList->count) {
	    runningProxy = proxyList->list[proxyList->current];

	    if (runningProxy->pmConn != NULL) {
		SendGetProxyAddr (runningProxy->pmConn, serviceName,
				  serverAddress, hostAddress, NULL,
				  authLen, authName, authData);
		break;
	    }
	    proxyList->current++;
	}

	pushRequest = 1;
    }
    else
    {
	Bool managed;
	char *startCommand;
	char *proxyAddress;

	if (!GetConfig (configFile, serviceName, &managed,
	    &startCommand, &proxyAddress))
	{
	    SendGetProxyAddrReply (requestor, PM_Failure,
		NULL, "Could not find requested service");
	}
	else
	{
	    if (managed)
	    {
		runningProxy = StartNewProxy (serviceName, startCommand);

		if (runningProxy)
		    pushRequest = 1;
		else
		{
		    SendGetProxyAddrReply (requestor, PM_Failure,
			NULL, "Can't start new proxy");
		}
	    }
	    else
	    {
		/*
		 * We have the unmanged proxy's address; now forward
		 * the request to it.
		 */

		runningProxy = ConnectToProxy (PMOriginatorOpcode,
					       serviceName, proxyAddress);

		if (runningProxy) {
		    SendGetProxyAddr (runningProxy->pmConn,
				      serviceName, serverAddress,
				      hostAddress, startOptions,
				      authLen, authName, authData);
		    pushRequest = 1;
		}
		else
		{
		    /* %%% We should reread the config file and look
		     * for another proxy address before giving up.
		     */
		    SendGetProxyAddrReply (requestor, PM_Failure,
			NULL, "Can't connect to proxy");
		}
	    }

	    if (startCommand)
		free (startCommand);
	    if (proxyAddress)
		free (proxyAddress);
	}
    }

    if (pushRequest)
    {
	PushRequestorQueue (runningProxy, requestor, proxyList,
	    serviceName, serverAddress, hostAddress, startOptions,
	    authLen, authName, authData);
    }
}


/* ARGSUSED */
void
_XtProcessIceMsgProc (client_data, source, id)

XtPointer	client_data;
int 		*source;
XtInputId	*id;

{
    IceConn			ice_conn = (IceConn) client_data;
    IceProcessMessagesStatus	status;

    status = IceProcessMessages (ice_conn, NULL, NULL);

    if (status == IceProcessMessagesIOError)
    {
	Bool activeReqs;

	ProxyGone (ice_conn, &activeReqs);
	IceSetShutdownNegotiation (ice_conn, False);
	IceCloseConnection (ice_conn);
    }
}


void
_XtIceWatchProc (ice_conn, client_data, opening, watch_data)

IceConn 	ice_conn;
IcePointer	client_data;
Bool		opening;
IcePointer	*watch_data;

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


Status
InitWatchProcs (appContext)

XtAppContext appContext;

{
    return (IceAddConnectionWatch (_XtIceWatchProc, (IcePointer) appContext));
}


/*
 * The real way to handle IO errors is to check the return status
 * of IceProcessMessages.  xsm properly does this.
 *
 * Unfortunately, a design flaw exists in the ICE library in which
 * a default IO error handler is invoked if no IO error handler is
 * installed.  This default handler exits.  We must avoid this.
 *
 * To get around this problem, we install an IO error handler that
 * does a little magic.  Since a previous IO handler might have been
 * installed, when we install our IO error handler, we do a little
 * trick to get both the previous IO error handler and the default
 * IO error handler.  When our IO error handler is called, if the
 * previous handler is not the default handler, we call it.  This
 * way, everyone's IO error handler gets called except the stupid
 * default one which does an exit!
 */

static IceIOErrorHandler prev_handler;

void
MyIoErrorHandler (ice_conn)

IceConn ice_conn;

{
    if (prev_handler)
	(*prev_handler) (ice_conn);
}    

void
InstallIOErrorHandler ()

{
    IceIOErrorHandler default_handler;

    prev_handler = IceSetIOErrorHandler (NULL);
    default_handler = IceSetIOErrorHandler (MyIoErrorHandler);
    if (prev_handler == default_handler)
	prev_handler = NULL;
}


/*
 * Since proxy manager does not authenticate connections, we disable
 * authentication by always returning true in the host based auth proc.
 */

Bool
HostBasedAuthProc (hostname)

char *hostname;

{
    return (1);
}
