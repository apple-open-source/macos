/* $Xorg: pmdb.c,v 1.5 2001/02/09 02:05:34 xorgcvs Exp $ */

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
/* $XFree86: xc/programs/proxymngr/pmdb.c,v 1.7 2001/12/14 20:01:02 dawes Exp $ */

#include "pmint.h"
#include "pmdb.h"
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#if defined(X_NOT_POSIX) && defined(SIGNALRETURNSINT)
#define SIGVAL int
#else
#define SIGVAL void
#endif
typedef SIGVAL (*Signal_Handler)(int);

void SetCloseOnExec (int fd);

static proxy_service *proxyServiceList = NULL;

static Signal_Handler 
Signal (int sig, Signal_Handler handler)
{
#ifndef X_NOT_POSIX
    struct sigaction sigact, osigact;
    sigact.sa_handler = handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(sig, &sigact, &osigact);
    return osigact.sa_handler;
#else
    return signal(sig, handler);
#endif
}

proxy_service *
FindProxyService (
    char *serviceName,
    Bool createIf)

{
    proxy_service *service = proxyServiceList;
    int nameLen = strlen (serviceName);

    while (service)
    {
	if (strcmp (service->serviceName, serviceName) == 0)
	    return service;
	else if (ncasecmp (service->serviceName, serviceName, nameLen) == 0)
	    return service;
	else
	    service = service->next;
    }

    if (createIf) {
	service = (proxy_service *) malloc (sizeof (proxy_service));
	if (!service)
	    return NULL;

	service->serviceName = (char *) malloc (nameLen + 1);
	if (!service->serviceName)
	{
	    free (service);
	    return NULL;
	}

	strcpy (service->serviceName, serviceName);
	service->proxyCount = 0;
	service->proxyList = NULL;

	if (proxyServiceList == NULL)
	{
	    proxyServiceList = service;
	    service->next = NULL;
	}
	else
	{
	    service->next = proxyServiceList;
	    proxyServiceList = service;
	}
    }

    return service;

}


running_proxy *
StartNewProxy (
    char *serviceName,
    char *startCommand)

{
    proxy_service *service = FindProxyService (serviceName, True);
    running_proxy *proxy;

    if (!service)
	return NULL;

    proxy = (running_proxy *) malloc (sizeof (running_proxy));
    if (!proxy)
	return NULL;

    proxy->active = 0;
    proxy->pmConn = NULL;
    proxy->requests = NULL;
    proxy->servers = NULL;
    proxy->refused_service = False;

    if (service->proxyList == NULL)
    {
	service->proxyList = proxy;
	proxy->next = NULL;
    }
    else
    {
	proxy->next = service->proxyList;
	service->proxyList = proxy;
    }

    if (system (startCommand) == -1)
    {
	printf ("unable to start managed proxy: %s\n", startCommand);
	service->proxyList = proxy->next;
	free (proxy);
	return NULL;
    }

    if (verbose) {
	printf ("started managed proxy: %s\n", startCommand);
	printf ("waiting for StartProxy message\n");
    }

    service->proxyCount++;

    return proxy;
}

/*
 * ConnectToProxy( pmOpcode, serviceName, proxyAddress )
 *
 * Connects to an unmanaged proxy to forward the GetProxyAddr request
 * to it to handle.
 *
 * Ideally this would be non-blocking but there is no non-blocking
 * variant of IceOpenConnection/IceProtocolSetup.  So we sit here a while.
 */
running_proxy *
ConnectToProxy (
    int pmOpcode,
    char *serviceName,
    char *proxyAddress)

{
    proxy_service *service = FindProxyService (serviceName, True);
    running_proxy *proxy;
    IceConn proxy_iceConn;
    PMconn *pmConn;

    if (!service)
	return NULL;

    {
	int majorVersion, minorVersion;
	char *vendor, *release;
	char errorString[256];

	/*
	 * IceOpenConnection will do more than one write to the proxy.
	 * If the proxy closes the connection before the second write,
	 * the second write may generate a SIGPIPE (empirically this
	 * happens on at least AIX).  So, temporarily ignore this signal.
	 */

	Signal (SIGPIPE, SIG_IGN);

	proxy_iceConn = IceOpenConnection( proxyAddress, NULL,
					   False, pmOpcode,
					   sizeof(errorString), errorString);

	Signal (SIGPIPE, SIG_DFL);

	if (! proxy_iceConn) {
	    printf("unable to open connection to unmanaged proxy \"%s\" at %s\n",
		   serviceName, proxyAddress);
	    return NULL;
	}

        /*
	 * Mark this fd to be closed upon exec
	 */
        SetCloseOnExec (IceConnectionNumber (proxy_iceConn));

	/* See PMprotocolSetupProc */
	pmConn = (PMconn *) malloc (sizeof (PMconn));

	if (pmConn == NULL) {
	    IceCloseConnection (proxy_iceConn);
	    return NULL;
	}

	if (IceProtocolSetup (proxy_iceConn, pmOpcode,
			      (IcePointer)pmConn, /* client_data */
			      False, /* must_authenticate */
			      &majorVersion, &minorVersion,
			      &vendor, &release,
			      sizeof(errorString), errorString)
	        != IceProtocolSetupSuccess) {
	    IceCloseConnection (proxy_iceConn);
	    free (pmConn);
	    printf ("Could not initialize proxy management protocol with\n  unmanaged proxy \"%s\" at address %s:\n  %s\n",
		    serviceName, proxyAddress, errorString);
	    return NULL;
	}

	pmConn->iceConn = proxy_iceConn;
	pmConn->pmOpcode = pmOpcode;
	pmConn->proto_major_version = majorVersion;
	pmConn->proto_minor_version = minorVersion;
	pmConn->vendor = vendor;
	pmConn->release = release;
    }

    proxy = (running_proxy *) malloc (sizeof (running_proxy));
    if (!proxy) {
	IceCloseConnection (proxy_iceConn);
	free (pmConn);
	return NULL;
    }

    proxy->active = 1;
    proxy->pmConn = pmConn;
    proxy->requests = NULL;
    proxy->servers = NULL;
    proxy->refused_service = False;

    if (service->proxyList == NULL)
    {
	service->proxyList = proxy;
	proxy->next = NULL;
    }
    else
    {
	proxy->next = service->proxyList;
	service->proxyList = proxy;
    }

    if (verbose)
	printf ("connected to unmanaged proxy: %s at %s\n",
		serviceName, proxyAddress);

    service->proxyCount++;

    return proxy;
}


Status
ActivateProxyService (
    char *serviceName,
    PMconn *pmConn)

{
    proxy_service *service = FindProxyService (serviceName, False);
    running_proxy *proxy;

    if (!service)
	return 0;

    proxy = service->proxyList;

    while (proxy)
    {
	if (!proxy->active)
	{
	    proxy->active = 1;
	    proxy->pmConn = pmConn;
	    return 1;
	}
	else
	    proxy = proxy->next;
    }

    return 0;
}


void
ProxyGone (
    IceConn proxyIceConn,
    Bool *activeReqs)

{
    proxy_service *service = proxyServiceList;

    while (service)
    {
	running_proxy *proxy = service->proxyList;
	running_proxy *prevProxy = NULL;

	while (proxy)
	{
	    if (proxy->pmConn && (proxy->pmConn->iceConn == proxyIceConn))
	    {
		server_list *server;
		request_list *req;

		if (verbose)
		    printf ("Proxy disconnected on fd %d",
			    IceConnectionNumber(proxyIceConn));

		server = proxy->servers;
		if (verbose && server)
		    fputs (" for server", stdout);

		while (server)
		{
		    server_list *next_server = server->next;
		    if (verbose) {
			fputc (' ', stdout);
			fputs (server->serverAddress, stdout);
		    }
		    free (server->serverAddress);
		    free (server);
		    server = next_server;
		}

		if (verbose)
		    fputc ('\n', stdout);

		if (prevProxy == NULL)
		    service->proxyList = proxy->next;
		else
		    prevProxy->next = proxy->next;

		service->proxyCount--;

		*activeReqs = proxy->requests != NULL;
		req = proxy->requests;
		while (req)
		{
		    request_list *nextreq = req->next;

		    if (req->requestor) {
			assert (req->requestor->iceConn != NULL);
			if (verbose)
			    printf ("Reprocessing request from fd %d for service %s at %s\n",
				    IceConnectionNumber (req->requestor->iceConn),
				    req->serviceName, req->serverAddress);

			ForwardRequest( req->requestor,
					req->serviceName, req->serverAddress,
					req->hostAddress, req->startOptions,
					req->authLen, req->authName,
					req->authData);
		    }
		    if (req->serviceName)
			free (req->serviceName);
		    if (req->serverAddress)
			free (req->serverAddress);
		    if (req->hostAddress)
			free (req->hostAddress);
		    if (req->startOptions)
			free (req->startOptions);
		    if (req->listData)
			free (req->listData);	/* proxyList */
		    if (req->authName)
			free (req->authName);
		    if (req->authData)
			free (req->authData);
		    free (req);
		    req = nextreq;
		}

		free (proxy);
		return;
	    }
	    else if (proxy->requests) {
		/*
		 * If it wasn't a proxy that disconnected, so it might
		 * have been a requestor.  Search through all the requests
		 * while we're here and look for a match.  If found, delete
		 * the request.
		 */
		request_list **prev_reqP = &proxy->requests;
		request_list *req = proxy->requests;
		while (req) {
		    if (req->requestor->iceConn == proxyIceConn) {
			if (verbose) {
			    printf ("Requestor disconnected on fd %d while awaiting reply\n  for service %s (%s)",
				    IceConnectionNumber(proxyIceConn),
				    req->serviceName, req->serverAddress);
			    if (proxy->pmConn && proxy->pmConn->iceConn) {
				printf (" from proxy on fd %d\n",
					IceConnectionNumber(proxy->pmConn->iceConn));
			    }
			    else
				fputc ('\n', stdout);
			}

			*prev_reqP = req->next;

			if (req->serviceName)
			    free (req->serviceName);
			if (req->serverAddress)
			    free (req->serverAddress);
			if (req->hostAddress)
			    free (req->hostAddress);
			if (req->startOptions)
			    free (req->startOptions);
			if (req->listData)
			    free (req->listData);	/* proxyList */
			if (req->authName)
			    free (req->authName);
			if (req->authData)
			    free (req->authData);
			free (req);

			/* return; */ /* should but only one req, but... */
		    }
		    else
			prev_reqP = &req->next;

		    req = *prev_reqP;
		}
	    }

	    prevProxy = proxy;
	    proxy = proxy->next;
	}

	service = service->next;
    }
}


/*
 * GetRuningProxyList returns a list of current proxies for a given
 * service.  The list is ordered, with proxies serving an address that
 * matches the argument appearing first on the list and all others
 * appearing at the end.  If a proxy ever refused a request for additional
 * service then it is excluded from the list if it doesn't match the
 * server address.
 */
running_proxy_list *
GetRunningProxyList (
    char *serviceName, char *serverAddress)

{
    proxy_service *service = FindProxyService (serviceName, False);
    running_proxy **proxyList, *proxy;
    running_proxy_list *runList;
    int headIndex, tailIndex;

    if (!service || !service->proxyCount)
	return NULL;

    runList = (running_proxy_list *) malloc (sizeof (running_proxy_list) +
	service->proxyCount * sizeof (running_proxy *));

    if (!runList)
	return NULL;

    runList->count = 0;
    runList->current = 0;
    runList->list = proxyList = (running_proxy **) (runList + 1);

    proxy = service->proxyList;
    headIndex = 0;
    tailIndex = service->proxyCount - 1;

    while (proxy)
    {
	server_list *server = proxy->servers;
	int match = 0;

	while (server)
	{
	    if (strcmp (server->serverAddress, serverAddress) == 0)
	    {
		match = 1;
		break;
	    }

	    server = server->next;
	}

	if (match) {
	    proxyList[headIndex++] = proxy;
	    runList->count++;
	}
	else if (! proxy->refused_service) {
	    proxyList[tailIndex--] = proxy;
	    runList->count++;
	}

	proxy = proxy->next;
    }

    if (!runList->count) {
	free ((char*)runList);
	return NULL;
    }

    /* if we didn't fill the list due to skipping proxies that had previously
     * refused to service a new address, then remove the gaps in the list
     * between the matched and unmatched server names
     */
    if (runList->count < service->proxyCount) {
	while (tailIndex < service->proxyCount - 1)
	    proxyList[headIndex++] = proxyList[++tailIndex];
    }

    return runList;
}


void
FreeProxyList (running_proxy_list *list)

{
    free (list);
}


Status
PushRequestorQueue (
    running_proxy *proxy,
    PMconn *requestor,
    running_proxy_list *runList,
    char *serviceName,
    char *serverAddress,
    char *hostAddress,
    char *startOptions,
    int authLen,
    char *authName,
    char *authData)

{
    request_list *newreq = (request_list *) malloc (sizeof (request_list));

    if (!newreq)
	return 0;

    newreq->serviceName = (char *) malloc (strlen (serviceName) + 1);
    newreq->serverAddress = (char *) malloc (strlen (serverAddress) + 1);
    newreq->hostAddress = (char *) malloc (strlen (hostAddress) + 1);
    newreq->startOptions = (char *) malloc (strlen (startOptions) + 1);
    if (authLen > 0)
    {
	newreq->authName = (char *) malloc (strlen (authName) + 1);
	newreq->authData = (char *) malloc (authLen);
    }

    if (!newreq->serviceName ||
	!newreq->serverAddress ||
	!newreq->hostAddress ||
	!newreq->startOptions ||
	(authLen > 0 && (!newreq->authName || !newreq->authData)))
    {
	if (newreq->serviceName)
	    free (newreq->serviceName);
	if (newreq->serverAddress)
	    free (newreq->serverAddress);
	if (newreq->hostAddress)
	    free (newreq->hostAddress);
	if (newreq->startOptions)
	    free (newreq->startOptions);
	if (newreq->authName)
	    free (newreq->authName);
	if (newreq->authData)
	    free (newreq->authData);
	free (newreq);
	return 0;
    }

    strcpy (newreq->serviceName, serviceName);
    strcpy (newreq->serverAddress, serverAddress);
    strcpy (newreq->hostAddress, hostAddress);
    strcpy (newreq->startOptions, startOptions);
    if (authLen > 0)
    {
	strcpy (newreq->authName, authName);
	memcpy (newreq->authData, authData, authLen);
    }
    else
    {
	newreq->authName = newreq->authData = NULL;
    }

    newreq->requestor = requestor;
    newreq->listData = (char *) runList;
    newreq->authLen = authLen;
    newreq->next = NULL;

    if (proxy->requests == NULL)
	proxy->requests = newreq;
    else
    {
	request_list *p = proxy->requests;

	while (p->next)
	    p = p->next;

	p->next = newreq;
    }

    return 1;
}


Status
PeekRequestorQueue (
    PMconn *pmConn,
    PMconn **requestor,
    running_proxy_list **runList,
    char **serviceName,
    char **serverAddress,
    char **hostAddress,
    char **startOptions,
    int *authLen,
    char **authName,
    char **authData)
{
    running_proxy *proxy = ProxyForPMconn (pmConn);

    if (proxy && proxy->requests) {
	if (requestor)
	    *requestor = proxy->requests->requestor;
	if (runList)
	    *runList = (running_proxy_list *)
		proxy->requests->listData; 
	if (serviceName)
	    *serviceName = proxy->requests->serviceName;
	if (serverAddress)
	    *serverAddress = proxy->requests->serverAddress;
	if (hostAddress)
	    *hostAddress = proxy->requests->hostAddress;
	if (startOptions)
	    *startOptions = proxy->requests->startOptions;
	if (authLen)
	    *authLen = proxy->requests->authLen;
	if (authName)
	    *authName = proxy->requests->authName;
	if (authData)
	    *authData = proxy->requests->authData;

	return 1;
    }
    else
	return 0;
}


running_proxy *
ProxyForPMconn (
    PMconn *pmConn)
{
    proxy_service *service = proxyServiceList;

    while (service)
    {
	running_proxy *proxy = service->proxyList;

	while (proxy)
	{
	    if (proxy->pmConn == pmConn)
		return proxy;
	    else
		proxy = proxy->next;
	}

	service = service->next;
    }

    return NULL;
}


PMconn*
PopRequestorQueue (
    PMconn *pmConn,
    Bool addServer,		/* record this server address */
    Bool freeProxyList)
{
    running_proxy *proxy = ProxyForPMconn (pmConn);

    if (proxy) {
	PMconn *requestor;
	server_list *server;
	request_list *nextreq;
	Bool newServer = False;

	if (addServer) {
	    newServer = True;
	    server = proxy->servers;

	    while (server)
	    {
		if (strcmp (server->serverAddress,
		    proxy->requests->serverAddress) == 0)
		{
		    newServer = False;
		    break;
		}

		server = server->next;
	    }

	    if (newServer)
	    {
		server = (server_list *) malloc (sizeof (server_list));
		server->serverAddress = proxy->requests->serverAddress;
		server->next = proxy->servers;
		proxy->servers = server;
	    }
	}

	if (!newServer)
	    free (proxy->requests->serverAddress);

	if (proxy->requests->serviceName)
	    free (proxy->requests->serviceName);
	if (proxy->requests->hostAddress)
	    free (proxy->requests->hostAddress);
	if (proxy->requests->startOptions)
	    free (proxy->requests->startOptions);
	if (freeProxyList && proxy->requests->listData)
	    free (proxy->requests->listData);	/* proxyList */
	if (proxy->requests->authName)
	    free (proxy->requests->authName);
	if (proxy->requests->authData)
	    free (proxy->requests->authData);

	requestor = proxy->requests->requestor;

	nextreq = proxy->requests->next;
	free (proxy->requests);
	proxy->requests = nextreq;

	return requestor;
    }

    return NULL;
}
