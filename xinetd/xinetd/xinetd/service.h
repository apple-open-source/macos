/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef SERVICE_H
#define SERVICE_H

#include "config.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <time.h>

#include "defs.h"
#include "pset.h"
#include "xlog.h"
#include "server.h"

/*
 * $Id: service.h,v 1.1.1.3 2002/10/02 21:07:30 bbraun Exp $
 */



/*
 * NOTE: A service can be disabled but not deleted if it has any servers
 *       running
 */
typedef enum                     /* service states */
   {
      SVC_NOT_STARTED = 0,       /* no attempt to start it yet       */
      SVC_ACTIVE,                /* service is available             */
      SVC_SUSPENDED,             /* service is suspended             */
      SVC_DISABLED               /* service disabled                 */
   } state_e ;


/*
 * NOTE: Clearing the structure will give all its fields their default values
 */
struct service
{
   state_e                svc_state ;
   int                    svc_ref_count ;   /* # of pters to this struct */
   struct service_config *svc_conf ;   /* service configuration */
   int                    svc_fd ;
   unsigned               svc_running_servers ;
   unsigned               svc_retry_servers ;
   unsigned               svc_attempts ; /* # of attempts to start server */
   int                    svc_not_generic ; /* 1 spec_service, 0 generic */

   /*
    * These fields are used to avoid generating too many messages when
    * receiving datagrams from a bad address.
    */
   union xsockaddr                        *svc_last_dgram_addr ;
   time_t                                  svc_last_dgram_time ;
   xlog_h                                  svc_log ;
} ;


#define SP( p )                  ( (struct service *) (p) )
#define SUSPEND( sp )          (sp)->svc_state = SVC_SUSPENDED
#define RESUME( sp )           (sp)->svc_state = SVC_ACTIVE


/*
 * Field access macros
 */
#define SVC_CONF( sp )             ( (sp)->svc_conf )
#define SVC_FD( sp )               ( (sp)->svc_fd )
#define SVC_RUNNING_SERVERS( sp )  (sp)->svc_running_servers
#define SVC_RETRIES( sp )          (sp)->svc_retry_servers
#define SVC_LOG( sp )              (sp)->svc_log
#define SVC_REFCOUNT( sp )         (sp)->svc_ref_count
#define SVC_ID( sp )               SC_ID( SVC_CONF( sp ) )
#define SVC_SOCKET_TYPE( sp )      SC_SOCKET_TYPE( SVC_CONF( sp ) )

#define SVC_IS_ACTIVE( sp )      ( (sp)->svc_state == SVC_ACTIVE )
#define SVC_IS_SUSPENDED( sp )   ( (sp)->svc_state == SVC_SUSPENDED )
#define SVC_IS_AVAILABLE( sp )   ( SVC_IS_ACTIVE(sp) || SVC_IS_SUSPENDED(sp) )
#define SVC_IS_DISABLED( sp )    ( (sp)->svc_state == SVC_DISABLED )
#define SVC_IS_MUXCLIENT( sp )   ( SC_IS_MUXCLIENT( SVC_CONF ( sp ) ) )
#define SVC_IS_MUXPLUSCLIENT(sp) ( SC_IS_MUXPLUSCLIENT( SVC_CONF ( sp ) ) )
#define SVC_IS_TCPMUX( sp )      ( SC_IS_TCPMUX( SVC_CONF ( sp ) ) )

#define TCPMUX_ACK "+Go\r\n"
/*
 * Predicate checking macros
 */
#define SVC_FORKS( sp )            SC_FORKS( SVC_CONF( sp ) )
#define SVC_RETRY( sp )            SC_RETRY( SVC_CONF( sp ) )
#define SVC_WAITS( sp )            SC_WAITS( SVC_CONF( sp ) )
#define SVC_IS_INTERCEPTED( sp )   SC_IS_INTERCEPTED( SVC_CONF( sp ) )
#define SVC_ACCEPTS_CONNECTIONS( sp )   \
                                   SC_ACCEPTS_CONNECTIONS( SVC_CONF( sp ) )

#define SVC_IS_LOGGING( sp )       ( (sp)->svc_log != NULL )
#define SVC_LOGS_ON_SUCCESS( sp )         \
      ( SVC_IS_LOGGING( sp ) && SC_LOGS_ON_SUCCESS( SVC_CONF( sp ) ) )
#define SVC_LOGS_ON_FAILURE( sp )         \
      ( SVC_IS_LOGGING( sp ) && SC_LOGS_ON_FAILURE( SVC_CONF( sp ) ) )
#define SVC_LOGS_ON_EXIT( sp )            \
      ( SVC_IS_LOGGING( sp ) && SC_LOGS_ON_EXIT( SVC_CONF( sp ) ) )
#define SVC_LOGS_USERID_ON_SUCCESS( sp )   \
      ( SVC_IS_LOGGING( sp ) && SC_LOGS_USERID_ON_SUCCESS( SVC_CONF( sp ) ) )
#define SVC_LOGS_USERID_ON_FAILURE( sp )   \
      ( SVC_IS_LOGGING( sp ) && SC_LOGS_USERID_ON_FAILURE( SVC_CONF( sp ) ) )
#define SVC_RECORDS( sp )                  \
      ( SVC_IS_LOGGING( sp ) && SC_RECORDS( SVC_CONF( sp ) ) )

/*
 * Reference counting macros
 */
#define SVC_HOLD( sp )            (sp)->svc_ref_count++
#define SVC_RELE( sp )   \
   ( ( (sp)->svc_ref_count <= 1 ) ? svc_release( sp ) : --(sp)->svc_ref_count )


#define SVC_INTERNAL( sp, serp )     SC_INTERNAL( SVC_CONF( sp ), serp )
#define SVC_MAKE_EXTERNAL( sp )      SC_MAKE_EXTERNAL( SVC_CONF( sp ) )

#define SVC_DEC_RUNNING_SERVERS( sp )                                         \
   {                                                                          \
      if ( SVC_RUNNING_SERVERS( sp ) != 0 )                                   \
         (sp)->svc_running_servers-- ;                                        \
      else                                                                    \
         msg( LOG_ERR, func,                                                  \
            "Service %s: server exit with 0 running servers", SVC_ID( sp ) ) ;\
   }

#define SVC_INC_RUNNING_SERVERS( sp )       (sp)->svc_running_servers++

#define SVC_INC_RETRIES( sp )               (sp)->svc_retry_servers++
#define SVC_DEC_RETRIES( sp )               (sp)->svc_retry_servers--

struct service *svc_new(struct service_config *scp);
struct service *svc_make_special(struct service_config *scp);
void svc_free(struct service *sp);
status_e svc_activate(struct service *sp);
void svc_deactivate(struct service *sp);
void svc_suspend(struct service *sp);
void svc_resume(struct service *sp);
int svc_release(struct service *sp);
void svc_dump(const struct service *sp,int fd);
void svc_request(struct service *sp);
status_e svc_generic_handler( struct service *sp, connection_s *cp );
status_e svc_parent_access_control(struct service *sp,connection_s *cp);
status_e svc_child_access_control(struct service *sp,connection_s *cp);
void svc_postmortem(struct service *sp,struct server *serp);


#endif   /* SERVICE_H */
