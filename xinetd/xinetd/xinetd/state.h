/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef STATE_H
#define STATE_H

/*
 * $Id: state.h,v 1.1.1.2 2002/06/17 22:51:26 zarzycki Exp $
 */

#include "config.h"
#include <setjmp.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include "libportable.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "xlog.h"
#include "pset.h"
#include "defs.h"
#include "mask.h"

struct read_only_state
{
   rlim_t      orig_max_descriptors ; /* original soft rlimit                */
   rlim_t      max_descriptors ;      /* original hard rlimit or OPEN_MAX    */
   rlim_t      process_limit ;        /* if 0, there is no limit             */
   int         cc_interval ;          /* # of seconds the cc gets invoked.   */
   char       *pid_file ;             /* where the pidfile is located        */
   char       *config_file ;
   int         is_superuser ;
   char      **Argv ;
   int         Argc ;
} ;


struct defaults
{
   struct service_config  *def_settings ;
   xlog_h                  def_log ;
   bool_int                def_log_creation_failed ;
} ;


struct read_write_state
{
   int              descriptors_free ;     /* may be negative (reserved)    */
   int              available_services ;   /* # of available services       */
   int              active_services ;      /* services with descriptors set */
                                           /* in socket mask                */
   fd_set           socket_mask ;
   int              mask_max ;
   pset_h           servers ;              /* table of running servers      */
   pset_h           retries ;              /* table of servers to retry     */
   pset_h           services ;             /* table of services             */
   struct service  *logging ;
   struct defaults  defs ;
   xlog_h           program_log ;
   sigjmp_buf       env ;
   bool_int         env_is_valid ;
} ;

struct program_state
{
   struct read_only_state   ros ;
   struct read_write_state  rws ;
} ;

#define DEFAULTS( ps )                  (ps).rws.defs.def_settings
#define DEFAULT_LOG( ps )               (ps).rws.defs.def_log
#define DEFAULT_LOG_ERROR( ps )         (ps).rws.defs.def_log_creation_failed
#define LOG_SERVICE( ps )               (ps).rws.logging
#define SERVICES( ps )                  (ps).rws.services
#define SERVERS( ps )                   (ps).rws.servers
#define RETRIES( ps )                   (ps).rws.retries


#endif   /* STATE_H */
