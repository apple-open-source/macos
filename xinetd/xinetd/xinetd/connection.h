/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef CONNECTION_H
#define CONNECTION_H

/*
 * $Id: connection.h,v 1.1.1.8 2003/07/02 19:31:54 rbraun Exp $
 */

#include "config.h"
#include <sys/types.h>
#include <netinet/in.h>
#if defined( HAVE_ARPA_INET_H )
#include <arpa/inet.h>
#endif
#include <string.h>

#include "mask.h"
#include "service.h"
#include "defs.h"
#include "msg.h"
#include "sio.h"

#ifndef IN6_IS_ADDR_V4MAPPED
#define IN6_IS_ADDR_V4MAPPED(a) \
   ((((uint32_t *) (a))[0] == 0) && (((uint32_t *) (a))[1] == 0) && \
   (((uint32_t *) (a))[2] == htonl (0xffff)))
#endif
#ifndef IN6_IS_ADDR_V4COMPAT
#define IN6_IS_ADDR_V4COMPAT(a) \
   ((((uint32_t *) (a))[0] == 0) && (((uint32_t *) (a))[1] == 0) && \
   (((uint32_t *) (a))[2] == 0) && (ntohl (((uint32_t *) (a))[3]) > 1))
#endif

#define MAX_ALTERNATIVES            3

/* Connection flags */
#define COF_HAVE_ADDRESS            1
#define COF_NEW_DESCRIPTOR          2

struct connection
{
   struct service        *co_sp ;
   int                   co_descriptor ;
   mask_t                co_flags ;
   union xsockaddr       co_remote_address ;
} ;

#define CONN_CLOSE( cp ) { Sclose( (cp)->co_descriptor ); (cp)->co_descriptor = -1; }

#define COP( p )       ((connection_s *)(p))
#define CONN_NULL      COP( NULL )

/*
 * Field access macros
 */
#define CONN_DESCRIPTOR( cp )       (cp)->co_descriptor
#define CONN_SERVICE( cp )          (cp)->co_sp
#define CONN_SET_FLAG( cp, flag )   M_SET( (cp)->co_flags, flag )
#define CONN_SET_DESCRIPTOR( cp, fd )   (cp)->co_descriptor = (fd)

#define CONN_SETADDR( cp, sinp )               \
   {                        \
      CONN_SET_FLAG( cp, COF_HAVE_ADDRESS ) ;         \
      memcpy(((cp)->co_remote_address.pad), sinp, sizeof(*sinp) ); \
   }

#define CONN_ADDRESS( cp )                     \
   (                           \
      M_IS_SET( (cp)->co_flags, COF_HAVE_ADDRESS )         \
         ? &((cp)->co_remote_address.sa)    \
         : SA(NULL)               \
   )
#define CONN_XADDRESS( cp )                     \
   (                           \
      M_IS_SET( (cp)->co_flags, COF_HAVE_ADDRESS )         \
         ? &((cp)->co_remote_address)    \
         : NULL               \
   )

connection_s *conn_new(struct service *sp);
void conn_free(connection_s *cp, int);
void conn_dump(const connection_s *cp,int fd);
const char *conn_addrstr( const connection_s *cp );

#endif   /* CONNECTION_H */

