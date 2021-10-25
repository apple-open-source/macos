/*
 * Client routines for the CUPS scheduler.
 *
 * Copyright © 2007-2019 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products, all rights reserved.
 *
 * This file contains Kerberos support code, copyright 2006 by
 * Jelmer Vernooij.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#define _CUPS_NO_DEPRECATED
#define _HTTP_NO_PRIVATE
#include "cupsd.h"

#ifdef __APPLE__
#  include <libproc.h>
#endif /* __APPLE__ */
#ifdef HAVE_TCPD_H
#  include <tcpd.h>
#endif /* HAVE_TCPD_H */

#include "mime-private.h"

/*
 * Local functions...
 */

static int		check_if_modified(cupsd_client_t *con,
			                const struct stat *filestats);
static int		compare_clients(cupsd_client_t *a, cupsd_client_t *b,
			                void *data);
#ifdef HAVE_SSL
static int		cupsd_start_tls(cupsd_client_t *con, http_encryption_t e);
#endif /* HAVE_SSL */
//static char		*get_file(cupsd_client_t *con, struct stat *filestats,
//			          char *filename, size_t len);
static http_status_t	install_cupsd_conf(cupsd_client_t *con);
//static int		is_cgi(cupsd_client_t *con, const char *filename,
//		               struct stat *filestats, mime_type_t *type);
static int		is_path_absolute(const char *path);
static int		pipe_command(cupsd_client_t *con, int infile, int *outfile,
			             char *command, char *options, int root, int percent_decode);
static int		valid_host(cupsd_client_t *con);
//static int		write_file(cupsd_client_t *con, http_status_t code,
//		        	   char *filename, char *type,
//				   struct stat *filestats);
static void		write_pipe(cupsd_client_t *con);

/* - rooted file
 * Avoid races by working with paths resolved from well known places.
 * Prevents open(2) from working with a different file than we first stat'd.
 * These rf_ things are built on top of a further abstraction that finds
 * the best match given the original implementation of get_file.
 * The cleanuup here would be to remove this scaffolding (where it calls through
 * to the above static routines.)
 */
enum _rf_disposition_e {
  RF_FOUND,       /* file->_fd is valid - you should rf_close_file(&file) */
  RF_MISSING,     /* we don't have an open file - it might have been a symlink, or just not found */
  RF_PASS_ON      /* file is zero'd */
};
typedef enum _rf_disposition_e rf_disposition_t;

struct _rf_file_t {
  rf_disposition_t _disposition;
  int              _fd;           /* if disposition is RF_FOUND, then this will be != -1 and you need to rf_closefile */
  char             _filename[MAXPATHLEN];
  struct stat      _filestats;
};
typedef struct _rf_file_t rf_file_t;

static void rf_get_file(cupsd_client_t* con, rf_file_t* file);
static void rf_closefile(rf_file_t* file);
static mime_type_t *rf_mimeFileType(mime_t *mime, const rf_file_t* file);
static int rf_is_cgi(cupsd_client_t* con, const rf_file_t* file, mime_type_t *type);
static int rf_check_if_modified(cupsd_client_t* con, const rf_file_t* file);
static int rf_write_file(cupsd_client_t* con, http_status_t code, const rf_file_t* file, char* mime_type);

/*
 * 'cupsdAcceptClient()' - Accept a new client.
 */

void
cupsdAcceptClient(cupsd_listener_t *lis)/* I - Listener socket */
{
  const char		*hostname;	/* Hostname of client */
  char			name[256];	/* Hostname of client */
  int			count;		/* Count of connections on a host */
  cupsd_client_t	*con,		/* New client pointer */
			*tempcon;	/* Temporary client pointer */
  socklen_t		addrlen;	/* Length of address */
  http_addr_t		temp;		/* Temporary address variable */
  static time_t		last_dos = 0;	/* Time of last DoS attack */
#ifdef HAVE_TCPD_H
  struct request_info	wrap_req;	/* TCP wrappers request information */
#endif /* HAVE_TCPD_H */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdAcceptClient(lis=%p(%d)) Clients=%d", lis, lis->fd, cupsArrayCount(Clients));

 /*
  * Make sure we don't have a full set of clients already...
  */

  if (cupsArrayCount(Clients) == MaxClients)
    return;

  cupsdSetBusyState(1);

 /*
  * Get a pointer to the next available client...
  */

  if (!Clients)
    Clients = cupsArrayNew(NULL, NULL);

  if (!Clients)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to allocate memory for clients array!");
    cupsdPauseListening();
    return;
  }

  if (!ActiveClients)
    ActiveClients = cupsArrayNew((cups_array_func_t)compare_clients, NULL);

  if (!ActiveClients)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to allocate memory for active clients array!");
    cupsdPauseListening();
    return;
  }

  if ((con = calloc(1, sizeof(cupsd_client_t))) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to allocate memory for client!");
    cupsdPauseListening();
    return;
  }

 /*
  * Accept the client and get the remote address...
  */

  con->number = ++ LastClientNumber;
  con->file   = -1;

  if ((con->http = httpAcceptConnection(lis->fd, 0)) == NULL)
  {
    if (errno == ENFILE || errno == EMFILE)
      cupsdPauseListening();

    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to accept client connection - %s.",
                    strerror(errno));
    free(con);

    return;
  }

 /*
  * Save the connected address and port number...
  */

  addrlen = sizeof(con->clientaddr);

  if (getsockname(httpGetFd(con->http), (struct sockaddr *)&con->clientaddr, &addrlen) || addrlen == 0)
    con->clientaddr = lis->address;

  cupsdLogClient(con, CUPSD_LOG_DEBUG, "Server address is \"%s\".", httpAddrString(&con->clientaddr, name, sizeof(name)));

 /*
  * Check the number of clients on the same address...
  */

  for (count = 0, tempcon = (cupsd_client_t *)cupsArrayFirst(Clients);
       tempcon;
       tempcon = (cupsd_client_t *)cupsArrayNext(Clients))
    if (httpAddrEqual(httpGetAddress(tempcon->http), httpGetAddress(con->http)))
    {
      count ++;
      if (count >= MaxClientsPerHost)
	break;
    }

  if (count >= MaxClientsPerHost)
  {
    if ((time(NULL) - last_dos) >= 60)
    {
      last_dos = time(NULL);
      cupsdLogMessage(CUPSD_LOG_WARN,
                      "Possible DoS attack - more than %d clients connecting "
		      "from %s.",
	              MaxClientsPerHost,
		      httpGetHostname(con->http, name, sizeof(name)));
    }

    httpClose(con->http);
    free(con);
    return;
  }

 /*
  * Get the hostname or format the IP address as needed...
  */

  if (HostNameLookups)
    hostname = httpResolveHostname(con->http, NULL, 0);
  else
    hostname = httpGetHostname(con->http, NULL, 0);

  if (hostname == NULL && HostNameLookups == 2)
  {
   /*
    * Can't have an unresolved IP address with double-lookups enabled...
    */

    httpClose(con->http);

    cupsdLogClient(con, CUPSD_LOG_WARN,
                    "Name lookup failed - connection from %s closed!",
                    httpGetHostname(con->http, NULL, 0));

    free(con);
    return;
  }

  if (HostNameLookups == 2)
  {
   /*
    * Do double lookups as needed...
    */

    http_addrlist_t	*addrlist,	/* List of addresses */
			*addr;		/* Current address */

    if ((addrlist = httpAddrGetList(hostname, AF_UNSPEC, NULL)) != NULL)
    {
     /*
      * See if the hostname maps to the same IP address...
      */

      for (addr = addrlist; addr; addr = addr->next)
        if (httpAddrEqual(httpGetAddress(con->http), &(addr->addr)))
          break;
    }
    else
      addr = NULL;

    httpAddrFreeList(addrlist);

    if (!addr)
    {
     /*
      * Can't have a hostname that doesn't resolve to the same IP address
      * with double-lookups enabled...
      */

      httpClose(con->http);

      cupsdLogClient(con, CUPSD_LOG_WARN,
                      "IP lookup failed - connection from %s closed!",
                      httpGetHostname(con->http, NULL, 0));
      free(con);
      return;
    }
  }

#ifdef HAVE_TCPD_H
 /*
  * See if the connection is denied by TCP wrappers...
  */

  request_init(&wrap_req, RQ_DAEMON, "cupsd", RQ_FILE, httpGetFd(con->http),
               NULL);
  fromhost(&wrap_req);

  if (!hosts_access(&wrap_req))
  {
    httpClose(con->http);

    cupsdLogClient(con, CUPSD_LOG_WARN,
                    "Connection from %s refused by /etc/hosts.allow and "
		    "/etc/hosts.deny rules.", httpGetHostname(con->http, NULL, 0));
    free(con);
    return;
  }
#endif /* HAVE_TCPD_H */

#ifdef AF_LOCAL
  if (httpAddrFamily(httpGetAddress(con->http)) == AF_LOCAL)
  {
#  ifdef __APPLE__
    socklen_t	peersize;		/* Size of peer credentials */
    pid_t	peerpid;		/* Peer process ID */
    char	peername[256];		/* Name of process */

    peersize = sizeof(peerpid);
    if (!getsockopt(httpGetFd(con->http), SOL_LOCAL, LOCAL_PEERPID, &peerpid,
                    &peersize))
    {
      if (!proc_name((int)peerpid, peername, sizeof(peername)))
	cupsdLogClient(con, CUPSD_LOG_DEBUG,
	               "Accepted from %s (Domain ???[%d])",
                       httpGetHostname(con->http, NULL, 0), (int)peerpid);
      else
	cupsdLogClient(con, CUPSD_LOG_DEBUG,
                       "Accepted from %s (Domain %s[%d])",
                       httpGetHostname(con->http, NULL, 0), peername, (int)peerpid);
    }
    else
#  endif /* __APPLE__ */

    cupsdLogClient(con, CUPSD_LOG_DEBUG, "Accepted from %s (Domain)",
                   httpGetHostname(con->http, NULL, 0));
  }
  else
#endif /* AF_LOCAL */
  cupsdLogClient(con, CUPSD_LOG_DEBUG, "Accepted from %s:%d (IPv%d)",
                 httpGetHostname(con->http, NULL, 0),
		 httpAddrPort(httpGetAddress(con->http)),
		 httpAddrFamily(httpGetAddress(con->http)) == AF_INET ? 4 : 6);

 /*
  * Get the local address the client connected to...
  */

  addrlen = sizeof(temp);
  if (getsockname(httpGetFd(con->http), (struct sockaddr *)&temp, &addrlen))
  {
    cupsdLogClient(con, CUPSD_LOG_ERROR, "Unable to get local address - %s",
                   strerror(errno));

    strlcpy(con->servername, "localhost", sizeof(con->servername));
    con->serverport = LocalPort;
  }
#ifdef AF_LOCAL
  else if (httpAddrFamily(&temp) == AF_LOCAL)
  {
    strlcpy(con->servername, "localhost", sizeof(con->servername));
    con->serverport = LocalPort;
  }
#endif /* AF_LOCAL */
  else
  {
    if (httpAddrLocalhost(&temp))
      strlcpy(con->servername, "localhost", sizeof(con->servername));
    else if (HostNameLookups)
      httpAddrLookup(&temp, con->servername, sizeof(con->servername));
    else
      httpAddrString(&temp, con->servername, sizeof(con->servername));

    con->serverport = httpAddrPort(&(lis->address));
  }

 /*
  * Add the connection to the array of active clients...
  */

  cupsArrayAdd(Clients, con);

 /*
  * Add the socket to the server select.
  */

  cupsdAddSelect(httpGetFd(con->http), (cupsd_selfunc_t)cupsdReadClient, NULL,
                 con);

  cupsdLogClient(con, CUPSD_LOG_DEBUG, "Waiting for request.");

 /*
  * Temporarily suspend accept()'s until we lose a client...
  */

  if (cupsArrayCount(Clients) == MaxClients)
    cupsdPauseListening();

#ifdef HAVE_SSL
 /*
  * See if we are connecting on a secure port...
  */

  if (lis->encryption == HTTP_ENCRYPTION_ALWAYS)
  {
   /*
    * https connection; go secure...
    */

    if (cupsd_start_tls(con, HTTP_ENCRYPTION_ALWAYS))
      cupsdCloseClient(con);
  }
  else
    con->auto_ssl = 1;
#endif /* HAVE_SSL */
}


/*
 * 'cupsdCloseAllClients()' - Close all remote clients immediately.
 */

void
cupsdCloseAllClients(void)
{
  cupsd_client_t	*con;		/* Current client */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdCloseAllClients() Clients=%d", cupsArrayCount(Clients));

  for (con = (cupsd_client_t *)cupsArrayFirst(Clients);
       con;
       con = (cupsd_client_t *)cupsArrayNext(Clients))
    if (cupsdCloseClient(con))
      cupsdCloseClient(con);
}


/*
 * 'cupsdCloseClient()' - Close a remote client.
 */

int					/* O - 1 if partial close, 0 if fully closed */
cupsdCloseClient(cupsd_client_t *con)	/* I - Client to close */
{
  int		partial;		/* Do partial close for SSL? */


  cupsdLogClient(con, CUPSD_LOG_DEBUG, "Closing connection.");

 /*
  * Flush pending writes before closing...
  */

  httpFlushWrite(con->http);

  partial = 0;

  if (con->pipe_pid != 0)
  {
   /*
    * Stop any CGI process...
    */

    cupsdEndProcess(con->pipe_pid, 1);
    con->pipe_pid = 0;
  }

  if (con->file >= 0)
  {
    cupsdRemoveSelect(con->file);

    close(con->file);
    con->file = -1;
  }

 /*
  * Close the socket and clear the file from the input set for select()...
  */

  if (httpGetFd(con->http) >= 0)
  {
    cupsArrayRemove(ActiveClients, con);
    cupsdSetBusyState(0);

#ifdef HAVE_SSL
   /*
    * Shutdown encryption as needed...
    */

    if (httpIsEncrypted(con->http))
      partial = 1;
#endif /* HAVE_SSL */

    if (partial)
    {
     /*
      * Only do a partial close so that the encrypted client gets everything.
      */

      httpShutdown(con->http);
      cupsdAddSelect(httpGetFd(con->http), (cupsd_selfunc_t)cupsdReadClient,
                     NULL, con);

      cupsdLogClient(con, CUPSD_LOG_DEBUG, "Waiting for socket close.");
    }
    else
    {
     /*
      * Shut the socket down fully...
      */

      cupsdRemoveSelect(httpGetFd(con->http));
      httpClose(con->http);
      con->http = NULL;
    }
  }

  if (!partial)
  {
   /*
    * Free memory...
    */

    cupsdRemoveSelect(httpGetFd(con->http));

    httpClose(con->http);

    if (con->filename)
    {
      unlink(con->filename);
      cupsdClearString(&con->filename);
    }

    cupsdClearString(&con->command);
    cupsdClearString(&con->options);
    cupsdClearString(&con->query_string);

    if (con->request)
    {
      ippDelete(con->request);
      con->request = NULL;
    }

    if (con->response)
    {
      ippDelete(con->response);
      con->response = NULL;
    }

    if (con->language)
    {
      cupsLangFree(con->language);
      con->language = NULL;
    }

#ifdef HAVE_AUTHORIZATION_H
    if (con->authref)
    {
      AuthorizationFree(con->authref, kAuthorizationFlagDefaults);
      con->authref = NULL;
    }
#endif /* HAVE_AUTHORIZATION_H */

   /*
    * Re-enable new client connections if we are going back under the
    * limit...
    */

    if (cupsArrayCount(Clients) == MaxClients)
      cupsdResumeListening();

   /*
    * Compact the list of clients as necessary...
    */

    cupsArrayRemove(Clients, con);

    free(con);
  }

  return (partial);
}


/*
 * 'cupsdReadClient()' - Read data from a client.
 */

void
cupsdReadClient(cupsd_client_t *con)	/* I - Client to read from */
{
  char			line[32768],	/* Line from client... */
			locale[64],	/* Locale */
			*ptr;		/* Pointer into strings */
  http_status_t		status;		/* Transfer status */
  ipp_state_t		ipp_state;	/* State of IPP transfer */
  int			bytes;		/* Number of bytes to POST */
  mime_type_t		*type;		/* MIME type of file */
  static unsigned	request_id = 0;	/* Request ID for temp files */
  char                  request_uri[HTTP_MAX_URI];  /* uri - percent encoded */
  char              recv_buf[1];

  status = HTTP_STATUS_CONTINUE;

  cupsdLogClient(con, CUPSD_LOG_DEBUG2, "cupsdReadClient: error=%d, used=%d, state=%s, data_encoding=HTTP_ENCODING_%s, data_remaining=" CUPS_LLFMT ", request=%p(%s), file=%d", httpError(con->http), (int)httpGetReady(con->http), httpStateString(httpGetState(con->http)), httpIsChunked(con->http) ? "CHUNKED" : "LENGTH", CUPS_LLCAST httpGetRemaining(con->http), con->request, con->request ? ippStateString(ippGetState(con->request)) : "", con->file);

  if (httpError(con->http) == EPIPE && !httpGetReady(con->http) && recv(httpGetFd(con->http), recv_buf, 1, MSG_PEEK) < 1)
  {
   /*
    * Connection closed...
    */

    cupsdLogClient(con, CUPSD_LOG_DEBUG, "Closing on EOF.");
    cupsdCloseClient(con);
    return;
  }

  if (httpGetState(con->http) == HTTP_STATE_GET_SEND ||
      httpGetState(con->http) == HTTP_STATE_POST_SEND ||
      httpGetState(con->http) == HTTP_STATE_STATUS)
  {
   /*
    * If we get called in the wrong state, then something went wrong with the
    * connection and we need to shut it down...
    */

    cupsdLogClient(con, CUPSD_LOG_DEBUG, "Closing on unexpected HTTP read state %s.", httpStateString(httpGetState(con->http)));
    cupsdCloseClient(con);
    return;
  }

#ifdef HAVE_SSL
  if (con->auto_ssl)
  {
   /*
    * Automatically check for a SSL/TLS handshake...
    */

    con->auto_ssl = 0;

    if (recv(httpGetFd(con->http), recv_buf, 1, MSG_PEEK) == 1 &&
        (!recv_buf[0] || !strchr("DGHOPT", recv_buf[0])))
    {
     /*
      * Encrypt this connection...
      */

      cupsdLogClient(con, CUPSD_LOG_DEBUG2, "Saw first byte %02X, auto-negotiating SSL/TLS session.", recv_buf[0] & 255);

      if (cupsd_start_tls(con, HTTP_ENCRYPTION_ALWAYS))
        cupsdCloseClient(con);

      return;
    }
  }
#endif /* HAVE_SSL */

  switch (httpGetState(con->http))
  {
    case HTTP_STATE_WAITING :
       /*
        * See if we've received a request line...
	*/

        con->operation = httpReadRequest(con->http, request_uri, sizeof(request_uri));
        if (con->operation == HTTP_STATE_ERROR ||
	    con->operation == HTTP_STATE_UNKNOWN_METHOD ||
	    con->operation == HTTP_STATE_UNKNOWN_VERSION)
	{
	  if (httpError(con->http))
	    cupsdLogClient(con, CUPSD_LOG_DEBUG,
			   "HTTP_STATE_WAITING Closing for error %d (%s)",
			   httpError(con->http), strerror(httpError(con->http)));
	  else
	    cupsdLogClient(con, CUPSD_LOG_DEBUG,
	                   "HTTP_STATE_WAITING Closing on error: %s",
			   cupsLastErrorString());

	  cupsdCloseClient(con);
	  return;
	}

       /*
        * Ignore blank request lines...
	*/

        if (con->operation == HTTP_STATE_WAITING)
	  break;

       /*
        * Clear other state variables...
	*/

	con->bytes       = 0;
	con->file        = -1;
	con->file_ready  = 0;
	con->pipe_pid    = 0;
	con->username[0] = '\0';
	con->password[0] = '\0';

	cupsdClearString(&con->command);
	cupsdClearString(&con->options);
	cupsdClearString(&con->query_string);

	if (con->request)
	{
	  ippDelete(con->request);
	  con->request = NULL;
	}

	if (con->response)
	{
	  ippDelete(con->response);
	  con->response = NULL;
	}

	if (con->language)
	{
	  cupsLangFree(con->language);
	  con->language = NULL;
	}

#ifdef HAVE_GSSAPI
        con->have_gss = 0;
	con->gss_uid  = 0;
#endif /* HAVE_GSSAPI */

       /*
        * Handle full URLs in the request line...
	*/

        if (strcmp(request_uri, "*") == 0)
        {
          strlcpy(con->uri, request_uri, sizeof(con->uri));
        }
        else
	{
	  char	scheme[HTTP_MAX_URI],	/* Method/scheme */
		userpass[HTTP_MAX_URI],	/* Username:password */
		hostname[HTTP_MAX_URI],	/* Hostname */
		resource[HTTP_MAX_URI];	/* Resource path */
          int	port;			/* Port number */

         /*
	  * Separate the URI into its components...
	  */

          if (httpSeparateURI(HTTP_URI_CODING_MOST, request_uri,
	                      scheme, sizeof(scheme),
	                      userpass, sizeof(userpass),
			      hostname, sizeof(hostname), &port,
			      resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
          {
	    cupsdLogClient(con, CUPSD_LOG_ERROR, "Bad URI \"%s\" in request.",
                           request_uri);
	    cupsdSendError(con, HTTP_STATUS_METHOD_NOT_ALLOWED, CUPSD_AUTH_NONE);
	    cupsdCloseClient(con);
	    return;
	  }

	 /*
	  * Only allow URIs with the servername, localhost, or an IP
	  * address...
	  */

	  if (strcmp(scheme, "file") &&
	      _cups_strcasecmp(hostname, ServerName) &&
	      _cups_strcasecmp(hostname, "localhost") &&
	      !cupsArrayFind(ServerAlias, hostname) &&
	      !isdigit(hostname[0]) && hostname[0] != '[')
	  {
	   /*
	    * Nope, we don't do proxies...
	    */

	    cupsdLogClient(con, CUPSD_LOG_ERROR, "Bad URI \"%s\" in request.",
                           request_uri);
	    cupsdSendError(con, HTTP_STATUS_METHOD_NOT_ALLOWED, CUPSD_AUTH_NONE);
	    cupsdCloseClient(con);
	    return;
	  }

         /*
	  * Copy the percent-decoded resource portion into the URI; both resource and
	  * con->uri are HTTP_MAX_URI bytes in size...
	  */

          strlcpy(con->uri, resource, sizeof(con->uri));
	}

       /*
        * Process the request...
	*/

        gettimeofday(&(con->start), NULL);

        cupsdLogClient(con, CUPSD_LOG_DEBUG, "%s %s HTTP/%d.%d",
	               httpStateString(con->operation) + 11, con->uri,
		       httpGetVersion(con->http) / 100,
                       httpGetVersion(con->http) % 100);

        if (!cupsArrayFind(ActiveClients, con))
	{
	  cupsArrayAdd(ActiveClients, con);
          cupsdSetBusyState(0);
        }

    case HTTP_STATE_OPTIONS :
    case HTTP_STATE_DELETE :
    case HTTP_STATE_GET :
    case HTTP_STATE_HEAD :
    case HTTP_STATE_POST :
    case HTTP_STATE_PUT :
    case HTTP_STATE_TRACE :
       /*
        * Parse incoming parameters until the status changes...
	*/

        while ((status = httpUpdate(con->http)) == HTTP_STATUS_CONTINUE)
	  if (!httpGetReady(con->http))
	    break;

	if (status != HTTP_STATUS_OK && status != HTTP_STATUS_CONTINUE)
	{
	  if (httpError(con->http) && httpError(con->http) != EPIPE)
	    cupsdLogClient(con, CUPSD_LOG_DEBUG,
                           "Closing for error %d (%s) while reading headers.",
                           httpError(con->http), strerror(httpError(con->http)));
	  else
	    cupsdLogClient(con, CUPSD_LOG_DEBUG,
	                   "Closing on EOF while reading headers.");

	  cupsdSendError(con, HTTP_STATUS_BAD_REQUEST, CUPSD_AUTH_NONE);
	  cupsdCloseClient(con);
	  return;
	}
	break;

    default :
        if (!httpGetReady(con->http) && recv(httpGetFd(con->http), recv_buf, 1, MSG_PEEK) < 1)
	{
	 /*
	  * Connection closed...
	  */

	  cupsdLogClient(con, CUPSD_LOG_DEBUG, "Closing on EOF.");
          cupsdCloseClient(con);
	  return;
	}
        break; /* Anti-compiler-warning-code */
  }

 /*
  * Handle new transfers...
  */

  cupsdLogClient(con, CUPSD_LOG_DEBUG, "Read: status=%d, state=%d", status, httpGetState(con->http));

  if (status == HTTP_STATUS_OK)
  {
   /*
    * Record whether the client is a web browser.  "Mozilla" was the original
    * and it seems that every web browser in existence now uses that as the
    * prefix with additional information identifying *which* browser.
    *
    * Chrome (at least) has problems with multiple WWW-Authenticate values in
    * a single header, so we only report Basic or Negotiate to web browsers and
    * leave the multiple choices to the native CUPS client...
    */

    con->is_browser = !strncmp(httpGetField(con->http, HTTP_FIELD_USER_AGENT), "Mozilla/", 8);

    if (httpGetField(con->http, HTTP_FIELD_ACCEPT_LANGUAGE)[0])
    {
     /*
      * Figure out the locale from the Accept-Language and Content-Type
      * fields...
      */

      if ((ptr = strchr(httpGetField(con->http, HTTP_FIELD_ACCEPT_LANGUAGE),
                        ',')) != NULL)
        *ptr = '\0';

      if ((ptr = strchr(httpGetField(con->http, HTTP_FIELD_ACCEPT_LANGUAGE),
                        ';')) != NULL)
        *ptr = '\0';

      if ((ptr = strstr(httpGetField(con->http, HTTP_FIELD_CONTENT_TYPE),
                        "charset=")) != NULL)
      {
       /*
        * Combine language and charset, and trim any extra params in the
	* content-type.
	*/

        snprintf(locale, sizeof(locale), "%s.%s",
	         httpGetField(con->http, HTTP_FIELD_ACCEPT_LANGUAGE), ptr + 8);

	if ((ptr = strchr(locale, ',')) != NULL)
	  *ptr = '\0';
      }
      else
        snprintf(locale, sizeof(locale), "%s.UTF-8",
	         httpGetField(con->http, HTTP_FIELD_ACCEPT_LANGUAGE));

      con->language = cupsLangGet(locale);
    }
    else
      con->language = cupsLangGet(DefaultLocale);

    cupsdAuthorize(con);

    if (!_cups_strncasecmp(httpGetField(con->http, HTTP_FIELD_CONNECTION),
                           "Keep-Alive", 10) && KeepAlive)
      httpSetKeepAlive(con->http, HTTP_KEEPALIVE_ON);
    else if (!_cups_strncasecmp(httpGetField(con->http, HTTP_FIELD_CONNECTION),
                                "close", 5))
      httpSetKeepAlive(con->http, HTTP_KEEPALIVE_OFF);

    if (!httpGetField(con->http, HTTP_FIELD_HOST)[0] &&
        httpGetVersion(con->http) >= HTTP_VERSION_1_1)
    {
     /*
      * HTTP/1.1 and higher require the "Host:" field...
      */

      if (!cupsdSendError(con, HTTP_STATUS_BAD_REQUEST, CUPSD_AUTH_NONE))
      {
        cupsdLogClient(con, CUPSD_LOG_ERROR, "Missing Host: field in request.");
	cupsdCloseClient(con);
	return;
      }
    }
    else if (!valid_host(con))
    {
     /*
      * Access to localhost must use "localhost" or the corresponding IPv4
      * or IPv6 values in the Host: field.
      */

      cupsdLogClient(con, CUPSD_LOG_ERROR,
                     "Request from \"%s\" using invalid Host: field \"%s\".",
                     httpGetHostname(con->http, NULL, 0), httpGetField(con->http, HTTP_FIELD_HOST));

      if (!cupsdSendError(con, HTTP_STATUS_BAD_REQUEST, CUPSD_AUTH_NONE))
      {
	cupsdCloseClient(con);
	return;
      }
    }
    else if (con->operation == HTTP_STATE_OPTIONS)
    {
     /*
      * Do OPTIONS command...
      */

      if (con->best && con->best->type != CUPSD_AUTH_NONE)
      {
        httpClearFields(con->http);

	if (!cupsdSendHeader(con, HTTP_STATUS_UNAUTHORIZED, NULL, CUPSD_AUTH_NONE))
	{
	  cupsdCloseClient(con);
	  return;
	}
      }

      if (!_cups_strcasecmp(httpGetField(con->http, HTTP_FIELD_CONNECTION), "Upgrade") && strstr(httpGetField(con->http, HTTP_FIELD_UPGRADE), "TLS/") != NULL && !httpIsEncrypted(con->http))
      {
#ifdef HAVE_SSL
       /*
        * Do encryption stuff...
	*/

        httpClearFields(con->http);

	if (!cupsdSendHeader(con, HTTP_STATUS_SWITCHING_PROTOCOLS, NULL, CUPSD_AUTH_NONE))
	{
	  cupsdCloseClient(con);
	  return;
	}

        if (cupsd_start_tls(con, HTTP_ENCRYPTION_REQUIRED))
        {
	  cupsdCloseClient(con);
	  return;
	}
#else
	if (!cupsdSendError(con, HTTP_STATUS_NOT_IMPLEMENTED, CUPSD_AUTH_NONE))
	{
	  cupsdCloseClient(con);
	  return;
	}
#endif /* HAVE_SSL */
      }

      httpClearFields(con->http);
      httpSetField(con->http, HTTP_FIELD_CONTENT_LENGTH, "0");

      if (!cupsdSendHeader(con, HTTP_STATUS_OK, NULL, CUPSD_AUTH_NONE))
      {
	cupsdCloseClient(con);
	return;
      }
    }
    else if (!is_path_absolute(con->uri))
    {
     /*
      * Protect against malicious users!
      */

      cupsdLogClient(con, CUPSD_LOG_ERROR,
                     "Request for non-absolute resource \"%s\".", con->uri);

      if (!cupsdSendError(con, HTTP_STATUS_FORBIDDEN, CUPSD_AUTH_NONE))
      {
	cupsdCloseClient(con);
	return;
      }
    }
    else
    {
      if (!_cups_strcasecmp(httpGetField(con->http, HTTP_FIELD_CONNECTION),
                            "Upgrade") && !httpIsEncrypted(con->http))
      {
#ifdef HAVE_SSL
       /*
        * Do encryption stuff...
	*/

        httpClearFields(con->http);

	if (!cupsdSendHeader(con, HTTP_STATUS_SWITCHING_PROTOCOLS, NULL,
	                     CUPSD_AUTH_NONE))
	{
	  cupsdCloseClient(con);
	  return;
	}

        if (cupsd_start_tls(con, HTTP_ENCRYPTION_REQUIRED))
        {
	  cupsdCloseClient(con);
	  return;
	}
#else
	if (!cupsdSendError(con, HTTP_STATUS_NOT_IMPLEMENTED, CUPSD_AUTH_NONE))
	{
	  cupsdCloseClient(con);
	  return;
	}
#endif /* HAVE_SSL */
      }

      if ((status = cupsdIsAuthorized(con, NULL)) != HTTP_STATUS_OK)
      {
	cupsdSendError(con, status, CUPSD_AUTH_NONE);
	cupsdCloseClient(con);
	return;
      }

      if (httpGetExpect(con->http) &&
          (con->operation == HTTP_STATE_POST || con->operation == HTTP_STATE_PUT))
      {
        if (httpGetExpect(con->http) == HTTP_STATUS_CONTINUE)
	{
	 /*
	  * Send 100-continue header...
	  */

          if (httpWriteResponse(con->http, HTTP_STATUS_CONTINUE))
	  {
	    cupsdCloseClient(con);
	    return;
	  }
	}
	else
	{
	 /*
	  * Send 417-expectation-failed header...
	  */

          httpClearFields(con->http);
	  httpSetField(con->http, HTTP_FIELD_CONTENT_LENGTH, "0");

	  cupsdSendError(con, HTTP_STATUS_EXPECTATION_FAILED, CUPSD_AUTH_NONE);
          cupsdCloseClient(con);
          return;
	}
      }

      switch (httpGetState(con->http))
      {
        case HTTP_STATE_GET_SEND : {
          cupsdLogClient(con, CUPSD_LOG_DEBUG, "Processing GET %s", con->uri);

          rf_file_t getfile;
          rf_get_file(con, &getfile);

          if (getfile._disposition == RF_FOUND)
          {
              type = rf_mimeFileType(MimeDatabase, &getfile);

              cupsdLogClient(con, CUPSD_LOG_DEBUG, "filename=\"%s\", type=%s/%s", getfile._filename, type ? type->super : "", type ? type->type : "");

              /** NOTE: rf_is_cgi / old is_cgi mutates con for cgi parameter extraction */
              if (rf_is_cgi(con, &getfile, type))
              {
                rf_closefile(&getfile);

                /*
                 * Note: con->command and con->options were set by is_cgi()...
                 */

                  if (!cupsdSendCommand(con, con->command, con->options, 0, 0))
                  {
                      if (!cupsdSendError(con, HTTP_STATUS_NOT_FOUND, CUPSD_AUTH_NONE))
                      {
                          cupsdCloseClient(con);
                          return;
                      }
                  }
                  else
                      cupsdLogRequest(con, HTTP_STATUS_OK);

                if (httpGetVersion(con->http) <= HTTP_VERSION_1_0)
                    httpSetKeepAlive(con->http, HTTP_KEEPALIVE_OFF);

                break;
              }

              if (!rf_check_if_modified(con, &getfile))
              {
                rf_closefile(&getfile);
                if (!cupsdSendError(con, HTTP_STATUS_NOT_MODIFIED, CUPSD_AUTH_NONE))
                {
                    cupsdCloseClient(con);
                    return;
                }
              }
              else
              {
                char mime_line[256];

                if (type == NULL)
                    strlcpy(mime_line, "text/plain", sizeof(mime_line));
                else
                    snprintf(mime_line, sizeof(mime_line), "%s/%s", type->super, type->type);

                int did_write = rf_write_file(con, HTTP_STATUS_OK, &getfile, mime_line);

                rf_closefile(&getfile);

                if (!did_write)
                {
                    cupsdCloseClient(con);
                    return;
                }
              }
            }
            else if ((getfile._disposition == RF_PASS_ON) && (!strncmp(con->uri, "/admin", 6) || !strncmp(con->uri, "/classes", 8) || !strncmp(con->uri, "/help", 5) || !strncmp(con->uri, "/jobs", 5) || !strncmp(con->uri, "/printers", 9)))
	    {
	      if (!WebInterface)
	      {
	       /*
		* Web interface is disabled. Show an appropriate message...
		*/

		if (!cupsdSendError(con, HTTP_STATUS_CUPS_WEBIF_DISABLED, CUPSD_AUTH_NONE))
		{
		  cupsdCloseClient(con);
		  return;
		}

		break;
	      }

	     /*
	      * Send CGI output...
	      */

              if (!strncmp(con->uri, "/admin", 6))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/admin.cgi", ServerBin);
		cupsdSetString(&con->options, strchr(con->uri + 6, '?'));
	      }
	      else if (!strncmp(con->uri, "/classes", 8))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/classes.cgi", ServerBin);
                if (con->uri[8] && con->uri[9])
		  cupsdSetString(&con->options, con->uri + 8);
		else
		  cupsdSetString(&con->options, NULL);
	      }
	      else if (!strncmp(con->uri, "/jobs", 5))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/jobs.cgi", ServerBin);
                if (con->uri[5] && con->uri[6])
		  cupsdSetString(&con->options, con->uri + 5);
		else
		  cupsdSetString(&con->options, NULL);
	      }
              else if (!strncmp(con->uri, "/printers", 9))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/printers.cgi", ServerBin);
                if (con->uri[9] && con->uri[10])
		  cupsdSetString(&con->options, con->uri + 9);
		else
		  cupsdSetString(&con->options, NULL);
	      }
	      else
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/help.cgi", ServerBin);
                if (con->uri[5] && con->uri[6])
		  cupsdSetString(&con->options, con->uri + 5);
		else
		  cupsdSetString(&con->options, NULL);
	      }

              // REMINDSMA: At this point con->options has been % expanded - %252f becomes -> %2f
              if (!cupsdSendCommand(con, con->command, con->options, 0, 0))
	      {
		if (!cupsdSendError(con, HTTP_STATUS_NOT_FOUND, CUPSD_AUTH_NONE))
		{
		  cupsdCloseClient(con);
		  return;
		}
              }
	      else
        	cupsdLogRequest(con, HTTP_STATUS_OK);

	      if (httpGetVersion(con->http) <= HTTP_VERSION_1_0)
		httpSetKeepAlive(con->http, HTTP_KEEPALIVE_OFF);
	    }
	    else
	    {
	      if (!cupsdSendError(con, HTTP_STATUS_NOT_FOUND, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }
	    }
        }   break;

        case HTTP_STATE_POST_RECV : {
           /*
	    * See if the POST request includes a Content-Length field, and if
	    * so check the length against any limits that are set...
	    */

            if (httpGetField(con->http, HTTP_FIELD_CONTENT_LENGTH)[0] && MaxRequestSize > 0 && httpGetLength2(con->http) > MaxRequestSize)
	    {
	     /*
	      * Request too large...
	      */

              if (!cupsdSendError(con, HTTP_STATUS_REQUEST_TOO_LARGE, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }

	      break;
            }
	    else if (httpGetLength2(con->http) < 0)
	    {
	     /*
	      * Negative content lengths are invalid!
	      */

              if (!cupsdSendError(con, HTTP_STATUS_BAD_REQUEST, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }

	      break;
	    }

           /*
	    * See what kind of POST request this is; for IPP requests the
	    * content-type field will be "application/ipp"...
	    */

	    if (!strcmp(httpGetField(con->http, HTTP_FIELD_CONTENT_TYPE), "application/ipp"))
	    {
              con->request = ippNew();
              break;
            }
            else if (!WebInterface)
	    {
	     /*
	      * Web interface is disabled. Show an appropriate message...
	      */

	      if (!cupsdSendError(con, HTTP_STATUS_CUPS_WEBIF_DISABLED, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }

	      break;
	    }

            rf_file_t postfile;
            rf_get_file(con, &postfile);

	    if (postfile._disposition == RF_FOUND)
            {
	     /*
	      * POST to a file...
	      */

              type = rf_mimeFileType(MimeDatabase, &postfile);

              int is_it_cgi = rf_is_cgi(con, &postfile, type);

              // we don't ever actually use the file
              rf_closefile(&postfile);

              // so .. based on this logic, if the file exists and is a CGI, then we don't do anything?
              // REMINDSMA...

              if (!is_it_cgi)
	      {
	       /*
	        * Only POST to CGI's...
		*/

		if (!cupsdSendError(con, HTTP_STATUS_UNAUTHORIZED, CUPSD_AUTH_NONE))
		{
		  cupsdCloseClient(con);
		  return;
		}
	      }
            }
	    else if (!strncmp(con->uri, "/admin", 6) || !strncmp(con->uri, "/printers", 9) ||  !strncmp(con->uri, "/classes", 8) || !strncmp(con->uri, "/help", 5) || !strncmp(con->uri, "/jobs", 5))
	    {
	     /*
	      * CGI request...
	      */

              if (!strncmp(con->uri, "/admin", 6))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/admin.cgi", ServerBin);
		cupsdSetString(&con->options, strchr(con->uri + 6, '?'));
	      }
              else if (!strncmp(con->uri, "/printers", 9))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/printers.cgi", ServerBin);
                if (con->uri[9] && con->uri[10])
		  cupsdSetString(&con->options, con->uri + 9);
		else
		  cupsdSetString(&con->options, NULL);
	      }
	      else if (!strncmp(con->uri, "/classes", 8))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/classes.cgi", ServerBin);
                if (con->uri[8] && con->uri[9])
		  cupsdSetString(&con->options, con->uri + 8);
		else
		  cupsdSetString(&con->options, NULL);
	      }
	      else if (!strncmp(con->uri, "/jobs", 5))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/jobs.cgi", ServerBin);
                if (con->uri[5] && con->uri[6])
		  cupsdSetString(&con->options, con->uri + 5);
		else
		  cupsdSetString(&con->options, NULL);
	      }
	      else
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/help.cgi", ServerBin);
                if (con->uri[5] && con->uri[6])
		  cupsdSetString(&con->options, con->uri + 5);
		else
		  cupsdSetString(&con->options, NULL);
	      }

	      if (httpGetVersion(con->http) <= HTTP_VERSION_1_0)
		httpSetKeepAlive(con->http, HTTP_KEEPALIVE_OFF);
	    }
	    else
	    {
	      if (!cupsdSendError(con, HTTP_STATUS_NOT_FOUND, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }
	    }
        }    break;

	case HTTP_STATE_PUT_RECV :
	   /*
	    * Validate the resource name...
	    */

            if (strcmp(con->uri, "/admin/conf/cupsd.conf"))
	    {
	     /*
	      * PUT can only be done to the cupsd.conf file...
	      */

	      cupsdLogClient(con, CUPSD_LOG_ERROR, "Disallowed PUT request for \"%s\".", con->uri);

	      if (!cupsdSendError(con, HTTP_STATUS_FORBIDDEN, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }

	      break;
	    }

           /*
	    * See if the PUT request includes a Content-Length field, and if
	    * so check the length against any limits that are set...
	    */

            if (httpGetField(con->http, HTTP_FIELD_CONTENT_LENGTH)[0] && MaxRequestSize > 0 && httpGetLength2(con->http) > MaxRequestSize)
	    {
	     /*
	      * Request too large...
	      */

              if (!cupsdSendError(con, HTTP_STATUS_REQUEST_TOO_LARGE, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }

	      break;
            }
	    else if (httpGetLength2(con->http) < 0)
	    {
	     /*
	      * Negative content lengths are invalid!
	      */

              if (!cupsdSendError(con, HTTP_STATUS_BAD_REQUEST, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }

	      break;
	    }

           /*
	    * Open a temporary file to hold the request...
	    */

            cupsdSetStringf(&con->filename, "%s/%08x", RequestRoot, request_id ++);
	    con->file = open(con->filename, O_WRONLY | O_CREAT | O_TRUNC, 0640);

	    if (con->file < 0)
	    {
	      cupsdLogClient(con, CUPSD_LOG_ERROR, "Unable to create request file \"%s\": %s", con->filename, strerror(errno));

	      if (!cupsdSendError(con, HTTP_STATUS_REQUEST_TOO_LARGE, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }
	    }

	    fchmod(con->file, 0640);
	    fchown(con->file, RunUser, Group);
	    fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);
	    break;

	case HTTP_STATE_DELETE :
	case HTTP_STATE_TRACE :
            cupsdSendError(con, HTTP_STATUS_NOT_IMPLEMENTED, CUPSD_AUTH_NONE);
	    cupsdCloseClient(con);
	    return;

        case HTTP_STATE_HEAD : {
          rf_file_t head_file;
          rf_get_file(con, &head_file);

          if (head_file._disposition == RF_FOUND)
            {
	      if (!rf_check_if_modified(con, &head_file))
	      {
		if (!cupsdSendError(con, HTTP_STATUS_NOT_MODIFIED, CUPSD_AUTH_NONE))
		{
		  cupsdCloseClient(con);
		  return;
		}

		cupsdLogRequest(con, HTTP_STATUS_NOT_MODIFIED);
	      }
	      else
	      {
	       /*
		* Serve a file...
		*/

                type = rf_mimeFileType(MimeDatabase, &head_file);
		if (type == NULL)
		  strlcpy(line, "text/plain", sizeof(line));
		else
		  snprintf(line, sizeof(line), "%s/%s", type->super, type->type);

		httpClearFields(con->http);

		httpSetField(con->http, HTTP_FIELD_LAST_MODIFIED, httpGetDateString(head_file._filestats.st_mtime));
		httpSetLength(con->http, (size_t)head_file._filestats.st_size);

                rf_closefile(&head_file);

		if (!cupsdSendHeader(con, HTTP_STATUS_OK, line, CUPSD_AUTH_NONE))
		{
		  cupsdCloseClient(con);
		  return;
		}

		cupsdLogRequest(con, HTTP_STATUS_OK);
	      }
            }
	    else if (!WebInterface)
	    {
              httpClearFields(con->http);

              if (!cupsdSendHeader(con, HTTP_STATUS_OK, NULL, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }

              cupsdLogRequest(con, HTTP_STATUS_OK);
	      break;
	    }

	    if ((head_file._disposition == RF_PASS_ON) && (!strncmp(con->uri, "/admin", 6) || !strncmp(con->uri, "/classes", 8) || !strncmp(con->uri, "/help", 5) || !strncmp(con->uri, "/jobs", 5) || !strncmp(con->uri, "/printers", 9)))
	    {
	     /*
	      * CGI output...
	      */

              httpClearFields(con->http);

              if (!cupsdSendHeader(con, HTTP_STATUS_OK, "text/html", CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }

              cupsdLogRequest(con, HTTP_STATUS_OK);
	    }
	    else
	    {
              httpClearFields(con->http);

	      if (!cupsdSendHeader(con, HTTP_STATUS_NOT_FOUND, "text/html", CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }

              cupsdLogRequest(con, HTTP_STATUS_NOT_FOUND);
	    }
        }    break;

	default :
            break; /* Anti-compiler-warning-code */
      }
    }
  }

 /*
  * Handle any incoming data...
  */

  switch (httpGetState(con->http))
  {
    case HTTP_STATE_PUT_RECV :
        do
	{
          if ((bytes = httpRead2(con->http, line, sizeof(line))) < 0)
	  {
	    if (httpError(con->http) && httpError(con->http) != EPIPE)
	      cupsdLogClient(con, CUPSD_LOG_DEBUG,
                             "HTTP_STATE_PUT_RECV Closing for error %d (%s)",
                             httpError(con->http), strerror(httpError(con->http)));
	    else
	      cupsdLogClient(con, CUPSD_LOG_DEBUG,
			     "HTTP_STATE_PUT_RECV Closing on EOF.");

	    cupsdCloseClient(con);
	    return;
	  }
	  else if (bytes > 0)
	  {
	    con->bytes += bytes;

            if (MaxRequestSize > 0 && con->bytes > MaxRequestSize)
            {
	      close(con->file);
	      con->file = -1;
	      unlink(con->filename);
	      cupsdClearString(&con->filename);

              if (!cupsdSendError(con, HTTP_STATUS_REQUEST_TOO_LARGE, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }
            }

            if (write(con->file, line, (size_t)bytes) < bytes)
	    {
              cupsdLogClient(con, CUPSD_LOG_ERROR,
	                     "Unable to write %d bytes to \"%s\": %s", bytes,
                             con->filename, strerror(errno));

	      close(con->file);
	      con->file = -1;
	      unlink(con->filename);
	      cupsdClearString(&con->filename);

              if (!cupsdSendError(con, HTTP_STATUS_REQUEST_TOO_LARGE, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }
	    }
	  }
          else if (httpGetState(con->http) == HTTP_STATE_PUT_RECV)
          {
            cupsdCloseClient(con);
            return;
          }
        }
	while (httpGetState(con->http) == HTTP_STATE_PUT_RECV && httpGetReady(con->http));

        if (httpGetState(con->http) == HTTP_STATE_STATUS)
	{
	 /*
	  * End of file, see how big it is...
	  */

          struct stat    filestats;  /* File information */
	  fstat(con->file, &filestats);

	  close(con->file);
	  con->file = -1;

          if (filestats.st_size > MaxRequestSize &&
	      MaxRequestSize > 0)
	  {
	   /*
	    * Request is too big; remove it and send an error...
	    */

	    unlink(con->filename);
	    cupsdClearString(&con->filename);

            if (!cupsdSendError(con, HTTP_STATUS_REQUEST_TOO_LARGE, CUPSD_AUTH_NONE))
	    {
	      cupsdCloseClient(con);
	      return;
	    }
	  }

         /*
	  * Install the configuration file...
	  */

          status = install_cupsd_conf(con);

         /*
	  * Return the status to the client...
	  */

          if (!cupsdSendError(con, status, CUPSD_AUTH_NONE))
	  {
	    cupsdCloseClient(con);
	    return;
	  }
	}
        break;

    case HTTP_STATE_POST_RECV :
        do
	{
          if (con->request && con->file < 0)
	  {
	   /*
	    * Grab any request data from the connection...
	    */

	    if (!httpWait(con->http, 0))
	      return;

	    if ((ipp_state = ippRead(con->http, con->request)) == IPP_STATE_ERROR)
	    {
              cupsdLogClient(con, CUPSD_LOG_ERROR, "IPP read error: %s",
                             cupsLastErrorString());

	      cupsdSendError(con, HTTP_STATUS_BAD_REQUEST, CUPSD_AUTH_NONE);
	      cupsdCloseClient(con);
	      return;
	    }
	    else if (ipp_state != IPP_STATE_DATA)
	    {
              if (httpGetState(con->http) == HTTP_STATE_POST_SEND)
	      {
		cupsdSendError(con, HTTP_STATUS_BAD_REQUEST, CUPSD_AUTH_NONE);
		cupsdCloseClient(con);
		return;
	      }

	      if (httpGetReady(con->http))
	        continue;
	      break;
            }
	    else
	    {
	      cupsdLogClient(con, CUPSD_LOG_DEBUG, "%d.%d %s %d",
			      con->request->request.op.version[0],
			      con->request->request.op.version[1],
			      ippOpString(con->request->request.op.operation_id),
			      con->request->request.op.request_id);
	      con->bytes += (off_t)ippLength(con->request);
	    }
	  }

          if (con->file < 0 && httpGetState(con->http) != HTTP_STATE_POST_SEND)
	  {
           /*
	    * Create a file as needed for the request data...
	    */

            cupsdSetStringf(&con->filename, "%s/%08x", RequestRoot,
	                    request_id ++);
	    con->file = open(con->filename, O_WRONLY | O_CREAT | O_TRUNC, 0640);

	    if (con->file < 0)
	    {
	      cupsdLogClient(con, CUPSD_LOG_ERROR,
	                     "Unable to create request file \"%s\": %s",
                             con->filename, strerror(errno));

	      if (!cupsdSendError(con, HTTP_STATUS_REQUEST_TOO_LARGE, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }
	    }

	    fchmod(con->file, 0640);
	    fchown(con->file, RunUser, Group);
            fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);
	  }

	  if (httpGetState(con->http) != HTTP_STATE_POST_SEND)
	  {
	    if (!httpWait(con->http, 0))
	      return;
            else if ((bytes = httpRead2(con->http, line, sizeof(line))) < 0)
	    {
	      if (httpError(con->http) && httpError(con->http) != EPIPE)
		cupsdLogClient(con, CUPSD_LOG_DEBUG,
			       "HTTP_STATE_POST_SEND Closing for error %d (%s)",
                               httpError(con->http), strerror(httpError(con->http)));
	      else
		cupsdLogClient(con, CUPSD_LOG_DEBUG,
			       "HTTP_STATE_POST_SEND Closing on EOF.");

	      cupsdCloseClient(con);
	      return;
	    }
	    else if (bytes > 0)
	    {
	      con->bytes += bytes;

              if (MaxRequestSize > 0 && con->bytes > MaxRequestSize)
              {
                close(con->file);
                con->file = -1;
                unlink(con->filename);
                cupsdClearString(&con->filename);

                if (!cupsdSendError(con, HTTP_STATUS_REQUEST_TOO_LARGE, CUPSD_AUTH_NONE))
                {
                  cupsdCloseClient(con);
                  return;
                }
              }

              if (write(con->file, line, (size_t)bytes) < bytes)
	      {
        	cupsdLogClient(con, CUPSD_LOG_ERROR,
	                       "Unable to write %d bytes to \"%s\": %s",
                               bytes, con->filename, strerror(errno));

		close(con->file);
		con->file = -1;
		unlink(con->filename);
		cupsdClearString(&con->filename);

        	if (!cupsdSendError(con, HTTP_STATUS_REQUEST_TOO_LARGE,
		                    CUPSD_AUTH_NONE))
		{
		  cupsdCloseClient(con);
		  return;
		}
	      }
	    }
	    else if (httpGetState(con->http) == HTTP_STATE_POST_RECV)
              return;
	    else if (httpGetState(con->http) != HTTP_STATE_POST_SEND)
	    {
	      cupsdLogClient(con, CUPSD_LOG_DEBUG,
	                     "Closing on unexpected state %s.",
			     httpStateString(httpGetState(con->http)));
	      cupsdCloseClient(con);
	      return;
	    }
	  }
        }
	while (httpGetState(con->http) == HTTP_STATE_POST_RECV && httpGetReady(con->http));

	if (httpGetState(con->http) == HTTP_STATE_POST_SEND)
	{
	  if (con->file >= 0)
	  {
            struct stat    filestats;  /* File information */
	    fstat(con->file, &filestats);

	    close(con->file);
	    con->file = -1;

            if (filestats.st_size > MaxRequestSize &&
	        MaxRequestSize > 0)
	    {
	     /*
	      * Request is too big; remove it and send an error...
	      */

	      unlink(con->filename);
	      cupsdClearString(&con->filename);

	      if (con->request)
	      {
	       /*
	        * Delete any IPP request data...
		*/

	        ippDelete(con->request);
		con->request = NULL;
              }

              if (!cupsdSendError(con, HTTP_STATUS_REQUEST_TOO_LARGE, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }
	    }
	    else if (filestats.st_size == 0)
	    {
	     /*
	      * Don't allow empty file...
	      */

	      unlink(con->filename);
	      cupsdClearString(&con->filename);
	    }

	    if (con->command)
	    {
	      if (!cupsdSendCommand(con, con->command, con->options, 0, 0))
	      {
		if (!cupsdSendError(con, HTTP_STATUS_NOT_FOUND, CUPSD_AUTH_NONE))
		{
		  cupsdCloseClient(con);
		  return;
		}
              }
	      else
        	cupsdLogRequest(con, HTTP_STATUS_OK);
            }
	  }

          if (con->request)
	  {
	    cupsdProcessIPPRequest(con);

	    if (con->filename)
	    {
	      unlink(con->filename);
	      cupsdClearString(&con->filename);
	    }

	    return;
	  }
	}
        break;

    default :
        break; /* Anti-compiler-warning-code */
  }

  if (httpGetState(con->http) == HTTP_STATE_WAITING)
  {
    if (!httpGetKeepAlive(con->http))
    {
      cupsdLogClient(con, CUPSD_LOG_DEBUG,
                     "Closing because Keep-Alive is disabled.");
      cupsdCloseClient(con);
    }
    else
    {
      cupsArrayRemove(ActiveClients, con);
      cupsdSetBusyState(0);
    }
  }
}


/*
 * 'cupsdSendCommand()' - Send output from a command via HTTP.
 */

int					/* O - 1 on success, 0 on failure */
cupsdSendCommand(
    cupsd_client_t *con,		/* I - Client connection */
    char           *command,		/* I - Command to run */
    char           *options,		/* I - Command-line options */
    int            root,		/* I - Run as root? */
    int            percent_decode)      /* I - Percent decode the optoins string */
{
  int	fd;				/* Standard input file descriptor */


  if (con->filename)
  {
    fd = open(con->filename, O_RDONLY);

    if (fd < 0)
    {
      cupsdLogClient(con, CUPSD_LOG_ERROR,
                     "Unable to open \"%s\" for reading: %s",
                     con->filename ? con->filename : "/dev/null",
	             strerror(errno));
      return (0);
    }

    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
  }
  else
    fd = -1;

  con->pipe_pid    = pipe_command(con, fd, &(con->file), command, options, root, percent_decode);
  con->pipe_status = HTTP_STATUS_OK;

  httpClearFields(con->http);

  if (fd >= 0)
    close(fd);

  cupsdLogClient(con, CUPSD_LOG_INFO, "Started \"%s\" (pid=%d, file=%d)",
                 command, con->pipe_pid, con->file);

  if (con->pipe_pid == 0)
    return (0);

  fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);

  cupsdAddSelect(con->file, (cupsd_selfunc_t)write_pipe, NULL, con);

  cupsdLogClient(con, CUPSD_LOG_DEBUG, "Waiting for CGI data.");

  con->sent_header = 0;
  con->file_ready  = 0;
  con->got_fields  = 0;
  con->header_used = 0;

  return (1);
}


/*
 * 'cupsdSendError()' - Send an error message via HTTP.
 */

int					/* O - 1 if successful, 0 otherwise */
cupsdSendError(cupsd_client_t *con,	/* I - Connection */
               http_status_t  code,	/* I - Error code */
	       int            auth_type)/* I - Authentication type */
{
  char	location[HTTP_MAX_VALUE];	/* Location field */


  cupsdLogClient(con, CUPSD_LOG_DEBUG2, "cupsdSendError code=%d, auth_type=%d", code, auth_type);

#ifdef HAVE_SSL
 /*
  * Force client to upgrade for authentication if that is how the
  * server is configured...
  */

  if (code == HTTP_STATUS_UNAUTHORIZED &&
      DefaultEncryption == HTTP_ENCRYPTION_REQUIRED &&
      _cups_strcasecmp(httpGetHostname(con->http, NULL, 0), "localhost") &&
      !httpIsEncrypted(con->http))
  {
    code = HTTP_STATUS_UPGRADE_REQUIRED;
  }
#endif /* HAVE_SSL */

 /*
  * Put the request in the access_log file...
  */

  cupsdLogRequest(con, code);

 /*
  * To work around bugs in some proxies, don't use Keep-Alive for some
  * error messages...
  *
  * Kerberos authentication doesn't work without Keep-Alive, so
  * never disable it in that case.
  */

  strlcpy(location, httpGetField(con->http, HTTP_FIELD_LOCATION), sizeof(location));

  httpClearFields(con->http);
  httpClearCookie(con->http);

  httpSetField(con->http, HTTP_FIELD_LOCATION, location);

  if (code >= HTTP_STATUS_BAD_REQUEST && con->type != CUPSD_AUTH_NEGOTIATE)
    httpSetKeepAlive(con->http, HTTP_KEEPALIVE_OFF);

  if (httpGetVersion(con->http) >= HTTP_VERSION_1_1 &&
      httpGetKeepAlive(con->http) == HTTP_KEEPALIVE_OFF)
    httpSetField(con->http, HTTP_FIELD_CONNECTION, "close");

  if (code >= HTTP_STATUS_BAD_REQUEST)
  {
   /*
    * Send a human-readable error message.
    */

    char	message[4096],		/* Message for user */
		urltext[1024],		/* URL redirection text */
		redirect[1024];		/* Redirection link */
    const char	*text;			/* Status-specific text */


    redirect[0] = '\0';

    if (code == HTTP_STATUS_UNAUTHORIZED)
    {
      text = _cupsLangString(con->language,
                             _("Enter your username and password or the "
			       "root username and password to access this "
			       "page. If you are using Kerberos authentication, "
			       "make sure you have a valid Kerberos ticket."));
    }
    else if (code == HTTP_STATUS_FORBIDDEN)
    {
      if (con->username[0])
        text = _cupsLangString(con->language, _("Your account does not have the necessary privileges."));
      else
        text = _cupsLangString(con->language, _("You cannot access this page."));
    }
    else if (code == HTTP_STATUS_UPGRADE_REQUIRED)
    {
      text = urltext;

      snprintf(urltext, sizeof(urltext), _cupsLangString(con->language, _("You must access this page using the URL https://%s:%d%s.")), con->servername, con->serverport, con->uri);

      snprintf(redirect, sizeof(redirect), "<META HTTP-EQUIV=\"Refresh\" CONTENT=\"3;URL=https://%s:%d%s\">\n", con->servername, con->serverport, con->uri);
    }
    else if (code == HTTP_STATUS_CUPS_WEBIF_DISABLED)
      text = _cupsLangString(con->language,
                             _("The web interface is currently disabled. Run "
			       "\"cupsctl WebInterface=yes\" to enable it."));
    else
      text = "";

    snprintf(message, sizeof(message),
             "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" "
	     "\"http://www.w3.org/TR/html4/loose.dtd\">\n"
	     "<HTML>\n"
	     "<HEAD>\n"
             "\t<META HTTP-EQUIV=\"Content-Type\" "
	     "CONTENT=\"text/html; charset=utf-8\">\n"
	     "\t<TITLE>%s - " CUPS_SVERSION "</TITLE>\n"
	     "\t<LINK REL=\"STYLESHEET\" TYPE=\"text/css\" "
	     "HREF=\"/cups.css\">\n"
	     "%s"
	     "</HEAD>\n"
             "<BODY>\n"
	     "<H1>%s</H1>\n"
	     "<P>%s</P>\n"
	     "</BODY>\n"
	     "</HTML>\n",
	     _httpStatus(con->language, code), redirect,
	     _httpStatus(con->language, code), text);

   /*
    * Send an error message back to the client.  If the error code is a
    * 400 or 500 series, make sure the message contains some text, too!
    */

    size_t length = strlen(message);	/* Length of message */

    httpSetLength(con->http, length);

    if (!cupsdSendHeader(con, code, "text/html", auth_type))
      return (0);

    if (httpWrite2(con->http, message, length) < 0)
      return (0);

    if (httpFlushWrite(con->http) < 0)
      return (0);
  }
  else
  {
    httpSetField(con->http, HTTP_FIELD_CONTENT_LENGTH, "0");

    if (!cupsdSendHeader(con, code, NULL, auth_type))
      return (0);
  }

  return (1);
}


/*
 * 'cupsdSendHeader()' - Send an HTTP request.
 */

int					/* O - 1 on success, 0 on failure */
cupsdSendHeader(
    cupsd_client_t *con,		/* I - Client to send to */
    http_status_t  code,		/* I - HTTP status code */
    char           *type,		/* I - MIME type of document */
    int            auth_type)		/* I - Type of authentication */
{
  char		auth_str[1024];		/* Authorization string */


  cupsdLogClient(con, CUPSD_LOG_DEBUG, "cupsdSendHeader: code=%d, type=\"%s\", auth_type=%d", code, type, auth_type);

 /*
  * Send the HTTP status header...
  */

  if (code == HTTP_STATUS_CUPS_WEBIF_DISABLED)
  {
   /*
    * Treat our special "web interface is disabled" status as "200 OK" for web
    * browsers.
    */

    code = HTTP_STATUS_OK;
  }

  if (ServerHeader)
    httpSetField(con->http, HTTP_FIELD_SERVER, ServerHeader);

  if (code == HTTP_STATUS_METHOD_NOT_ALLOWED)
    httpSetField(con->http, HTTP_FIELD_ALLOW, "GET, HEAD, OPTIONS, POST, PUT");

  if (code == HTTP_STATUS_UNAUTHORIZED)
  {
    if (auth_type == CUPSD_AUTH_NONE)
    {
      if (!con->best || con->best->type <= CUPSD_AUTH_NONE)
	auth_type = cupsdDefaultAuthType();
      else
	auth_type = con->best->type;
    }

    auth_str[0] = '\0';

    if (auth_type == CUPSD_AUTH_BASIC)
    {
      strlcpy(auth_str, "Basic realm=\"CUPS\"", sizeof(auth_str));
    }
    else if (auth_type == CUPSD_AUTH_NEGOTIATE)
    {
#if defined(SO_PEERCRED) && defined(AF_LOCAL)
      if (httpAddrFamily(httpGetAddress(con->http)) == AF_LOCAL)
	strlcpy(auth_str, "PeerCred", sizeof(auth_str));
      else
#endif /* SO_PEERCRED && AF_LOCAL */
      strlcpy(auth_str, "Negotiate", sizeof(auth_str));
    }

    if (con->best && auth_type != CUPSD_AUTH_NEGOTIATE && !con->is_browser && !_cups_strcasecmp(httpGetHostname(con->http, NULL, 0), "localhost"))
    {
     /*
      * Add a "trc" (try root certification) parameter for local non-Kerberos
      * requests when the request requires system group membership - then the
      * client knows the root certificate can/should be used.
      *
      * Also, for macOS we also look for @AUTHKEY and add an "AuthRef key=foo"
      * method as needed...
      */

      char	*name,			/* Current user name */
		*auth_key;		/* Auth key buffer */
      size_t	auth_size;		/* Size of remaining buffer */
      int	need_local = 1;		/* Do we need to list "Local" method? */

      auth_key  = auth_str + strlen(auth_str);
      auth_size = sizeof(auth_str) - (size_t)(auth_key - auth_str);

#if defined(SO_PEERCRED) && defined(AF_LOCAL)
      if (httpAddrFamily(httpGetAddress(con->http)) == AF_LOCAL)
      {
        strlcpy(auth_key, ", PeerCred", auth_size);
        auth_key += 10;
        auth_size -= 10;
      }
#endif /* SO_PEERCRED && AF_LOCAL */

      for (name = (char *)cupsArrayFirst(con->best->names);
           name;
	   name = (char *)cupsArrayNext(con->best->names))
      {
        cupsdLogClient(con, CUPSD_LOG_DEBUG2, "cupsdSendHeader: require \"%s\"", name);

#ifdef HAVE_AUTHORIZATION_H
	if (!_cups_strncasecmp(name, "@AUTHKEY(", 9))
	{
	  snprintf(auth_key, auth_size, ", AuthRef key=\"%s\", Local trc=\"y\"", name + 9);
	  need_local = 0;
	  /* end parenthesis is stripped in conf.c */
	  break;
        }
	else
#endif /* HAVE_AUTHORIZATION_H */
	if (!_cups_strcasecmp(name, "@SYSTEM"))
	{
#ifdef HAVE_AUTHORIZATION_H
	  if (SystemGroupAuthKey)
	    snprintf(auth_key, auth_size, ", AuthRef key=\"%s\", Local trc=\"y\"", SystemGroupAuthKey);
          else
#endif /* HAVE_AUTHORIZATION_H */
	  strlcpy(auth_key, ", Local trc=\"y\"", auth_size);
	  need_local = 0;
	  break;
	}
      }

      if (need_local)
	strlcat(auth_key, ", Local", auth_size);
    }

    if (auth_str[0])
    {
      cupsdLogClient(con, CUPSD_LOG_DEBUG, "WWW-Authenticate: %s", auth_str);

      httpSetField(con->http, HTTP_FIELD_WWW_AUTHENTICATE, auth_str);
    }
  }

  if (con->language && strcmp(con->language->language, "C"))
    httpSetField(con->http, HTTP_FIELD_CONTENT_LANGUAGE, con->language->language);

  if (type)
  {
    if (!strcmp(type, "text/html"))
      httpSetField(con->http, HTTP_FIELD_CONTENT_TYPE, "text/html; charset=utf-8");
    else
      httpSetField(con->http, HTTP_FIELD_CONTENT_TYPE, type);
  }

  return (!httpWriteResponse(con->http, code));
}


/*
 * 'cupsdUpdateCGI()' - Read status messages from CGI scripts and programs.
 */

void
cupsdUpdateCGI(void)
{
  char		*ptr,			/* Pointer to end of line in buffer */
		message[1024];		/* Pointer to message text */
  int		loglevel;		/* Log level for message */


  while ((ptr = cupsdStatBufUpdate(CGIStatusBuffer, &loglevel,
                                   message, sizeof(message))) != NULL)
  {
    if (loglevel == CUPSD_LOG_INFO)
      cupsdLogMessage(CUPSD_LOG_INFO, "%s", message);

    if (!strchr(CGIStatusBuffer->buffer, '\n'))
      break;
  }

  if (ptr == NULL && !CGIStatusBuffer->bufused)
  {
   /*
    * Fatal error on pipe - should never happen!
    */

    cupsdLogMessage(CUPSD_LOG_CRIT,
                    "cupsdUpdateCGI: error reading from CGI error pipe - %s",
                    strerror(errno));
  }
}


/*
 * 'cupsdWriteClient()' - Write data to a client as needed.
 */

void
cupsdWriteClient(cupsd_client_t *con)	/* I - Client connection */
{
  int		bytes,			/* Number of bytes written */
		field_col;		/* Current column */
  char		*bufptr,		/* Pointer into buffer */
		*bufend;		/* Pointer to end of buffer */
  ipp_state_t	ipp_state;		/* IPP state value */


  cupsdLogClient(con, CUPSD_LOG_DEBUG, "con->http=%p", con->http);
  cupsdLogClient(con, CUPSD_LOG_DEBUG,
		 "cupsdWriteClient "
		 "error=%d, "
		 "used=%d, "
		 "state=%s, "
		 "data_encoding=HTTP_ENCODING_%s, "
		 "data_remaining=" CUPS_LLFMT ", "
		 "response=%p(%s), "
		 "pipe_pid=%d, "
		 "file=%d",
		 httpError(con->http), (int)httpGetReady(con->http),
		 httpStateString(httpGetState(con->http)),
		 httpIsChunked(con->http) ? "CHUNKED" : "LENGTH",
		 CUPS_LLCAST httpGetLength2(con->http),
		 con->response,
		 con->response ? ippStateString(ippGetState(con->request)) : "",
		 con->pipe_pid, con->file);

  if (httpGetState(con->http) != HTTP_STATE_GET_SEND &&
      httpGetState(con->http) != HTTP_STATE_POST_SEND)
  {
   /*
    * If we get called in the wrong state, then something went wrong with the
    * connection and we need to shut it down...
    */

    cupsdLogClient(con, CUPSD_LOG_DEBUG, "Closing on unexpected HTTP write state %s.",
		   httpStateString(httpGetState(con->http)));
    cupsdCloseClient(con);
    return;
  }

  if (con->pipe_pid)
  {
   /*
    * Make sure we select on the CGI output...
    */

    cupsdAddSelect(con->file, (cupsd_selfunc_t)write_pipe, NULL, con);

    cupsdLogClient(con, CUPSD_LOG_DEBUG, "Waiting for CGI data.");

    if (!con->file_ready)
    {
     /*
      * Try again later when there is CGI output available...
      */

      cupsdRemoveSelect(httpGetFd(con->http));
      return;
    }

    con->file_ready = 0;
  }

  bytes = (ssize_t)(sizeof(con->header) - (size_t)con->header_used);

  if (!con->pipe_pid && bytes > (ssize_t)httpGetRemaining(con->http))
  {
   /*
    * Limit GET bytes to original size of file (STR #3265)...
    */

    bytes = (ssize_t)httpGetRemaining(con->http);
  }

  if (con->response && con->response->state != IPP_STATE_DATA)
  {
    size_t wused = httpGetPending(con->http);	/* Previous write buffer use */

    do
    {
     /*
      * Write a single attribute or the IPP message header...
      */

      ipp_state = ippWrite(con->http, con->response);

     /*
      * If the write buffer has been flushed, stop buffering up attributes...
      */

      if (httpGetPending(con->http) <= wused)
        break;
    }
    while (ipp_state != IPP_STATE_DATA && ipp_state != IPP_STATE_ERROR);

    cupsdLogClient(con, CUPSD_LOG_DEBUG,
                   "Writing IPP response, ipp_state=%s, old "
                   "wused=" CUPS_LLFMT ", new wused=" CUPS_LLFMT,
                   ippStateString(ipp_state),
		   CUPS_LLCAST wused, CUPS_LLCAST httpGetPending(con->http));

    if (httpGetPending(con->http) > 0)
      httpFlushWrite(con->http);

    bytes = ipp_state != IPP_STATE_ERROR &&
	    (con->file >= 0 || ipp_state != IPP_STATE_DATA);

    cupsdLogClient(con, CUPSD_LOG_DEBUG,
                   "bytes=%d, http_state=%d, data_remaining=" CUPS_LLFMT,
                   (int)bytes, httpGetState(con->http),
                   CUPS_LLCAST httpGetLength2(con->http));
  }
  else if ((bytes = read(con->file, con->header + con->header_used, (size_t)bytes)) > 0)
  {
    con->header_used += bytes;

    if (con->pipe_pid && !con->got_fields)
    {
     /*
      * Inspect the data for Content-Type and other fields.
      */

      for (bufptr = con->header, bufend = con->header + con->header_used,
               field_col = 0;
           !con->got_fields && bufptr < bufend;
	   bufptr ++)
      {
        if (*bufptr == '\n')
	{
	 /*
	  * Send line to client...
	  */

	  if (bufptr > con->header && bufptr[-1] == '\r')
	    bufptr[-1] = '\0';
	  *bufptr++ = '\0';

          cupsdLogClient(con, CUPSD_LOG_DEBUG, "Script header: %s", con->header);

          if (!con->sent_header)
	  {
	   /*
	    * Handle redirection and CGI status codes...
	    */

	    http_field_t field;		/* HTTP field */
	    char	*value = strchr(con->header, ':');
					/* Value of field */

	    if (value)
	    {
	      *value++ = '\0';
	      while (isspace(*value & 255))
		value ++;
	    }

	    field = httpFieldValue(con->header);

	    if (field != HTTP_FIELD_UNKNOWN && value)
	    {
	      httpSetField(con->http, field, value);

	      if (field == HTTP_FIELD_LOCATION)
	      {
		con->pipe_status = HTTP_STATUS_SEE_OTHER;
		con->sent_header = 2;
	      }
	      else
	        con->sent_header = 1;
	    }
	    else if (!_cups_strcasecmp(con->header, "Status") && value)
	    {
  	      con->pipe_status = (http_status_t)atoi(value);
	      con->sent_header = 2;
	    }
	    else if (!_cups_strcasecmp(con->header, "Set-Cookie") && value)
	    {
	      httpSetCookie(con->http, value);
	      con->sent_header = 1;
	    }
	  }

         /*
	  * Update buffer...
	  */

	  con->header_used -= bufptr - con->header;

	  if (con->header_used > 0)
	    memmove(con->header, bufptr, (size_t)con->header_used);

	  bufptr = con->header - 1;

         /*
	  * See if the line was empty...
	  */

	  if (field_col == 0)
	  {
	    con->got_fields = 1;

	    if (httpGetVersion(con->http) == HTTP_VERSION_1_1 &&
		!httpGetField(con->http, HTTP_FIELD_CONTENT_LENGTH)[0])
	      httpSetLength(con->http, 0);

            cupsdLogClient(con, CUPSD_LOG_DEBUG, "Sending status %d for CGI.", con->pipe_status);

            if (con->pipe_status == HTTP_STATUS_OK)
	    {
	      if (!cupsdSendHeader(con, con->pipe_status, NULL, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }
	    }
	    else
	    {
	      if (!cupsdSendError(con, con->pipe_status, CUPSD_AUTH_NONE))
	      {
		cupsdCloseClient(con);
		return;
	      }
	    }
          }
	  else
	    field_col = 0;
	}
	else if (*bufptr != '\r')
	  field_col ++;
      }

      if (!con->got_fields)
        return;
    }

    if (con->header_used > 0)
    {
      if (httpWrite2(con->http, con->header, (size_t)con->header_used) < 0)
      {
	cupsdLogClient(con, CUPSD_LOG_DEBUG, "Closing for error %d (%s)",
		       httpError(con->http), strerror(httpError(con->http)));
	cupsdCloseClient(con);
	return;
      }

      if (httpIsChunked(con->http))
        httpFlushWrite(con->http);

      con->bytes += con->header_used;

      if (httpGetState(con->http) == HTTP_STATE_WAITING)
	bytes = 0;
      else
        bytes = con->header_used;

      con->header_used = 0;
    }
  }

  if (bytes <= 0 ||
      (httpGetState(con->http) != HTTP_STATE_GET_SEND &&
       httpGetState(con->http) != HTTP_STATE_POST_SEND))
  {
    if (!con->sent_header && con->pipe_pid)
      cupsdSendError(con, HTTP_STATUS_SERVER_ERROR, CUPSD_AUTH_NONE);
    else
    {
      cupsdLogRequest(con, HTTP_STATUS_OK);

      if (httpIsChunked(con->http) && (!con->pipe_pid || con->sent_header > 0))
      {
        cupsdLogClient(con, CUPSD_LOG_DEBUG, "Sending 0-length chunk.");

	if (httpWrite2(con->http, "", 0) < 0)
	{
	  cupsdLogClient(con, CUPSD_LOG_DEBUG, "Closing for error %d (%s)",
			 httpError(con->http), strerror(httpError(con->http)));
	  cupsdCloseClient(con);
	  return;
	}
      }

      cupsdLogClient(con, CUPSD_LOG_DEBUG, "Flushing write buffer.");
      httpFlushWrite(con->http);
      cupsdLogClient(con, CUPSD_LOG_DEBUG, "New state is %s", httpStateString(httpGetState(con->http)));
    }

    cupsdAddSelect(httpGetFd(con->http), (cupsd_selfunc_t)cupsdReadClient, NULL, con);

    cupsdLogClient(con, CUPSD_LOG_DEBUG, "Waiting for request.");

    if (con->file >= 0)
    {
      cupsdRemoveSelect(con->file);

      if (con->pipe_pid)
	cupsdEndProcess(con->pipe_pid, 0);

      close(con->file);
      con->file     = -1;
      con->pipe_pid = 0;
    }

    if (con->filename)
    {
      unlink(con->filename);
      cupsdClearString(&con->filename);
    }

    if (con->request)
    {
      ippDelete(con->request);
      con->request = NULL;
    }

    if (con->response)
    {
      ippDelete(con->response);
      con->response = NULL;
    }

    cupsdClearString(&con->command);
    cupsdClearString(&con->options);
    cupsdClearString(&con->query_string);

    if (!httpGetKeepAlive(con->http))
    {
      cupsdLogClient(con, CUPSD_LOG_DEBUG,
		     "Closing because Keep-Alive is disabled.");
      cupsdCloseClient(con);
      return;
    }
    else
    {
      cupsArrayRemove(ActiveClients, con);
      cupsdSetBusyState(0);
    }
  }
}


/*
 * 'check_if_modified()' - Decode an "If-Modified-Since" line.
 */

static int				/* O - 1 if modified since */
check_if_modified(
    cupsd_client_t *con,		/* I - Client connection */
    const struct stat    *filestats)		/* I - File information */
{
  const char	*ptr;			/* Pointer into field */
  time_t	date;			/* Time/date value */
  off_t		size;			/* Size/length value */


  size = 0;
  date = 0;
  ptr  = httpGetField(con->http, HTTP_FIELD_IF_MODIFIED_SINCE);

  if (*ptr == '\0')
    return (1);

  cupsdLogClient(con, CUPSD_LOG_DEBUG2, "check_if_modified: filestats=%p(" CUPS_LLFMT ", %d)) If-Modified-Since=\"%s\"", filestats, CUPS_LLCAST filestats->st_size, (int)filestats->st_mtime, ptr);

  while (*ptr != '\0')
  {
    while (isspace(*ptr) || *ptr == ';')
      ptr ++;

    if (_cups_strncasecmp(ptr, "length=", 7) == 0)
    {
      ptr += 7;
      size = strtoll(ptr, NULL, 10);

      while (isdigit(*ptr))
        ptr ++;
    }
    else if (isalpha(*ptr))
    {
      date = httpGetDateTime(ptr);
      while (*ptr != '\0' && *ptr != ';')
        ptr ++;
    }
    else
      ptr ++;
  }

  return ((size != filestats->st_size && size != 0) ||
          (date < filestats->st_mtime && date != 0) ||
	  (size == 0 && date == 0));
}


/*
 * 'compare_clients()' - Compare two client connections.
 */

static int				/* O - Result of comparison */
compare_clients(cupsd_client_t *a,	/* I - First client */
                cupsd_client_t *b,	/* I - Second client */
                void           *data)	/* I - User data (not used) */
{
  (void)data;

  if (a == b)
    return (0);
  else if (a < b)
    return (-1);
  else
    return (1);
}


#ifdef HAVE_SSL
/*
 * 'cupsd_start_tls()' - Start encryption on a connection.
 */

static int				/* O - 0 on success, -1 on error */
cupsd_start_tls(cupsd_client_t    *con,	/* I - Client connection */
                http_encryption_t e)	/* I - Encryption mode */
{
  if (httpEncryption(con->http, e))
  {
    cupsdLogClient(con, CUPSD_LOG_ERROR, "Unable to encrypt connection: %s",
                   cupsLastErrorString());
    return (-1);
  }

  cupsdLogClient(con, CUPSD_LOG_DEBUG, "Connection now encrypted.");
  return (0);
}
#endif /* HAVE_SSL */


///*
// * 'get_file()' - Get a filename and state info.
// */
//
//static char *				/* O  - Real filename */
//get_file2(cupsd_client_t *con,		/* I  - Client connection */
//         struct stat    *filestats,	/* O  - File information */
//         char           *filename,	/* IO - Filename buffer */
//         size_t         len)		/* I  - Buffer length */
//{
//  int		status;			/* Status of filesystem calls */
//  char		*ptr;			/* Pointer info filename */
//  size_t	plen;			/* Remaining length after pointer */
//  char		language[7],		/* Language subdirectory, if any */
//		dest[1024];		/* Destination name */
//  int		perm_check = 1;		/* Do permissions check? */
//  cupsd_printer_t *found_printer;			/* Printer */
//
//  CFLog(kCFLogLevelInfo, CFSTR("get_file called here, uri %s, serverroot %s"), con->uri, ServerRoot);
//
// /*
//  * Figure out the real filename...
//  */
//
//  filename[0] = '\0';
//  language[0] = '\0';
//
//  if (!strncmp(con->uri, "/help", 5) && (con->uri[5] == '/' || !con->uri[5]))
//  {
//   /*
//    * All help files are served by the help.cgi program...
//    */
//
//    return (NULL);
//  }
//  else if ((!strncmp(con->uri, "/ppd/", 5) || !strncmp(con->uri, "/printers/", 10) || !strncmp(con->uri, "/classes/", 9)) && !strcmp(con->uri + strlen(con->uri) - 4, ".ppd"))
//  {
//    strlcpy(dest, strchr(con->uri + 1, '/') + 1, sizeof(dest));
//    dest[strlen(dest) - 4] = '\0'; /* Strip .ppd */
//
//    if ((found_printer = cupsdFindDest(dest)) == NULL)
//    {
//      strlcpy(filename, "/", len);
//      cupsdLogClient(con, CUPSD_LOG_INFO, "No destination \"%s\" found.", dest);
//      return (NULL);
//    }
//
//    if (found_printer->type & CUPS_PRINTER_CLASS)
//    {
//      int i;				/* Looping var */
//
//      for (i = 0; i < found_printer->num_printers; i ++)
//      {
//	if (!(found_printer->printers[i]->type & CUPS_PRINTER_CLASS))
//	{
//	  snprintf(filename, len, "%s/ppd/%s.ppd", ServerRoot, found_printer->printers[i]->name);
//	  if (!access(filename, 0))
//	  {
//	    found_printer = found_printer->printers[i];
//	    break;
//	  }
//	}
//      }
//
//      if (i >= found_printer->num_printers)
//	found_printer = NULL;
//    }
//    else
//      snprintf(filename, len, "%s/ppd/%s.ppd", ServerRoot, found_printer->name);
//
//    perm_check = 0;
//  }
//  else if ((!strncmp(con->uri, "/icons/", 7) || !strncmp(con->uri, "/printers/", 10) || !strncmp(con->uri, "/classes/", 9)) && !strcmp(con->uri + strlen(con->uri) - 4, ".png"))
//  {
//    strlcpy(dest, strchr(con->uri + 1, '/') + 1, sizeof(dest));
//    dest[strlen(dest) - 4] = '\0'; /* Strip .png */
//
//    if ((found_printer = cupsdFindDest(dest)) == NULL)
//    {
//      strlcpy(filename, "/", len);
//      cupsdLogClient(con, CUPSD_LOG_INFO, "No destination \"%s\" found.", dest);
//      return (NULL);
//    }
//
//    if (found_printer->type & CUPS_PRINTER_CLASS)
//    {
//      int i;				/* Looping var */
//
//      for (i = 0; i < found_printer->num_printers; i ++)
//      {
//	if (!(found_printer->printers[i]->type & CUPS_PRINTER_CLASS))
//	{
//	  snprintf(filename, len, "%s/images/%s.png", CacheDir, found_printer->printers[i]->name);
//	  if (!access(filename, 0))
//	  {
//	    found_printer = found_printer->printers[i];
//	    break;
//	  }
//	}
//      }
//
//      if (i >= found_printer->num_printers)
//	found_printer = NULL;
//    }
//    else
//      snprintf(filename, len, "%s/images/%s.png", CacheDir, found_printer->name);
//
//    if (access(filename, F_OK) < 0)
//      snprintf(filename, len, "%s/images/generic.png", DocumentRoot);
//
//    perm_check = 0;
//  }
//  else if (!strncmp(con->uri, "/admin", 6) || !strncmp(con->uri, "/classes", 8) || !strncmp(con->uri, "/jobs", 5) || !strncmp(con->uri, "/printers", 9))
//  {
//   /*
//    * Admin/class/job/printer pages are served by CGI...
//    */
//
//    return (NULL);
//  }
//  else if (!strncmp(con->uri, "/rss/", 5) && !strchr(con->uri + 5, '/'))
//    snprintf(filename, len, "%s/rss/%s", CacheDir, con->uri + 5);
//  else if (!strncmp(con->uri, "/strings/", 9) && !strcmp(con->uri + strlen(con->uri) - 8, ".strings"))
//  {
//    strlcpy(dest, con->uri + 9, sizeof(dest));
//    dest[strlen(dest) - 8] = '\0';
//
//    if ((found_printer = cupsdFindDest(dest)) == NULL)
//    {
//      strlcpy(filename, "/", len);
//      cupsdLogClient(con, CUPSD_LOG_INFO, "No destination \"%s\" found.", dest);
//      return (NULL);
//    }
//
//    if (!found_printer->strings)
//    {
//      strlcpy(filename, "/", len);
//      cupsdLogClient(con, CUPSD_LOG_INFO, "No strings files for \"%s\".", dest);
//      return (NULL);
//    }
//
//    strlcpy(filename, found_printer->strings, len);
//
//    perm_check = 0;
//  }
//  else if (!strcmp(con->uri, "/admin/conf/cupsd.conf"))
//  {
//    strlcpy(filename, ConfigurationFile, len);
//
//    perm_check = 0;
//  }
//  else if (!strncmp(con->uri, "/admin/log/", 11))
//  {
//    if (!strncmp(con->uri + 11, "access_log", 10) && AccessLog[0] == '/')
//      strlcpy(filename, AccessLog, len);
//    else if (!strncmp(con->uri + 11, "error_log", 9) && ErrorLog[0] == '/')
//      strlcpy(filename, ErrorLog, len);
//    else if (!strncmp(con->uri + 11, "page_log", 8) && PageLog[0] == '/')
//      strlcpy(filename, PageLog, len);
//    else
//      return (NULL);
//
//    perm_check = 0;
//  }
//  else if (con->language)
//  {
//    snprintf(language, sizeof(language), "/%s", con->language->language);
//    snprintf(filename, len, "%s%s%s", DocumentRoot, language, con->uri);
//  }
//  else
//    snprintf(filename, len, "%s%s", DocumentRoot, con->uri);
//
//  if ((ptr = strchr(filename, '?')) != NULL)
//    *ptr = '\0';
//
// /*
//  * Grab the status for this language; if there isn't a language-specific file
//  * then fallback to the default one...
//  */
//
//  if ((status = lstat(filename, filestats)) != 0 && language[0] &&
//      strncmp(con->uri, "/icons/", 7) &&
//      strncmp(con->uri, "/ppd/", 5) &&
//      strncmp(con->uri, "/rss/", 5) &&
//      strncmp(con->uri, "/strings/", 9) &&
//      strncmp(con->uri, "/admin/conf/", 12) &&
//      strncmp(con->uri, "/admin/log/", 11))
//  {
//   /*
//    * Drop the country code...
//    */
//
//    language[3] = '\0';
//    snprintf(filename, len, "%s%s%s", DocumentRoot, language, con->uri);
//
//    if ((ptr = strchr(filename, '?')) != NULL)
//      *ptr = '\0';
//
//    if ((status = lstat(filename, filestats)) != 0)
//    {
//     /*
//      * Drop the language prefix and try the root directory...
//      */
//
//      language[0] = '\0';
//      snprintf(filename, len, "%s%s", DocumentRoot, con->uri);
//
//      if ((ptr = strchr(filename, '?')) != NULL)
//	*ptr = '\0';
//
//      status = lstat(filename, filestats);
//    }
//  }
//
// /*
//  * If we've found a symlink, 404 the sucker to avoid disclosing information.
//  */
//
//  if (!status && S_ISLNK(filestats->st_mode))
//  {
//    cupsdLogClient(con, CUPSD_LOG_INFO, "Symlinks such as \"%s\" are not allowed.", filename);
//    return (NULL);
//  }
//
// /*
//  * Similarly, if the file/directory does not have world read permissions, do
//  * not allow access...
//  */
//
//  if (!status && perm_check && !(filestats->st_mode & S_IROTH))
//  {
//    cupsdLogClient(con, CUPSD_LOG_INFO, "Files/directories such as \"%s\" must be world-readable.", filename);
//    return (NULL);
//  }
//
// /*
//  * If we've found a directory, get the index.html file instead...
//  */
//
//  if (!status && S_ISDIR(filestats->st_mode))
//  {
//   /*
//    * Make sure the URI ends with a slash...
//    */
//
//    if (con->uri[strlen(con->uri) - 1] != '/')
//      strlcat(con->uri, "/", sizeof(con->uri));
//
//   /*
//    * Find the directory index file, trying every language...
//    */
//
//    do
//    {
//      if (status && language[0])
//      {
//       /*
//        * Try a different language subset...
//	*/
//
//	if (language[3])
//	  language[0] = '\0';		/* Strip country code */
//	else
//	  language[0] = '\0';		/* Strip language */
//      }
//
//     /*
//      * Look for the index file...
//      */
//
//      snprintf(filename, len, "%s%s%s", DocumentRoot, language, con->uri);
//
//      if ((ptr = strchr(filename, '?')) != NULL)
//	*ptr = '\0';
//
//      ptr  = filename + strlen(filename);
//      plen = len - (size_t)(ptr - filename);
//
//      strlcpy(ptr, "index.html", plen);
//      status = lstat(filename, filestats);
//    }
//    while (status && language[0]);
//
//   /*
//    * If we've found a symlink, 404 the sucker to avoid disclosing information.
//    */
//
//    if (!status && S_ISLNK(filestats->st_mode))
//    {
//      cupsdLogClient(con, CUPSD_LOG_INFO, "Symlinks such as \"%s\" are not allowed.", filename);
//      return (NULL);
//    }
//
//   /*
//    * Similarly, if the file/directory does not have world read permissions, do
//    * not allow access...
//    */
//
//    if (!status && perm_check && !(filestats->st_mode & S_IROTH))
//    {
//      cupsdLogClient(con, CUPSD_LOG_INFO, "Files/directories such as \"%s\" must be world-readable.", filename);
//      return (NULL);
//    }
//  }
//
//  cupsdLogClient(con, CUPSD_LOG_DEBUG2, "get_file: filestats=%p, filename=%p, len=" CUPS_LLFMT ", returning \"%s\".", filestats, filename, CUPS_LLCAST len, status ? "(null)" : filename);
//  CFLog(kCFLogLevelInfo, CFSTR("get_file: filestats=%p, filename=%p, len=" CUPS_LLFMT ", returning \"%s\"."), filestats, filename, CUPS_LLCAST len, status ? "(null)" : filename);
//
//  if (status)
//    return (NULL);
//  else
//    return (filename);
//}
//
//extern const char* check_file(const char*, const char*, char*, size_t);
//
//static char *        /* O  - Real filename */
//get_file(cupsd_client_t *con,    /* I  - Client connection */
//          struct stat    *filestats,  /* O  - File information */
//          char           *filename,  /* IO - Filename buffer */
//          size_t         len)    /* I  - Buffer length */
//{
//  char tmp[MAXPATHLEN];
//  bzero(filename, len);
//  bzero(tmp, sizeof(tmp));
//
//  const char* check = check_file(con->uri, con->language->language, tmp, sizeof(tmp));
//  const char* result = get_file2(con, filestats, filename, len);
//
//  if (check == NULL && result == NULL) {
//    return NULL;
//  }
//
//  if (check == NULL && result != NULL) {
//    CFLog(kCFLogLevelInfo, CFSTR("## GETFILECHECK [%s] -> { WE FAIL, THEM '%s' }"), con->uri, result);
//    const char* check2 = check_file(con->uri, con->language->language, tmp, sizeof(tmp));
//    const char* result2 = get_file2(con, filestats, filename, len);
//  } else if (check != NULL && result == NULL) {
//    CFLog(kCFLogLevelInfo, CFSTR("## GETFILECHECK [%s] -> { '%s', THEY FAIL }"), con->uri, check);
//    const char* check2 = check_file(con->uri, con->language->language, tmp, sizeof(tmp));
//    const char* result2 = get_file2(con, filestats, filename, len);
//  } else if (strcmp(check, result) != 0) {
//    CFLog(kCFLogLevelInfo, CFSTR("## GETFILECHECK [%s] -> DIFFER { WE '%s', THEY '%s' }"), con->uri, check, result);
//    const char* check2 = check_file(con->uri, con->language->language, tmp, sizeof(tmp));
//    const char* result2 = get_file2(con, filestats, filename, len);
//  }
//
//  return result;
//}

/*
 * 'install_cupsd_conf()' - Install a configuration file.
 */

static http_status_t			/* O - Status */
install_cupsd_conf(cupsd_client_t *con)	/* I - Connection */
{
  char		filename[1024];		/* Configuration filename */
  cups_file_t	*in,			/* Input file */
		*out;			/* Output file */
  char		buffer[16384];		/* Copy buffer */
  ssize_t	bytes;			/* Number of bytes */


 /*
  * Open the request file...
  */

  if ((in = cupsFileOpen(con->filename, "rb")) == NULL)
  {
    cupsdLogClient(con, CUPSD_LOG_ERROR, "Unable to open request file \"%s\": %s",
                    con->filename, strerror(errno));
    goto server_error;
  }

 /*
  * Open the new config file...
  */

  if ((out = cupsdCreateConfFile(ConfigurationFile, ConfigFilePerm)) == NULL)
  {
    cupsFileClose(in);
    goto server_error;
  }

  cupsdLogClient(con, CUPSD_LOG_INFO, "Installing config file \"%s\"...",
                  ConfigurationFile);

 /*
  * Copy from the request to the new config file...
  */

  while ((bytes = cupsFileRead(in, buffer, sizeof(buffer))) > 0)
    if (cupsFileWrite(out, buffer, (size_t)bytes) < bytes)
    {
      cupsdLogClient(con, CUPSD_LOG_ERROR,
                      "Unable to copy to config file \"%s\": %s",
        	      ConfigurationFile, strerror(errno));

      cupsFileClose(in);
      cupsFileClose(out);

      snprintf(filename, sizeof(filename), "%s.N", ConfigurationFile);
      cupsdUnlinkOrRemoveFile(filename);

      goto server_error;
    }

 /*
  * Close the files...
  */

  cupsFileClose(in);

  if (cupsdCloseCreatedConfFile(out, ConfigurationFile))
    goto server_error;

 /*
  * Remove the request file...
  */

  cupsdUnlinkOrRemoveFile(con->filename);
  cupsdClearString(&con->filename);

 /*
  * Set the NeedReload flag...
  */

  NeedReload = RELOAD_CUPSD;
  ReloadTime = time(NULL);

 /*
  * Return that the file was created successfully...
  */

  return (HTTP_STATUS_CREATED);

 /*
  * Common exit for errors...
  */

  server_error:

  cupsdUnlinkOrRemoveFile(con->filename);
  cupsdClearString(&con->filename);

  return (HTTP_STATUS_SERVER_ERROR);
}


///*
// * 'is_cgi()' - Is the resource a CGI script/program?
// */
//
//static int				/* O - 1 = CGI, 0 = file */
//is_cgi(cupsd_client_t *con,		/* I - Client connection */
//       const char     *filename,	/* I - Real filename */
//       struct stat    *filestats,	/* I - File information */
//       mime_type_t    *type)		/* I - MIME type */
//{
//  const char	*options;		/* Options on URL */
//
//
// /*
//  * Get the options, if any...
//  */
//
//  if ((options = strchr(con->uri, '?')) != NULL)
//  {
//    options ++;
//    cupsdSetStringf(&(con->query_string), "QUERY_STRING=%s", options);
//  }
//
// /*
//  * Check for known types...
//  */
//
//  if (!type || _cups_strcasecmp(type->super, "application"))
//  {
//    cupsdLogClient(con, CUPSD_LOG_DEBUG2, "is_cgi: filename=\"%s\", filestats=%p, type=%s/%s, returning 0.", filename, filestats, type ? type->super : "unknown", type ? type->type : "unknown");
//    return (0);
//  }
//
//  if (!_cups_strcasecmp(type->type, "x-httpd-cgi") &&
//      (filestats->st_mode & 0111))
//  {
//   /*
//    * "application/x-httpd-cgi" is a CGI script.
//    */
//
//    cupsdSetString(&con->command, filename);
//
//    if (options)
//      cupsdSetStringf(&con->options, " %s", options);
//
//    cupsdLogClient(con, CUPSD_LOG_DEBUG2, "is_cgi: filename=\"%s\", filestats=%p, type=%s/%s, returning 1.", filename, filestats, type->super, type->type);
//    return (1);
//  }
//
//  cupsdLogClient(con, CUPSD_LOG_DEBUG2, "is_cgi: filename=\"%s\", filestats=%p, type=%s/%s, returning 0.", filename, filestats, type->super, type->type);
//  return (0);
//}


/*
 * 'is_path_absolute()' - Is a path absolute and free of relative elements (i.e. "..").
 */

static int				/* O - 0 if relative, 1 if absolute */
is_path_absolute(const char *path)	/* I - Input path */
{
 /*
  * Check for a leading slash...
  */

  if (path[0] != '/')
    return (0);

 /*
  * Check for "<" or quotes in the path and reject since this is probably
  * someone trying to inject HTML...
  */

  if (strchr(path, '<') != NULL || strchr(path, '\"') != NULL || strchr(path, '\'') != NULL)
    return (0);

 /*
  * Check for "/.." in the path...
  */

  while ((path = strstr(path, "/..")) != NULL)
  {
    if (!path[3] || path[3] == '/')
      return (0);

    path ++;
  }

 /*
  * If we haven't found any relative paths, return 1 indicating an
  * absolute path...
  */

  return (1);
}


/*
 * 'pipe_command()' - Pipe the output of a command to the remote client.
 */

static int				/* O - Process ID */
pipe_command(cupsd_client_t *con,	/* I - Client connection */
             int            infile,	/* I - Standard input for command */
             int            *outfile,	/* O - Standard output for command */
	     char           *command,	/* I - Command to run */
	     char           *options,	/* I - Options for command (already percent decoded) */
	     int            root,       /* I - Run as root? */
         int            percent_decode) /* I - Should we percent decode the string after all? */
{
  int		i;			/* Looping var */
  int		pid;			/* Process ID */
  char		*commptr,		/* Command string pointer */
		commch;			/* Command string character */
  char		*uriptr;		/* URI string pointer */
  int		fds[2];			/* Pipe FDs */
  int		argc;			/* Number of arguments */
  int		envc;			/* Number of environment variables */
  char		argbuf[10240],		/* Argument buffer */
		*argv[100],		/* Argument strings */
		*envp[MAX_ENV + 20];	/* Environment variables */
  char		auth_type[256],		/* AUTH_TYPE environment variable */
		content_length[1024],	/* CONTENT_LENGTH environment variable */
		content_type[1024],	/* CONTENT_TYPE environment variable */
		http_cookie[32768],	/* HTTP_COOKIE environment variable */
		http_referer[1024],	/* HTTP_REFERER environment variable */
		http_user_agent[1024],	/* HTTP_USER_AGENT environment variable */
		lang[1024],		/* LANG environment variable */
		path_info[1024],	/* PATH_INFO environment variable */
		remote_addr[1024],	/* REMOTE_ADDR environment variable */
		remote_host[1024],	/* REMOTE_HOST environment variable */
		remote_user[1024],	/* REMOTE_USER environment variable */
		script_filename[1024],	/* SCRIPT_FILENAME environment variable */
		script_name[1024],	/* SCRIPT_NAME environment variable */
		server_name[1024],	/* SERVER_NAME environment variable */
		server_port[1024];	/* SERVER_PORT environment variable */
  ipp_attribute_t *attr;		/* attributes-natural-language attribute */


 /*
  * Parse a copy of the options string, which is of the form:
  *
  *     argument+argument+argument
  *     ?argument+argument+argument
  *     param=value&param=value
  *     ?param=value&param=value
  *     /name?argument+argument+argument
  *     /name?param=value&param=value
  *
  * If the string contains an "=" character after the initial name,
  * then we treat it as a HTTP GET form request and make a copy of
  * the remaining string for the environment variable.
  *
  * The string is always parsed out as command-line arguments, to
  * be consistent with Apache...
  *
  * Note that this assumes %xx decoding has already taken place on the options string.
  */

  cupsdLogClient(con, CUPSD_LOG_DEBUG2, "pipe_command: infile=%d, outfile=%p, command=\"%s\", options=\"%s\", root=%d", infile, outfile, command, options ? options : "(null)", root);

  argv[0] = command;

  if (options)
    strlcpy(argbuf, options, sizeof(argbuf));
  else
    argbuf[0] = '\0';

  if (argbuf[0] == '/')
  {
   /*
    * Found some trailing path information, set PATH_INFO...
    */

    if ((commptr = strchr(argbuf, '?')) == NULL)
      commptr = argbuf + strlen(argbuf);

    commch   = *commptr;
    *commptr = '\0';
    snprintf(path_info, sizeof(path_info), "PATH_INFO=%s", argbuf);
    *commptr = commch;
  }
  else
  {
    commptr      = argbuf;
    path_info[0] = '\0';

    if (*commptr == ' ')
      commptr ++;
  }

  if (*commptr == '?' && con->operation == HTTP_STATE_GET && !con->query_string)
  {
    commptr ++;
    cupsdSetStringf(&(con->query_string), "QUERY_STRING=%s", commptr);
  }

  argc = 1;

  if (*commptr)
  {
    argv[argc ++] = commptr;

    for (; *commptr && argc < 99; commptr ++)
    {
     /*
      * Break arguments whenever we see a + or space...
      */

      if (*commptr == ' ' || *commptr == '+')
      {
	while (*commptr == ' ' || *commptr == '+')
	  *commptr++ = '\0';

       /*
	* If we don't have a blank string, save it as another argument...
	*/

	if (*commptr)
	{
	  argv[argc] = commptr;
	  argc ++;
	}
	else
	  break;
      }
      else if (percent_decode && *commptr == '%' && isxdigit(commptr[1] & 255) &&
               isxdigit(commptr[2] & 255))
      {
       /*
	* Convert the %xx notation to the individual character.
	*/

	if (commptr[1] >= '0' && commptr[1] <= '9')
          *commptr = (char)((commptr[1] - '0') << 4);
	else
          *commptr = (char)((tolower(commptr[1]) - 'a' + 10) << 4);

	if (commptr[2] >= '0' && commptr[2] <= '9')
          *commptr |= commptr[2] - '0';
	else
          *commptr |= tolower(commptr[2]) - 'a' + 10;

	_cups_strcpy(commptr + 1, commptr + 3);

       /*
	* Check for a %00 and break if that is the case...
	*/

	if (!*commptr)
          break;
      }
    }
  }

  argv[argc] = NULL;

 /*
  * Setup the environment variables as needed...
  */

  if (con->username[0])
  {
    snprintf(auth_type, sizeof(auth_type), "AUTH_TYPE=%s",
             httpGetField(con->http, HTTP_FIELD_AUTHORIZATION));

    if ((uriptr = strchr(auth_type + 10, ' ')) != NULL)
      *uriptr = '\0';
  }
  else
    auth_type[0] = '\0';

  if (con->request && (attr = ippFindAttribute(con->request, "attributes-natural-language", IPP_TAG_LANGUAGE)) != NULL)
  {
    cups_lang_t *language = cupsLangGet(ippGetString(attr, 0, NULL));

    snprintf(lang, sizeof(lang), "LANG=%s.UTF8", language->language);
    cupsLangFree(language);
  }
  else if (con->language)
    snprintf(lang, sizeof(lang), "LANG=%s.UTF8", con->language->language);
  else
    strlcpy(lang, "LANG=C", sizeof(lang));

  strlcpy(remote_addr, "REMOTE_ADDR=", sizeof(remote_addr));
  httpAddrString(httpGetAddress(con->http), remote_addr + 12,
                 sizeof(remote_addr) - 12);

  snprintf(remote_host, sizeof(remote_host), "REMOTE_HOST=%s",
           httpGetHostname(con->http, NULL, 0));

  snprintf(script_name, sizeof(script_name), "SCRIPT_NAME=%s", con->uri);
  if ((uriptr = strchr(script_name, '?')) != NULL)
    *uriptr = '\0';

  snprintf(script_filename, sizeof(script_filename), "SCRIPT_FILENAME=%s%s",
           DocumentRoot, script_name + 12);

  snprintf(server_port, sizeof(server_port), "SERVER_PORT=%d", con->serverport);

  if (httpGetField(con->http, HTTP_FIELD_HOST)[0])
  {
    char *nameptr;			/* Pointer to ":port" */

    snprintf(server_name, sizeof(server_name), "SERVER_NAME=%s",
	     httpGetField(con->http, HTTP_FIELD_HOST));
    if ((nameptr = strrchr(server_name, ':')) != NULL && !strchr(nameptr, ']'))
      *nameptr = '\0';			/* Strip trailing ":port" */
  }
  else
    snprintf(server_name, sizeof(server_name), "SERVER_NAME=%s",
	     con->servername);

  envc = cupsdLoadEnv(envp, (int)(sizeof(envp) / sizeof(envp[0])));

  if (auth_type[0])
    envp[envc ++] = auth_type;

  envp[envc ++] = lang;
  envp[envc ++] = "REDIRECT_STATUS=1";
  envp[envc ++] = "GATEWAY_INTERFACE=CGI/1.1";
  envp[envc ++] = server_name;
  envp[envc ++] = server_port;
  envp[envc ++] = remote_addr;
  envp[envc ++] = remote_host;
  envp[envc ++] = script_name;
  envp[envc ++] = script_filename;

  if (path_info[0])
    envp[envc ++] = path_info;

  if (con->username[0])
  {
    snprintf(remote_user, sizeof(remote_user), "REMOTE_USER=%s", con->username);

    envp[envc ++] = remote_user;
  }

  if (httpGetVersion(con->http) == HTTP_VERSION_1_1)
    envp[envc ++] = "SERVER_PROTOCOL=HTTP/1.1";
  else if (httpGetVersion(con->http) == HTTP_VERSION_1_0)
    envp[envc ++] = "SERVER_PROTOCOL=HTTP/1.0";
  else
    envp[envc ++] = "SERVER_PROTOCOL=HTTP/0.9";

  if (httpGetCookie(con->http))
  {
    snprintf(http_cookie, sizeof(http_cookie), "HTTP_COOKIE=%s",
             httpGetCookie(con->http));
    envp[envc ++] = http_cookie;
  }

  if (httpGetField(con->http, HTTP_FIELD_USER_AGENT)[0])
  {
    snprintf(http_user_agent, sizeof(http_user_agent), "HTTP_USER_AGENT=%s",
             httpGetField(con->http, HTTP_FIELD_USER_AGENT));
    envp[envc ++] = http_user_agent;
  }

  if (httpGetField(con->http, HTTP_FIELD_REFERER)[0])
  {
    snprintf(http_referer, sizeof(http_referer), "HTTP_REFERER=%s",
             httpGetField(con->http, HTTP_FIELD_REFERER));
    envp[envc ++] = http_referer;
  }

  if (con->operation == HTTP_STATE_GET)
  {
    envp[envc ++] = "REQUEST_METHOD=GET";

    if (con->query_string)
    {
     /*
      * Add GET form variables after ?...
      */

      envp[envc ++] = con->query_string;
    }
    else
      envp[envc ++] = "QUERY_STRING=";
  }
  else
  {
    sprintf(content_length, "CONTENT_LENGTH=" CUPS_LLFMT,
            CUPS_LLCAST con->bytes);
    snprintf(content_type, sizeof(content_type), "CONTENT_TYPE=%s",
             httpGetField(con->http, HTTP_FIELD_CONTENT_TYPE));

    envp[envc ++] = "REQUEST_METHOD=POST";
    envp[envc ++] = content_length;
    envp[envc ++] = content_type;
  }

 /*
  * Tell the CGI if we are using encryption...
  */

  if (httpIsEncrypted(con->http))
    envp[envc ++] = "HTTPS=ON";

 /*
  * Terminate the environment array...
  */

  envp[envc] = NULL;

  if (LogLevel >= CUPSD_LOG_DEBUG)
  {
    for (i = 0; i < argc; i ++)
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "[CGI] argv[%d] = \"%s\"", i, argv[i]);
    for (i = 0; i < envc; i ++)
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "[CGI] envp[%d] = \"%s\"", i, envp[i]);
  }

 /*
  * Create a pipe for the output...
  */

  if (cupsdOpenPipe(fds))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "[CGI] Unable to create pipe for %s - %s",
                    argv[0], strerror(errno));
    return (0);
  }

 /*
  * Then execute the command...
  */

  if (cupsdStartProcess(command, argv, envp, infile, fds[1], CGIPipes[1],
			-1, -1, root, DefaultProfile, NULL, &pid) < 0)
  {
   /*
    * Error - can't fork!
    */

    cupsdLogMessage(CUPSD_LOG_ERROR, "[CGI] Unable to start %s - %s", argv[0],
                    strerror(errno));

    cupsdClosePipe(fds);
    pid = 0;
  }
  else
  {
   /*
    * Fork successful - return the PID...
    */

    if (con->username[0])
      cupsdAddCert(pid, con->username, con->type);

    cupsdLogMessage(CUPSD_LOG_DEBUG, "[CGI] Started %s (PID %d)", command, pid);

    *outfile = fds[0];
    close(fds[1]);
  }

  return (pid);
}


/*
 * 'valid_host()' - Is the Host: field valid?
 */

static int				/* O - 1 if valid, 0 if not */
valid_host(cupsd_client_t *con)		/* I - Client connection */
{
  cupsd_alias_t	*a;			/* Current alias */
  cupsd_netif_t	*netif;			/* Current network interface */
  const char	*end;			/* End character */
  char		*ptr;			/* Pointer into host value */


 /*
  * Copy the Host: header for later use...
  */

  strlcpy(con->clientname, httpGetField(con->http, HTTP_FIELD_HOST),
          sizeof(con->clientname));
  if ((ptr = strrchr(con->clientname, ':')) != NULL && !strchr(ptr, ']'))
  {
    *ptr++ = '\0';
    con->clientport = atoi(ptr);
  }
  else
    con->clientport = con->serverport;

 /*
  * Then validate...
  */

  if (httpAddrLocalhost(httpGetAddress(con->http)))
  {
   /*
    * Only allow "localhost" or the equivalent IPv4 or IPv6 numerical
    * addresses when accessing CUPS via the loopback interface...
    */

    return (!_cups_strcasecmp(con->clientname, "localhost") ||
	    !_cups_strcasecmp(con->clientname, "localhost.") ||
            !strcmp(con->clientname, "127.0.0.1") ||
	    !strcmp(con->clientname, "[::1]"));
  }

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
 /*
  * Check if the hostname is something.local (Bonjour); if so, allow it.
  */

  if ((end = strrchr(con->clientname, '.')) != NULL && end > con->clientname &&
      !end[1])
  {
   /*
    * "." on end, work back to second-to-last "."...
    */

    for (end --; end > con->clientname && *end != '.'; end --);
  }

  if (end && (!_cups_strcasecmp(end, ".local") ||
	      !_cups_strcasecmp(end, ".local.")))
    return (1);
#endif /* HAVE_DNSSD || HAVE_AVAHI */

 /*
  * Check if the hostname is an IP address...
  */

  if (isdigit(con->clientname[0] & 255) || con->clientname[0] == '[')
  {
   /*
    * Possible IPv4/IPv6 address...
    */

    http_addrlist_t *addrlist;		/* List of addresses */


    if ((addrlist = httpAddrGetList(con->clientname, AF_UNSPEC, NULL)) != NULL)
    {
     /*
      * Good IPv4/IPv6 address...
      */

      httpAddrFreeList(addrlist);
      return (1);
    }
  }

 /*
  * Check for (alias) name matches...
  */

  for (a = (cupsd_alias_t *)cupsArrayFirst(ServerAlias);
       a;
       a = (cupsd_alias_t *)cupsArrayNext(ServerAlias))
  {
   /*
    * "ServerAlias *" allows all host values through...
    */

    if (!strcmp(a->name, "*"))
      return (1);

    if (!_cups_strncasecmp(con->clientname, a->name, a->namelen))
    {
     /*
      * Prefix matches; check the character at the end - it must be "." or nul.
      */

      end = con->clientname + a->namelen;

      if (!*end || (*end == '.' && !end[1]))
        return (1);
    }
  }

#if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  for (a = (cupsd_alias_t *)cupsArrayFirst(DNSSDAlias);
       a;
       a = (cupsd_alias_t *)cupsArrayNext(DNSSDAlias))
  {
   /*
    * "ServerAlias *" allows all host values through...
    */

    if (!strcmp(a->name, "*"))
      return (1);

    if (!_cups_strncasecmp(con->clientname, a->name, a->namelen))
    {
     /*
      * Prefix matches; check the character at the end - it must be "." or nul.
      */

      end = con->clientname + a->namelen;

      if (!*end || (*end == '.' && !end[1]))
        return (1);
    }
  }
#endif /* HAVE_DNSSD || HAVE_AVAHI */

 /*
  * Check for interface hostname matches...
  */

  for (netif = (cupsd_netif_t *)cupsArrayFirst(NetIFList);
       netif;
       netif = (cupsd_netif_t *)cupsArrayNext(NetIFList))
  {
    if (!_cups_strncasecmp(con->clientname, netif->hostname, netif->hostlen))
    {
     /*
      * Prefix matches; check the character at the end - it must be "." or nul.
      */

      end = con->clientname + netif->hostlen;

      if (!*end || (*end == '.' && !end[1]))
        return (1);
    }
  }

  return (0);
}


///*
// * 'write_file()' - Send a file via HTTP.
// */
//
//static int				/* O - 0 on failure, 1 on success */
//write_file(cupsd_client_t *con,		/* I - Client connection */
//           http_status_t  code,		/* I - HTTP status */
//	   char           *filename,	/* I - Filename */
//	   char           *type,	/* I - File type */
//	   struct stat    *filestats)	/* O - File information */
//{
//  con->file = open(filename, O_RDONLY, 0600);
//
//  cupsdLogClient(con, CUPSD_LOG_DEBUG2, "write_file: code=%d, filename=\"%s\" (%d), type=\"%s\", filestats=%p.", code, filename, con->file, type ? type : "(null)", filestats);
//
//  if (con->file < 0)
//    return (0);
//
//  fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);
//
//  con->pipe_pid    = 0;
//  con->sent_header = 1;
//
//  httpClearFields(con->http);
//
//  httpSetLength(con->http, (size_t)filestats->st_size);
//
//  httpSetField(con->http, HTTP_FIELD_LAST_MODIFIED,
//	       httpGetDateString(filestats->st_mtime));
//
//  if (!cupsdSendHeader(con, code, type, CUPSD_AUTH_NONE))
//    return (0);
//
//  cupsdAddSelect(httpGetFd(con->http), NULL, (cupsd_selfunc_t)cupsdWriteClient, con);
//
//  cupsdLogClient(con, CUPSD_LOG_DEBUG, "Sending file.");
//
//  return (1);
//}
//

/*
 * 'write_pipe()' - Flag that data is available on the CGI pipe.
 */

static void
write_pipe(cupsd_client_t *con)		/* I - Client connection */
{
  cupsdLogClient(con, CUPSD_LOG_DEBUG2, "write_pipe: CGI output on fd %d.", con->file);

  con->file_ready = 1;

  cupsdRemoveSelect(con->file);
  cupsdAddSelect(httpGetFd(con->http), NULL, (cupsd_selfunc_t)cupsdWriteClient, con);

  cupsdLogClient(con, CUPSD_LOG_DEBUG, "CGI data ready to be sent.");
}

/*
 * Rooted File -
 */

static void rf_closefile(rf_file_t* file)
{
  if (file->_fd != -1) {
    close(file->_fd);
  }
  bzero(file, sizeof(*file));
  file->_disposition = RF_MISSING;
  file->_fd = -1;
}

static mime_type_t *        /* O - Type of file */
rf_mimeFileType(mime_t     *mime,    /* I - MIME database */
             const rf_file_t* file)  /* O - Is the file compressed? */
{
  const char* fileName = strrchr(file->_filename, '/');
  if (fileName == NULL)
    fileName = file->_filename;
  else {
    ++fileName;
  }
  return mimeFileTypeFD(mime, file->_fd, fileName);
}

static int
rf_is_cgi(cupsd_client_t* con, const rf_file_t* file, mime_type_t *type)
{
  const char  *options;    /* Options on URL */

  /*
   * Get the options, if any...
   */

  if ((options = strchr(con->uri, '?')) != NULL)
  {
    options ++;
    cupsdSetStringf(&(con->query_string), "QUERY_STRING=%s", options);
  }

  /*
   * Check for known types...
   */

  if (!type || _cups_strcasecmp(type->super, "application"))
  {
//    CFLog(kCFLogLevelInfo, CFSTR("is_cgi: filename=\"%s\", type=%s/%s, returning 0."), file->filename, type ? type->super : "unknown", type ? type->type : "unknown");
    return (0);
  }

  if (!_cups_strcasecmp(type->type, "x-httpd-cgi") && (file->_filestats.st_mode & 0111))
  {
    /*
     * "application/x-httpd-cgi" is a CGI script.
     */

    cupsdSetString(&con->command, file->_filename);

    if (options)
      cupsdSetStringf(&con->options, " %s", options);

//    CFLog(kCFLogLevelInfo, CFSTR("is_cgi: filename=\"%s\", type=%s/%s, returning 1."), file->filename, type->super, type->type);
    return (1);
  }

//  CFLog(kCFLogLevelInfo, CFSTR("is_cgi: filename=\"%s\", type=%s/%s, returning 0."), file->filename, type->super, type->type);
  return (0);
}

static int
rf_check_if_modified(cupsd_client_t* con, const rf_file_t* file)
{
  return check_if_modified(con, &file->_filestats);
}

static int
rf_write_file(cupsd_client_t* con, http_status_t code, const rf_file_t* file, char* type)
{
  con->file = dup(file->_fd);

  cupsdLogClient(con, CUPSD_LOG_DEBUG2, "rf_write_file: code=%d, filename=\"%s\", fd=%d from %d, type=\"%s\"", code, file->_filename, con->file, file->_fd, type ? type : "(null)");

  if (con->file < 0)
    return (0);

  struct stat stat2;

  if (fstat(con->file, &stat2) == 0) {
    if ((file->_filestats.st_dev != stat2.st_dev) || (file->_filestats.st_ino != stat2.st_ino)) {
      cupsdLogClient(con, CUPSD_LOG_DEBUG2, "write_file: filestats different (%d %d)", (int) file->_filestats.st_ino, (int) stat2.st_ino);
      close(con->file);
      con->file = -1;
      return (0);
    }
  }


  fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);

  con->pipe_pid    = 0;
  con->sent_header = 1;

  httpClearFields(con->http);

  httpSetLength(con->http, (size_t)file->_filestats.st_size);

  httpSetField(con->http, HTTP_FIELD_LAST_MODIFIED,
               httpGetDateString(file->_filestats.st_mtime));

  if (!cupsdSendHeader(con, code, type, CUPSD_AUTH_NONE))
    return (0);

  cupsdAddSelect(httpGetFd(con->http), NULL, (cupsd_selfunc_t)cupsdWriteClient, con);

  cupsdLogClient(con, CUPSD_LOG_DEBUG, "Sending file.");

  return (1);
}

//#define ppLog(fmt, ...) fprintf(stderr, fmt "\n",__VA_ARGS__)
#define ppLog(fmt, ...) do { cupsdLogClient(NULL, CUPSD_LOG_DEBUG, fmt,__VA_ARGS__); } while (0)

/*
 path to a full file (log or conf)
 root + partial + filename + suffix
 root + partial + [ lang ]
 */


enum rooted_path_location {
  ROOTED_Invalid = 0,

  ROOTED_ServerRoot,
  ROOTED_DocumentRoot,
  ROOTED_ConfigurationFile,
  ROOTED_CacheDir,
  ROOTED_AccessLog,
  ROOTED_ErrorLog,
  ROOTED_PageLog,
};

enum rooted_path_kind {
  ROOTED_PATH_INVALID,
  ROOTED_PATH_ONLY,
  ROOTED_PATH_FULL,
  ROOTED_PATH_PARTIAL,
  ROOTED_PATH_LANG_INDEX,
  ROOTED_PATH_MULTI
};

struct _rooted_path_parts {
  enum rooted_path_location _root_path;
  char _partial[MAXPATHLEN];
};

struct _rooted_path_parts_lang {
  enum rooted_path_location _root_path;
  char _partial[MAXPATHLEN];
  char _language[32];
};

struct _rooted_path_s {
  enum rooted_path_kind _kind;
  union {
    enum rooted_path_location _root;
    char _full_path[MAXPATHLEN];
    struct _rooted_path_parts _partialParts;
    struct _rooted_path_parts_lang _partialPartsLang;
    cups_array_t* _multiPaths;
  } _u;
};
typedef struct _rooted_path_s rooted_path_t;

static rooted_path_t* pathNew_Full(const char* path);
static rooted_path_t* pathNew_Rooted(enum rooted_path_location root);
static rooted_path_t* pathNew_Partial(enum rooted_path_location root, const char* dir, const char* name);
static rooted_path_t* pathNew_PartialSuffix(enum rooted_path_location root, const char* dir, const char* name, const char* suffix);

static void pathDelete(rooted_path_t*);

static int pathResolveAndOpen(rooted_path_t* p, struct stat* sb, char* dst, size_t dstLen);

static void* _path_copy(void* e, void* ignored)
{
  (void) ignored;
  rooted_path_t* p = (rooted_path_t*) e;
  rooted_path_t* cpy = (rooted_path_t*) calloc(1, sizeof(rooted_path_t));
  cpy->_kind = p->_kind;

  if (p->_kind == ROOTED_PATH_MULTI) {
    cpy->_u._multiPaths = cupsArrayDup(p->_u._multiPaths);
  } else {
    memcpy(&cpy->_u, &p->_u, sizeof(p->_u));
  }

  return (void*) cpy;
}

static void _path_delete(void* e, void* ignored)
{
  (void) ignored;
  rooted_path_t* p = (rooted_path_t*) e;
  pathDelete(p);
}

static rooted_path_t* pathNew(enum rooted_path_kind kind)
{
  rooted_path_t* p = (rooted_path_t*) calloc(1, sizeof(rooted_path_t));
  p->_kind = kind;
  if (kind == ROOTED_PATH_MULTI) {
    p->_u._multiPaths = cupsArrayNew3(NULL, NULL, NULL, 0, _path_copy, _path_delete);
  }
  return p;
}

static const char* pathRoot(enum rooted_path_location root)
{
  switch (root) {
    default:
      return NULL;
#define XX(yy)  case ROOTED_##yy: return yy
      XX(ServerRoot);
      XX(DocumentRoot);
      XX(ConfigurationFile);
      XX(CacheDir);
      XX(AccessLog);
      XX(ErrorLog);
      XX(PageLog);
#undef XX
  }
}

#if DEBUG
static char* pathDebugStr(rooted_path_t* p)
{
  if (p == NULL)
    return strdup("NULL PATH (pass through)");
  char* s = NULL;
  switch (p->_kind) {
    case ROOTED_PATH_INVALID:
      asprintf(&s, "Path<%p> { Invalid }", p);
      break;
    case ROOTED_PATH_ONLY:
      asprintf(&s, "Path<%p> { Rooted '%s' }", p, pathRoot(p->_u._root));
      break;
    case ROOTED_PATH_FULL:
      asprintf(&s, "Path<%p> { Full '%s' }", p, p->_u._full_path);
      break;
    case ROOTED_PATH_PARTIAL:
      asprintf(&s, "Path<%p> { Partial '%s', '%s' }", p, pathRoot(p->_u._partialParts._root_path), p->_u._partialParts._partial);
      break;
    case ROOTED_PATH_LANG_INDEX:
      asprintf(&s, "Path<%p> { PartialLangIndex '%s', '%s', '%s' }", p, pathRoot(p->_u._partialPartsLang._root_path), p->_u._partialPartsLang._partial, p->_u._partialPartsLang._language);
      break;
    case ROOTED_PATH_MULTI:
      asprintf(&s, "Path<%p> { Multi '%d' }", p, cupsArrayCount(p->_u._multiPaths));
      break;
  }
  return s;
}
#endif

static int _full_resolveAndOpen(const char* fullPath, struct stat* sb, char* dst, size_t dstLen)
{
  int fd = open(fullPath, O_RDONLY);

  if (fd == -1) {
    ppLog("FPR: can't open file '%s'", fullPath);
    return -1;
  }

  int ok = fstat(fd, sb) == 0;

  if (! ok) {
    ppLog("FPR: can't stat open file fd %d path %s err %d/%s", fd, fullPath, errno, strerror(errno));
  }

  if (ok) {
    ok = S_ISREG(sb->st_mode);
    if (! ok) {
      ppLog("FPR: open file fd %d path %s is not a regular file: %x", fd, fullPath, sb->st_mode);
    }
  }

  if (! ok) {
    close(fd);
    return -1;
  }

  strlcpy(dst, fullPath, dstLen);

  return fd;
}

static int _root_resolveAndOpen(enum rooted_path_location root, struct stat* sb, char* dst, size_t dstLen)
{
  return _full_resolveAndOpen(pathRoot(root), sb, dst, dstLen);
}

static int _partial_resolveAndOpen(const struct _rooted_path_parts* pp, struct stat* sb, char* dst, size_t dstLen)
{
  int root_fd = open(pathRoot(pp->_root_path), O_RDONLY | O_DIRECTORY);
  if (root_fd == -1) {
    ppLog("PPR: Can't open root '%s' %d/%s", pathRoot(pp->_root_path), errno, strerror(errno));
    return -1;
  }

  int fd = openat(root_fd, pp->_partial, O_RDONLY | O_NOFOLLOW);

  int serr = errno;
  close(root_fd);
  if (fd == -1) {
    ppLog("PPR: Can't open partial '%s' / '%s' %d/%s", pathRoot(pp->_root_path), pp->_partial, serr, strerror(serr));
    return -1;
  }
  if (fstat(fd, sb) != 0) {
    ppLog("PPR: Can't fstat partial '%s' / '%s' %d/%s", pathRoot(pp->_root_path), pp->_partial, errno, strerror(errno));
    close(fd);
    return -1;
  }
  snprintf(dst, dstLen, "%s/%s", pathRoot(pp->_root_path), pp->_partial);
  return fd;
}

static int permissions_check_file(const struct stat* sb)
{
  if (S_ISDIR(sb->st_mode)) {
    return 0;
  }

  if (S_ISLNK(sb->st_mode)) {
    return 0;
  }

  if ((sb->st_mode & S_IROTH) == 0) {
    return 0;
  }

  return 1;
}

static int _partial_lang_resolveAndOpen(struct _rooted_path_parts_lang* pp, struct stat* sb, char* dst, size_t dstLen)
{
  // start with _root_path
  // get to _root_path / _partial
  // -> if its a file, we can return it
  // -> if a directory, search for lang_loc/index.html, lang/index.html or index.html

  rooted_path_t* pDir = pathNew_Partial(pp->_root_path, NULL, pp->_partial);
  int fd = pathResolveAndOpen(pDir, sb, dst, dstLen);
  pathDelete(pDir);

  if (fd != -1) {
    if (permissions_check_file(sb)) {
      ppLog("PLR: Found an ok file: %s", dst);
      return fd;
    }

    // going to try again
    close(fd);
  }

  // If we got here, then we're looking at a directory - so we need to probe around the languages
  // for index.html.  First XX_YY/index.html, then XX/index.html, finally index.html
  char lang_dir[MAXPATHLEN];
  strlcpy(lang_dir, pp->_language, sizeof(lang_dir));
  for (int i = 0;  i < 3;  i++) {
    switch (i) {
      case 0:
        break;
      case 1:
        lang_dir[2] = '\0';
        break;
      case 2:
        lang_dir[0] = '\0';
        break;
    }

    char index_name[MAXPATHLEN];
    if (strlen(lang_dir) == 0)
      strlcpy(index_name, "index.html", sizeof(index_name));
    else {
      snprintf(index_name, sizeof(index_name), "%s/%s", lang_dir, "index.html");
    }

    rooted_path_t* p = pathNew_Partial(pp->_root_path, pp->_partial, index_name);
    fd = pathResolveAndOpen(p, sb, dst, dstLen);
    pathDelete(p);

    if (fd != -1) {
      ppLog("PLR: Found a file candidate - %s", dst);

      if (permissions_check_file(sb)) {
        ppLog("PLR: using this file - %s", dst);
        return fd;
      }

     ppLog("PLR: File %s is not a regular file!", dst);
     close(fd);
    }

    if (strlen(lang_dir) == 0) {
      break;
    }
  }

  return -1;
}

static int _multi_resolveAndOpen(cups_array_t* arr, struct stat* sb, char* dst, size_t dstLen)
{
  for (void* e = cupsArrayFirst(arr);
       e != NULL;
       e = cupsArrayNext(arr)) {
    rooted_path_t* p = (rooted_path_t*) e;

    int fd = pathResolveAndOpen(p, sb, dst, dstLen);
    if (fd != -1) {
      ppLog("MPR: found fd %d path %s", fd, dst);
      return fd;
    }
  }

  return -1;
}

static int pathResolveAndOpen(rooted_path_t* p, struct stat* sb, char* dst, size_t dstLen)
{
  switch (p->_kind) {
    case ROOTED_PATH_INVALID:
      return -1;

    case ROOTED_PATH_ONLY:
      return _root_resolveAndOpen(p->_u._root, sb, dst, dstLen);

    case ROOTED_PATH_FULL:
      return _full_resolveAndOpen(p->_u._full_path, sb, dst, dstLen);

    case ROOTED_PATH_PARTIAL:
      return _partial_resolveAndOpen(&p->_u._partialParts, sb, dst, dstLen);

    case ROOTED_PATH_LANG_INDEX:
      return _partial_lang_resolveAndOpen(&p->_u._partialPartsLang, sb, dst, dstLen);

    case ROOTED_PATH_MULTI:
      return _multi_resolveAndOpen(p->_u._multiPaths, sb, dst, dstLen);
  }

  return -1;
}

static void pathDelete(rooted_path_t* p)
{
  if (p->_kind == ROOTED_PATH_MULTI) {
    cupsArrayDelete(p->_u._multiPaths);
  }
  free(p);
}

static rooted_path_t* pathNew_Full(const char* path)
{
  rooted_path_t* p = pathNew(ROOTED_PATH_FULL);
  strlcpy(p->_u._full_path, path, sizeof(p->_u._full_path));
  return p;
}

static rooted_path_t* pathNew_Rooted(enum rooted_path_location root)
{
  rooted_path_t* p = pathNew(ROOTED_PATH_ONLY);
  p->_u._root = root;
  return p;
}

static rooted_path_t* pathNew_Partial(enum rooted_path_location root, const char* dir, const char* name)
{
  rooted_path_t* p = pathNew(ROOTED_PATH_PARTIAL);
  struct _rooted_path_parts* pu = &p->_u._partialParts;

  pu->_root_path = root;

  if (dir == NULL || dir[0] == '\0')
    strlcpy(pu->_partial, name, sizeof(pu->_partial));
  else {
    snprintf(pu->_partial, sizeof(pu->_partial), "%s/%s", dir, name);
  }

  return p;
}

static rooted_path_t* pathNew_PartialSuffix(enum rooted_path_location root, const char* dir, const char* name, const char* suffix)
{
  char tmp[MAXPATHLEN];
  snprintf(tmp, sizeof(tmp), "%s%s", name, suffix);
  return pathNew_Partial(root, dir, tmp);
}

static rooted_path_t* pathNew_PartialLang(enum rooted_path_location root, const char* partial, const char* lang)
{
  rooted_path_t* p = pathNew(ROOTED_PATH_LANG_INDEX);
  struct _rooted_path_parts_lang* pu = &p->_u._partialPartsLang;

  pu->_root_path = root;
  if (partial && partial[0] == '/')
    ++partial;
  strlcpy(pu->_partial, partial, sizeof(pu->_partial));
  if (lang == NULL)
    pu->_language[0] = 0;
  else {
    strlcpy(pu->_language, lang, sizeof(pu->_language));
  }

  return p;
}

static void pathAddMulti(rooted_path_t* p, rooted_path_t* e)
{
  cupsArrayAdd(p->_u._multiPaths, e);
}

static rooted_path_t* newPrinterPaths(enum rooted_path_location root, const char* uri, const char* dir, const char* suffix)
{
  char dest[1024];
  strlcpy(dest, strchr(uri + 1, '/') + 1, sizeof(dest));
  dest[strlen(dest) - strlen(suffix)] = '\0'; /* Strip suffix */

  cupsd_printer_t* found = cupsdFindDest(dest);
  if (found == NULL) {
    return NULL;
  }

  rooted_path_t* multi = pathNew(ROOTED_PATH_MULTI);

  if ((found->type & CUPS_PRINTER_CLASS) == 0) {
    rooted_path_t* pp = pathNew_PartialSuffix(root, dir, found->name, suffix);
    pathAddMulti(multi, pp);
    pathDelete(pp);
  }  else {
    for (int i = 0; i < found->num_printers; i ++) {
      if ((found->printers[i]->type & CUPS_PRINTER_CLASS) == 0) {
        rooted_path_t* pp = pathNew_PartialSuffix(root, dir, found->printers[i]->name, suffix);
        pathAddMulti(multi, pp);
        pathDelete(pp);
      }
    }
  }

  return multi;
}

/*
 * basic replacement for the existing get_file
 */
static rooted_path_t* get_file_paths(const char* uri, const char* lang)
{
  /*
   * Figure out the real filename...
   */

  if (!strncmp(uri, "/help", 5) && (uri[5] == '/' || !uri[5])) {
    /*
     * All help files are served by the help.cgi program...
     */

    return (NULL);
  }

  int is_ppd = 0 == strncmp(uri, "/ppd/", 5);
  int is_prn = 0 == strncmp(uri, "/printers/", 10);
  int is_cls = 0 == strncmp(uri, "/classes/", 9);
  int is_icn = 0 == strncmp(uri, "/icons/", 7);
  int is_rss = 0 == strncmp(uri, "/rss/", 5);
  int is_str = 0 == strncmp(uri, "/strings/", 9);
  int is_cnf = 0 == strncmp(uri, "/admin/conf/", 12);
  int is_log = 0 == strncmp(uri, "/admin/log/", 11);

  const char* suffix = strchr(uri, '.');

  int is_ppd_suffix = (suffix && 0 == strcmp(suffix, ".ppd"));
  int is_png_suffix = (suffix && 0 == strcmp(suffix, ".png"));
  int is_str_suffix = (suffix && 0 == strcmp(suffix, ".strings"));

  if (is_ppd_suffix && (is_ppd || is_prn || is_cls)) {
    rooted_path_t* multi = newPrinterPaths(ROOTED_ServerRoot, uri, "ppd", ".ppd");
    if (multi == NULL) {
      ppLog("No destination found for \"%s\".", uri);
      return pathNew(ROOTED_PATH_INVALID);
    }
    return multi;
  } else if (is_png_suffix && (is_icn || is_prn || is_cls))  {
    rooted_path_t* multi = newPrinterPaths(ROOTED_CacheDir, uri, "images", ".png");
    if (multi == NULL) {
      ppLog("No destination found for \"%s\".", uri);
      return pathNew(ROOTED_PATH_INVALID);
    }
    rooted_path_t* genericPath = pathNew_Partial(ROOTED_DocumentRoot, "images", "generic.png");
    pathAddMulti(multi, genericPath);
    pathDelete(genericPath);
    return multi;
  } else if (!strncmp(uri, "/admin", 6) || !strncmp(uri, "/classes", 8) || !strncmp(uri, "/jobs", 5) || !strncmp(uri, "/printers", 9)) {
    /*
     * Admin/class/job/printer pages are served by CGI...
     */

    return NULL;
  } else if (is_rss && !strchr(uri + 5, '/')) {
    return pathNew_Partial(ROOTED_CacheDir, "rss", (uri + 5));
  } else if (is_str && is_str_suffix) {
    char dest[1024];
    strlcpy(dest, uri + 9, sizeof(dest));
    dest[strlen(dest) - 8] = '\0';

    cupsd_printer_t* found = cupsdFindDest(dest);

    if (found == NULL) {
      ppLog("No destination \"%s\" found.", dest);
      return pathNew(ROOTED_PATH_INVALID);
    }

    if ((found->type & CUPS_PRINTER_CLASS) != 0) {
      ppLog("No strings files for class \"%s\".", dest);
      return pathNew(ROOTED_PATH_INVALID);
    }

    if (found->strings == NULL) {
      ppLog("No strings files for \"%s\".", dest);
      return pathNew(ROOTED_PATH_INVALID);
    }

    return pathNew_Full(found->strings);
  }
  else if (is_cnf && (0 == strcmp(uri, "/admin/conf/cupsd.conf"))) {
    // based on the existing get_file, this case never gets hit - consumed bin /admin going to CGI above...
    // so kept here for consistency...
    return pathNew_Rooted(ROOTED_ConfigurationFile);
  }  else if (is_log) {
    enum rooted_path_location where = ROOTED_Invalid;

    if (!strncmp(uri + 11, "access_log", 10) && AccessLog[0] == '/')
      where = ROOTED_AccessLog;
    else if (!strncmp(uri + 11, "error_log", 9) && ErrorLog[0] == '/')
      where = ROOTED_ErrorLog;
    else if (!strncmp(uri + 11, "page_log", 8) && PageLog[0] == '/')
      where = ROOTED_PageLog;

    if (where == ROOTED_Invalid)
      return pathNew(ROOTED_PATH_INVALID);
    else {
      return pathNew_Rooted(where);
    }
  }  else {
    return pathNew_PartialLang(ROOTED_DocumentRoot, uri, lang);
  }

  return NULL;
}

static void
rf_get_file(cupsd_client_t* con, rf_file_t* file)
{
  bzero(file, sizeof(*file));
  file->_disposition = RF_MISSING;
  file->_fd = -1;

  rooted_path_t* p = get_file_paths(con->uri, con->language == NULL? NULL : con->language->language);

  if (p == NULL) {
    // file should be handled as a cgi
    file->_disposition = RF_PASS_ON;
    return;
  }

  file->_fd = pathResolveAndOpen(p, &file->_filestats, file->_filename, sizeof(file->_filename));

#if DEBUG
  {
    char* s = pathDebugStr(p);
    ppLog("rf_get_file: fd %d for %s", file->_fd, s);
    free(s);
  }
#endif

  pathDelete(p);

  if (file->_fd == -1) {
    // file should have been there,
    // or it was there, but we don't want to use it (its a symlink or something)
    file->_disposition = RF_MISSING;
    return;
  }

  file->_disposition = RF_FOUND;
}
