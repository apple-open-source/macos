/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef XCONFIG_H
#define XCONFIG_H

/*
 * $Id: xconfig.h,v 1.1.1.4 2003/05/22 01:16:36 rbraun Exp $
 */

/*
 * Deal with stupid Compaq (DEC) Unix problem ...
 */
#ifdef __osf__
#undef DUMP_FILE
#endif

/*
 * The purpose of customconf.h is to allow the override of
 * constants defined in this file. These include all constants that
 * cannot be overriden from the command line via some option.
 */
#ifdef CUSTOMCONF
#include "customconf.h"
#endif


#define DEFAULT_CONFIG_FILE			"/etc/xinetd.conf"

/*
 * This is the facility used by xinetd to log syslog messages
 */
#define DEFAULT_SYSLOG_FACILITY  	LOG_DAEMON

/*
 * This is the level used for log messages when a service logs to syslog
 */
#define DEFAULT_SERVICE_SYSLOG_LEVEL	LOG_INFO

/*
 * Max number of concurrently running processes forked to get 
 * the user id from the remote host
 */
#define DEFAULT_LOGPROCS			15

/*
 * The loop rate is a bound on the rate of forking servers for a
 * particular service. If that rate is exceeded, the service is deactivated.
 */
#define DEFAULT_LOOP_RATE			50

/*
 * The number of seconds to wait before re-enabling a looping service.
 */
#define DEFAULT_LOOP_TIME			10

/*
 * Signal-to-action mapping
 */
#ifndef RECONFIG_HARD_SIG
#define RECONFIG_HARD_SIG			SIGHUP
#endif
#ifndef OLD_RECONFIG_HARD_SIG
#define OLD_RECONFIG_HARD_SIG			SIGUSR2
#endif
#ifndef TERMINATION_SIG
#define TERMINATION_SIG				SIGTERM
#endif
#ifndef STATE_DUMP_SIG
#define STATE_DUMP_SIG				SIGUSR1
#endif
#ifndef CONSISTENCY_CHECK_SIG
#define CONSISTENCY_CHECK_SIG                   SIGIOT
#endif
#ifndef SERVER_EXIT_SIG
#define SERVER_EXIT_SIG				SIGCHLD
#endif
#ifndef QUIT_SIG
#define QUIT_SIG				SIGQUIT
#endif

/*
 * This is the file where the internal state of xinetd is dumped upon 
 * receipt of STATE_DUMP_SIG
 */
#ifndef DUMP_FILE
#define DUMP_FILE		"/var/run/xinetd.dump"
#endif

/*
 * There are 2 timeouts (in seconds) when trying to get the user id from 
 * the remote host. Any timeout value specified as 0 implies an infinite
 * timeout.
 * LOGUSER_SUCCESS_TIMEOUT is the timeout when access control has been passed 
 * and a new process has been forked to exec the server. 
 * LOGUSER_FAILURE_TIMEOUT is the timeout when the attempt to access the
 * service has been rejected.
 *
 * Both timeouts should be at least 30 seconds as suggested in RFC 1413
 * (assuming they are not 'infinite' timeouts).
 */
#ifndef LOGUSER_SUCCESS_TIMEOUT
#define LOGUSER_SUCCESS_TIMEOUT		30
#endif
#ifndef LOGUSER_FAILURE_TIMEOUT
#define LOGUSER_FAILURE_TIMEOUT		30
#endif

/*
 * This is used when an instance limit is not specified for a service
 * and the defaults entry does not specify an instance limit either.
 * It can be a positive number or "UNLIMITED".
 */
#ifndef DEFAULT_INSTANCE_LIMIT
#define DEFAULT_INSTANCE_LIMIT		UNLIMITED
#endif

/*
 * This is the interval (in seconds) over which we check if the service 
 * loop rate has been exceeded.
 */
#ifndef LOOP_INTERVAL
#define LOOP_INTERVAL			2
#endif

/*
 * LOG_OPEN_FLAGS are the flags used to open a log file (this is used as 
 * the 3rd argument of open(2))
 */
#ifndef LOG_OPEN_FLAGS
#define LOG_OPEN_FLAGS			O_CREAT+O_APPEND+O_WRONLY
#endif

/*
 * Number of consecutive fork failures that we are willing to put up with
 * before giving up.
 */
#ifndef MAX_FORK_FAILURES
#define MAX_FORK_FAILURES        	5
#endif

/*
 * This is the time period during which we will not log subsequest attempts 
 * to access a datagram-based service from the same bad address after logging 
 * the first one.
 * For example, if we receive a datagram at time t from address A which
 * has a match in the no_access list, we will log the failed attempt and 
 * during the interval (t, t+DGRAM_IGNORE_TIME) we will not log attempts 
 * from address A (for the same service).
 * In this context, the address is defined as (IP address, port number).
 */
#ifndef DGRAM_IGNORE_TIME
#define DGRAM_IGNORE_TIME		60			/* seconds */
#endif

#ifndef DUMP_FILE_MODE
#define DUMP_FILE_MODE			0644
#endif

#ifndef LOG_FILE_MODE
#define LOG_FILE_MODE         		0644
#endif

/*
 * The DATAGRAM_SIZE should be big enough for an ethernet packet
 */
#ifndef DATAGRAM_SIZE
#define DATAGRAM_SIZE			2048
#endif

/*
 * Time interval between retry attempts
 */
#ifndef RETRY_INTERVAL
#define RETRY_INTERVAL			5		/* seconds */
#endif

/*
 * LOG_EXTRA_MIN, LOG_EXTRA_MAX define the limits by which the hard limit
 * on the log size can exceed the soft limit
 */
#ifndef LOG_EXTRA_MIN
#define LOG_EXTRA_MIN			(  5 * 1024 )
#endif
#ifndef LOG_EXTRA_MAX
#define LOG_EXTRA_MAX			( 20 * 1024 )
#endif

/*
 * If SENSORS are used and someone trips it, they are added to the
 * global_no_access table for whatever the configured time is. This
 * define determines the size of the table to use.
 */
#define MAX_GLOBAL_NO_ACCESS      10240


#endif	/* CONFIG_H */
