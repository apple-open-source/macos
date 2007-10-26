/*$Id: network.h,v 1.1 1999/09/23 17:30:07 wsanchez Exp $*/

#include <sys/socket.h>			/* socket() sendto() AF_INET
					/* SOCK_DGRAM */
#include <netdb.h>			/* gethostbyname() getservbyname()
					/* getprotobyname() */
#include <netinet/in.h>			/* htons() struct sockaddr_in */

#ifndef BIFF_serviceport
#define BIFF_serviceport	COMSATservice
#endif

#ifndef h_0addr_list
#define h_0addr_list	h_addr_list[0]		      /* POSIX struct member */
#endif

#ifndef NO_const      /* since network.h is outside the autoconf const check */
#ifdef const		    /* loop, we need this backcheck for some systems */
#undef const
#endif
#endif
