/* $Xorg: transport.c,v 1.4 2001/02/09 02:05:45 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xfwp/transport.c,v 1.7 2001/12/14 20:01:43 dawes Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <X11/Xos.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <X11/Xfuncs.h>		/* Need for bzero() */

#include <X11/ICE/ICElib.h>	/* Need typedef for Bool */

#include "xfwp.h"
#include "transport.h"

int 
doSetupRemClientListen(
    char ** listen_port_string,
    struct clientDataStruct * program_data,
    char * server_address)
{
  int			this_server;
  int 			one = 1;
  struct sockaddr_in 	rem_sockaddr_in;
  int 			listen_port;
  char			hostname[MAX_HOSTNAME_LEN];
  struct timeval	time_val;
  struct timezone	time_zone;
  int			num_servers = program_data->config_info->num_servers;

  /* 
   * ugh.  This really shouldn't be kept as a sparse list but no time...
   */
  for (this_server = 0;
       this_server < num_servers && server_array[this_server] != NULL;
       this_server++);
  if (this_server == num_servers)
  {
      (void) fprintf(stderr, 
		 "Maximum number of server connections has been reached (%d)\n",
		 program_data->config_info->num_servers);
      return FAILURE;
  }

  /*
   * offset listen port into the X protocol range; 
   * must be > X_SERVER_PORT_BASE < 65535 
   */
  listen_port = this_server + X_SERVER_PORT_BASE + 1;

  /*
   * allocate the server_array struct and init the fd elements;
   * can't use the PM connection fd as an index into this array, since
   * there could be multiple servers per PM connection
   */
  if ((server_array[this_server] =
      (struct server_list *) malloc(sizeof(struct server_list))) == NULL)
  {
    (void) fprintf(stderr,"malloc - server_array\n");
    return FAILURE;
  }

  if ((server_array[this_server]->client_listen_fd = 
	socket(AF_INET, SOCK_STREAM, 0)) < 0) 
  {
      (void) fprintf(stderr,"socket call failed\n");
      free(server_array[this_server]);
      server_array[this_server] = NULL;
      return FAILURE;
  }

  /*
   * this is where we initialize the current time and timeout on this 
   * client_listen object
   */
  gettimeofday(&time_val, &time_zone);
  server_array[this_server]->creation_time = time_val.tv_sec; 
  server_array[this_server]->time_to_close = 
      global_data.config_info->client_listen_timeout;

  /*
   * set up the rest of the remote client listener
   */
  bzero((char * ) &rem_sockaddr_in, sizeof(rem_sockaddr_in));
  rem_sockaddr_in.sin_family = AF_INET;
#ifdef BSD44SOCKETS
  rem_sockaddr_in.sin_len = sizeof rem_sockaddr_in;
#endif

#ifdef SO_REUSEADDR
  if (setsockopt(server_array[this_server]->client_listen_fd,
		 SOL_SOCKET, SO_REUSEADDR,
                 (char *) &one, sizeof(int)) < 0) 
  {
      (void) fprintf(stderr, "setsockopt(SO_REUSEADDR) failed\n");
  returnFailure:
      close(server_array[this_server]->client_listen_fd);
      free(server_array[this_server]);
      server_array[this_server] = NULL;
      return FAILURE;
  }
#endif /* SO_REUSEADDR */

  while (True) {
      rem_sockaddr_in.sin_port = htons(listen_port);
      if (bind(server_array[this_server]->client_listen_fd, 
	       (struct sockaddr *)&rem_sockaddr_in, 
	       sizeof(rem_sockaddr_in)) == 0)
	  break;
      if (errno != EADDRINUSE)
      {
	  (void) fprintf(stderr,"bind call failed\n");
	  goto returnFailure;
      }
      listen_port++;

      /*
       * Cann't keep going forever.  
       * 
       * Why 65535 - it's the same value used by the LBXProxy.
       */
      if (listen_port > 65535)
      {
	  (void) fprintf(stderr,"failed to find a valid port for bind\n");
	  goto returnFailure;
      }
  }

#ifdef DEBUG
  (void) fprintf (stderr, "Client connect port: %d\n", listen_port);
#endif

  if (listen(server_array[this_server]->client_listen_fd, SOMAXCONN) < 0) 
  {
      (void) fprintf(stderr, "listen call failed\n");
      goto returnFailure;
  }

  /*
   *  update the nfds
   */
  *(program_data->nfds) = max(*(program_data->nfds), 
	  server_array[this_server]->client_listen_fd + 1);

  /*
   * get fully qualified name of host on which FWP is running
   */
  if ((gethostname(hostname, MAX_HOSTNAME_LEN)) < 0)
  {
     (void) fprintf(stderr, "gethostname call failed\n");
     goto returnFailure;
  }

  /*
   * allocate and convert the listen_port string for return to PM;
   * string equals address of host on which FWP is running 
   * plus ":<listen_port - X_SERVER_PORT_BASE> (up to xxx)"
   */
  if (((*listen_port_string) = 
	(char *) malloc (strlen(hostname) + 10)) == NULL)
  {
    (void) fprintf(stderr, "malloc - proxy address\n");
    goto returnFailure;
  }
  (void) sprintf (*listen_port_string, "%s:%d", hostname, 
		  listen_port - X_SERVER_PORT_BASE);

  /*
   * add the server name associated with the current PM request
   * to the list
   */
  if ((server_array[this_server]->x_server_hostport =
       strdup (server_address)) == NULL)
  {
    (void) fprintf(stderr, "malloc - server_array string\n");
    free(*listen_port_string);
    *listen_port_string = NULL;
    goto returnFailure;
  }

  /*
   * add the client listen port associated with the current PM connection
   * to the list
   */
  if ((server_array[this_server]->listen_port_string =
       strdup (*listen_port_string)) == NULL)
  {
    (void) fprintf(stderr, "malloc - server_array string\n");
    free(server_array[this_server]->x_server_hostport);
    free(*listen_port_string);
    *listen_port_string = NULL;
    goto returnFailure;
  }

  /*
   *  set the select() read mask for this descriptor
   */
  FD_SET(server_array[this_server]->client_listen_fd, program_data->rinit);

  return SUCCESS;
}

void 
doSelect(struct 
    config * config_info,
    int * nfds, 
    int * nready, 
    fd_set * readable, 
    fd_set * writable)
{
  if ((*nready = select(*nfds, 
			readable, 
			writable, 
			NULL, 
			&config_info->select_timeout)) == -1)
  {
    if (errno == EINTR)
      return;
    (void) fprintf(stderr, "select call failed\n");
    perror("select");
    exit(1);
  }
}

int 
doServerConnectSetup(
    char * x_server_hostport,
    int * server_connect_fd,
    struct sockaddr_in * server_sockaddr_in)
{
  struct hostent * 	hostptr;
  char *		server_name_base;
  char			server_port_base[10];
  int			server_port;
  int			i = 0;
  char *		tmp_str;
  char *		tmp_hostport_str;

  /*
   * need to copy the host port string because strtok() changes it   
   */
  if ((tmp_hostport_str = strdup (x_server_hostport)) == NULL)
  {
    (void) fprintf(stderr, "malloc - hostport copy\n");
    return FAILURE;
  }

  tmp_str = x_server_hostport;
  while (tmp_str[i] != ':')
     tmp_str++;
  tmp_str++;
  strcpy(server_port_base, tmp_str);
  server_name_base = strtok(tmp_hostport_str,":");
  server_port = atoi(server_port_base) + X_SERVER_PORT_BASE; 
  hostptr = gethostbyname(server_name_base);
  free(tmp_hostport_str);

  if (hostptr == NULL)
  {
    (void) fprintf(stderr, "gethostbyname call failed\n");
    return FAILURE;
  }

  if ((*server_connect_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    (void) fprintf(stderr, "socket call for server failed: %s\n", 
		   strerror(errno)); 
    return FAILURE;
  }

  memset(server_sockaddr_in, 0, sizeof(*server_sockaddr_in));
  server_sockaddr_in->sin_family = hostptr->h_addrtype;
#ifdef BSD44SOCKETS
  server_sockaddr_in->sin_len = sizeof server_sockaddr_in;
#endif
  memcpy((char *) &server_sockaddr_in->sin_addr, 
	 hostptr->h_addr,
	 hostptr->h_length);
  server_sockaddr_in->sin_port = htons(server_port);

  return SUCCESS;
}

int 
doServerConnect(
    int * server_connect_fd,
    struct sockaddr_in * server_sockaddr_in)
{
  if(connect(*server_connect_fd, (struct sockaddr * )server_sockaddr_in,
	     sizeof(*server_sockaddr_in)) < 0)
  {
    (void) fprintf(stderr, "connect call to server failed: %s\n", 
	    	   strerror(errno)); 
    return FAILURE;
  }
  return SUCCESS;
}

