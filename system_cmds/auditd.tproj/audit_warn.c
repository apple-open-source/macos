/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 2004 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <auditd.h>

/* Write to the audit log. */
static int auditwarnlog(char *args[])
{ 
	char *loc_args[9];
	int i;
	pid_t pid;
	loc_args[0] = AUDITWARN_SCRIPT;
	for (i = 0; args[i] != NULL && i < 8; i++) {
		loc_args[i+1] = args[i];
	}
	loc_args[i+1] = NULL;
        
	pid = fork();
	if (pid == 0) {
	return execv(AUDITWARN_SCRIPT, loc_args);
		/* not reached */
		exit(1);
	} else if (pid == -1) {
		return -1;
	} else {
		return 0;
	}
}

/*
 * Indicates that the hard limit for all filesystems 
 * has been exceeded count times
 */
int audit_warn_allhard(int count)
{
	char intstr[12];
	char *args[3];
	
	snprintf(intstr, 12, "%d", count);	

	args[0] = HARDLIM_ALL_WARN;
	args[1] = intstr; 	
	args[2] = NULL;

	return auditwarnlog(args);
}

/*
 * Indicates that the soft limit for all filesystems 
 * has been exceeded 
 */
int audit_warn_allsoft()
{
	char *args[2];
	
	args[0] = SOFTLIM_ALL_WARN;
	args[1] = NULL;

	return auditwarnlog(args);
}

/*
 * Indicates that someone other than the audit daemon 
 * turned off auditing
 * XXX Its not clear at this point how this function will 
 * XXX be invoked
 */
int audit_warn_auditoff()
{
	char *args[2];
	
	args[0] = AUDITOFF_WARN;
	args[1] = NULL;

	return auditwarnlog(args);
}

/*
 * Indicates that the audit deammn is already running
 */
int audit_warn_ebusy()
{
	char *args[2];
	
	args[0] = EBUSY_WARN;
	args[1] = NULL;

	return auditwarnlog(args);

}

/*
 * Indicates that there is a problem getting the directory 
 * from audit_control
 *
 * XXX Note that we take the filename instead of a count 
 * XXX as the argument here (different from BSM)
 */
int audit_warn_getacdir(char *filename)
{
	char *args[3];
	
	args[0] = GETACDIR_WARN;
	args[1] = filename; 	
	args[2] = NULL;

	return auditwarnlog(args);
}


/*
 * Indicates that the hard limit for this file has been
 * exceeded
 */
int audit_warn_hard(char *filename)
{
	char *args[3];
	
	args[0] = HARDLIM_WARN;
	args[1] = filename; 	
	args[2] = NULL;

	return auditwarnlog(args);

}

/*
 * Indicates that auditing could not be started
 */
int audit_warn_nostart()
{
	char *args[2];
	
	args[0] = NOSTART_WARN;
	args[1] = NULL;

	return auditwarnlog(args);
}

/*
 * Indicaes that an error occrred during the orderly shutdown 
 * of the audit daemon
 */
int audit_warn_postsigterm()
{
	char *args[2];
	
	args[0] = POSTSIGTERM_WARN;
	args[1] = NULL;

	return auditwarnlog(args);
}

/*
 * Indicates that the soft limit for this file has been
 * exceeded
 */
int audit_warn_soft(char *filename)
{
	char *args[3];
	
	args[0] = SOFTLIM_WARN;
	args[1] = filename; 	
	args[2] = NULL;

	return auditwarnlog(args);

}

/*
 * Indicates that the temporary audit file already exists 
 * indicating a fatal error
 */
int audit_warn_tmpfile()
{
	char *args[2];
	
	args[0] = TMPFILE_WARN;
	args[1] = NULL;

	return auditwarnlog(args);
}
