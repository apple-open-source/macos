/* 
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <bootstrap_priv.h>
#include <launch.h>
#include <pwd.h>
#include <dirent.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>

#include "LKDCHelper-main.h"
#include "LKDCHelper.h"

/* MIG Generated files */
#include "LKDCHelperMessage.h"
#include "LKDCHelperMessageServer.h"

union max_msg_size {
		union __RequestUnion__request_LKDCHelper_subsystem	request;
		union __ReplyUnion__request_LKDCHelper_subsystem	reply;
};

static const mach_msg_size_t MAX_MSG_SIZE = sizeof(union max_msg_size) + MAX_TRAILER_SIZE;
static aslclient logclient = NULL;
static int opt_debug;
static struct timeval last_message;
static pthread_t idletimer_thread;

volatile int	LKDCLogLevel = ASL_LEVEL_DEBUG;

unsigned long maxidle = 0;

static uid_t LKDCHelperUID = -1;
static gid_t LKDCHelperGID = -1;

static void
helplogv(int level, const char *fmt, va_list ap)
{
	if (NULL == logclient) {
		vfprintf(stderr, fmt, ap);
		fflush(stderr);
	} else {
		asl_vlog(logclient, NULL, level, fmt, ap);
	}
}

void
helplog(int level, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	helplogv(level, fmt, ap);
	va_end(ap);
}

int
authorized(audit_token_t *token)
{
	int ok = 0;
	pid_t pid = (pid_t)-1;
	uid_t euid = (uid_t)-1;
	
	audit_token_to_au32(*token, NULL, &euid, NULL, NULL, NULL, &pid, NULL, NULL);
	
	ok = (euid == LKDCHelperUID || euid == 0);
	if (!ok) {
		helplog(ASL_LEVEL_NOTICE, "Unauthorized access by euid=%lu pid=%lu",
				(unsigned long)euid, (unsigned long)pid);
	}
	return ok;
}

static void
initialize_logging(void)
{
	logclient = asl_open(NULL, NULL, (opt_debug ? ASL_OPT_STDERR : 0));
	if (NULL == logclient) {
		fprintf(stderr, "Could not initialize ASL logging.\n");
		fflush(stderr);
		return;
	}
	if (opt_debug) {
		asl_set_filter(logclient, ASL_FILTER_MASK_UPTO(ASL_LEVEL_DEBUG));
	}
}

void
update_idle_timer(void)
{
	assert(0 == gettimeofday(&last_message, NULL));
}

static void *
idletimer_main (void *context)
{
	static struct timeval now;

	for (;;) {
		assert(0 == gettimeofday(&now, NULL));
		if (now.tv_sec - last_message.tv_sec > (long) maxidle) {
			(void) request_LKDCHelperExit (*(mach_port_t*)context);
			sleep(1);
		} else {
			int t = maxidle - (now.tv_sec - last_message.tv_sec);
			if (t < 1)
				t = 1;
			sleep(t);
		}
	}

	return NULL;
}

static void
initialize_timer(mach_port_t port)
{
	int err;

	update_idle_timer();

	err = pthread_create(&idletimer_thread, NULL, idletimer_main, &port);

	if (0 != err) {
		helplog(ASL_LEVEL_ERR, "Failed to start idletimer thread: %s", strerror(err));
	}
}

static mach_port_t
checkin(char *service_name)
{
	kern_return_t kr = KERN_SUCCESS;
	mach_port_t port = MACH_PORT_NULL;
	launch_data_t msg, reply = NULL, datum = NULL;

	msg = launch_data_new_string (LAUNCH_KEY_CHECKIN);
	if (NULL == msg) {
		helplog(ASL_LEVEL_ERR, "Could not create checkin message for launchd.");
		goto fin;
	}

	reply = launch_msg (msg);
	if (NULL == reply) {
		helplog(ASL_LEVEL_ERR, "Could not message launchd.");
		goto fin;
	}

	if (LAUNCH_DATA_ERRNO == launch_data_get_type (reply)) {
		helplog(ASL_LEVEL_ERR, "Launchd checkin failed: %s.", 
			strerror (launch_data_get_errno (reply))
		       );	
		goto fin;
	}
	
	datum = launch_data_dict_lookup (reply, LAUNCH_JOBKEY_MACHSERVICES); 
	if (NULL == datum || LAUNCH_DATA_DICTIONARY != launch_data_get_type (datum)) {
		helplog(ASL_LEVEL_ERR, "Launchd reply does not contain %s dictionary.", LAUNCH_JOBKEY_MACHSERVICES);
		goto fin;
	}

	datum = launch_data_dict_lookup (datum, service_name);
	if (NULL == datum || LAUNCH_DATA_MACHPORT != launch_data_get_type (datum)) {
		helplog(ASL_LEVEL_ERR, "Launchd reply does not contain %s Mach port.", service_name);
		goto fin;
	}

	port = launch_data_get_machport (datum);
	if (MACH_PORT_NULL == port) {
		helplog(ASL_LEVEL_ERR, "Launchd gave me a null Mach port.");
		goto fin;
	}

	kr = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
	if (KERN_SUCCESS != kr) {
		helplog(ASL_LEVEL_ERR, "mach_port_insert_right: %s", mach_error_string(kr));
		goto fin;
	}

fin:
	if (NULL != msg)   { launch_data_free(msg); }
	if (NULL != reply) { launch_data_free(reply); }
	if (MACH_PORT_NULL == port) { exit(EXIT_FAILURE); }
	return port;
}

static mach_port_t
register_service (const char *service_name)
{
	mach_port_t port = MACH_PORT_NULL;
	kern_return_t kr;

	kr = mach_port_allocate (mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);

	if (KERN_SUCCESS != kr) {
		helplog(ASL_LEVEL_ERR, "mach_port_allocate: %s", mach_error_string(kr));
		goto error;
	}

	kr = mach_port_insert_right (mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
	if (KERN_SUCCESS != kr) {
		helplog(ASL_LEVEL_ERR, "mach_port_insert_right: %s", mach_error_string (kr));
		goto error;
	}

	/* bootstrap_register2 does not modify its second argument, but the prototype does not include const. */
	kr = bootstrap_register2 (bootstrap_port, (char *)service_name, port, 0);
	if (KERN_SUCCESS != kr) {
		helplog(ASL_LEVEL_ERR, "bootstrap_register2 failed: %s", mach_error_string (kr));
		goto error;
	}
	return port;
error:
	if (MACH_PORT_NULL != port) { mach_port_deallocate (mach_task_self (), port); }
	return MACH_PORT_NULL;
}

int
main(int ac, char *av[])
{
	char *p = NULL;
	kern_return_t kr = KERN_FAILURE;
	mach_port_t port = MACH_PORT_NULL;
	long n;
	int ch;

	while ((ch = getopt(ac, av, "dt:")) != -1)
		switch (ch) {
		case 'd':
			opt_debug = 1;
			LKDCLogLevel = ASL_LEVEL_NOTICE;
			break;
		case 't':
			n = strtol(optarg, &p, 0);
			if ('\0' == optarg[0] || '\0' != *p || n > LONG_MAX || n < 1) {
				fprintf(stderr, "Invalid idle timeout: %s\n", optarg);
				exit(EXIT_FAILURE);
			}
			maxidle = n;
			break;
		case '?':
		default:
			fprintf(stderr, "Usage: [-d] [-t maxidle]\n");
			exit(EXIT_FAILURE);
		}
	ac -= optind;
	av += optind;

	initialize_logging ();

	LKDCHelperUID = getuid ();
	LKDCHelperGID = getgid ();
	
	helplog (ASL_LEVEL_NOTICE, "Starting (uid=%lu)", (unsigned long)LKDCHelperUID);

	if (opt_debug) {
		port = register_service (kLKDCHelperName);
	} else {
		port = checkin (kLKDCHelperName);
	}

	if (maxidle > 0) {
		initialize_timer(port);
	}

	kr = mach_msg_server(LKDCHelper_server, MAX_MSG_SIZE, port, 
				MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
				MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0)
			    );

	if (KERN_SUCCESS != kr) {
		helplog(ASL_LEVEL_ERR, "mach_msg_server: %s\n", mach_error_string(kr));
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

