/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef SCONF_H
#define SCONF_H

/*
 * $Id: sconf.h,v 1.1.1.3 2002/10/02 21:07:29 bbraun Exp $
 */
#include "config.h"
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include "libportable.h"

#ifdef HAVE_DNSREGISTRATION
#include <DNSServiceDiscovery/DNSServiceDiscovery.h>
#endif

#include "pset.h"
#include "m_env.h"
#include "mask.h"
#include "defs.h"
#include "log.h"
#include "builtins.h"
#include "attr.h"

/*
 * Service types
 */
#define ST_RPC          1
#define ST_INTERNAL     2
#define ST_UNLISTED     3
#define ST_SPECIAL      4
#define ST_DISABLED     5 
#define ST_TCPMUX	6
#define ST_TCPMUXPLUS	7

/*
 * Service flags
 */
#define SF_INTERCEPT    1
#define SF_REUSE        2
#define SF_NORETRY      3
#define SF_IDONLY       4
#define SF_NAMEINARGS   5
#define SF_NODELAY      6
#define SF_KEEPALIVE    7
#define SF_NOLIBWRAP    8
#define SF_SENSOR       9
#define SF_IPV4         10
#define SF_IPV6         11

/*
 * Values for log options
 */
#define LO_HOST      1
#define LO_DURATION  3
#define LO_ATTEMPT   4
#define LO_EXIT      5
#define LO_RECORD    6
#define LO_PID       7
#define LO_USERID    8

struct rpc_data
{
   unsigned long rd_min_version ;
   unsigned long rd_max_version ;
   unsigned long rd_program_number ;
} ;

#define RD_MINVERS( rdp )      (rdp)->rd_min_version
#define RD_MAXVERS( rdp )      (rdp)->rd_max_version
#define RD_PROGNUM( rdp )      (rdp)->rd_program_number

typedef enum { NO_ENV = 0, STD_ENV, DEF_ENV, CUSTOM_ENV } environ_e ;

struct environment
{
   environ_e  env_type ;
   env_h      env_handle ;
} ;

/*
 * NOTE: Clearing the structure will give all its fields their default values
 */
struct service_config
{
   mask_t               sc_specified_attributes; /* specified attributes */
   mask_t               sc_attributes_present; /* includes those from defaults*/
   mask_t               sc_type;               /* RPC, UNLISTED etc           */
   mask_t               sc_xflags ;            /* INTERCEPT etc               */
   char                *sc_name;               /* e g  "echo"                 */
   char                *sc_id ;                /* e.g. "echo-stream"          */
   uint16_t             sc_port ;              /* in host byte order          */
   int                  sc_socket_type ;       /* e.g. SOCK_DGRAM             */
   struct name_value    sc_protocol ;          /* e.g. "TCP", IPPROTO_TCP     */
   boolean_e            sc_wait ;
   uid_t                sc_uid ;
   gid_t                sc_user_gid ;          /* gid corresponding to uid    */
   gid_t                sc_gid ;               /* gid corresponding to group  */
   char                *sc_server ;
   char               **sc_server_argv ;
   int                  sc_instances ;
   int                  sc_nice ;              /* argument for nice(3) */
   pset_h               sc_env_var_defs ;      /* list of env strings         */
   pset_h               sc_pass_env_vars ;     /* env vars to pass to server  */
   pset_h               sc_access_times ;
   pset_h               sc_only_from ;
   pset_h               sc_no_access ;
   mask_t               sc_log_on_success ;
   mask_t               sc_log_on_failure ;
   struct log           sc_log ;
   struct rpc_data      sc_rd ;
   pset_h               sc_disabled ;     /* used only by the default entry */
   pset_h               sc_enabled ;      /* used only by the default entry */
   struct environment   sc_environment ;
   const builtin_s     *sc_builtin ;
   union xsockaddr     *sc_redir_addr ;
   char                *sc_orig_bind_addr ; /* used only when dual stack */
   union xsockaddr     *sc_bind_addr ;
   boolean_e            sc_v6only;
   char                *sc_banner ;
   int                  sc_per_source ;
   boolean_e            sc_groups ;
   char                *sc_banner_success ;
   char                *sc_banner_fail ;
   double               sc_max_load ;
   time_t               sc_time_limit ;
   time_t               sc_time_conn ;
   time_t               sc_time_conn_max ;
   time_t               sc_time_wait ;
   time_t               sc_time_reenable ;
   rlim_t               sc_rlim_as;
   rlim_t               sc_rlim_cpu;
   rlim_t               sc_rlim_data;
   rlim_t               sc_rlim_rss;
   rlim_t               sc_rlim_stack;
   mode_t               sc_umask;
   int                  sc_deny_time;         /* Sensor deny access time:
                                                 -1: forever
                                                  0: never
                                                  X: X minutes          */
#ifdef HAVE_DNSREGISTRATION
   dns_service_discovery_ref sc_mdnscon;
#endif
} ;

#define SCP( p ) ((struct service_config *)(p))

/*
 * Field access macros
 */
#define SC_LOG( scp )            (&(scp)->sc_log)
#define SC_RPCDATA( scp )        (&(scp)->sc_rd)
#define SC_ENV( scp )            (&(scp)->sc_environment)
#define SC_PORT( scp )           (scp)->sc_port
#define SC_NICE( scp )           (scp)->sc_nice
#define SC_SOCKET_TYPE( scp )    (scp)->sc_socket_type
#define SC_ID( scp )             (scp)->sc_id
#define SC_NAME( scp )           (scp)->sc_name
#define SC_PROTOVAL( scp )       (scp)->sc_protocol.value
#define SC_PROTONAME( scp )      (scp)->sc_protocol.name
#define SC_INSTANCES( scp )      (scp)->sc_instances
#define SC_UID( scp )            (scp)->sc_uid
#define SC_SERVER( scp )         (scp)->sc_server
#define SC_SERVER_ARGV( scp )    (scp)->sc_server_argv
#define SC_ONLY_FROM( scp )      (scp)->sc_only_from
#define SC_NO_ACCESS( scp )      (scp)->sc_no_access
#define SC_ACCESS_TIMES( scp )   (scp)->sc_access_times
#define SC_LOG_ON_SUCCESS( scp ) (scp)->sc_log_on_success
#define SC_LOG_ON_FAILURE( scp ) (scp)->sc_log_on_failure
#define SC_RLIM_AS( scp )        (scp)->sc_rlim_as
#define SC_RLIM_CPU( scp )       (scp)->sc_rlim_cpu
#define SC_RLIM_DATA( scp )      (scp)->sc_rlim_data
#define SC_RLIM_RSS( scp )       (scp)->sc_rlim_rss
#define SC_RLIM_STACK( scp )     (scp)->sc_rlim_stack
#define SC_V6ONLY( scp )         (scp)->sc_v6only

/*
 * Field set macros
 */
#define SC_SET_PORT( scp, port ) (scp)->sc_port = (port)

/*
 * Predicate checking macros
 */
#define SC_FORKS( scp )           ( ! SC_IS_INTERNAL( scp ) ||   \
    BUILTIN_FORKS( (scp)->sc_builtin ) )
#define SC_WAITS( scp )           ( (scp)->sc_wait == YES )
#define SC_RETRY( scp )           ( M_IS_CLEAR( (scp)->sc_xflags, SF_NORETRY ) )
#define SC_MUST_IDENTIFY( scp )   M_IS_SET( (scp)->sc_xflags, SF_IDONLY )
#define SC_NAMEINARGS( scp )      M_IS_SET( (scp)->sc_xflags, SF_NAMEINARGS )
#define SC_NODELAY( scp )         M_IS_SET( (scp)->sc_xflags, SF_NODELAY )
#define SC_KEEPALIVE( scp )       M_IS_SET( (scp)->sc_xflags, SF_KEEPALIVE )
#define SC_NOLIBWRAP( scp )       M_IS_SET( (scp)->sc_xflags, SF_NOLIBWRAP )
#define SC_IS_INTERCEPTED( scp ) ( M_IS_SET( (scp)->sc_xflags, SF_INTERCEPT ) )
#define SC_SENSOR( scp )          M_IS_SET( (scp)->sc_xflags, SF_SENSOR )
#define SC_IPV4( scp )            M_IS_SET( (scp)->sc_xflags, SF_IPV4 )
#define SC_IPV6( scp )            M_IS_SET( (scp)->sc_xflags, SF_IPV6 )

#define SC_IS_RPC( scp )         ( M_IS_SET( (scp)->sc_type, ST_RPC ) )
#define SC_IS_INTERNAL( scp )    ( M_IS_SET( (scp)->sc_type, ST_INTERNAL ) )
#define SC_IS_SPECIAL( scp )     ( M_IS_SET( (scp)->sc_type, ST_SPECIAL ) )
#define SC_IS_UNLISTED( scp )    ( M_IS_SET( (scp)->sc_type, ST_UNLISTED ) )
#define SC_IS_DISABLED( scp )    ( M_IS_SET( (scp)->sc_type, ST_DISABLED ) )
#define SC_DISABLE(scp)          ( M_SET( (scp)->sc_type, ST_DISABLED ) )
#define SC_ENABLE(scp)           ( M_CLEAR( (scp)->sc_type, ST_DISABLED ) )
#define SC_IS_MUXCLIENT( scp )   ( M_IS_SET( (scp)->sc_type, ST_TCPMUX ) || \
				   M_IS_SET( (scp)->sc_type, ST_TCPMUXPLUS ) )
#define SC_IS_MUXPLUSCLIENT(scp) ( M_IS_SET( (scp)->sc_type, ST_TCPMUXPLUS ) )
#define SC_IS_TCPMUX( scp )	 ( (scp)->sc_builtin &&                     \
                                   (BUILTIN_HANDLER( (scp)->sc_builtin ) == \
				   (void *)tcpmux_handler ) )

#define LOGS_USERID( scp, flags ) \
   ( M_IS_SET( (scp)->flags, LO_USERID ) && SC_ACCEPTS_CONNECTIONS( scp ) )

#define LOGS_ANY( scp, flags )   ( ! M_ARE_ALL_CLEAR( (scp)->flags ) )

#define SC_LOGS_ON_SUCCESS( scp )        LOGS_ANY( scp, sc_log_on_success )
#define SC_LOGS_ON_FAILURE( scp )        LOGS_ANY( scp, sc_log_on_failure )
#define SC_LOGS_USERID_ON_FAILURE( scp ) LOGS_USERID( scp, sc_log_on_failure )
#define SC_LOGS_USERID_ON_SUCCESS( scp ) LOGS_USERID( scp, sc_log_on_success )
#define SC_LOGS_ON_EXIT( scp )  \
   ( M_IS_SET( (scp)->sc_log_on_success, LO_DURATION ) || \
      M_IS_SET( (scp)->sc_log_on_success, LO_EXIT ) )
#define SC_RECORDS( scp )    M_IS_SET( (scp)->sc_log_on_failure, LO_RECORD ) 
#define SC_LOGS_PID( scp )   M_IS_SET( (scp)->sc_log_on_success, LO_PID )
#define SC_LOGS_EXITS( scp ) M_IS_SET( (scp)->sc_log_on_success, LO_EXIT )
#define SC_LOGS_DURATION( scp ) \
   M_IS_SET( (scp)->sc_log_on_success, LO_DURATION )


#define SC_MUST_LISTEN( scp )   ( (scp)->sc_socket_type == SOCK_STREAM )

#define SC_ACCEPTS_CONNECTIONS( scp )     \
      ( (scp)->sc_wait == NO && (scp)->sc_socket_type == SOCK_STREAM )

#define SC_SPECIFIED( scp, attr )   \
               M_IS_SET( (scp)->sc_specified_attributes, (attr) )
#define SC_SPECIFY( scp, attr )     \
               {                                                     \
                  M_SET( (scp)->sc_specified_attributes, (attr) ) ;  \
                  SC_PRESENT( (scp), (attr) ) ;                      \
               }

#define SC_IS_PRESENT( scp, attr )  \
               M_IS_SET( (scp)->sc_attributes_present, (attr) )
#define SC_PRESENT( scp, attr )     \
               M_SET( (scp)->sc_attributes_present, (attr) )

#define SC_GETGID( scp ) ( SC_SPECIFIED( scp, A_GROUP ) \
   ? (scp)->sc_gid : (scp)->sc_user_gid )
#define SC_INTERNAL( scp, serp )  BUILTIN_INVOKE( (scp)->sc_builtin, serp )
#define SC_MAKE_EXTERNAL( scp )   M_CLEAR( (scp)->sc_type, ST_INTERNAL )

struct service_config *sc_alloc(char *name);
void sc_free(struct service_config *scp);
struct service_config *sc_make_special(const char *service_name,const builtin_s *bp,int instances);
void sc_dump(struct service_config *scp,int fd,int tab_level,bool_int is_defaults);
bool_int sc_different_confs(struct service_config *scp1,struct service_config *scp2);


#endif   /* SCONF_H */
