/*
 * Copyright (c) 2000-2006 Apple Computer, Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>

#define PRIVATE_WEBDAVFS_AGENT_COMMAND "/System/Library/Extensions/webdav_fs.kext/Contents/Resources/webdavfs_agent"
#define WEBDAVFS_AGENT_PATH "PATH=/System/Library/Extensions/webdav_fs.kext/Contents/Resources"
#define CFENVFORMATSTRING "__CF_USER_TEXT_ENCODING=0x%X:0:0"

/*****************************************************************************/

static void usage(void)
{
	(void)fprintf(stderr,
		"usage: mount_webdav [-s] [-S] [-a<fd>] [-o options] [-v <volume name>]\n");
	(void)fprintf(stderr,
		"\t<WebDAV_URL> node\n");
}

/*****************************************************************************/

/* called when child normally terminates the parent */
static void parentexit(int x)
{
#pragma unused(x)
	exit(EXIT_SUCCESS);
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
	int pid, terminated_pid, cnt;
	union wait status;
	char **argv_child;
	int error;
	
	if (argc < 2) {
		usage();
		error = EINVAL;
		goto error_exit;
	}
	
	error = 0;

	/*
	 * Create an argument vector to pass to execve.
	 * The only difference is argv[0] is set to "webdavfs_agent",
	 * which allows the 'ps' command to display the correct command name
	 * in the listing. 
	 */
	for (cnt = 0; argv[cnt]; cnt++)
		;
		
	// allocate new arg vector, adding an extra element for the list terminator
	argv_child = alloca((cnt + 1) * sizeof(char *));
	if (argv_child == NULL) {
		error = ENOMEM;
		goto error_exit;
	}

	/* this is the command name of the new process */
	argv_child[0] = "webdavfs_agent";
	argv_child[cnt] = (char *)0;
	/* copy over the remaining argv strings */
	bcopy(argv + 1, argv_child + 1, (cnt - 1) * sizeof(char *));

	/* if the parent is terminated, it will exit with EXIT_SUCCESS */
	signal(SIGTERM, parentexit);
	
	pid = fork();
	
	if (pid < 0) {
		error = errno;
		goto error_exit;
	}
	
	if (pid == 0)
	{
		char CFUserTextEncodingEnvSetting[sizeof(CFENVFORMATSTRING) + 20];
		/*
		 * Add  WEBDAVFS_AGENT_PATH to the environment so that libsecurity knows where
		 * to look for webdavfs_agent when  adding it to the keychain.
		 */
		char *env[] = {CFUserTextEncodingEnvSetting, WEBDAVFS_AGENT_PATH, "", (char *) 0 };
		
		/* 
		 * Create a new environment with a definition of __CF_USER_TEXT_ENCODING to work 
		 * around CF's interest in the user's home directory (which could be networked, 
		 * causing recursive references through automount). Make sure we include the uid
		 * since CF will check for this when deciding if to look in the home directory.
		 */ 
		snprintf(CFUserTextEncodingEnvSetting, sizeof(CFUserTextEncodingEnvSetting), CFENVFORMATSTRING, getuid());
		
		/* child executes the webdavfs agent */
		execve(PRIVATE_WEBDAVFS_AGENT_COMMAND, argv_child, env);
		
		/* We can only get here if the exec failed */
		error = errno;
		goto error_exit;
	}
	
	/* Wait for child's signal or for child's completion */
	while ( (terminated_pid = wait4(pid, (int *)&status, 0, NULL)) < 0 )
	{
		/* retry if EINTR, else break out with error */
		if ( errno != EINTR )
		{
			break;
		}
	}
		
	/* we'll get here only if the child completed before killing us */
	if ( (terminated_pid == pid) && WIFEXITED(status) )
	{
		error = WEXITSTATUS(status);
	}
	else
	{
		error = ECHILD;
	}

error_exit:

	/* Return the set of error codes things expect mounts to return */

	switch (error)
	{
		/* The server directory could not be mounted by mount_webdav because the node path is invalid. */
		case ENOENT:
			break;
			
		/* Could not connect to the server because the name or password is not correct and the user canceled */
		case ECANCELED:
			break;
			
		/* You are already connected to this server volume */
		case EBUSY:
			break;
			
		/* You cannot connect to this server because it cannot be found on the network. Try again later or try a different URL. */
		case ENODEV:
			break;
			
		/* ap everything else to a generic unexpected error */
		default:
			error = EINVAL;
			break;
	}

	exit(error);
}
