/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef INT_H
#define INT_H

/*
 * $Id: int.h,v 1.1.1.3 2003/05/22 01:16:36 rbraun Exp $
 */

#include <sys/types.h>
#include <netinet/in.h>

#include "pset.h"
#include "defs.h"
#include "server.h"

typedef enum { GOOD_CHANNEL, BAD_CHANNEL } channel_state_e ;

struct channel
{
   channel_state_e      ch_state ;
   union xsockaddr      ch_from ;
   int                  ch_local_socket ;
   int                  ch_remote_socket ;
} ;

typedef struct channel channel_s ;

#define CHP( p )                 ((struct channel *)(p))

#define CHANNEL_NULL             CHP( NULL )

#define NEW_CHANNEL()            NEW( channel_s )
#define FREE_CHANNEL( chp )      FREE( chp )


struct intercept_common
{
   bool_int             ic_intercept ;
   int                  ic_remote_socket ;
   union xsockaddr      ic_local_addr ;
   pset_h               ic_connections ;
   struct server        ic_server ;
} ;


struct intercept_ops
{
   void (*mux)() ;
   void (*exit)() ;
} ;


struct intercept_s
{
   int                        int_socket_type ;
   struct intercept_common    int_common ;
   void                      *int_priv ;
   const struct intercept_ops *int_ops ;
} ;

#define INT_SERVER( p )              (&(p)->int_common.ic_server)
#define INT_LOCALADDR( p )           (&(p)->int_common.ic_local_addr)
#define INT_REMOTE( p )              ((p)->int_common.ic_remote_socket)
#define INT_CONNECTIONS( p )         ((p)->int_common.ic_connections)
#define INTERCEPT( p )               ((p)->int_common.ic_intercept)

void intercept(struct server *serp);
void int_sighandler(int sig);

#endif   /* INT_H */
