/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <syslog.h>
#include <memory.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>

#include "str.h"
#include "parse.h"
#include "parsers.h"
#include "msg.h"
#include "parsesup.h"
#include "addr.h"
#include "includedir.h"
#include "main.h"
#include "sio.h"

#ifndef NAME_MAX
#define NAME_MAX 255
#endif


/*
 * A NULL value for the name field marks the end of the table
 *
 * The 3rd value is the number of attribute values.
 * If the number is positive, exactly that many values must be specified.
 * If the number is -1, 0 or more values may be specified.
 * If the number is -2, 0 or more values may be specified and the operators
 * '+=' and '-=' may be used.
 */
static const struct attribute service_attributes[] =
{
   { "socket_type",    A_SOCKET_TYPE,    1,  socket_type_parser     },
   { "protocol",       A_PROTOCOL,       1,  protocol_parser        },
   { "wait",           A_WAIT,           1,  wait_parser            },
   { "user",           A_USER,           1,  user_parser            },
   { "group",          A_GROUP,          1,  group_parser           },
   { "server",         A_SERVER,         1,  server_parser          },
   { "server_args",    A_SERVER_ARGS,   -1,  server_args_parser     },
   { "instances",      A_INSTANCES,      1,  instances_parser       },
   { "log_on_success", A_LOG_ON_SUCCESS,-2,  log_on_success_parser  },
   { "log_on_failure", A_LOG_ON_FAILURE,-2,  log_on_failure_parser  },
   { "log_type",       A_LOG_TYPE,      -1,  log_type_parser        },
   { "only_from",      A_ONLY_FROM,     -2,  only_from_parser       },
   { "no_access",      A_NO_ACCESS,     -2,  no_access_parser       },
   { "access_times",   A_ACCESS_TIMES,  -1,  access_times_parser    },
   { "type",           A_TYPE,          -1,  type_parser            },
#ifndef NO_RPC
   { "rpc_version",    A_RPC_VERSION,    1,  rpc_version_parser     },
   { "rpc_number",     A_RPC_NUMBER,     1,  rpc_number_parser      },
#endif
   { "id",             A_ID,             1,  id_parser              },
   { "env",            A_ENV,           -2,  env_parser             },
   { "port",           A_PORT,           1,  port_parser            },
   { "passenv",        A_PASSENV,       -2,  passenv_parser         },
   { "flags",          A_FLAGS,         -1,  flags_parser           },
   { "nice",           A_NICE,           1,  nice_parser            },
   { "redirect",       A_REDIR,          2,  redir_parser           },
   { "banner",         A_BANNER,         1,  banner_parser          },
   { "bind",           A_BIND,           1,  bind_parser            },
   { "interface",      A_BIND,           1,  bind_parser            },
   { "per_source",     A_PER_SOURCE,     1,  per_source_parser      },
   { "groups",         A_GROUPS,         1,  groups_parser          },
   { "banner_success", A_BANNER_SUCCESS, 1,  banner_success_parser  },
   { "banner_fail",    A_BANNER_FAIL,    1,  banner_fail_parser     },
   { "cps",            A_CPS,            2,  cps_parser             },
   { "disable",        A_SVCDISABLE,     1,  svcdisable_parser      },
#ifdef HAVE_LOADAVG
   { "max_load",       A_MAX_LOAD,       1,  max_load_parser        },
#endif
#ifdef RLIMIT_AS
   { "rlimit_as",      A_RLIMIT_AS,      1,  rlim_as_parser         },
#endif
#ifdef RLIMIT_CPU
   { "rlimit_cpu",     A_RLIMIT_CPU,     1,  rlim_cpu_parser        },
#endif
#ifdef RLIMIT_DATA
   { "rlimit_data",    A_RLIMIT_DATA,    1,  rlim_data_parser       },
#endif
#ifdef RLIMIT_RSS
   { "rlimit_rss",     A_RLIMIT_RSS,     1,  rlim_rss_parser        },
#endif
#ifdef RLIMIT_STACK
   { "rlimit_stack",   A_RLIMIT_STACK,   1,  rlim_stack_parser      },
#endif
   { "v6only",         A_V6ONLY,         1,  v6only_parser          },
   { "deny_time",      A_DENY_TIME,      1,  deny_time_parser       },
   { "umask",          A_UMASK,          1,  umask_parser           },
   { NULL,             A_NONE,          -1,  NULL                   }
} ;

static const struct attribute default_attributes[] =
{
   { "log_type",        A_LOG_TYPE,       -2,   log_type_parser       },
   { "log_on_success",  A_LOG_ON_SUCCESS, -2,   log_on_success_parser },
   { "log_on_failure",  A_LOG_ON_FAILURE, -2,   log_on_failure_parser },
   { "disabled",        A_DISABLED,       -2,   disabled_parser       },
   { "no_access",       A_NO_ACCESS,      -2,   no_access_parser      },
   { "only_from",       A_ONLY_FROM,      -2,   only_from_parser      },
   { "instances",       A_INSTANCES,       1,   instances_parser      },
   { "passenv",         A_PASSENV,        -2,   passenv_parser        },
   { "banner",          A_BANNER,          1,   banner_parser         },
   { "bind",            A_BIND,            1,   bind_parser           },
   { "interface",       A_BIND,            1,   bind_parser           },
   { "per_source",      A_PER_SOURCE,      1,   per_source_parser     },
   { "groups",          A_GROUPS,          1,   groups_parser         },
   { "banner_success",  A_BANNER_SUCCESS,  1,   banner_success_parser },
   { "banner_fail",     A_BANNER_FAIL,     1,   banner_fail_parser    },
   { "cps",             A_CPS,             2,   cps_parser            },
   { "enabled",         A_ENABLED,        -2,   enabled_parser        },
#ifdef HAVE_LOADAVG
   { "max_load",        A_MAX_LOAD,        1,   max_load_parser       },
#endif
   { "v6only",          A_V6ONLY,         1,    v6only_parser         },
   { "umask",           A_UMASK,          1,    umask_parser          },
   { NULL,              A_NONE,           0,    NULL                  }
} ;


#define MODIFIABLE( ap )       ( (ap)->a_nvalues == -2 )
#define FIXED_VALUES( ap )         ( (ap)->a_nvalues > 0 )

int line_count ;

static void get_service_entry( int fd, pset_h, char *, 
	struct service_config * );
static void fill_attribute( unsigned attr_id, struct service_config *scp, 
        struct service_config *def );
static entry_e  find_next_entry(int , char **) ;
static status_e parse_entry(entry_e, int, struct service_config *) ;

/*
 * Given the id, return the name (only the service attributes are searched)
 */
char *attr_name_lookup( int id )
{
   const struct attribute *ap ;

   for ( ap = &service_attributes[ 0 ] ; ap->a_name ; ap++ )
      if ( id == ap->a_id )
         return( ap->a_name ) ;
   return( CHAR_NULL ) ;
}


void parse_end(void)
{
   endprotoent() ;
   endpwent() ;
   endgrent() ;
   endnetent() ;
   endhostent() ;
}


/*
 * Parsing rules and rationale
 *
 * The parse_conf_file function parses a configuration file identified
 * by a file descriptor and fills the service table and defaults of
 * the configuration argument.
 *
 * The configuration information for a service comes from 2 sources: the
 * service entry and, possibly, the defaults entry.
 * Attributes specified in the defaults entry can be overriden or
 * modified by the service entry. Modifiable attributes can be identified
 * by the value -2 for the 'a_nvalues' field of the struct attribute. Those
 * attributes with a different value for 'a_nvalues' are overridable ones.
 * The modifiable attributes are filled in only if the entry tries to modify
 * them.
 */

/*
 * Read the configuration file (descriptor fd) and place all
 * services found there in the configuration.
 */
void parse_conf_file( int fd, struct configuration *confp )
{
   pset_h                   sconfs       = CNF_SERVICE_CONFS( confp ) ;
   struct service_config   *default_config   = CNF_DEFAULTS( confp ) ;
   boolean_e                found_defaults   = NO ;
   struct service_config    default_default_config ;
   const char              *func      = "parse_conf_file" ;
   int                      incfd;

   line_count = 0 ;
   CLEAR( default_default_config ) ;

   for ( ;; )
   {
      entry_e   entry_type ;
      char      *service_name ;

      /*
       * if find_next_entry is successful, service_name
       * will point to malloc'ed memory
       */
      entry_type = find_next_entry( fd, &service_name ) ;
      switch ( entry_type )
      {
      case INCLUDE_ENTRY:
         incfd = open(service_name, O_RDONLY);
         if( incfd < 0 ) {
            parsemsg( LOG_ERR, func, 
               "Unable to open included configuration file: %s", 
               service_name);
            break;
         }
         parsemsg( LOG_DEBUG,func,
            "Reading included configuration file: %s",service_name);
         parse_conf_file(incfd, confp);
         Sclose(incfd);
         break;
      case INCLUDEDIR_ENTRY:
         handle_includedir(service_name, confp);
         break;
      case SERVICE_ENTRY:
         get_service_entry( fd, sconfs, service_name, default_config ) ; 
         break ;

      case DEFAULTS_ENTRY:
         if ( found_defaults == YES )
         {
            parsemsg( LOG_ERR, func,
            "only 1 defaults entry is allowed. This entry will be ignored" ) ;
            skip_entry( fd ) ;
         }
         else if ( parse_entry( DEFAULTS_ENTRY, fd,
                           default_config ) == OK )
            found_defaults = YES ;
         break ;
         
      case BAD_ENTRY:
         skip_entry( fd ) ;
         break ;

      case NO_ENTRY:
         return ;
      }
      free(service_name);
   }
}


/*
 * Find the next service entry.
 * Look for a line of the form:
 *
 *      <white-space> service <white-space> <service_name>
 *
 * followed by a line containing only the ENTRY_BEGIN character
 */
static entry_e find_next_entry( int fd, char **snamep )
{
   char           *p ;
   str_h           strp ;
   char           *sname = NULL;
   entry_e         entry_type=0;
   char           *line = next_line( fd ) ;
   const char     *func = "find_next_entry" ;

   if ( line == CHAR_NULL )
      return( NO_ENTRY ) ;

   strp = str_parse( line, " \t", STR_RETURN_ERROR, INT_NULL ) ;
   if ( strp == NULL )
   {
      parsemsg( LOG_CRIT, func, "str_parse failed" ) ;
      return( BAD_ENTRY ) ;
   }

   if ( ( p = str_component( strp ) ) == CHAR_NULL )
   {
      /*
       * This shouldn't happen since it implies that there is a bug
       * in next_line
       */
      parsemsg( LOG_WARNING, func, "empty line" ) ;
      str_endparse( strp ) ;
      return( BAD_ENTRY ) ;
   }

   /*
    * Look for a keyword
    */
   if ( EQ( p, KW_SERVICE ) || EQ( p, KW_INCLUDE ) || EQ(p, KW_INCLUDEDIR))
   {
      if ( EQ( p, KW_INCLUDE ))
         entry_type = INCLUDE_ENTRY;
      else if ( EQ( p, KW_INCLUDEDIR ))
         entry_type = INCLUDEDIR_ENTRY;

      /*
       * Now get the service name
       */
      if ( ( p = str_component( strp ) ) == CHAR_NULL )
      {
         parsemsg( LOG_ERR, func, "service name missing" ) ;
         str_endparse( strp ) ;
         return( BAD_ENTRY ) ;
      }
   
      sname = new_string( p ) ;
      if ( sname == CHAR_NULL )
      {
         out_of_memory( func ) ;
         str_endparse( strp ) ;
         return( BAD_ENTRY ) ;
      }
      str_endparse( strp ) ;

      if( (entry_type == INCLUDE_ENTRY) || 
          (entry_type == INCLUDEDIR_ENTRY))
      {
         *snamep = sname ;
         return( entry_type ) ;
      }
      else
         entry_type = SERVICE_ENTRY ;
   }
   else if ( EQ( p, KW_DEFAULTS ) )
   {
      str_endparse( strp ) ;
      entry_type = DEFAULTS_ENTRY ;
   }
   else
   {
      parsemsg( LOG_ERR, func, "missing service keyword" ) ;
      str_endparse( strp ) ;
      return( BAD_ENTRY ) ;
   }

   /*
    * Now look for ENTRY_BEGIN
    */
   line = next_line( fd ) ;
   if ( line == NULL || ! line_has_only_1_char( line, ENTRY_BEGIN ) )
   {
      parsemsg( LOG_ERR, func,
         "Service %s: missing '%c'", sname, ENTRY_BEGIN ) ;
      if ( entry_type == SERVICE_ENTRY )
         free( sname ) ;
      return( BAD_ENTRY ) ;
   }
   *snamep = sname ;
   return( entry_type ) ;
}


/*
 * Get a service entry. Steps:
 *
 *      1. Parse entry attributes
 *      2. Determine service id
 *      3. Insert entry in table
 */
static void get_service_entry( int fd, 
                                pset_h sconfs,
                                char *name,
                                struct service_config *defaults )
{
   struct service_config   *scp ;
   const char              *func = "get_service_entry" ;

   scp = sc_alloc( name ) ;
   if ( scp == NULL )
   {
      skip_entry( fd ) ;
      return ;
   }

   /* Now fill in default attributes if given. */
   if ( SC_SPECIFIED( defaults, A_LOG_ON_SUCCESS ) &&
      ! SC_IS_PRESENT( scp, A_LOG_ON_SUCCESS) )
      fill_attribute( A_LOG_ON_SUCCESS, scp, defaults ) ;
   if ( SC_SPECIFIED( defaults, A_LOG_ON_FAILURE ) &&
      ! SC_IS_PRESENT( scp, A_LOG_ON_FAILURE ) )
      fill_attribute( A_LOG_ON_FAILURE, scp, defaults ) ;
   if ( SC_SPECIFIED( defaults, A_ONLY_FROM ) &&
      ! SC_IS_PRESENT( scp, A_ONLY_FROM ) )
      fill_attribute( A_ONLY_FROM, scp, defaults ) ;
   if ( SC_SPECIFIED( defaults, A_NO_ACCESS ) &&
      ! SC_IS_PRESENT( scp, A_NO_ACCESS ) )
      fill_attribute( A_NO_ACCESS, scp, defaults ) ;
   if ( SC_SPECIFIED( defaults, A_PASSENV ) &&
      ! SC_IS_PRESENT( scp, A_PASSENV ) )
      fill_attribute( A_PASSENV, scp, defaults ) ;
  
   if ( parse_entry( SERVICE_ENTRY, fd, scp ) == FAILED )
   {
      sc_free( scp ) ;
      skip_entry( fd ) ;
      return ;
   }

   /*
    * If no service id was specified, set it equal to the service name
    */
   if ( ! SC_SPECIFIED( scp, A_ID ) ) {
      if ( (scp->sc_id = new_string( scp->sc_name )) )
         SC_PRESENT( scp, A_ID ) ;
      else
      {
         out_of_memory( func ) ;
         sc_free( scp ) ;
         return ;
      }
   }

   if ( ! (pset_add( sconfs, scp )) )
   {
      out_of_memory( func ) ;
      sc_free( scp ) ;
      return ;
   }

}


/*
 * Fill in scp the value of the modifiable attribute attr from def.
 * These modifiable attributes are:
 *      log_on_{success,failure}
 *      only_from
 *      no_access
 *      passenv
 */
static void fill_attribute( unsigned attr_id, 
                             struct service_config *scp, 
                             struct service_config *def )
{
   switch ( attr_id )
   {
      case A_LOG_ON_SUCCESS:
         M_ASSIGN( scp->sc_log_on_success, def->sc_log_on_success ) ;
         SC_PRESENT( scp, A_LOG_ON_SUCCESS ) ;
         break ;

      case A_LOG_ON_FAILURE:
         M_ASSIGN( scp->sc_log_on_failure, def->sc_log_on_failure ) ;
         SC_PRESENT( scp, A_LOG_ON_FAILURE ) ;
         break ;

      case A_ONLY_FROM:
         if ( addrlist_copy( def->sc_only_from, &scp->sc_only_from ) == OK )
            SC_PRESENT( scp, A_ONLY_FROM ) ;
         break ;

      case A_NO_ACCESS:
         if ( addrlist_copy( def->sc_no_access, &scp->sc_no_access ) == OK )
            SC_PRESENT( scp, A_NO_ACCESS ) ;
         break ;
      
      case A_PASSENV:
         if ( copy_pset( def->sc_pass_env_vars,
                           &scp->sc_pass_env_vars, 0 ) == OK )
            SC_PRESENT( scp, A_PASSENV ) ;
         break ;
   }
}


/*
 * Find the attribute with the specified name
 */
static const struct attribute *attr_lookup( 
	const struct attribute attr_array[], char *attr_name )
{
   const struct attribute *ap ;
   const char *func = "attr_lookup" ;

   for ( ap = &attr_array[ 0 ] ; ap->a_name ; ap++ )
      if ( EQ( attr_name, ap->a_name ) )
         return ap;
   if ( attr_array == service_attributes )
      parsemsg( LOG_WARNING, func, "bad service attribute: %s", attr_name ) ;
   else 
      parsemsg( LOG_WARNING, func, 
         "attribute: %s should not be in default section", attr_name ) ;
   return NULL; 
}


/*
 * Identify the attribute in <attr_name>.
 *
 * Check if
 *      1) the attribute has been defined already
 *      2) the value count is correct
 *      3) the assign op is appropriate
 *
 * Invoke appropriate parser
 */
static void identify_attribute( entry_e entry_type, 
                                 struct service_config *scp, 
                                 char *attr_name, 
                                 enum assign_op op, 
                                 pset_h attr_values )
{
   const struct attribute   *ap ;
   const char         *func = "identify_attribute" ;

   if ( entry_type == SERVICE_ENTRY )
      ap = attr_lookup( service_attributes, attr_name ) ;
   else
      ap = attr_lookup( default_attributes, attr_name ) ;
  
   if ( ap == NULL )
      return ;   /* We simply ignore keywords not on the list */

   if ( ! MODIFIABLE( ap ) )
   {
      if ( SC_SPECIFIED( scp, ap->a_id ) )
      {
         parsemsg( LOG_WARNING, func, "Service %s: attribute already set: %s",
                  scp->sc_name, attr_name ) ;
         return ;
      }

      if ( op != SET_EQ )
      {
         parsemsg( LOG_WARNING, func,
            "Service %s: operator '%s' cannot be used for attribute '%s'",
               scp->sc_name, ( op == PLUS_EQ ) ? "+=" : "-=", attr_name ) ;
         return ;
      }
   }
   else      /* modifiable attribute */
   {
      /*
       * For the defaults entry, '=' and '+=' have the same meaning
       */
      if ( entry_type == DEFAULTS_ENTRY && op == SET_EQ )
         op = PLUS_EQ ;
   }

   if ( FIXED_VALUES( ap ) && ap->a_nvalues != pset_count( attr_values ) )
   {
      parsemsg( LOG_WARNING, func,
         "attribute %s expects %d values and %d values were specified",
         attr_name, ap->a_nvalues, pset_count( attr_values ) ) ;
      return ;
   }

   if ( (*ap->a_parser)( attr_values, scp, op ) == OK )
   {    /* This is the normal path. */
	SC_SPECIFY( scp, ap->a_id ) ;
   }
   else if ( entry_type == SERVICE_ENTRY )
   {
      parsemsg( LOG_ERR, func,
         "Error parsing attribute %s - DISABLING SERVICE", attr_name ) ;
      SC_DISABLE( scp );
   }
   /* We are in the default section and an error was detected. At
    * this point, we should terminate since whatever attribute 
    * was trying to be specified cannot be propagated.
    */
   else if ( !debug.on )
   {
      msg(LOG_ERR, func, 
         "A fatal error was encountered while parsing the default section."
	 " xinetd will exit.");
      terminate_program();
   }
}


/*
 * Read the entry line-by-line and add the information in scp
 * Use defaults to initialize modifiable entry fields.
 */
static status_e parse_entry( entry_e entry_type, 
                              int fd, 
                              struct service_config *scp )
{
   static pset_h      attr_values = NULL;
   char               *line ;
   char               *attr_name ;
   enum assign_op      op ;
   const char         *func = "get_attributes" ;

   if ( ! attr_values && ( attr_values = pset_create( 10, 10 ) ) == NULL )
   {
      out_of_memory( func ) ;
      return( FAILED ) ;
   }

   for ( ;; )
   {
      line = next_line( fd ) ;
      if ( line == CHAR_NULL )
      {
         parsemsg( LOG_ERR, func, "incomplete entry" ) ;
         return( FAILED ) ;
      }

      if ( line_has_only_1_char( line, ENTRY_END ) )
         return( OK ) ;

      if ( parse_line( line, &attr_name, &op, attr_values ) == FAILED )
      {
         pset_clear( attr_values ) ;
         return( FAILED ) ;
      }

      identify_attribute( entry_type,
               scp, attr_name, op, attr_values ) ;
      pset_clear( attr_values ) ;
   }
}

