/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 
#ifndef __CommandLineUtilities__
#define __CommandLineUtilities__	1

/* Includes */
#include <sys/types.h>
#include <stdio.h>
#include <pwd.h>

/* Includes */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <syslog.h>

#include <signal.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <kvm.h>
#include <sys/sysctl.h>

#include "BSDHeaders.h"

/* Definitions */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef ENOERR
#define ENOERR 0
#endif

#define kTimeOutVal		25					// in seconds?
#define DEFAULT_EXEC_DELAY	 	5
#define SHELL					"/bin/sh"
#define KILL_CHILDREN_IF_TIMEOUT 1

/* Core routine and support routines */
int 							executecommand(
									char *command, 
									char **output = NULL);

int								myexecutecommandas(
                                    const char *command, 
									const char* path,
									const char * argv[],
									Boolean useSHELL, 
                                    size_t timeout_delay, 
                                    char **output, 
                                    Boolean* canceledFlag,
                                    uid_t uid, 
                                    gid_t gid,
									int* has_timedout);

FILE 							*ec_popen(
									const char *cmdstring,
									const char* path,
									const char* argv[],
									Boolean useSHELL, 
									uid_t uid,
									gid_t gid);

int 							ec_pclose(
									FILE *fp,
									int killit);
									
int 							ec_fprintf(
									FILE *file, 
									const char *format, 
									...);
									
void 							ec_terminate_process_by_id(
									int pid);
									
int 							ec_terminate_process_by_name(
									const char *name);

int 							ec_terminate_daemon_by_name(
									const char *name,
									char **log);


#ifdef __cplusplus
}
#endif
									
#endif
