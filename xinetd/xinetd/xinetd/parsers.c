/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <ctype.h>
#include <sys/file.h>
#include <syslog.h>
#include <netdb.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h> /* For LONG_MIN and LONG_MAX */
#include <errno.h>
#ifdef HAVE_LOADAVG
#include <stdio.h>
#endif

#if defined(hpux) && !defined(X_OK)
#define X_OK 1
#endif

#include "str.h"
#include "parsers.h"
#include "msg.h"
#include "nvlists.h"
#include "env.h"
#include "xconfig.h"
#include "addr.h"
#include "libportable.h"
#include "timex.h"
#include "addr.h"	/* check_hostname() */

#define NEW_SET( set, v1, v2 )                 \
   if ( (set) == NULL && \
   ( (set) = pset_create( (v1), (v2) ) ) == NULL )   \
   {   \
      out_of_memory( func ) ;   \
      return( FAILED ) ;   \
   }

static void missing_attr_msg(const char *par, const char *item)
{
       parsemsg( LOG_WARNING, par, 
          "attribute %s expects at least 1 value and none were given.",
	   item );
}


/*
 * Find the flags corresponding to strings in "values" and apply
 * them to "*maskp" (apply means add or remove depending on "op")
 * "description" describes the type of flags.
 */
static status_e parse_value_list( pset_h values, 
                                   mask_t *maskp, 
                                   const struct name_value list[], 
                                   enum assign_op op, 
                                   const char *description )
{
   unsigned            u ;
   const struct name_value  *nvp ;
   const char               *func = "parse_value_list" ;

   for ( u=0; u<pset_count( values ); u++ )
   {
      const char *name = (char *) pset_pointer( values, u ) ;

      nvp = nv_find_value( list, name ) ;
      if ( nvp != NULL )
      {
         if ( op == PLUS_EQ )
            M_SET( *maskp, nvp->value ) ;
         else
            M_CLEAR( *maskp, nvp->value ) ;
      }
      else
      {
         parsemsg( LOG_WARNING, func, "Bad %s: %s", description, name ) ;
	 return( FAILED );
      }
   }
   return( OK ) ;
}


status_e type_parser( pset_h values, 
                      struct service_config *scp, 
                      enum assign_op op )
{
   if ( pset_count( values ) >= 1 )
   {
      return( parse_value_list( values,
                  &scp->sc_type, service_types, PLUS_EQ, "service type" ) ) ;
    }
    else
    {
       missing_attr_msg("type_parser", "type");
       return FAILED ;
    }
}


status_e flags_parser( pset_h values, 
                       struct service_config *scp, 
                       enum assign_op op )
{
   if ( pset_count( values ) >= 1 )
   {
      return( parse_value_list( values,
                  &scp->sc_xflags, service_flags, PLUS_EQ, "service flag" ) ) ;
    }
    else
    {
       missing_attr_msg("flags_parser", "flags");
       return FAILED ;
    }
}


status_e socket_type_parser( const pset_h values, 
                             struct service_config *scp, 
                             enum assign_op op )
{
   const struct name_value *nvp ;
   const char *type = (char *) pset_pointer( values, 0 ) ;
   const char *func = "socket_type_parser" ;

   nvp = nv_find_value( socket_types, type ) ;
   if ( nvp != NULL )
   {
      scp->sc_socket_type = nvp->value ;
      return( OK ) ;
   }
   else
   {
      parsemsg( LOG_ERR, func, "Bad socket type: %s", type ) ;
      return( FAILED ) ;
   }
}


status_e rpc_version_parser( pset_h values, 
                             struct service_config *scp, 
                             enum assign_op op )
{
   struct rpc_data *rdp = SC_RPCDATA( scp ) ;
   char *version = (char *) pset_pointer( values, 0 ) ;
   int min_version=0, max_version=0;
   char *p = strchr( version, '-' ) ;
   const char *func = "rpc_version_parser" ;

   if ( p == NULL )
   {
      if ( parse_base10(version, &min_version) )
         max_version = min_version - 1;
      else
         max_version = min_version;
   }
   else
   {
      *p = NUL ;
      if ( parse_base10(version, &min_version) ||
           parse_base10(p+1, &max_version) )
         max_version = min_version - 1;
   }
   if ( min_version > max_version )
   {
      parsemsg( LOG_ERR, func, "bad version range: %s", version ) ;
      return( FAILED ) ;
   }
   rdp->rd_min_version = min_version;
   rdp->rd_max_version = max_version;
   return( OK ) ;
}


status_e rpc_number_parser( pset_h values, 
                            struct service_config *scp, 
                            enum assign_op op )
{
   int num;

   if ( parse_base10((char *) pset_pointer( values, 0 ), &num) ) {
      parsemsg(LOG_ERR, "rpc_number_parser", "Error parsing: %s", 
         (char *)pset_pointer( values, 0 ));
      return( FAILED );
   }
   SC_RPCDATA( scp )->rd_program_number = num;
   return( OK ) ;
}


status_e protocol_parser( pset_h values, 
                          struct service_config *scp, 
                           enum assign_op op )
{
   char *proto_name = (char *) pset_pointer( values, 0 ) ;
   struct protoent *pep ;
   const char *func = "protocol_parser" ;

   if ( ( pep = getprotobyname( proto_name ) ) == NULL )
   {
      parsemsg( LOG_ERR, func, 
               "Protocol %s not in /etc/protocols", proto_name ) ;
      return( FAILED ) ;
   }

   scp->sc_protocol.name = new_string( proto_name ) ;
   if ( scp->sc_protocol.name == NULL )
   {
      out_of_memory( func ) ;
      return( FAILED ) ;
   }
   scp->sc_protocol.value = pep->p_proto ;
   return( OK ) ;
}


status_e wait_parser( pset_h values, 
                      struct service_config *scp, 
                      enum assign_op op )
{
   char *val = (char *) pset_pointer( values, 0 ) ;
   const char *func = "wait_parser" ;

   if ( EQ( val, "yes" ) )
      scp->sc_wait = YES ;
   else if ( EQ( val, "no" ) )
      scp->sc_wait = NO ;
   else
   {
      parsemsg( LOG_ERR, func, "Bad value for wait: %s", val ) ;
      return( FAILED );
   }
   return( OK ) ;
}


status_e user_parser( pset_h values, 
                      struct service_config *scp, 
                      enum assign_op op )
{
   char *user = (char *) pset_pointer( values, 0 ) ;
   struct passwd *pw ;
   const char *func = "user_parser" ;

   pw = getpwnam( user ) ;
   if ( pw == NULL )
   {
      parsemsg( LOG_ERR, func, "Unknown user: %s", user ) ;
      return( FAILED ) ;
   }
   str_fill( pw->pw_passwd, ' ' );
   scp->sc_uid = pw->pw_uid ;
   scp->sc_user_gid = pw->pw_gid ;
   return( OK ) ;
}


status_e group_parser( pset_h values, 
                       struct service_config *scp, 
                       enum assign_op op )
{
   char *group_ptr = (char *) pset_pointer( values, 0 ) ;
   struct group *grp ;
   const char *func = "group_parser" ;

   grp = getgrnam( group_ptr ) ;
   if ( grp == NULL )
   {
      parsemsg( LOG_ERR, func, "Unknown group: %s", group_ptr ) ;
      return( FAILED ) ;
   }
   
   scp->sc_gid = grp->gr_gid ;
   return( OK ) ;
}


status_e svcdisable_parser( pset_h values, 
                            struct service_config *scp, 
                            enum assign_op op )
{
   const char *func = "svcdisable_parser" ;
   char *val = (char *) pset_pointer( values, 0 ) ;
   
   if( EQ( val, "yes" ) )
      SC_DISABLE( scp );
   else if( EQ( val, "no" ) ) 
      SC_ENABLE( scp );
   else
   {
      parsemsg( LOG_ERR, func, "Bad value: %s", val ) ;
      return( FAILED );
   }

   return( OK );
}


status_e groups_parser( pset_h values, 
                        struct service_config *scp, 
                        enum assign_op op )
{
   char *val = (char *) pset_pointer( values, 0 ) ;
   const char *func = "groups_parser" ;

   if ( EQ( val, "yes" ) )
      scp->sc_groups = YES ;
   else if ( EQ( val, "no" ) )
      scp->sc_groups = NO ;
   else
   {
      parsemsg( LOG_ERR, func, "Bad value for groups: %s", val ) ;
      return( FAILED );
   }

   return( OK );
}


status_e v6only_parser( pset_h values, 
                        struct service_config *scp, 
                        enum assign_op op )
{
   char *val = (char *) pset_pointer( values, 0 );
   const char *func = "v6only_parser" ;

   if ( EQ( val, "yes" ) )
      scp->sc_v6only = YES;
   else if ( EQ( val, "no" ) )
      scp->sc_v6only = NO;
   else
   {
      parsemsg( LOG_ERR, func, "Bad value for v6only: %s", val );
      return( FAILED );
   }
   return( OK );
}


status_e server_parser( pset_h values, 
                        struct service_config *scp, 
                        enum assign_op op )
{
   char *server = (char *) pset_pointer( values, 0 ) ;
   const char *func = "server_parser" ;
   struct stat sb;

   /* 
    * Access is used so that the real user ID permissions
    * are checked.
    */
   if ( access( server, X_OK ) == -1 )
   {
      parsemsg( LOG_ERR, func, "Server %s is not executable", server ) ;
      return( FAILED ) ;
   }
   if (stat(server, &sb) == -1)
   {
      parsemsg( LOG_ERR, func, "Unable to stat: %s.", server ) ;
      return( FAILED ) ;
   }

   scp->sc_server = new_string( server ) ;
   if ( scp->sc_server == NULL )
   {
      out_of_memory( func ) ;
      return( FAILED ) ;
   }
   return( OK ) ;
}


status_e server_args_parser( pset_h values, 
                             struct service_config *scp, 
                             enum assign_op op )
{
   char **argv ;
   unsigned u ;
   unsigned i ;
   unsigned count;
   unsigned argv_index ;
   unsigned n_args = pset_count( values ) ;
   const char *func = "server_args_parser" ;

   /*
    * Create the argv for a future exec call
    * Reserve space for the server. We cannot use scp->sc_server
    * since it may not have a value yet.
    */
   argv = argv_alloc( n_args+1 ) ;
   count = pset_count( values );
   if ( count == 0 )
   {
       missing_attr_msg("server_args_parser", "server_args");
       return FAILED;
   }
   
   if( SC_NAMEINARGS( scp ) )
   {
      for (u = 0; u < count; u++)
      {
         register char *s = new_string( (char *) pset_pointer( values, u )) ;

         if ( s == NULL )
         {
             for ( i = 1 ; i < u ; i++ )
               free( argv[ i ] ) ;
             free( (char *) argv ) ;
             out_of_memory( func ) ;
             return( FAILED ) ;
         }
         argv[ u ] = s ;
      }
   }
   else
   {
      for (u = 0, argv_index = 1 ; u < count; u++, argv_index++)
      {
         register char *s = new_string((char *) pset_pointer( values, u )) ;

         if ( s == NULL )
         {
             for ( i = 1 ; i < argv_index ; i++ )
               free( argv[ i ] ) ;
             free( (char *) argv ) ;
             out_of_memory( func ) ;
             return( FAILED ) ;
         }
         argv[ argv_index ] = s ;
      }
      argv[ argv_index ] = argv[ 0 ] = NULL ;
   }
   scp->sc_server_argv = argv ;
   return( OK ) ;
}


status_e instances_parser( pset_h values, 
                           struct service_config *scp, 
                           enum assign_op op )
{
   char *instances = (char *) pset_pointer( values, 0 ) ;
   const char *func = "instances_parser" ;

   if ( EQ( instances, "UNLIMITED" ) )
      scp->sc_instances = UNLIMITED ;
   else
   {
      if ( parse_base10(instances, &scp->sc_instances) ||
           scp->sc_instances < 0 )
      {
         parsemsg( LOG_ERR, func,
            "Number of instances is invalid: %s", instances ) ;
         return( FAILED ) ;
      }
   }
   return( OK ) ;
}


status_e per_source_parser( pset_h values, 
                            struct service_config *scp, 
                            enum assign_op op )
{
   char *per_source = (char *) pset_pointer( values, 0 ) ;
   const char *func = "per_source_parser" ;

   if ( EQ( per_source, "UNLIMITED" ) )
      scp->sc_per_source = UNLIMITED;
   else
   {
      if ( parse_base10(per_source, &scp->sc_per_source) ||
           scp->sc_per_source < 0 )
      {
         parsemsg( LOG_ERR, func, "Number of per source instances is invalid: %s", per_source ) ;
         return( FAILED );
      }
   }
   return(OK);
}


status_e cps_parser( pset_h values, 
                     struct service_config *scp, 
                     enum assign_op op )
{
   char *cps = (char *) pset_pointer(values, 0);
   char *waittime = (char *) pset_pointer(values, 1);
   int waittime_int, conn_max;

   if( cps == NULL || waittime == NULL ) {
      parsemsg(LOG_ERR, "cps_parser", "NULL options specified in cps");
      return( FAILED );
   }
   if( parse_base10(cps, &conn_max) ) {
      parsemsg(LOG_ERR, "cps_parser", "cps argument not a number");
      scp->sc_time_conn_max = 0;
      scp->sc_time_wait = 0;
      return( FAILED );
   }
   if( parse_base10(waittime, &waittime_int) ) {
      parsemsg(LOG_ERR, "cps_parser", "cps time argument not a number");
      scp->sc_time_conn_max = 0;
      scp->sc_time_wait = 0;
      return( FAILED );
   }
   scp->sc_time_wait = waittime_int;
   scp->sc_time_conn_max = conn_max;

   if( scp->sc_time_conn_max < 0 || scp->sc_time_wait < 0 ) {
      parsemsg(LOG_ERR, "cps_parser", "cps arguments invalid");
      scp->sc_time_conn_max = 0;
      scp->sc_time_wait = 0;
      return( FAILED );
   }

   return(OK);
}

status_e id_parser( pset_h values, 
                    struct service_config *scp, 
                    enum assign_op op )
{
   const char *func = "id_parser" ;

   scp->sc_id = new_string( (char *) pset_pointer( values, 0 ) ) ;
   if ( scp->sc_id != NULL )
      return( OK ) ;
   out_of_memory( func ) ;
   return( FAILED ) ;
}



#define PORT_BITS            16
#define PORT_MAX            ( 1 << PORT_BITS )

status_e port_parser( pset_h values, 
                      struct service_config *scp, 
                      enum assign_op op )
{
   int port;
   const char *func = "port_parser" ;

   if ( parse_base10((char *) pset_pointer( values, 0 ), &port) ||
        port < 0 || port >= PORT_MAX )
   {
      parsemsg( LOG_ERR, func, "port number is invalid" ) ;
      return( FAILED ) ;
   }
   scp->sc_port = (uint16_t)port ;
   return( OK ) ;
}


static status_e add_new_string( pset_h set, char *str )
{
   char *p = new_string( str ) ;
   const char *func = "add_new_string" ;

   if ( p == NULL )
   {
      parsemsg( LOG_CRIT, func, ES_NOMEM ) ;
      return( FAILED ) ;
   }
   if ( pset_add( set, p ) == NULL )
   {
      free( p ) ;
      parsemsg( LOG_CRIT, func, ES_NOMEM ) ;
      return( FAILED ) ;
   }
   return( OK ) ;
}


status_e env_parser( pset_h values, 
                     struct service_config *scp, 
                     enum assign_op op )
{
   unsigned u ;
   const char *func = "env_parser" ;

   if ( op == MINUS_EQ )
   {
      parsemsg( LOG_WARNING, func,
         "operator '-=' not supported for env attribute" ) ;
      return( FAILED ) ;
   }

   NEW_SET( scp->sc_env_var_defs, 5, 5 ) ;

   if ( op == SET_EQ && pset_count( scp->sc_env_var_defs ) != 0 )
   {
      pset_apply( scp->sc_env_var_defs, free, NULL ) ;
      pset_clear( scp->sc_env_var_defs ) ;
   }

   for ( u = 0 ; u < pset_count( values ) ; u++ )
   {
      char *str = (char *) pset_pointer( values, u ) ;

      /*
       * Check if the string contains an '='
       */
      if ( strchr( str, '=' ) == NULL )
      {
         parsemsg( LOG_ERR, func, "%s has no '='", str ) ;
         return( FAILED ) ;
      }

      if ( add_new_string( scp->sc_env_var_defs, str ) == FAILED )
         return( FAILED ) ;
   }
   return( OK ) ;
}


status_e passenv_parser( pset_h values, 
                         struct service_config *scp, 
                         enum assign_op op )
{
   pset_h var_set ;
   unsigned u ;
   const char *func = "passenv_parser" ;

   NEW_SET( scp->sc_pass_env_vars, 0, 0 ) ;

   var_set = scp->sc_pass_env_vars ;

   if ( op == SET_EQ )
   {
      pset_apply( var_set, free, NULL ) ;
      pset_clear( var_set ) ;
      op = PLUS_EQ ;
   }

   for ( u = 0 ; u < pset_count( values ) ; u++ )
   {
      char *env_var = (char *) pset_pointer( values, u ) ;
      unsigned v ;
      boolean_e found ;

      /*
       * Check if it is already there
       */
      for ( found = NO, v = 0 ; v < pset_count( var_set ) ; v++ )
         if ( EQ( env_var, (char *) pset_pointer( var_set, v ) ) )
         {
            found = YES ;
            break ;
         }
      
      if ( ((op == MINUS_EQ) && (found == NO)) || ((op != MINUS_EQ) && (found == YES)) )
         continue ;
      
      if ( op == MINUS_EQ )
      {
         free( (char *) pset_pointer( var_set, v ) ) ;
         pset_remove_index( var_set, v ) ;
      }
      else
      {
         if ( env_lookup( std_env, env_var ) == CHAR_NULL )
         {
            parsemsg( LOG_WARNING, func,
               "undefined environment variable: %s", env_var ) ;
            continue ;
         }

         if ( add_new_string( var_set, env_var ) == FAILED )
            return( FAILED ) ;
      }
   }
   return( OK ) ;
}


status_e disabled_parser( pset_h values, 
                          struct service_config *scp, 
                          enum assign_op op )
{
   unsigned u ;
   const char *func = "disabled_parser" ;

   NEW_SET( scp->sc_disabled, pset_count( values ), 0 ) ;
   
   for ( u = 0 ; u < pset_count( values ) ; u++ )
   {
      char *name = (char *) pset_pointer( values, u ) ;

      if ( add_new_string( scp->sc_disabled, name ) == FAILED )
         return( FAILED ) ;
   }
   return( OK ) ;
}


status_e enabled_parser( pset_h values, 
                         struct service_config *scp, 
                         enum assign_op op )
{
   unsigned u ;
   const char *func = "enabled_parser" ;

   NEW_SET( scp->sc_enabled, pset_count( values ), 0 ) ;
   
   for ( u = 0 ; u < pset_count( values ) ; u++ )
   {
      char *name = (char *) pset_pointer( values, u ) ;

      if ( add_new_string( scp->sc_enabled, name ) == FAILED )
         return( FAILED ) ;
   }
   return( OK ) ;
}

/*
 * Interpret a number of the form: <num>[m|M|k|K]
 * m and M mean megabytes, k and K mean kilobytes, nothing means bytes
 */
static int get_limit( char *limit_str, rlim_t *res )
{
   int limit_int;
   int multiplier;
   char *p;

   if (*limit_str == NUL) {
      *res = 0;
      return -1;
   }

   p = limit_str + strlen( limit_str ) - 1;
   while ( p > limit_str && isspace( *p ) )
      p--;

   if (*p == 'k' || *p == 'K') {
      *p = NUL;
      multiplier = 1024;
   } else if (*p == 'm' || *p == 'M') {
      *p = NUL;
      multiplier = 1024 * 1024;
   } else
      multiplier = 1;

   if (parse_base10(limit_str, &limit_int)) {
      *res = 0;
      return -1;
   }

   *res = (rlim_t)limit_int * multiplier;
   if (*res / multiplier != limit_int) {
      *res = 0;
      return -1;
   }

   return 0;
}


static status_e parse_filelog( struct filelog *flp, pset_h values )
{
   rlim_t      soft_limit ;
   rlim_t      hard_limit ;
   char         *file ;
   unsigned      count = pset_count( values ) ;
   const char    *func = "parse_filelog" ;

   if ( count < 2 || count > 4 )
   {
      parsemsg( LOG_ERR, func, "wrong number of arguments" ) ;
      return( FAILED ) ;
   }

   file = new_string( (char *) pset_pointer( values, 1 ) ) ;
   if ( file == NULL )
   {
      out_of_memory( func ) ;
      return( FAILED ) ;
   }

   /*
    * Get the limits, if any
    */
   if ( count > 2 )
   {
      if ( get_limit( (char *) pset_pointer( values, 2 ), &soft_limit ) )
      {
         parsemsg( LOG_ERR, func, "soft limit is invalid" ) ;
         free( file ) ;
         return( FAILED ) ;
      }

      /*
       * If a hard limit was specified check that it is at least equal 
       * to the soft limit. If no hard limit was specified, determine
       * it from the formula:
       *      hard = soft + x
       * where 
       *      min( 1%soft,LOG_EXTRA_MIN ) <= x <= max( 1%soft,LOG_EXTRA_MAX )
       */
      if ( count == 4 )
      {
         if ( get_limit( (char *) pset_pointer( values, 3 ), &hard_limit) )
         {
            parsemsg( LOG_ERR, func, "hard limit is invalid" ) ;
            free( file ) ;
            return( FAILED ) ;
         }
         if ( hard_limit < soft_limit )
         {
            parsemsg( LOG_ERR, func,
               "hard limit (%lu) is less than soft limit (%lu)",
                     (unsigned long)hard_limit, (unsigned long)soft_limit ) ;
            free( file ) ;
            return( FAILED ) ;
         }
      }
      else
      {
         unsigned extra = soft_limit / 100 ;      /* 1% of soft limit */

         if ( extra < LOG_EXTRA_MIN )
            extra = LOG_EXTRA_MIN ;
         else if ( extra > LOG_EXTRA_MAX )
            extra = LOG_EXTRA_MAX ;
         hard_limit = soft_limit + extra ;
      }
      flp->fl_soft_limit = soft_limit ;
      flp->fl_hard_limit = hard_limit ;
   }
   flp->fl_filename = file ;
   return( OK ) ;
}


static status_e parse_syslog( struct syslog *slp, pset_h values )
{
   const char               *facility ;
   const char               *level ;
   const struct name_value  *nvp ;
   unsigned                  count = pset_count( values ) ;
   const char               *func  = "parse_syslog" ;

   if ( count < 2 || count > 3 )
   {
      parsemsg( LOG_ERR, func, "wrong number of arguments" ) ;
      return( FAILED ) ;
   }

   facility = (char *) pset_pointer( values, 1 ) ;
   if ( ( nvp = nv_find_value( syslog_facilities, facility ) ) == NULL )
   {
      parsemsg( LOG_ERR, func, "Unknown syslog facility: %s", facility ) ;
      return( FAILED ) ;
   }
   slp->sl_facility = nvp->value ;

   if ( count == 3 )
   {
      level = (char *) pset_pointer( values, 2 ) ;
      if ( ( nvp = nv_find_value( syslog_levels, level ) ) == NULL )
      {
         parsemsg( LOG_ERR, func, "Unknown syslog level: %s", level ) ;
         return( FAILED ) ;
      }
      slp->sl_level = nvp->value ;
   }
   else
      slp->sl_level = DEFAULT_SERVICE_SYSLOG_LEVEL ;

   return( OK ) ;
}


status_e log_type_parser( pset_h values, 
                          struct service_config *scp, 
                          enum assign_op op )
{
   struct log *lp = SC_LOG( scp ) ;
   char *type ;
   const char *func = "parse_log_type" ;
   int count = pset_count( values);

   if ( count == 0 )
   {
      missing_attr_msg(func, "log_type");
      return( FAILED );
   }
   
   type = (char *) pset_pointer( values, 0 ) ;

   if ( EQ( type, "FILE" ) )
   {
      if ( parse_filelog( LOG_GET_FILELOG( lp ), values ) == FAILED )
         return( FAILED ) ;
      lp->l_type = L_FILE ;
   }
   else if ( EQ( type, "SYSLOG" ) )
   {
      if ( parse_syslog( LOG_GET_SYSLOG( lp ), values ) == FAILED )
         return( FAILED ) ;
      lp->l_type = L_SYSLOG ;
   }
   else
   {
      parsemsg( LOG_ERR, func, "Unknown log type: %s", type ) ;
      return( FAILED ) ;
   }
   return( OK ) ;
}


static status_e parse_log_flags( pset_h values, 
                                  enum assign_op op, 
                                  mask_t *maskp, 
                                  const struct name_value options[], 
                                  char *name )
{
   if ( op == SET_EQ )
   {
      M_CLEAR_ALL( *maskp ) ;
      op = PLUS_EQ ;
   }

   return( parse_value_list( values, maskp, options, op, name ) ) ;
}


status_e log_on_success_parser( pset_h values, 
                                struct service_config *scp, 
                                enum assign_op op )
{
   return( parse_log_flags( values, op,
      &scp->sc_log_on_success, success_log_options, "log-on-success flag" ) ) ;
}


status_e log_on_failure_parser( pset_h values, 
                                struct service_config *scp, 
                                enum assign_op op )
{
   return( parse_log_flags( values, op,
      &scp->sc_log_on_failure, failure_log_options, "log-on_failure flag" ) ) ;
}


static status_e parse_inet_addresses( pset_h values, 
                                       enum assign_op op, 
                                       pset_h *addr_list )
{
   unsigned             u ;
   pset_h               addr_set ;
   statfunc             addrlist_func ;
   const char          *func = "parse_inet_addresses" ;

   NEW_SET( *addr_list, 0, 0 );

   addr_set = *addr_list;

   /*
    * If the op was '=' clear the existing list of addresses
    */
   if ( op == SET_EQ )
   {
      op = PLUS_EQ ;
      addrlist_free( addr_set ) ;
      pset_clear( addr_set ) ;
   }

   addrlist_func = ( op == PLUS_EQ ) ? addrlist_add : addrlist_remove ;

   for ( u = 0 ; u < pset_count( values ) ; u++ )
   {
      register char *str_addr = (char *) pset_pointer( values, u ) ;

      if ( (*addrlist_func)( addr_set, str_addr ) == FAILED )
      {
         parsemsg( LOG_ERR, func, "Failed adding: %s", str_addr ) ;
         return( FAILED );
      }
   }
   return( OK ) ;
}


status_e only_from_parser( pset_h values, 
                           struct service_config *scp, 
                           enum assign_op op )
{
   return( parse_inet_addresses( values, op, &scp->sc_only_from ) ) ;
}


status_e no_access_parser( pset_h values, 
                           struct service_config *scp, 
                           enum assign_op op )
{
   return( parse_inet_addresses( values, op, &scp->sc_no_access ) ) ;
}


status_e banner_parser(pset_h values, 
                       struct service_config *scp, 
                       enum assign_op op)
{
   const char *func = "banner_parser";

   if( pset_pointer(values, 0) == NULL )
   {
      msg(LOG_ERR, func, "pset_pointer returned NULL");
      return( FAILED );
   }

   scp->sc_banner = new_string( pset_pointer(values,0) );
   if( scp->sc_banner == NULL ) {
      msg(LOG_ERR, func, ES_NOMEM);
      return( FAILED );
   }

   return OK;
}

status_e banner_success_parser(pset_h values, 
                               struct service_config *scp, 
                               enum assign_op op)
{
   const char *func = "banner_success_parser";

   if( pset_pointer(values, 0) == NULL ) {
      msg(LOG_ERR, func, "pset_pointer returned NULL" );
      return( FAILED );
   }

   scp->sc_banner_success = new_string(pset_pointer(values,0) );
   if( scp->sc_banner_success == NULL ) {
      msg(LOG_ERR, func, ES_NOMEM);
      return( FAILED );
   }

   return OK;
}

status_e banner_fail_parser(pset_h values, 
                            struct service_config *scp, 
                            enum assign_op op)
{
   const char *func = "banner_fail_parser";

   if( pset_pointer(values, 0) == NULL ) {
      msg(LOG_ERR, func, "pset_pointer returned NULL");
      return( FAILED );
   }

   scp->sc_banner_fail = new_string(pset_pointer(values,0) );
   if( scp->sc_banner_fail == NULL ) {
      msg(LOG_ERR, func, ES_NOMEM);
      return( FAILED );
   }

   return OK;
}

#ifdef HAVE_LOADAVG
status_e max_load_parser(pset_h values, 
                         struct service_config *scp, 
                         enum assign_op op)
{
   const char *func = "max_load_parser" ;
   char *adr = (char *)pset_pointer(values, 0);

   if( sscanf(adr, "%lf", &scp->sc_max_load) < 1 ) {
      parsemsg(LOG_ERR, func, "error reading max_load argument");
      return( FAILED );
   }

   if( scp->sc_max_load == 0 ) {
      parsemsg(LOG_ERR, func, "error parsing max_load argument");
      return( FAILED );
   }

   return OK;
}
#endif

status_e redir_parser(pset_h values, 
                      struct service_config *scp, 
                      enum assign_op op)
{
   char *adr = (char *)pset_pointer(values, 0);
   const char *func = "redir_parser";
   char *port_char;
   int port_int;
   struct addrinfo hints, *res;

   port_char = pset_pointer(values, 1);
   if (parse_base10(port_char, &port_int) || port_int <= 0)
   {  /* OK, maybe its a service name... */
      struct servent *entry;
      entry = getservbyname(port_char, "tcp");
      if (entry == 0)
      {
         parsemsg(LOG_ERR, func, "port number invalid");
         return FAILED;
      }
      port_int = ntohs(entry->s_port);
   }
   if (port_int >= PORT_MAX)
   {
      parsemsg(LOG_ERR, func, "port number too large");
      return FAILED;
   }
   
   scp->sc_redir_addr = (union xsockaddr *)malloc(sizeof(union xsockaddr));
   if( scp->sc_redir_addr == NULL )
   {
      parsemsg(LOG_ERR, func, "can't allocate space for redir addr");
      return FAILED;
   }

   memset(&hints, 0, sizeof(hints));
   hints.ai_flags = AI_CANONNAME;
   hints.ai_socktype = SOCK_STREAM;
   if (strchr(adr, ':'))
      hints.ai_family = AF_INET6;
   else
      hints.ai_family = AF_INET;

   if( getaddrinfo(adr, NULL, &hints, &res) < 0 ) {
      parsemsg(LOG_ERR, func, "bad address");
      free( scp->sc_redir_addr );
      scp->sc_redir_addr = NULL;
      return FAILED;
   }

   if( (res == NULL) || (res->ai_addr == NULL) ) {
      parsemsg(LOG_ERR, func, "no addresses returned");
      free( scp->sc_redir_addr );
      scp->sc_redir_addr = NULL;
      return FAILED;
   }
      
   if( (res->ai_family == AF_INET) || (res->ai_family == AF_INET6) )
      memcpy(scp->sc_redir_addr, res->ai_addr, res->ai_addrlen);
   if( scp->sc_redir_addr->sa.sa_family == AF_INET ) 
      scp->sc_redir_addr->sa_in.sin_port = port_int;
   if( scp->sc_redir_addr->sa.sa_family == AF_INET6 ) 
      scp->sc_redir_addr->sa_in6.sin6_port = port_int;

   freeaddrinfo(res);
   return OK;
}

status_e bind_parser( pset_h values, 
                      struct service_config *scp, 
                      enum assign_op op)
{
   char *adr = (char *)pset_pointer(values, 0);
   const char *func = "bind_parser";
   struct addrinfo hints, *res, *ressave;
   int numeric, addr_cnt = 0;

   memset(&hints, 0, sizeof(hints));
   hints.ai_flags = AI_CANONNAME;
   if (check_hostname(adr) == 0)
   {
      hints.ai_family = AF_INET;
      hints.ai_flags |= AI_NUMERICHOST;
      numeric = 1;
   }
   else if (strchr(adr, ':'))
   {
      hints.ai_family = AF_INET6;
      hints.ai_flags |= AI_NUMERICHOST;
      numeric = 2;
   }
   else
   {
      hints.ai_family = AF_UNSPEC;
      numeric = 0;
   }
   
   if( getaddrinfo(adr, NULL, &hints, &res) < 0 ) {
      parsemsg(LOG_ERR, func, "bad address");
      return( FAILED );
   }

   if( (res == NULL) || (res->ai_addr == NULL) ) {
      parsemsg(LOG_ERR, func, "no addresses returned");
      return( FAILED );
   }

   /*
    * If more than 1 record comes back, we need to defer selection
    * until we are finished reading all attributes of the service.
    * Hopefully, they will have specified IPv4 or IPv6.
    */
   ressave = res;
   while (res)
   {
      addr_cnt++;
      res = res->ai_next;
   }
   res = ressave;

   if (addr_cnt == 1)
   {
      scp->sc_bind_addr = (union xsockaddr *)malloc(sizeof(union xsockaddr));
      if( scp->sc_bind_addr == NULL )
      {
         parsemsg(LOG_ERR, func, "can't allocate space for bind addr");
         return( FAILED );
      }
      memcpy(scp->sc_bind_addr, res->ai_addr, res->ai_addrlen);
   }	   
   else
      scp->sc_orig_bind_addr = new_string(adr);

   freeaddrinfo(res);
   return( OK );
}

status_e access_times_parser( pset_h values, 
                              struct service_config *scp, 
                              enum assign_op op )
{
   unsigned u, count ;
   const char *func = "access_times_parser" ;

   NEW_SET( scp->sc_access_times, 0, 0 ) ;
   count = pset_count( values) ;

   if ( count == 0 )
   {
       missing_attr_msg("access_times_parser", "access_times");
       return FAILED;
   }

   for ( u = 0 ; u < count ; u++ )
   {
      register char *interval = (char *) pset_pointer( values, u ) ;

      if ( ti_add( scp->sc_access_times, interval ) == FAILED )
         return FAILED ;
   }
   return OK ;
}


status_e nice_parser( pset_h values, 
                      struct service_config *scp, 
                      enum assign_op op )
{
   if ( parse_base10((char *) pset_pointer( values, 0 ), &scp->sc_nice) ) {
      parsemsg(LOG_ERR, "nice_parser", "Error parsing: %s", (char *)pset_pointer( values, 0 ));
      return( FAILED );
   }
   return( OK ) ;
}

#ifdef RLIMIT_AS
status_e rlim_as_parser( pset_h values, 
                         struct service_config *scp, 
                         enum assign_op op )
{
   char *mem = (char *) pset_pointer( values, 0 ) ;
   const char *func = "rlim_as_parser" ;

   if ( EQ( mem, "UNLIMITED" ) )
      scp->sc_rlim_as = (rlim_t)RLIM_INFINITY ;
   else
   {
      if ( get_limit ( mem, &scp->sc_rlim_as) )
      {
         parsemsg( LOG_ERR, func,
            "Address space limit is invalid: %s", mem ) ;
         return( FAILED ) ;
      }
   }
   return( OK ) ;
}
#endif

#ifdef RLIMIT_CPU
status_e rlim_cpu_parser( pset_h values, 
                          struct service_config *scp, 
                          enum assign_op op )
{
   char *cpu_str = (char *) pset_pointer( values, 0 ) ;
   int cpu_int;
   const char *func = "rlim_cpu_parser" ;

   if ( EQ( cpu_str, "UNLIMITED" ) )
      scp->sc_rlim_cpu = (rlim_t)RLIM_INFINITY ;
   else
   {
      if ( parse_base10(cpu_str, &cpu_int) || cpu_int < 0 )
      {
         parsemsg( LOG_ERR, func,
            "CPU limit is invalid: %s", cpu_str ) ;
         return( FAILED ) ;
      }
      scp->sc_rlim_cpu = (rlim_t) cpu_int ;
   }
   return( OK ) ;
}
#endif

#ifdef RLIMIT_DATA
status_e rlim_data_parser( pset_h values, 
                           struct service_config *scp, 
                           enum assign_op op )
{
   char *mem = (char *) pset_pointer( values, 0 ) ;
   const char *func = "rlim_data_parser" ;

   if ( EQ( mem, "UNLIMITED" ) )
      scp->sc_rlim_data = (rlim_t)RLIM_INFINITY ;
   else
   {
      if ( get_limit ( mem, &scp->sc_rlim_data ) )
      {
         parsemsg( LOG_ERR, func,
            "Data limit is invalid: %s", mem ) ;
         return( FAILED ) ;
      }
   }
   return( OK ) ;
}
#endif

#ifdef RLIMIT_RSS
status_e rlim_rss_parser( pset_h values, 
                          struct service_config *scp, 
                          enum assign_op op )
{
   char *mem = (char *) pset_pointer( values, 0 ) ;
   const char *func = "rlim_rss_parser" ;

   if ( EQ( mem, "UNLIMITED" ) )
      scp->sc_rlim_rss = (rlim_t)RLIM_INFINITY ;
   else
   {
      if ( get_limit ( mem, &scp->sc_rlim_rss ) )
      {
         parsemsg( LOG_ERR, func,
            "RSS limit is invalid: %s", mem ) ;
         return( FAILED ) ;
      }
   }
   return( OK ) ;
}
#endif

#ifdef RLIMIT_STACK
status_e rlim_stack_parser( pset_h values, 
                            struct service_config *scp, 
                            enum assign_op op )
{
   char *mem = (char *) pset_pointer( values, 0 ) ;
   const char *func = "rlim_stack_parser" ;

   if ( EQ( mem, "UNLIMITED" ) )
      scp->sc_rlim_stack = (rlim_t)RLIM_INFINITY ;
   else
   {
      if ( get_limit ( mem, &scp->sc_rlim_stack ) )
      {
         parsemsg( LOG_ERR, func,
            "Stack limit is invalid: %s", mem ) ;
         return( FAILED ) ;
      }
   }
   return( OK ) ;
}
#endif

status_e deny_time_parser( pset_h values, 
                           struct service_config *scp, 
                           enum assign_op op )
{
   char *deny_time = (char *) pset_pointer( values, 0 ) ;

   if ( EQ( deny_time, "FOREVER" ) )
      scp->sc_deny_time = -1 ;
   else if ( EQ( deny_time, "NEVER" ) )
      scp->sc_deny_time = 0 ;
   else if ( parse_base10( deny_time, &scp->sc_deny_time ) ) {
      parsemsg(LOG_ERR, "deny_time_parser", "Error parsing: %s", deny_time);
      return( FAILED );
   }
   return( OK ) ;
}

status_e umask_parser( pset_h values, 
                       struct service_config *scp, 
                       enum assign_op op )
{
   char *umask_str = (char *)pset_pointer(values, 0);
   int umask_int;

   if( parse_int(umask_str, 8, -1, &umask_int) ||
       umask_int < 0 || umask_int > 0777)
   {
      parsemsg(LOG_ERR, "umask_parser", "umask argument is invalid.\n");
      return( FAILED );
   }
   scp->sc_umask = umask_int;
   return( OK );
}

