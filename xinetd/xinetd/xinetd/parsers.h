/*
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms
 * and conditions for redistribution.
 */
#ifndef _X_PARSERS
#define _X_PARSERS

#include "config.h"
#include "pset.h"
#include "defs.h"
#include "parse.h"

status_e socket_type_parser(pset_h, struct service_config *, enum assign_op) ;
status_e protocol_parser(pset_h, struct service_config *, enum assign_op) ;
status_e wait_parser(pset_h, struct service_config *, enum assign_op) ;
status_e user_parser(pset_h, struct service_config *, enum assign_op) ;
status_e group_parser(pset_h, struct service_config *, enum assign_op) ;
status_e server_parser(pset_h, struct service_config *, enum assign_op) ;
status_e server_args_parser(pset_h, struct service_config *, enum assign_op) ;
status_e instances_parser(pset_h, struct service_config *, enum assign_op) ;
status_e log_on_success_parser(pset_h, struct service_config *, enum assign_op) ;
status_e log_on_failure_parser(pset_h, struct service_config *, enum assign_op) ;
status_e log_type_parser(pset_h, struct service_config *, enum assign_op) ;
status_e only_from_parser(pset_h, struct service_config *, enum assign_op) ;
status_e no_access_parser(pset_h, struct service_config *, enum assign_op) ;
status_e access_times_parser(pset_h, struct service_config *, enum assign_op) ;
status_e type_parser(pset_h, struct service_config *, enum assign_op) ;
status_e id_parser(pset_h, struct service_config *, enum assign_op) ;
status_e env_parser(pset_h, struct service_config *, enum assign_op) ;
status_e port_parser(pset_h, struct service_config *, enum assign_op) ;
status_e rpc_version_parser(pset_h, struct service_config *, enum assign_op) ;
status_e passenv_parser(pset_h, struct service_config *, enum assign_op) ;
status_e flags_parser(pset_h, struct service_config *, enum assign_op) ;
status_e disabled_parser(pset_h, struct service_config *, enum assign_op) ;
status_e rpc_number_parser(pset_h, struct service_config *, enum assign_op) ;
status_e nice_parser(pset_h, struct service_config *, enum assign_op) ;
status_e redir_parser(pset_h, struct service_config *, enum assign_op) ;
status_e bind_parser(pset_h, struct service_config *, enum assign_op) ;
status_e banner_parser(pset_h, struct service_config *, enum assign_op) ;
status_e per_source_parser(pset_h, struct service_config *, enum assign_op) ;
status_e groups_parser(pset_h, struct service_config *, enum assign_op) ;
status_e banner_success_parser(pset_h, struct service_config *, enum assign_op) ;
status_e banner_fail_parser(pset_h, struct service_config *, enum assign_op) ;
status_e cps_parser(pset_h, struct service_config *, enum assign_op) ;
status_e enabled_parser(pset_h, struct service_config *, enum assign_op) ;
status_e svcdisable_parser(pset_h, struct service_config *, enum assign_op);
#ifdef HAVE_LOADAVG
status_e max_load_parser(pset_h, struct service_config *, enum assign_op) ;
#endif
#ifdef RLIMIT_AS
status_e rlim_as_parser(pset_h, struct service_config *, enum assign_op) ;
#endif
#ifdef RLIMIT_CPU
status_e rlim_cpu_parser(pset_h, struct service_config *, enum assign_op) ;
#endif
#ifdef RLIMIT_DATA
status_e rlim_data_parser(pset_h, struct service_config *, enum assign_op) ;
#endif
#ifdef RLIMIT_RSS
status_e rlim_rss_parser(pset_h, struct service_config *, enum assign_op) ;
#endif
#ifdef RLIMIT_STACK
status_e rlim_stack_parser(pset_h, struct service_config *, enum assign_op) ;
#endif
status_e v6only_parser(pset_h, struct service_config *, enum assign_op);
status_e deny_time_parser(pset_h, struct service_config *, enum assign_op) ;
status_e umask_parser(pset_h, struct service_config *, enum assign_op) ;

#endif
