/*
 *	File:		CommandLineUtilities.h (used to be executecommand.h)
 *
 *	Contains:	See comments in CommandLineUtilities.c.
 *
 *	Written by:	Jeff Albouze
 *
 *	Copyright:	© 2000-2001 Apple Computer, Inc., all rights reserved.
 *
 *	Change History (most recent first):
 *
 *		 <*>	  6/8/01	JFA		Last modified (S3.1 candidate).
 *		 <6>	 1/29/01	JFA		Last modified (Mac OS X Server GM).
 *		 <0>	 5/11/00	JFA		Created from scratch.
 */ 

#ifndef __CommandLineUtilities__
#define __CommandLineUtilities__	1

//#define DEBUG 1
#include <Carbon/Carbon.h>

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
//#include <varargs.h>
#include <syslog.h>

#include <signal.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <kvm.h>
#include <sys/sysctl.h>

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
                                    gid_t gid);

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
