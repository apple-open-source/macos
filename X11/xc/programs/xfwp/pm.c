/* $Xorg: pm.c,v 1.4 2001/02/09 02:05:45 xorgcvs Exp $ */
/*

Copyright "1986-1997, 1998  The Open Group 

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
/* $XFree86: xc/programs/xfwp/pm.c,v 1.8 2002/09/18 17:11:56 tsi Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* strdup */
#include <X11/Xos.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifdef X_POSIX_C_SOURCE
#define _POSIX_C_SOURCE X_POSIX_C_SOURCE
#include <signal.h>
#undef _POSIX_C_SOURCE
#else
#if defined(X_NOT_POSIX) || defined(_POSIX_SOURCE)
#include <signal.h>
#else
#define _POSIX_SOURCE
#include <signal.h>
#undef _POSIX_SOURCE
#endif
#endif

#include <X11/Xproto.h>

#include <X11/ICE/ICElib.h>
#include <X11/ICE/ICEmsg.h>
#include <X11/ICE/ICEproto.h>

#include <X11/PM/PM.h>
#include <X11/PM/PMproto.h>

#include "xfwp.h"
#include "pm.h"
#include "transport.h"
#include "misc.h"

void FWPprocessMessages(
    IceConn iceConn, 
    IcePointer * client_data,
    int opcode,
    unsigned long length,
    Bool swap)
{
  switch (opcode)
  { 
    /*
     * this is really the only opcode we care about -- the one
     * which indicates an XFindProxy request for a connection
     * to a specified server
     */
    case PM_GetProxyAddr:
    {
      pmGetProxyAddrMsg       	*pMsg;
      char                      *pData, *pStart;
      char                      *serviceName = NULL, *serverAddress = NULL;
      char                      *hostAddress = NULL, *startOptions = NULL;
      char                      *authName = NULL, *authData = NULL;
      int                       authLen;
      struct clientDataStruct * program_data; 
      char *			listen_port_string;
      int			pm_send_msg_len;
      pmGetProxyAddrReplyMsg *	pReply;
      char *			pReplyData;
      struct hostent * 		hostptr;
      struct sockaddr_in	server_sockaddr_in;
      struct sockaddr_in	dummy_sockaddr_in;
      char *			server_name_base;
      char *			config_failure = "unrecognized server or permission denied";
      char *			tmp_str;
      int			rule_number = -1;
      char *			colon;
      char *			tmpAddress = NULL;

      /*
       * this is where we need and get access to that client data we
       * went through such contortions to set up earlier!
       */
      program_data = (struct clientDataStruct *) client_data;
      /*
       * initial check on expected message size
       */ 
      CHECK_AT_LEAST_SIZE (iceConn, global_data.major_opcode, opcode,
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

      SKIP_STRING (pData, swap);      /* proxy-service */
      SKIP_STRING (pData, swap);      /* server-address */
      SKIP_STRING (pData, swap);      /* host-address */
      SKIP_STRING (pData, swap);      /* start-options */
      if (authLen > 0)
      {
        SKIP_STRING (pData, swap);              /* auth-name */
        pData += (authLen +  PAD64 (authLen));  /* auth-data */
      }
      /*
       * now a detailed check on message size
       */
      CHECK_COMPLETE_SIZE (iceConn, global_data.major_opcode, opcode,
         length, pData - pStart + SIZEOF (pmGetProxyAddrMsg),
         pStart, IceFatalToProtocol);

      pData = pStart;
      /*
       * extract message data, based on known characteristics
       * of this message type
       */
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
#ifdef DEBUG
      (void) fprintf (stderr, 
		      "Got GetProxyAddr, serviceName = %s, serverAddr = %s\n",
              	      serviceName, serverAddress);
      (void) fprintf (stderr, 
		      "\thostAddr = %s, options = %s, authLen = %d\n",
              	      hostAddress, startOptions, authLen);
      if (authLen > 0)
          (void) fprintf (stderr, "\tauthName = %s\n", authName);
#endif
      /*
       * need to copy the host port string because strtok() changes it   
       */
      if ((tmp_str = strdup (serverAddress)) == NULL)
      {
        (void) fprintf(stderr, "malloc - serverAddress copy\n");
        goto sendFailure;
      }

      /*
       * before proceeding we want to verify that we are allowed to
       * accept connections from the host who called xfindproxy(); 
       * the thing is, we don't get that host name from Proxy Manager
       * even if the "-host <hostname>" command-line option was present
       * in xfindproxy (and even if it was we shouldn't rely on it --
       * much better to have ProxyMngr query the xfindproxy connect
       * socket for its origin); the upshot of all this that we do
       * a configuration check *only* on the destination (which we
       * assume in this case to be the serverAddress passed in by
       * xfindproxy(); so get the destination IP address!
       */
      server_name_base = strtok(tmp_str, ":");
      if ((hostptr = gethostbyname(server_name_base)) == NULL)
      {
        (void) fprintf(stderr, "gethostbyname (%s) failed\n", server_name_base);
	goto sendFailure;
      }
      memset(&server_sockaddr_in, 0, sizeof(server_sockaddr_in));
      memset(&dummy_sockaddr_in, 0, sizeof(dummy_sockaddr_in)); 
      memcpy((char *) &server_sockaddr_in.sin_addr, 
	     hostptr->h_addr,
	     hostptr->h_length);

      /*
       * need to initialize dummy to something, but doesn't matter
       * what (should eventually be the true host address); 
       * NOTE:  source configuration will always match (see XFWP man
       * page) unless sysadmin explicitly chooses to deny 
       */
      memcpy((char *) &dummy_sockaddr_in.sin_addr, 
	     hostptr->h_addr,
	     hostptr->h_length);

      if ((doConfigCheck(&dummy_sockaddr_in, 
  			 &server_sockaddr_in,
			 global_data.config_info,
			 FINDPROXY,
			 &rule_number)) == FAILURE)
      { 
        (void) fprintf(stderr, "xfindproxy failed config check\n");
      sendFailure:
        /*
         * report failure back to the ProxyMgr
         * 
         */
        pm_send_msg_len = STRING_BYTES(config_failure)
	      		+ STRING_BYTES(NULL);
        IceGetHeaderExtra(iceConn, 
	      	        program_data->major_opcode, 
	      		PM_GetProxyAddrReply,
	       		SIZEOF(pmGetProxyAddrReplyMsg),
	       		WORD64COUNT (pm_send_msg_len),
	       		pmGetProxyAddrReplyMsg,
	       		pReply,
	       		pReplyData);
        pReply->status = PM_Failure;
        STORE_STRING(pReplyData, NULL);
        STORE_STRING(pReplyData, config_failure);
        IceFlush(iceConn);
	free(tmp_str);
        return; 
      }

      /* 
       * okay, you got what you need from the PM to proceed,
       * so extract the fd of the selected connection and use
       * it to set up the remote client listen port and add 
       * the name of the X server to your list of server connections
       */

      /*
       * Before checking to see if you already have a PM connection
       * request for this server, make serverAddress a
       * FQDN so that synonomous names like oregon:0 and oregon.com:0
       * will be recognized as the same Xserver.  If this server
       * already exists, don't allocate another listen port for it -
       * use the already allocated one
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

      if ((doCheckServerList(serverAddress, 
			    &listen_port_string,
			    program_data->config_info->num_servers)) == FAILURE)
      {
        /*
         * this server name isn't in your list; so set up a new
         * remote client listen port for it; extract the fd from
         * the connection and pass it in as index to array lookup
         */
        if ((doSetupRemClientListen(&listen_port_string,
			            program_data,
				    serverAddress)) == FAILURE)
 	{
          goto sendFailure;
	}
      }
      if (tmpAddress)
	free (tmpAddress);

      /*
       * the PM-sent server address *was* in your list, so send back
       * the rem client listen port you had already associated with 
       * that server (it will presumably be forwarded to the remote
       * client through some other channel)
       * use IceGetHeaderExtra() and the 
       */
      pm_send_msg_len = STRING_BYTES(listen_port_string)
			+ STRING_BYTES(NULL);
      IceGetHeaderExtra(iceConn, 
		        program_data->major_opcode, 
			PM_GetProxyAddrReply,
			SIZEOF(pmGetProxyAddrReplyMsg),
			WORD64COUNT (pm_send_msg_len),
			pmGetProxyAddrReplyMsg,
			pReply,
			pReplyData);
      pReply->status = PM_Success;
      STORE_STRING(pReplyData, listen_port_string);
      STORE_STRING(pReplyData, NULL); 
      IceFlush(iceConn);
      /*
       * before leaving this routine, change the select() timeout
       * here to be equal to the configured client listen timeout
       * (otherwise you'll never *get* to your listen timeout
       * if it's shorter than the startup select() default
       */
      program_data->config_info->select_timeout.tv_sec =
		program_data->config_info->client_listen_timeout;
      break;
    }

    case ICE_Error:
    {
	iceErrorMsg *pMsg;
	char *pStart;

	CHECK_AT_LEAST_SIZE (iceConn, global_data.major_opcode, ICE_Error, 
			     length, sizeof(iceErrorMsg), IceFatalToProtocol);

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

	(void) fprintf(stderr, "Proxy Manager reported ICE Error:\n");
	(void) fprintf(stderr, "\tclass = 0x%x, offending minor opcode = %d\n",
		       pMsg->errorClass, pMsg->offendingMinorOpcode);
	(void) fprintf(stderr, "\tseverity = %d, sequence = %ld\n",
		       pMsg->severity, (long)pMsg->offendingSequenceNum);

	IceDisposeCompleteMessage (iceConn, pStart);

	break;
    }

    default:
      break;
  } /* end switch */
}


/*ARGSUSED*/
Bool
FWPHostBasedAuthProc (
    char * hostname)
{
  /*
   * don't worry about config for now
   *
   * this routine gets called *after* IceAcceptConnection 
   * is called but *before* that routine returns its status;
   * it is therefore the logical place to check configuration 
   * data on which PM connections (from which hosts) will be
   * accepted; so do it and return either 0 to terminate 
   * connection (automatically informing PM) or 1 to proceed
   *
   * the PM host is not allowed; terminate connection and inform
   * requestor why (handled automatically by ICElib)
   */

  return True;
}


/*ARGSUSED*/
Status
FWPprotocolSetupProc(
    IceConn iceConn,
    int major_version,
    int minor_version,
    char * vendor,
    char * release,
    IcePointer * clientDataRet,
    char ** failureReasonRet)
{
  /*
   * This routine gets invoked when the remote ICE originator
   * (in this case PM) calls IceRegisterSetup() on *its* end
   * of the FWP connection; this is where the pointer to
   * client data should be initialized, so that this data
   * can be accessed when FMprocessMessages is called by
   * IceProcessMessages()
   */
  struct clientDataStruct * 	client_data;
  if ((client_data = (struct clientDataStruct *) 
		     malloc (sizeof (struct clientDataStruct))) == NULL)
  {
    (void) fprintf(stderr, "malloc - client data object\n");
    return (0);
  }
  /*
   * setup the client data struct; we need this object in order to
   * avoid making these variables global so they can be accessed in the
   * ICE FWPprocessMessages() callback; now you see that our global_data
   * struct was the only way of getting program data into the 
   * protocolReply setup routine!
   */
  client_data->config_info = global_data.config_info;
  client_data->nfds = global_data.nfds;
  client_data->rinit = global_data.rinit;
  client_data->winit = global_data.winit;
  client_data->major_opcode = global_data.major_opcode;
  *clientDataRet = client_data;

  return (1);
}

int 
doSetupPMListen(
    char 		*  pm_port,
    int			* size_pm_listen_array,
    int 		** pm_listen_array,
    IceListenObj 	** listen_objects,
    int 		* nfds,
    fd_set 		* rinit)
{
  int 	num_fds_returned;
  char  errormsg[256];
  int   fd_counter;
  IceListenObj * temp_obj;

  /*
   * establish PM listeners
   */  
  if (!IceListenForWellKnownConnections(pm_port, 
					&num_fds_returned,
					listen_objects,
					256,
					errormsg))
  {
    (void) fprintf(stderr, "IceListenForWellKnowConnections error: %s\n", 
		   errormsg); 
    return 0;
  }

  /*
   * Create space for pm_listen_array
   */
  *pm_listen_array = (int *) malloc (num_fds_returned * sizeof (int *));
  if (!pm_listen_array)
  {
    (void) fprintf (stderr, "malloc - pm_listen_array\n");
    return 0;
  }
  *size_pm_listen_array = num_fds_returned;

  /*
   * obtain the PM listen fd's for the connection objects 
   */
  for (fd_counter = 0; fd_counter < num_fds_returned; fd_counter++)
  {
    /*
     * get fd(s) for PM listen (could be more than one if different
     * transport mechanisms) 
     */ 
    temp_obj = *listen_objects;
    IceSetHostBasedAuthProc(temp_obj[fd_counter], FWPHostBasedAuthProc);
    (*pm_listen_array)[fd_counter] = 
     		IceGetListenConnectionNumber(temp_obj[fd_counter]);

    /*
     * set all read mask bits on which we are going to select(); 
     * [NOTE:  We don't care about write bits here because we don't
     * use select() to manage writing to the PM] 
     */
    FD_SET((*pm_listen_array)[fd_counter], rinit);

    /*
     * compute nfds for select()
     */
    *nfds = max(*nfds, (*pm_listen_array)[fd_counter] + 1);
  }
  return 1;
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

static void
MyIoErrorHandler (
    IceConn ice_conn)

{
    if (prev_handler)
        (*prev_handler) (ice_conn);
}

void 
doInstallIOErrorHandler (void)
{
    IceIOErrorHandler default_handler;

    prev_handler = IceSetIOErrorHandler (NULL);
    default_handler = IceSetIOErrorHandler (MyIoErrorHandler);
    if (prev_handler == default_handler)
        prev_handler = NULL;
#ifdef X_NOT_POSIX
    signal(SIGPIPE, SIG_IGN);
#else
    {
        struct sigaction act;

        (void) sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        act.sa_handler = SIG_IGN;
        (void) sigaction(SIGPIPE, &act, NULL);
    }
#endif
}

