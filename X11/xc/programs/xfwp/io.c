/* $Xorg: io.c,v 1.4 2001/02/09 02:05:45 xorgcvs Exp $ */
/*

Copyright "1986-1997, 1998 The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and the following permission notice
shall be included in all copies of the Software:

THE SOFTWARE IS PROVIDED "AS IS ", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
AND NON-INFRINGEMENT. IN NO EVENT SHALL THE OPEN GROUP BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER SIABILITIY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN
CONNNECTION WITH THE SOFTWARE OR THE USE OF OTHER DEALINGS IN
THE SOFTWARE.

Except as contained in this notice, the name of The Open Group
shall not be used in advertising or otherwise to promote the use
or other dealings in this Software without prior written
authorization from The Open Group.

X Window System is a trademark of The Open Group.

*/
/* $XFree86: xc/programs/xfwp/io.c,v 1.11 2001/12/14 20:01:43 dawes Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <X11/Xos.h>		/* Needed here for SunOS */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <assert.h>

#include <X11/X.h>
#include <X11/Xproto.h>
#include <X11/Xfuncs.h>

#include <X11/ICE/ICElib.h>

#include "xfwp.h"
#include "misc.h"
#include "pm.h"
#include "transport.h"
#include "io.h"


/*
 * Error messages returned to clients who are denied access
 */
static char * server_reason[2] = {
	"Authentication rejected", 
	"permission denied"
}; 


static void
RemoveFDFromServerListenArray (
    int			fd_counter, 
    fd_set		* rinit,
    int			num_servers)
{
    /*
     * File descriptor fd_counter had a read failure.  If this fd
     * is associated with an Xserver, then we assume the Xserver is
     * un-usable (e.g. has died) thus we should no longer listen for
     * for connections on the client listen port associated with
     * this Xserver.
     */

     int 		i;

     for (i = 0; i < num_servers; i++)
     {
	 if (server_array[i] != NULL &&
	     server_array[i]->server_fd == fd_counter)
	 {
             FD_CLR (server_array[i]->client_listen_fd, rinit);
	     (void) close (server_array[i]->client_listen_fd);
	     if (server_array[i]->x_server_hostport)
		 free ((char *) server_array[i]->x_server_hostport);
	     if (server_array[i]->listen_port_string)
		 free ((char *) server_array[i]->listen_port_string);
	     free ((char *) server_array[i]);
	     server_array[i] = NULL;
	     break;
	 }
     }
}

static void
doProcessWritables(
    int 			fd_counter,
    fd_set 			* rinit, 
    fd_set 			* winit)
{
    int 			bytes_written; 
    int 			remainder;

    /*
     * start off by writing from the selected fd to its connection
     * partner 
     */
    if (client_conn_array[fd_counter]->wbytes)
    {
	/*
	 * See how much you manage to write
	 */
	bytes_written = write (fd_counter, 
			       client_conn_array[fd_counter]->writebuf,
		   	       client_conn_array[fd_counter]->wbytes);
	/*
	 * handle some common error conditions
	 */
	if (bytes_written == -1)
	{
	    /*
	     * no process attached to the other end of this socket
	     */	
	    if (errno == EPIPE)
	    {
	        (void) fprintf (stderr, "write error - EPIPE\n");
		perror("socket write");
	    }
	    /*
	     * clean up
	     */
	    FD_CLR(fd_counter, rinit);
	    FD_CLR(fd_counter, winit);
	    (void) close (fd_counter);

	    if (client_conn_array[fd_counter]->conn_to != -1)
	    {
	      FD_CLR(client_conn_array[fd_counter]->conn_to, rinit);
	      FD_CLR(client_conn_array[fd_counter]->conn_to, winit);
	      (void) close (client_conn_array[fd_counter]->conn_to);
	    }  

	    client_conn_array[client_conn_array[fd_counter]->conn_to]->conn_to 
			= -1;
	    client_conn_array[fd_counter]->conn_to = -1;
	    if (client_conn_array[fd_counter]->source)
		free(client_conn_array[fd_counter]->source);
	    if (client_conn_array[fd_counter]->destination)
		free(client_conn_array[fd_counter]->destination);
	    free(client_conn_array[fd_counter]);
	    client_conn_array[fd_counter] = NULL;
	    return;
	} else
	{
	    /*
	     * no errors on write, but did you write everything in the buffer?
	     */
	    remainder = client_conn_array[fd_counter]->wbytes - bytes_written;
	    if (remainder)
	    {
		/*
		 * move any remainder to front of writebuffer and adjust
		 * writebuf byte counter
		 */
		bcopy(client_conn_array[fd_counter]->writebuf + bytes_written,
		      client_conn_array[fd_counter]->writebuf,
		      remainder);
		client_conn_array[fd_counter]->wbytes = remainder;
	    } else
		/*
		 * writebuffer *must* be empty, so zero byte counter
		 */ 
		client_conn_array[fd_counter]->wbytes = 0;

	    /*
	     * take this opportunity to get more read data, if any present
	     */
	    if ((client_conn_array[fd_counter]->conn_to != -1) &&
	        (client_conn_array[client_conn_array[fd_counter]->conn_to]->rbytes))
		 doCopyFromTo(client_conn_array[fd_counter]->conn_to, 
			      fd_counter, 
			      rinit, 
			      winit);

	    /*
	     * If the above operation didn't put anything on the writebuffer,
	     * and the readables handler marked the fd ready to close, then
	     * clean up and close the connection; otherwise, simply clear the
	     * fd_set mask for this fd
	     */
	    if (client_conn_array[fd_counter]->wbytes == 0)
	    {
		if (client_conn_array[fd_counter]->wclose == 1)
		{
		    FD_CLR(fd_counter, rinit);
		    client_conn_array[fd_counter]->conn_to = -1;
		    client_conn_array[fd_counter]->wclose = 0;
		    (void) close (fd_counter);
		    if (client_conn_array[fd_counter]->source)
			free(client_conn_array[fd_counter]->source);
		    if (client_conn_array[fd_counter]->destination)
			free(client_conn_array[fd_counter]->destination);
		    free(client_conn_array[fd_counter]);
		    client_conn_array[fd_counter] = NULL;
		}
		FD_CLR(fd_counter, winit);
	    }
	    /*
	     * since we just wrote data to the conn_to fd, mark it as ready 
	     * to check for reading when we go through select() the next time
	     */
	    if (client_conn_array[fd_counter] != NULL)
		if (client_conn_array[fd_counter]->conn_to != -1)
		    FD_SET(client_conn_array[fd_counter]->conn_to, rinit);
	} /* end else no errors on write  */
    } else
    {
	/*
	 * There was nothing to write on this fd (can't see how we'd get
	 * here if select() returned this fd as writable, but it's in 
	 * XForward so who am I to say?!) 
	 */
	if ((client_conn_array[fd_counter]->conn_to != -1) &&
	    (client_conn_array[client_conn_array[fd_counter]->conn_to]->rbytes))
	{
	    doCopyFromTo (client_conn_array[fd_counter]->conn_to, 
			  fd_counter,
			  rinit, 
			  winit);
	    /*
	     * if you got anything to write, then proceed to next 
	     * iter of select()
	     */
	    if (client_conn_array[fd_counter]->wbytes)
		return;
	}

	/*
	 * You didn't get anything from that copy; check to see if it was
	 * because the readables handler marked the fd closed; if so, 
	 * close this association; otherwise, simply clear the fd_set 
	 * writable mask for this fd 
	 */ 
	if (client_conn_array[fd_counter]->wclose)
	{
	    FD_CLR(fd_counter, rinit);
	    client_conn_array[fd_counter]->conn_to = -1;
	    client_conn_array[fd_counter]->wclose = 0;
	    (void) close (fd_counter);
	    if (client_conn_array[fd_counter]->source)
		free(client_conn_array[fd_counter]->source);
	    if (client_conn_array[fd_counter]->destination)
		free(client_conn_array[fd_counter]->destination);
	    free(client_conn_array[fd_counter]);
	    client_conn_array[fd_counter] = NULL;
	}
	FD_CLR(fd_counter, winit);
    }
}

static void
ProcessNewPMConnection (
    int				* nfds,
    fd_set			* rinit,
    struct config		* config_info,
    IceListenObj		** listen_objects,
    int				listen_fd)

{
    IceConn			new_ice_conn;
    IceAcceptStatus  		accept_status;
    int				temp_sock_fd;
    IceListenObj *		temp_obj;
    struct timeval		time_val;
    struct timezone		time_zone;
    struct sockaddr_in		temp_sockaddr_in;
    struct sockaddr_in		server_sockaddr_in;
    int				retval;
    int				addrlen = sizeof(temp_sockaddr_in);
    int				rule_number;
    int				pm_idx;

    /*
     * Only continue if there is room for another PM connection.
     */
    for (pm_idx = 0; pm_idx < config_info->num_pm_conns; pm_idx++)
    {
      if (!pm_conn_array[pm_idx])
        break;
    }
    if (pm_idx >= config_info->num_pm_conns)
    {
      (void) fprintf (stderr, 
	             "Maximum number of PM connections has been reached (%d)\n",
		     config_info->num_pm_conns);
      
      /*
       * Must accept and then close this connection or the PM will
       * continue to poll.
       */
      temp_obj = *listen_objects; 
      new_ice_conn = IceAcceptConnection(temp_obj[listen_fd], &accept_status);
      if (new_ice_conn)
        IceCloseConnection(new_ice_conn);

      return;
    }

    /*
     * accept the connection if you can, use pm_listen_array 
     * index to index into ICE listen_object list (this is because the
     * listen_objects list must correspond to the pm_listen_array)
     */
    temp_obj = *listen_objects; 
    new_ice_conn = IceAcceptConnection(temp_obj[listen_fd], &accept_status);
    if (!new_ice_conn)
    { 
        static int	been_here;

        /*
         * ICE initialization (bug?) makes this happen the
         * first time readables is hit.
         */
        if (!been_here)
            been_here++;
        else
           (void) fprintf(stderr, "IceAcceptConnection failed (%d)\n",
	  	          accept_status);
       return;
    }

    /*
     * extract the fd from this new connection; remember, the fd of
     * the listen socket is *not* the fd of the actual connection!
     */
    temp_sock_fd = IceConnectionNumber(new_ice_conn); 

    /*
     * before we get any further, do a config check on the new ICE
     * connection; start by using getpeername() to get endpoint info
     */
    retval = getpeername(temp_sock_fd, 
			 (struct sockaddr*)&temp_sockaddr_in, 
			 (void *)&addrlen);
    if (retval)
    {
        IceCloseConnection(new_ice_conn);
        (void) fprintf(stderr, "getpeername call failed\n");
	return;
    }

    assert(temp_sockaddr_in.sin_family == AF_INET);
    
    /*
     * Do a configuration check.  NOTE:  we're not doing anything
     * with the server_sockaddr_in argument 
     */
    if ((doConfigCheck(&temp_sockaddr_in, 
		       &server_sockaddr_in,
		       config_info,
		       PMGR,
		       &rule_number)) == FAILURE)
    {
        /*
         * close the PM connection 
         * 
         */
        (void) fprintf(stderr, "PM failed config check\n");
        IceCloseConnection(new_ice_conn);
        return;
    }

    /*	
     * you've started the connection process; allocate a buffer
     * for this connection, then continue processing other fd's without 
     * blocking while waiting to read the coming PM data; [NOTE:
     * we use the fd of the connection socket as index into the
     * pm_conn_array; this saves us much troublesome linked-list
     * management!]
     */
    if ((pm_conn_array[pm_idx] =
	    (struct pm_conn_buf *) malloc(sizeof(struct pm_conn_buf))) == NULL)
    {
        (void) fprintf (stderr, "malloc - PM connection object\n");
        return;
    }

    /*
     * save the ICEconn struct for future status checks; also
     * the fd (although you could extract it from the ICEconn
     * each time you need it, but that's a pain)
     */
    pm_conn_array[pm_idx]->fd = temp_sock_fd;
    pm_conn_array[pm_idx]->ice_conn = new_ice_conn; 

    /*
     * Set the readables select() to listen for a readable on this
     * fd; remember, we're not interested in pm writables, since
     * all the negotiation is handled inside this routine; adjust
     * the nfds (must do that everytime we get a new socket to
     * select() on), and then contnue processing current selections
     */
    FD_SET(temp_sock_fd, rinit);
    *nfds = max(*nfds, temp_sock_fd + 1);

    /*
     * this is where we initialize the current time and timeout on this 
     * pm_connection object
     */
    (void) gettimeofday(&time_val, &time_zone);
    pm_conn_array[pm_idx]->creation_time = time_val.tv_sec; 
    pm_conn_array[pm_idx]->time_to_close = config_info->pm_data_timeout; 
}

static void
ProcessPMInput (
    int				* nfds,
    fd_set			* rinit,
    int				pm_idx)
{
    IceProcessMessagesStatus    process_status;

    switch (IceConnectionStatus(pm_conn_array[pm_idx]->ice_conn))
    {
	case IceConnectPending:
	    /*
	     * for some reason this connection still isn't ready for 
	     * reading, so return and try next readable
	     */
	    (void) IceProcessMessages(pm_conn_array[pm_idx]->ice_conn, 
				      NULL, NULL);
	    break;

	case IceConnectAccepted:
	    /*
	     * you're ready to read the PM data, allocate and send back 
	     * your client listen port, etc., etc.; do this inside 
	     * FWPprocessMessages() by calling IceProcessMessages()
	     * [NOTE:  The NULL args set it up for non-blocking]
	     */
	    process_status = IceProcessMessages(pm_conn_array[pm_idx]->ice_conn,
						NULL, NULL);

	    switch (process_status)
	    {
		case IceProcessMessagesSuccess:

		    /*
		     * you read the server data, allocated a listen port 
		     * for the remote client and wrote it back to the PM,
		     * so you don't need to do anything more until PM
		     * closes the connection (NOTE:  Make sure we don't
		     * do this more than once!!)
		     */
		    break;

		case IceProcessMessagesIOError:
		case IceProcessMessagesConnectionClosed:

		    if (process_status == IceProcessMessagesIOError)
			/*
			 * there was a problem with the connection, close 
			 * it explicitly
			 */
	  	        IceCloseConnection(pm_conn_array[pm_idx]->ice_conn);
		    else
			/*
			 * the connection somehow closed itself, so don't call 
			 * IceCloseConnection
			 */
			;
		     
		     /*
		      * reset the select() readables mask and nfds, free 
		      * the buffer memory on this array element, reset the 
		      * pointer to NULL and return
		      */
	  	    FD_CLR(pm_conn_array[pm_idx]->fd, rinit);
	  	    *nfds = max(*nfds, pm_conn_array[pm_idx]->fd + 1);
	  	    free(pm_conn_array[pm_idx]);
	  	    pm_conn_array[pm_idx] = NULL;
	  	    break;

		default:
		    /*
		     * Should never get here since all of the return
		     * codes from IceProcessMessages have been checked.
		     */
		    (void) fprintf (stderr, "IceProcessMessages error\n");
	    }
	    break;

	case IceConnectRejected:
	    /*
	     * don't know yet what to do in this case, but for now simply
	     * output diagnostic and return to select() processing
	     */
	    (void) fprintf (stderr, "PM IceConnectRejected\n");
	    break;

	case IceConnectIOError:
	    /*
	     * don't know yet what to do in this case, but for now simply
	     * output diagnostic and return to select() processing
	     */
	    (void) fprintf (stderr, "PM IceConnectIOError\n");
	    break;

	default:
	    /*
	     * Should never get here since all of the return
	     * codes from IceConnectionStatus have been checked.
	     */
	    (void) fprintf (stderr, "IceConnectionStatus error\n");
      }
}

static void
ProcessNewClientConnection (
    int				* nfds,
    fd_set			* rinit,
    struct config		* config_info,
    int				accept_fd,
    int				server_idx)
{
    int				temp_sock_fd;
    struct sockaddr_in		temp_sockaddr_in;
    struct timeval		time_val;
    struct timezone		time_zone;
    int				rule_number = -1;
    int				server_fd;
    struct sockaddr_in		server_sockaddr_in;
    int				temp_sock_len = sizeof(temp_sockaddr_in);

    /*
     * The first thing we do is accept() this connection and check it 
     * against configuration data to see whether its origination host 
     * is allowed; next, we connect to the server found in the lookup,
     * synthesize a proxy connection setup request to be sent
     * to that server to determine whether it`s a secure server;
     * we can't block on the server reply so we go ahead and allocate
     * a data structure for this client connection, and mark its
     * state PENDING
     */

    if ((temp_sock_fd = accept(accept_fd,
                               (struct sockaddr *) &temp_sockaddr_in, 
	 		       (void *)&temp_sock_len)) < 0)
    {
	(void) fprintf (stderr, "accept call for a client failed\n");
	return;
    }

    /*
     * Try to initialize and open a connection to the server. If
     * an error occurs, those functions will output an appropriate
     * message
     */
    if ((doServerConnectSetup(server_array[server_idx]->x_server_hostport, 
			      &server_array[server_idx]->server_fd, 
			      &server_sockaddr_in)) == FAILURE)
    {
	(void) close (temp_sock_fd);
	return;
    }
    if ((doServerConnect(&server_array[server_idx]->server_fd, 
		         &server_sockaddr_in)) == FAILURE)
    {
	(void) close (temp_sock_fd);
	(void) close (server_array[server_idx]->server_fd);
	return;
    }

    server_fd = server_array[server_idx]->server_fd;

    /*
     * do config check on client source and destination (must do
     * it here because otherwise we don't have a server socket
     * to query and we may not be able to resolve server name 
     * alone from xfindproxy() 
     */
    if ((doConfigCheck(&temp_sockaddr_in, 
		       &server_sockaddr_in,
		       config_info,
		       CLIENT,
		       &rule_number)) == FAILURE)
    {
        /*
         * log the client connection failure, close client and server 
	 * sockets and return
         */
	doWriteLogEntry (inet_ntoa(temp_sockaddr_in.sin_addr),
			 inet_ntoa(server_sockaddr_in.sin_addr),
			 CLIENT_REJECT_CONFIG,
			 rule_number,
			 config_info);
	(void) close (temp_sock_fd);
        (void) close (server_fd);
        return;
    }

    /*
     * If configured authorization succeeds, go ahead and
     * allocate a client_conn_buf struct for client connection
     */
    if ((client_conn_array[temp_sock_fd] = (struct client_conn_buf *) 
	    malloc(sizeof (struct client_conn_buf))) == NULL)
    {
	(void) fprintf (stderr, "malloc - client connection buffer\n");
	return;
    }
    bzero (client_conn_array[temp_sock_fd], sizeof (struct client_conn_buf));

    /*
     * save the source and destination data for this connection (since
     * the log data struct will go out of scope before we check the
     * server security extension or other loggable events)
     */
    client_conn_array[temp_sock_fd]->source = 
	  Malloc(strlen(inet_ntoa(temp_sockaddr_in.sin_addr)) + 1);
    client_conn_array[temp_sock_fd]->destination = 
	  Malloc(strlen(inet_ntoa(server_sockaddr_in.sin_addr)) + 1);

    (void) strcpy(client_conn_array[temp_sock_fd]->source, 
	          inet_ntoa(temp_sockaddr_in.sin_addr));
    (void) strcpy(client_conn_array[temp_sock_fd]->destination, 
	          inet_ntoa(server_sockaddr_in.sin_addr)); 

    /*
     * allocate a buffer for the X server connection 
     * and create the association between client and server 
     */
    if ((client_conn_array[server_fd] = (struct client_conn_buf *) 
	malloc(sizeof (struct client_conn_buf))) == NULL)
    {
	(void) fprintf (stderr, "malloc - server connectioin buffer\n");
	return;
    }
    bzero (client_conn_array[server_fd], sizeof (struct client_conn_buf));

    client_conn_array[server_fd]->conn_to = temp_sock_fd;
    client_conn_array[temp_sock_fd]->conn_to = server_fd;

    /*
     * save this sock fd for future reference (in timeout computation)
     */ 
    client_conn_array[temp_sock_fd]->fd = temp_sock_fd;

    /*
     * mark this buffer as readable and writable and waiting for 
     * authentication to complete; mark the server conn buffer
     * with a special state to make sure that its reply to 
     * the authentication request can be read and interpreted
     * before it is simply forwarded to the client 
     */
    client_conn_array[temp_sock_fd]->state = CLIENT_WAITING;
    client_conn_array[server_fd]->state = SERVER_REPLY;

    /*
     * update the select() fd mask and the nfds
     */
    FD_SET(temp_sock_fd, rinit);
    *nfds = max(*nfds, temp_sock_fd + 1);
    FD_SET(server_fd, rinit);
    *nfds = max(*nfds, server_fd + 1);

    /*
     * this is where we initialize the current time and timeout on this 
     * client_data object
     */
    gettimeofday(&time_val, &time_zone);
    client_conn_array[temp_sock_fd]->creation_time = time_val.tv_sec;
    client_conn_array[temp_sock_fd]->time_to_close = 
	  config_info->client_data_timeout;

    /*
     * be sure the mark the server side of the association, too
     */
    client_conn_array[server_fd]->creation_time = time_val.tv_sec; 
    client_conn_array[server_fd]->time_to_close = 
	config_info->client_data_timeout;

    client_conn_array[server_fd]->fd = server_fd;
}

static void
ProcessClientWaiting (
    fd_set			* winit,
    int				client_idx)
{
    char *			conn_auth_name = "XC-QUERY-SECURITY-1"; 
    int 			conn_auth_namelen;
    int				conn_auth_datalen;
    xConnClientPrefix		client;
    int				endian;
    int 			name_remainder;
    int 			data_remainder;
    int 			idx;
    char 			*bufP;

    /*
     * The remote client is sending more data on an already-
     * established connection, but we still haven't checked
     * authentication on this client from the associated
     * X-server.
     * 
     * Do the following:
     *
     * 1. create the authentication header
     * 2. mark the server fd writable
     * 3. copy the header info into the write buffer
     * 3. set the wbytes field to the header size
     * 4. mark the state SERVER_WAITING
     */
    conn_auth_namelen = strlen(conn_auth_name);

    if (SitePolicyCount == 0)
	conn_auth_datalen = 0;
    else
    {
	int 		sitePolicy;

	conn_auth_datalen = 3;
	for (sitePolicy = 0; sitePolicy < SitePolicyCount; sitePolicy++)
	{
	    conn_auth_datalen += 1 + strlen(SitePolicies[sitePolicy]);
	}
    }

    endian = 1;
    if (*(char *) &endian)
	client.byteOrder = '\154'; /* 'l' */
    else
	client.byteOrder = '\102'; /* 'B' */

    client.majorVersion = X_PROTOCOL;
    client.minorVersion = X_PROTOCOL_REVISION;
    client.nbytesAuthProto = conn_auth_namelen;
    client.nbytesAuthString = conn_auth_datalen;

    /*
     * Put the authentication message into the appropriate 
     * client_conn_buf object
     *
     * compute required padding for name and data strings
     */
    name_remainder = (4 - (conn_auth_namelen % 4)) % 4;
    data_remainder = (4 - (conn_auth_datalen % 4)) % 4;

    idx = client_conn_array[client_idx]->conn_to;

    bufP = client_conn_array[idx]->writebuf;

    memcpy(bufP, (char *) &client, sizeof(client));
    bufP += sizeof(client);

    memcpy(bufP, (char *) conn_auth_name, conn_auth_namelen);
    bufP += conn_auth_namelen;

    bzero(bufP, name_remainder);
    bufP += name_remainder;

    if (conn_auth_datalen)
    {
	int sitePolicy;

	*bufP++ = 0x02;	/* Site Policies only at this point */
	*bufP++ = (SitePolicyPermit == False);
	*bufP++ = SitePolicyCount;

	for (sitePolicy = 0; sitePolicy < SitePolicyCount; sitePolicy++)
	{
	    char nameLen = strlen(SitePolicies[sitePolicy]);

	    *bufP++ = nameLen;
	    memcpy(bufP, SitePolicies[sitePolicy], nameLen);
	    bufP += nameLen;
	}
	bzero(bufP, data_remainder);
    }

    client_conn_array[idx]->wbytes = sizeof(client) + 
	      conn_auth_namelen + name_remainder + 
	      conn_auth_datalen + data_remainder;

    /*
     * Mark this fd as selectable to force a write() operation
     * of authentication request to server for this client 
     */
    FD_SET(client_conn_array[client_idx]->conn_to, winit);

    /*
     * Mark the connection SERVER_WAITING (so that we don't
     * read any more client data until the authentication
     * sequence is complete)
     */ 
    client_conn_array[client_idx]->state = SERVER_WAITING;
}

static void
ProcessConnectionReady (
    fd_set			* rinit,
    fd_set			* winit,
    struct config		* config_info,
    int				client_fd)
{
    /*
     * We've finished our authentication handshaking and are 
     * forwarding data either from client to server or vice versa
     */
    int				bytes_read;

    if (client_conn_array[client_fd]->rbytes < RWBUFFER_SIZE)
    {
        /*
         * read what you have room for
         */
        bytes_read = read(client_fd,
                          client_conn_array[client_fd]->readbuf +
                          client_conn_array[client_fd]->rbytes, RWBUFFER_SIZE -
                          client_conn_array[client_fd]->rbytes);
        /*
         * check for I/O error on this fd; this is our only way
         * of knowing if remote closed the connection
         */
        if (bytes_read == -1)
        {
	    /*
	     * remote apparently closed the connection;
	     * clear bits in the select() mask, reclaim conn_buffs and
	     * listen port 
	     */
	    FD_CLR(client_fd, rinit);
	    FD_CLR(client_fd, winit);
	    FD_CLR(client_conn_array[client_fd]->conn_to, rinit);
	    FD_CLR(client_conn_array[client_fd]->conn_to, winit);

	    (void) close (client_conn_array[client_fd]->conn_to);
	    (void) close (client_fd);

	    if (client_conn_array[client_fd]->source)
		free(client_conn_array[client_fd]->source);
	    if (client_conn_array[client_fd]->destination)
		free(client_conn_array[client_fd]->destination);
	    free(client_conn_array[client_fd]);

	    client_conn_array[client_fd] = NULL;

	    /*
	     * If this FD is for an Xserver must remove the server's
	     * listen fd so that clients will not attempt to connect
	     * on this fd.
	     */
	    RemoveFDFromServerListenArray (client_fd, 
					   rinit,
					   config_info->num_servers);

	    /*
	     * exit readables for loop
	     */
	    return;
        } else if (bytes_read == 0)
        {
	    /*
	     * make sure we don't try to read on this fd again 
	     */
	    FD_CLR(client_fd, rinit);
	    FD_CLR(client_fd, winit);

	    (void) close (client_fd);

	    if (client_conn_array[client_fd]->conn_to != -1)
	    {
		/* 
		 * mark this conn_fd fd ready to close 
		 */
		int idx = client_conn_array[client_fd]->conn_to;

		client_conn_array[idx]->wclose  = 1;
		client_conn_array[idx]->conn_to = -1;

		/*
		 * but still force a last write on the conn_to connection
		 */ 
		FD_SET(client_conn_array[client_fd]->conn_to, winit);
	    }	

	    /*
	     * and mark this connection for no further activity 
	     */
	    client_conn_array[client_fd]->rbytes = 0;
	    client_conn_array[client_fd]->wbytes = 0;
	    client_conn_array[client_fd]->conn_to = -1;

	    /*
	     * If this FD is for an Xserver must remove the server's
	     * listen fd so that clients will not attempt to connect
	     * on this fd.
	     */
	    RemoveFDFromServerListenArray (client_fd, 
					   rinit, 
					   config_info->num_servers);

        } else
	{
	    /*
	     * Move bytes between buffers on associated fd's
	     * (read data on one becomes write data on other)
	     */
	    client_conn_array[client_fd]->rbytes += bytes_read;

	    if (client_conn_array[client_fd]->conn_to != 0)
		doCopyFromTo(client_fd,
			     client_conn_array[client_fd]->conn_to,
			     rinit, 
			     winit);

	    /*
	     * check if you have more read data than available space
	     */
	    if (client_conn_array[client_fd]->rbytes >= RWBUFFER_SIZE)
	    {
		/*
		 * if so, abort the attempted copy until you can
		 * write some data out of the buffer first, and
		 * don't allow any more reading until that's done
		 */
		FD_CLR(client_fd, rinit);
	    } 
	}
    }
}

static void
ProcessServerReply (
    fd_set			* rinit,
    fd_set			* winit,
    struct config		* config_info,
    int				client_fd)
{
    int				idx;
    int				endian;
    int				four = 4;
    int				server_reason_remainder;
    char * 			server_reason_padded;
    int				server_reason_len;
    char			throw_away[RWBUFFER_SIZE];
    xConnSetupPrefix		prefix;

    if (client_conn_array[client_fd]->state == SERVER_REPLY)
    {
	/*
	 * read the server reply to the authentication request
         */      
        (void) read(client_fd,
                    client_conn_array[client_fd]->readbuf +
                    client_conn_array[client_fd]->rbytes, 
		    RWBUFFER_SIZE - client_conn_array[client_fd]->rbytes);

	switch ((BYTE) client_conn_array[client_fd]->readbuf[0])
	{
	    case SERVER_REPLY_FAILURE:
#ifdef DEBUG
	    {  
		char 	* replyP = client_conn_array[client_fd]->readbuf;
	        int 	reasonLength = *++replyP;

	        replyP += 6;	/* skip major & minor version, msg len */
	        *(replyP+reasonLength+1) = '\0';
	        (void) fprintf (stderr, "Server replied FAILURE: %s\n", replyP);
	    }
	    /* FALL-THROUGH */
#endif
	    case SERVER_REPLY_SUCCESS:
		/* 
		 * two possibilities here:  either the policy field 
		 * passed to the server is unauthorized, or the server
		 * does not support the security extension; in both cases
		 * we read the client fd then synthesize a response
		 * which we forward to the client before closing the 
		 * connection
		 */
		(void) read(client_conn_array[client_fd]->conn_to,
			    throw_away, 1);
		/*
		 * construct the client response
		 */
		prefix.success = 0;
		prefix.lengthReason = server_reason_len = 
		    strlen(server_reason
			   [(int) client_conn_array[client_fd]->readbuf[0]]);
		prefix.majorVersion = X_PROTOCOL;
		prefix.minorVersion = X_PROTOCOL_REVISION;
		server_reason_remainder = server_reason_len;

		/*
		 * calculate quadword padding required
		 */
		while (server_reason_remainder > 0)
		  server_reason_remainder = server_reason_remainder - four;
		server_reason_remainder = abs(server_reason_remainder);

		/*
		 * allocate the padded buffer
		 */
		if ((server_reason_padded = 
		     (char *) malloc (server_reason_len + 
				      server_reason_remainder)) == NULL)
		{
		    (void) fprintf (stderr, "malloc - server reason\n");
		    return;
		} 

		/*
		 * calculate the "additional data" field
		 */
		prefix.length = (server_reason_len + server_reason_remainder) /
				  four; 

		/*
		 * compare client and xfwp byte ordering and swap prefix fields
		 * as necessary
		 */
		endian = 1;
		if (((throw_away[0] == '\154') && !(*(char *) &endian)) ||
		    ((throw_away[0] == '\102') && (*(char *) &endian)))
		{
		    /*
		     * client and xfwp are different byte order
		     * so swap all fwp 2-byte fields to little endian
		     */
		    swab((char *) &prefix.majorVersion, 
			 (char *) &prefix.majorVersion, 
			 sizeof(prefix.majorVersion));
		    swab((char *) &prefix.minorVersion, 
			 (char *) &prefix.minorVersion, 
			 sizeof(prefix.minorVersion));
		    swab((char *) &prefix.length, 
			 (char *) &prefix.length, 
			 sizeof(prefix.length));
		}

		/*
		 * load the padded reason
		 */
		bzero((char *) server_reason_padded, 
		       server_reason_len + server_reason_remainder);
		memcpy((char *) server_reason_padded, 
		       (char *) server_reason
			    [(int) client_conn_array[client_fd]->readbuf[0]],
		       server_reason_len);
		/*
		 * load the complete synthesized server reply (which will
		 * be sent to the client next time the writables are
		 * processed (again, to avoid blocking) 
		 */
		memcpy((char *) client_conn_array[client_fd]->readbuf, 
		       (char *) &prefix, 
		       sizeof(prefix));

		memcpy((char *) client_conn_array[client_fd]->readbuf + 
			   sizeof(prefix),
		       (char *) server_reason_padded, 
		       server_reason_len + server_reason_remainder);

		client_conn_array[client_fd]->rbytes = sizeof(prefix) + 
		    server_reason_len + server_reason_remainder;

		/*
		 * make sure to zero the wbytes on the client conn object
		 * before forwarding the server reply
		 */
		idx = client_conn_array[client_fd]->conn_to;

		client_conn_array[idx]->wbytes = 0;
		doCopyFromTo(client_fd, idx, rinit, winit);
		client_conn_array[client_fd]->wclose = 1;
		client_conn_array[idx]->conn_to = -1;

		/*
		 * clear the select() mask for the client socket and for
		 * the server
		 */
		FD_CLR(client_conn_array[client_fd]->conn_to, rinit);
		FD_CLR(client_fd, rinit);
#ifdef DEBUG
		/*
		 * output a trace message
		 */
		if (((int) client_conn_array[client_fd]->readbuf[0]) == 
			SERVER_REPLY_SUCCESS)
		    (void) fprintf (stderr, "Server replied SUCCESS\n");
#endif
		/*
		 * clean up memory
		 */
		free(server_reason_padded);

		doWriteLogEntry (client_conn_array[idx]->source,
				 client_conn_array[idx]->destination,
				 CLIENT_REJECT_SERVER,
				 -1,
				 config_info);
		break;
                 
	    case SERVER_REPLY_AUTHENTICATE:
		/*
		 * the server supports the security extension; begin
		 * forwarding client and server messages normally
		 */
		idx = client_conn_array[client_fd]->conn_to;

		client_conn_array[client_fd]->state = CONNECTION_READY;
		client_conn_array[idx]->state        = CONNECTION_READY;

#ifdef DEBUG
		(void) fprintf (stderr, "Server replied AUTHENTICATE\n");
#endif
		doWriteLogEntry (client_conn_array[idx]->source,
				 client_conn_array[idx]->destination,
				 CLIENT_ACCEPT,
				 -1,
				 config_info);
		break;

	  default:
	      (void) fprintf (stderr, "unknown reply from server\n"); 
	}
    }
}

static void
doProcessReadables(
    int 			fd_counter,
    int 			* nfds,
    fd_set 			* rinit, 
    fd_set 			* winit,
    int 			pm_listen_array[],
    struct config 		* config_info,
    IceListenObj 		** listen_objects)
{
    int				i;

    /*
     * Check fd_counter to see if it is one of the PM listen fds
     */
    for (i = 0; i < config_info->num_pm_listen_ports; i++)
    {
        if (pm_listen_array[i] == fd_counter) 
        {
            if (!pm_conn_array[fd_counter])
            {
		/*
		 * Process this new PM connection
		 */
	        ProcessNewPMConnection (nfds,
	  			        rinit,
				        config_info,
				        listen_objects,
				        i);
	        return;
            }
	    break;
        }
    }

    /*
     * If this is an already-accepted PM connection, call
     * IceProcessMessages() to invoke the FWPprocessMessages
     * callback 
     */
    for (i = 0; i < config_info->num_pm_conns; i++)
    {
        if (pm_conn_array[i] &&
	    pm_conn_array[i]->fd == fd_counter)
        {
	    ProcessPMInput (nfds, rinit, i);
	    return;
	}
    }

    /*
     * Check fd_counter to see if it one of the ports that
     * clients use to connect to a server.
     */
    for (i = 0; i < config_info->num_servers; i++)
    {
	if (server_array[i] &&
	    server_array[i]->client_listen_fd == fd_counter)
	{
	    ProcessNewClientConnection (nfds,
					rinit,
					config_info,
					fd_counter,
					i);
	    return;
	}
    }

    /*
     * At this point the input can be from the following sources:
     *
     * 1. An X server replying to an authentication request
     *
     * 2. Data coming from a X server that needs to be re-directed
     *    to a client
     *
     * 3. Data from a client that needs to be re-directed to
     *    its X server
     */
    if (!client_conn_array[fd_counter])
    {
	/*
	 * Impossible - right?
	 */
	(void) fprintf (stderr, "input processing error\n");
	return;
    }

    switch (client_conn_array[fd_counter]->state)
    {
	case CLIENT_WAITING:
	    ProcessClientWaiting (winit, 
				  fd_counter);
	    break;

        case CONNECTION_READY:
	    ProcessConnectionReady (rinit, 
				    winit, 
				    config_info, 
				    fd_counter);
	    break;

	case SERVER_WAITING:
	    /*
	     * This is a do-nothing state while we're waiting for the
	     * server reply (see below)
	     */
	    break;

	case SERVER_REPLY:
	    ProcessServerReply (rinit, 
				winit, 
				config_info, 
				fd_counter);
	    break;

	default:
	    /*
	     * All of the values of state have been tested.
	     */
	    (void) fprintf (stderr, "client state error\n");
    }
}

void
doProcessSelect(
    int 			* nfds,
    int 			* nready, 
    fd_set 			* readable, 
    fd_set 			* writable,
    fd_set 			* rinit, 
    fd_set 			* winit,
    int 			pm_listen_array[],
    struct config 		* config_info,
    IceListenObj 		** listen_objects)
{
    int 			fd_counter;

    /*
     * Loop through descriptors
     */
    for (fd_counter = 0; fd_counter < *nfds && *nready; fd_counter++)
    {
	/*
	 * Look at fd's for writables
	 */
	if (FD_ISSET(fd_counter, writable))
	{
	    /*
	     * Decrement the list of read/write ready connections
	     */
	    *nready -= 1; 
	    doProcessWritables (fd_counter, 
			        rinit, 
			        winit);
	}

	/*
	 * Now, do the same thing for the readables
	 */
	if (FD_ISSET(fd_counter, readable))
	{
	    /*
	     * Decrement the list of read/write ready connections
	     */
	    *nready -= 1; 
	    doProcessReadables (fd_counter, 
				nfds, 
				rinit, 
				winit, 
				pm_listen_array, 
				config_info, 
				listen_objects);
	}
    }
}
