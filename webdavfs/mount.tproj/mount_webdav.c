/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
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

#include "webdavd.h"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <sys/syslog.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/syslimits.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <time.h>
#include <notify.h>

#include <CoreServices/CoreServices.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "mntopts.h"
#include "webdav_authcache.h"
#include "webdav_network.h"
#include "webdav_requestqueue.h"
#include "webdav_cache.h"

/*****************************************************************************/

/*
 * shared globals
 */
unsigned int gtimeout_val;		/* the pulse_thread runs at double this rate */
char *gtimeout_string;			/* the length of time LOCKs are held on on the server */
int gWebdavfsDebug = FALSE;		/* TRUE if the WEBDAVFS_DEBUG environment variable is set */
uid_t gProcessUID = -1;			/* the daemon's UID */
int gSuppressAllUI = FALSE;		/* if TRUE, the mount requested that all UI be supressed */
char gWebdavCachePath[MAXPATHLEN + 1] = ""; /* the current path to the cache directory */
int gSecureConnection = FALSE;	/* if TRUE, the connection is secure */
CFURLRef gBaseURL = NULL;		/* the base URL for this mount */

/*
 * mount_webdav.c file globals
 */
static int wakeupFDs[2] = { -1, -1 };	/* used by webdav_kill() to communicate with main select loop */
static char mountPoint[MAXPATHLEN];		/* path to our mount point */
static char mntfromname[MNAMELEN];		/* the mntfromname */

/*****************************************************************************/

void webdav_debug_assert(const char *componentNameString, const char *assertionString, 
	const char *exceptionLabelString, const char *errorString, 
	const char *fileName, long lineNumber, int errorCode)
{
	#pragma unused(componentNameString)

	if ( (assertionString != NULL) && (*assertionString != '\0') )
	{
		if ( errorCode != 0 )
		{
			syslog(WEBDAV_LOG_LEVEL, "(%s) failed with %d%s%s%s%s; file: %s; line: %ld", 
			assertionString,
			errorCode,
			(errorString != NULL) ? "; " : "",
			(errorString != NULL) ? errorString : "",
			(exceptionLabelString != NULL) ? "; going to " : "",
			(exceptionLabelString != NULL) ? exceptionLabelString : "",
			fileName,
			lineNumber);
		}
		else
		{
			syslog(WEBDAV_LOG_LEVEL, "(%s) failed%s%s%s%s; file: %s; line: %ld", 
			assertionString,
			(errorString != NULL) ? "; " : "",
			(errorString != NULL) ? errorString : "",
			(exceptionLabelString != NULL) ? "; going to " : "",
			(exceptionLabelString != NULL) ? exceptionLabelString : "",
			fileName,
			lineNumber);
		}
	}
	else
	{
		syslog(WEBDAV_LOG_LEVEL, "%s; file: %s; line: %ld", 
		errorString,
		fileName,
		lineNumber);
	}
}

/*****************************************************************************/

/*
 * webdav_kill lets the select loop know to call webdav_force_unmount by feeding
 * the wakeupFDs pipe. This is the signal handler for signals that should
 * terminate us.
 */
void webdav_kill(int message)
{
	/* if there's a read end of the pipe*/
	if (wakeupFDs[0] != -1)
	{
		/* if there's a write end of the  pipe */
		if (wakeupFDs[1] != -1)
		{
			/* write the message */
			verify(write(wakeupFDs[1], &message, sizeof(int)) == sizeof(int));
		}
		/* else we are already in the process of force unmounting */
	}
	else
	{
		/* there's no read end so just exit */
		exit(EXIT_FAILURE);
	}
}

/*****************************************************************************/

/*
 * webdav_force_unmount
 *
 * webdav_force_unmount is called from our select loop when the mount_webdav
 * process receives a signal, or hits some unrecoverable condition which
 * requires a force unmount.
 */
static void webdav_force_unmount(char *mntpt)
{
	int pid, terminated_pid;
	int result = -1;
	union wait status;

	pid = fork();
	if (pid == 0)
	{
		result = execl(PRIVATE_UNMOUNT_COMMAND, PRIVATE_UNMOUNT_COMMAND,
			PRIVATE_UNMOUNT_FLAGS, mntpt, NULL);
		/* We can only get here if the exec failed */
		goto Return;
	}
	
	require(pid != -1, Return);

	/* wait for completion here */
	while ( (terminated_pid = wait4(pid, (int *)&status, 0, NULL)) < 0 )
	{
		/* retry if EINTR, else break out with error */
		if ( errno != EINTR )
		{
			break;
		}
	}

Return:
	
	/* execution will not reach this point unless umount fails */
	check_noerr_string(errno, strerror(errno));

	_exit(EXIT_FAILURE);
}

/*****************************************************************************/

/*
 * The attempt_webdav_load function forks and executes the load_webdav command
 * which in turn loads the webdavfs kext.
 */
static int attempt_webdav_load(void)
{
	int pid, terminated_pid;
	int result = -1;
	union wait status;

	pid = fork();
	if (pid == 0)
	{
		result = execl(PRIVATE_LOAD_COMMAND, PRIVATE_LOAD_COMMAND, NULL);
		
		/* We can only get here if the exec failed */
		goto Return;
	}

	require_action(pid != -1, Return, result = errno);

	/* Success! */
	while ( (terminated_pid = wait4(pid, (int *)&status, 0, NULL)) < 0 )
	{
		/* retry if EINTR, else break out with error */
		if ( errno != EINTR )
		{
			break;
		}
    }

    if ( (terminated_pid == pid) && (WIFEXITED(status)) )
	{
		result = WEXITSTATUS(status);
	}
	else
	{
		result = -1;
    }

Return:
	
	check_noerr_string(result, strerror(errno));
	
	return result;
}

/*****************************************************************************/

/* called with child normally terminates the parent */
static void parentexit(int x)
{
#pragma unused(x)
	exit(EXIT_SUCCESS);
}

/*****************************************************************************/

/* start up a new thread to call webdav_force_unmount() */
static void create_unmount_thread(void)
{
	int error;
	pthread_t unmount_thread;
	pthread_attr_t unmount_thread_attr;

	error = pthread_attr_init(&unmount_thread_attr);
	require_noerr(error, pthread_attr_init);

	error = pthread_attr_setdetachstate(&unmount_thread_attr, PTHREAD_CREATE_DETACHED);
	require_noerr(error, pthread_attr_setdetachstate);

	error = pthread_create(&unmount_thread, &unmount_thread_attr,
		(void *)webdav_force_unmount, (void *)mntfromname);
	require_noerr(error, pthread_create);
	
	return;

pthread_create:
pthread_attr_setdetachstate:
pthread_attr_init:

	exit(error);
}

/*****************************************************************************/

/* start up a new thread to call network_update_proxy() */
static void create_change_thread(void)
{
	int error;
	pthread_t change_thread;
	pthread_attr_t change_thread_attr;

	error = pthread_attr_init(&change_thread_attr);
	require_noerr(error, pthread_attr_init);

	error = pthread_attr_setdetachstate(&change_thread_attr, PTHREAD_CREATE_DETACHED);
	require_noerr(error, pthread_attr_setdetachstate);

	error = pthread_create(&change_thread, &change_thread_attr,
		(void *)network_update_proxy, (void *)NULL);
	require_noerr(error, pthread_create);
	
	return;

pthread_create:
pthread_attr_setdetachstate:
pthread_attr_init:

	exit(error);
}

/*****************************************************************************/

static void usage(void)
{
	(void)fprintf(stderr,
		"usage: mount_webdav [-S] [-a<fd>] [-o options] [-v <volume name>]\n");
	(void)fprintf(stderr,
		"\t<WebDAV_URL> node\n");
}

/*****************************************************************************/

static
char *GetMountURI(char *arguri, int *isHTTPS)
{
	int hasScheme;
	int hasTrailingSlash;
	size_t argURILength;
	char *uri;
	
	argURILength = strlen(arguri);
	
	*isHTTPS = (strncmp(arguri, "https://", strlen("https://")) == 0);
	
	/* if there's no scheme, we'll have to add "http://" */
	hasScheme = ((strncmp(arguri, "http://", strlen("http://")) == 0) || *isHTTPS);
	
	/* if there's no trailing slash, we'll have to add one */
	hasTrailingSlash = arguri[argURILength - 1] == '/';
	
	/* allocate space for url */
	uri = malloc(argURILength + 
		(hasScheme ? 0 : strlen("http://")) +
		(hasTrailingSlash ? 0 : 1) +
		1);
	require(uri != NULL, malloc_uri);
	
	/* copy arguri adding scheme and trailing slash if needed */ 
	if ( !hasScheme )
	{
		strcpy(uri, "http://");
	}
	else
	{
		*uri = '\0';
	}
	strcat(uri, arguri);
	if ( !hasTrailingSlash )
	{
		strcat(uri, "/");
	}
		
malloc_uri:

	return ( uri );
}

/*****************************************************************************/

#define TMP_WEBDAV_UDS _PATH_TMP ".webdavUDS.XXXXXX"	/* Scratch socket name */

/* maximum length of username and password */
#define WEBDAV_MAX_USERNAME_LEN 256
#define WEBDAV_MAX_PASSWORD_LEN 256
#define WEBDAV_MAX_DOMAIN_LEN 256

/*****************************************************************************/

int main(int argc, char *argv[])
{
	struct webdav_args args;
	struct sockaddr_un un;
	int mntflags;
	int servermntflags;
	struct vfsconf vfc;
	mode_t mode_mask;
	int pid, terminated_pid;
	union wait status;
	int return_code;
	int listen_socket;
	int store_notify_fd;
	int lowdisk_notify_fd;
	int out_token;
	int error;
	int ch;
	int i;
	struct rlimit rlp;
	struct node_entry *root_node;
	char volumeName[NAME_MAX + 1] = "";
	char *uri;
	int mirrored_mount;
	int isMounted = FALSE;			/* TRUE if we make it past mount(2) */

	char user[WEBDAV_MAX_USERNAME_LEN];
	char pass[WEBDAV_MAX_PASSWORD_LEN];
	char domain[WEBDAV_MAX_DOMAIN_LEN];
	
	error = 0;
	
	/* store our UID */
	gProcessUID = getuid();
	
	user[0] = '\0';
	pass[0] = '\0';
	domain[0] = '\0';
	
	mntflags = 0;
	/*
	 * Crack command line args
	 */
	while ((ch = getopt(argc, argv, "Sa:o:v:")) != -1)
	{
		switch (ch)
		{
			case 'a':	/* get the username and password from URLMount */
				{
					int fd = atoi(optarg), 		/* fd from URLMount */
					zero = 0, len1 = 0, len2 = 0, len3 = 0;

					/* read the username length, the username, the password
					  length, and the password */
					if (fd >= 0 && lseek(fd, 0LL, SEEK_SET) != -1)
					{
						if (read(fd, &len1, sizeof(int)) > 0 && len1 > 0 && len1 < WEBDAV_MAX_USERNAME_LEN)
						{
							if (read(fd, user, (size_t)len1) > 0)
							{
								user[len1] = '\0';
								if (read(fd, &len2, sizeof(int)) > 0 && len2 > 0 && len2 < WEBDAV_MAX_PASSWORD_LEN)
								{
									if (read(fd, pass, (size_t)len2) > 0)
									{
										pass[len2] = '\0';
										if (read(fd, &len3, sizeof(int)) > 0 && len3 > 0 && len3 < WEBDAV_MAX_DOMAIN_LEN)
										{
											if (read(fd, domain, (size_t)len3) > 0)
											{
												domain[len3] = '\0';
											}
										}
									}
								}
							}
						}
						
						/* zero contents of file and close it if
						 * fd is not STDIN_FILENO, STDOUT_FILENO or STDERR_FILENO
						 */
						if ( (fd != STDIN_FILENO) &&
							 (fd != STDOUT_FILENO) &&
							 (fd != STDERR_FILENO) )
						{
							(void)lseek(fd, 0LL, SEEK_SET);
							for (i = 0; i < (((len1 + len2 + len3) / (int)sizeof(int)) + 3); i++)
							{
								if (write(fd, (char *) & zero, sizeof(int)) < 0)
								{
									break;
								}
							}
							(void)fsync(fd);
							(void)close(fd);
						}
					}
					break;
				}
			
			case 'S':	/* Suppress ALL dialogs and notifications */
				gSuppressAllUI = 1;
				break;
			
			case 'o':	/* Get the mount options */
				{
					const struct mntopt mopts[] = {
						MOPT_STDOPTS,
						{ NULL, 0, 0, 0 }
					};
					
					error = getmntopts(optarg, mopts, &mntflags, 0);
				}
				break;
			
			case 'v':	/* Use argument as volume name instead of parsing
						 * the mount point path for the volume name
						 */
				if ( strlen(optarg) <= NAME_MAX )
				{
					strcpy(volumeName, optarg);
				}
				else
				{
					error = 1;
				}
				break;
			
			default:
				error = 1;
				break;
		}
	}
	
	if (!error)
	{
		if (optind != (argc - 2) || strlen(argv[optind]) > MAXPATHLEN)
		{
			error = 1;
		}
	}

	require_noerr_action_quiet(error, error_exit, error = EINVAL; usage());
	
	/* does this look like a mirrored mount (UI suppressed and not browseable) */
	mirrored_mount = gSuppressAllUI && (mntflags & MNT_DONTBROWSE);
	
	/* get the mount point */
	require_action(realpath(argv[optind + 1], mountPoint) != NULL, error_exit, error = ENOENT);
	
	/* derive the volume name from the mount point if needed */
	if ( *volumeName == '\0' )
	{
		/* the volume name wasn't passed on the command line so use the
		 * last path segment of mountPoint
		 */
		strcpy(volumeName, strrchr(mountPoint, '/') + 1);
	}

	/* Get uri (fix it up if needed) */
	uri = GetMountURI(argv[optind], &gSecureConnection);
	require_action_quiet(uri != NULL, error_exit, error = EINVAL);

	/* Create a mntfromname from the uri. Make sure the string is no longer than MNAMELEN */
	strncpy(mntfromname, uri , MNAMELEN);
	mntfromname[MNAMELEN] = '\0';
	
	/* if this is going to be a volume on the desktop (the MNT_DONTBROWSE is not set)
	 * then check to see if this mntfromname is already used by a mount point by the
	 * current user. Sure, someone could mount using the DNS name one time and
	 * the IP address the next, or they could  munge the path with escaped characters,
	 * but this check will catch the obvious duplicates.
	 */
	if ( !(mntflags & MNT_DONTBROWSE) )
	{
		struct statfs *	buffer;
		int				count = getmntinfo(&buffer, MNT_NOWAIT);
		unsigned int	mntfromnameLength;
		
		mntfromnameLength = strlen(mntfromname);
		for (i = 0; i < count; i++)
		{
			/* Is mntfromname already being used as a mntfromname for a webdav mount
			 * owned by this user?
			 */
			if ( (buffer[i].f_owner == gProcessUID) &&
				 (strcmp("webdav", buffer[i].f_fstypename) == 0) &&
				 (strlen(buffer[i].f_mntfromname) == mntfromnameLength) &&
				 (strncasecmp(buffer[i].f_mntfromname, mntfromname, mntfromnameLength) == 0) )
			{
				/* Yes, this mntfromname is in use - return EBUSY
				 * (the same error that you'd get if you tried mount a disk device twice).
				 */
				syslog(LOG_ERR, "%s is already mounted: %s", mntfromname, strerror(EBUSY));
				error = EBUSY;
				goto error_exit;
			}
		}
	}
	
	/* is WEBDAVFS_DEBUG environment variable set? */
	gWebdavfsDebug = (getenv("WEBDAVFS_DEBUG") != NULL);
	
	/* if the parent is terminated, it will exit with EXIT_SUCCESS */
	signal(SIGTERM, parentexit);
	
	pid = fork();
	require_action(pid >= 0, error_exit, error = errno);
	
	if (pid > 0)
	{
		/* Parent waits for child's signal or for child's completion here */
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
		goto error_exit;
	}

	/* We're the child */
	
	/* detach from controlling tty and start a new process group */
	if ( setsid() < 0 )
	{
		debug_string("setsid() failed");
	}
	
	(void)chdir("/");

	if ( !gWebdavfsDebug )
	{
		/* redirect standard input, standard output, and standard error to /dev/null if not debugging */
		int fd;
		
		fd = open(_PATH_DEVNULL, O_RDWR, 0);
		if (fd != -1)
		{
			(void)dup2(fd, STDIN_FILENO);
			(void)dup2(fd, STDOUT_FILENO);
			(void)dup2(fd, STDERR_FILENO);
			if (fd > 2)
			{
				(void)close(fd);
			}
		}
	}
	
	/* Workaround for daemon/mach ports problem... */
	CFRunLoopGetCurrent();
	
	/*
	 * Set the default timeout value
	 */
	gtimeout_string = WEBDAV_PULSE_TIMEOUT;
	gtimeout_val = atoi(gtimeout_string);

	/* get the current maximum number of open files for this process */
	error = getrlimit(RLIMIT_NOFILE, &rlp);
	require_noerr_action(error, error_exit, error = EINVAL);

	/* Close any open file descriptors we may have inherited from our
	 * parent caller.  This excludes the first three.  We don't close
	 * STDIN_FILENO, STDOUT_FILENO or STDERR_FILENO. Note, this has to
	 * be done before we open any other file descriptors, but after we
	 * check for a file containing authentication in /tmp.
	 */
	closelog();
	for (i = 0; i < (int)rlp.rlim_cur; ++i)
	{
		switch (i)
		{
		case STDIN_FILENO:
		case STDOUT_FILENO:
		case STDERR_FILENO:
			break;
		default:
			(void)close(i);
		}
	}

	/* Start logging (and change name) */
	openlog("webdavd", LOG_CONS | LOG_PID, LOG_DAEMON);
	
	/* raise the maximum number of open files for this process if needed */
	if ( rlp.rlim_cur < WEBDAV_RLIMIT_NOFILE )
	{
		rlp.rlim_cur = WEBDAV_RLIMIT_NOFILE;
		error = setrlimit(RLIMIT_NOFILE, &rlp);
		require_noerr_action(error, error_exit, error = EINVAL);
	}

	/* Set global signal handling to protect us from SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	/* Make sure our kext and filesystem are loaded */
	vfc.vfc_typenum = -1;
	error = getvfsbyname("webdav", &vfc);
	if (error)
	{
		error = attempt_webdav_load();
		require_noerr_action_quiet(error, error_exit, error = EINVAL);

		error = getvfsbyname("webdav", &vfc);
	}
	require_noerr_action(error, error_exit, error = EINVAL);

	error = nodecache_init(strlen(uri), uri, &root_node);
	require_noerr_action_quiet(error, error_exit, error = EINVAL);
	
	error = authcache_init(user, pass, domain);
	require_noerr_action_quiet(error, error_exit, error = EINVAL);
	
	bzero(user, sizeof(user));
	bzero(pass, sizeof(pass));
	bzero(domain, sizeof(domain));
	
	error = network_init((const UInt8 *)uri, strlen(uri), &store_notify_fd, mirrored_mount);
	require_noerr_action_quiet(error, error_exit, error = EINVAL);
	
	error = filesystem_init(vfc.vfc_typenum);
	require_noerr_action_quiet(error, error_exit, error = EINVAL);

	error = requestqueue_init();
	require_noerr_action_quiet(error, error_exit, error = EINVAL);

	/*
	 * Check out the server and get the mount flags
	 */
	servermntflags = 0;
	error = filesystem_mount(&servermntflags);
	require_noerr_quiet(error, error_exit);
	
	/*
	 * OR in the mnt flags forced on us by the server
	 */
	mntflags |= servermntflags;

	/*
	 * Construct the listening socket
	 */
	listen_socket = socket(PF_LOCAL, SOCK_STREAM, 0);
	require_action(listen_socket >= 0, error_exit, error = EINVAL);

	bzero(&un, sizeof(un));
	un.sun_len = sizeof(un);
	un.sun_family = AF_LOCAL;
	require_action(sizeof(TMP_WEBDAV_UDS) < sizeof(un.sun_path), error_exit, error = EINVAL);
	
	strcpy(un.sun_path, TMP_WEBDAV_UDS);
	require_action(mktemp(un.sun_path) != NULL, error_exit, error = EINVAL);

	/* bind socket with write-only access which is all that's needed for the kext to connect */
	mode_mask = umask(0555);
	require_action(bind(listen_socket, (struct sockaddr *)&un, sizeof(un)) >= 0, error_exit, error = EINVAL);
	(void)umask(mode_mask);

	/* make it hard for anyone to unlink the name */
	require_action(chflags(un.sun_path, UF_IMMUTABLE) == 0, error_exit, error = EINVAL);

	/* listen with plenty of backlog */
	require_noerr_action(listen(listen_socket, WEBDAV_MAX_KEXT_CONNECTIONS), error_exit, error = EINVAL);
	
	/*
	 * Ok, we are about to set up the mount point so set the signal handlers
	 * so that we know if someone is trying to kill us.
	 */
	 
	/* open the wakeupFDs pipe */
	require_action(pipe(wakeupFDs) == 0, error_exit, wakeupFDs[0] = wakeupFDs[1] = -1; error = EINVAL);
	
	/* set the signal handler to webdav_kill for the signals that aren't ignored by default */
	signal(SIGHUP, webdav_kill);
	signal(SIGINT, webdav_kill);
	signal(SIGQUIT, webdav_kill);
	signal(SIGILL, webdav_kill);
	signal(SIGTRAP, webdav_kill);
	signal(SIGABRT, webdav_kill);
	signal(SIGEMT, webdav_kill);
	signal(SIGFPE, webdav_kill);
	signal(SIGBUS, webdav_kill);
	signal(SIGSEGV, webdav_kill);
	signal(SIGSYS, webdav_kill);
	signal(SIGALRM, webdav_kill);
	signal(SIGTSTP, webdav_kill);
	signal(SIGTTIN, webdav_kill);
	signal(SIGTTOU, webdav_kill);
	signal(SIGXCPU, webdav_kill);
	signal(SIGXFSZ, webdav_kill);
	signal(SIGVTALRM, webdav_kill);
	signal(SIGPROF, webdav_kill);
	signal(SIGUSR1, webdav_kill);
	signal(SIGUSR2, webdav_kill);

	/* prepare mount args */
	args.pa_mntfromname = mntfromname;
	args.pa_version = 1;
	args.pa_socket_namelen = sizeof(un);
	args.pa_socket_name = (struct sockaddr *)&un;
	args.pa_vol_name = volumeName;
	args.pa_flags = 0;
	if ( gSuppressAllUI )
	{
		args.pa_flags |= WEBDAV_SUPPRESSALLUI;
	}
	if ( gSecureConnection )
	{
		args.pa_flags |= WEBDAV_SECURECONNECTION;
	}
	args.pa_root_id = root_node->nodeid;
	args.pa_root_fileid = WEBDAV_ROOTFILEID;
	args.pa_dir_size = WEBDAV_DIR_SIZE;
	/* pathconf values: >=0 to return value; -1 if not supported */
	args.pa_link_max = 1;			/* 1 for file systems that do not support link counts */
	args.pa_name_max = NAME_MAX;	/* The maximum number of bytes in a file name */
	args.pa_path_max = PATH_MAX;	/* The maximum number of bytes in a pathname */
	args.pa_pipe_buf = -1;			/* no support for pipes */
	args.pa_chown_restricted = _POSIX_CHOWN_RESTRICTED; /* appropriate privileges are required for the chown(2) */
	args.pa_no_trunc = _POSIX_NO_TRUNC; /* file names longer than KERN_NAME_MAX are truncated */
	
	/* mount the volume */
	return_code = mount(vfc.vfc_name, mountPoint, mntflags, &args);
	require_noerr_action(return_code, error_exit, error = errno);
		
	/* we're mounted so kill our parent so it will call parentexit and exit with success */
 	kill(getppid(), SIGTERM);
	
	isMounted = TRUE;
	
	/* and set SIGTERM to webdav_kill */
	signal(SIGTERM, webdav_kill);
	
	syslog(LOG_INFO, "%s mounted", mountPoint);
	
	/*
	 * This code is needed so that the network reachability code doesn't have to
	 * be scheduled for every synchronous CFNetwork transaction. This is accomplished
	 * by setting a dummy callback on the main thread's CFRunLoop.
	 */
	{
		struct sockaddr_in socket_address;
		SCNetworkReachabilityRef reachabilityRef;
		
		bzero(&socket_address, sizeof(socket_address));
		socket_address.sin_len = sizeof(socket_address);
		socket_address.sin_family = AF_INET;
		socket_address.sin_port = 0;
		socket_address.sin_addr.s_addr = INADDR_LOOPBACK;
		
		reachabilityRef = SCNetworkReachabilityCreateWithAddress(kCFAllocatorDefault, (struct sockaddr*)&socket_address);
		if (reachabilityRef != NULL)
		{
			SCNetworkReachabilityScheduleWithRunLoop(reachabilityRef, CFRunLoopGetCurrent(), CFSTR("_nonexistent_"));
		}
	}
	
	/* register for lowdiskspace notifications on the system (boot) volume */
	verify(notify_register_file_descriptor("com.apple.system.lowdiskspace.system", &lowdisk_notify_fd, 0, &out_token) == 0);
	
	/*
	 * Just loop waiting for new connections and activating them
	 */
	while ( TRUE )
	{
		int accept_socket;
		fd_set readfds;
		int max_select_fd;
		
		/*
		 * Accept a new connection
		 * Will get EINTR if a signal has arrived, so just
		 * ignore that error code
		 */
		FD_ZERO(&readfds);
		FD_SET(listen_socket, &readfds);
		FD_SET(store_notify_fd, &readfds);
		max_select_fd = MAX(listen_socket, store_notify_fd);
		
		if ( lowdisk_notify_fd != -1 )
		{
			/* This allows low disk notifications to wake up the select() */
			FD_SET(lowdisk_notify_fd, &readfds);
			max_select_fd = MAX(lowdisk_notify_fd, max_select_fd);
		}
		
		if (wakeupFDs[0] != -1)
		{
			/* This allows writing to wakeupFDs[1] to wake up the select() */
			FD_SET(wakeupFDs[0], &readfds);
			max_select_fd = MAX(wakeupFDs[0], max_select_fd);
		}
		
		++max_select_fd;
				
		return_code = select(max_select_fd, &readfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0);
		if (return_code <= 0)
		{
			/* if select isn't working, then exit here */
			error = errno;
			require(error == EINTR, error_exit); /* EINTR is OK */
			continue;
		}
		
		/* was a signal received? */
		if ( (wakeupFDs[0] != -1) && FD_ISSET(wakeupFDs[0], &readfds) )
		{
			int message;
			
			/* read the message number out of the pipe */
			message = -1;
			(void)read(wakeupFDs[0], &message, sizeof(int));
			
			/* do nothing with SIGHUP -- we might want to use it to start some debug logging in the future */
			if (message == SIGHUP)
			{
			}
			else
			{
				/* time to force an unmount */
				wakeupFDs[0] = -1; /* don't look for anything else from the pipe */
				
				if ( message >= 0 )
				{
					/* positive messages are signal numbers */
					syslog(LOG_ERR, "mount_webdav received signal: %d. Unmounting %s", message, mountPoint);
				}
				else
				{
					if ( message == -2 )
					{
						/* this is the normal way out of the select loop */
						break;
					}
					else
					{
						syslog(LOG_ERR, "force unmounting %s", mountPoint);
					}
				}
				
				/* start a new thread to unmount */
				create_unmount_thread();
			}
		}
		
		/* was a message from the webdav kext received? */
		if ( FD_ISSET(listen_socket, &readfds) )
		{
			struct sockaddr_un addr;
			socklen_t addrlen;
			
			addrlen = sizeof(addr);
			accept_socket = accept(listen_socket, (struct sockaddr *)&addr, &addrlen);
			if (accept_socket < 0)
			{
				error = errno;
				require(error == EINTR, error_exit);
				continue;
			}
	
			/*
			 * Now put a new element on the thread queue so that a thread
			 *  will handle this.
			 */
			error = requestqueue_enqueue_request(accept_socket);
			require_noerr_quiet(error, error_exit);
		}
		
		/* was a message from the dynamic store received? */
		if ( FD_ISSET(store_notify_fd, &readfds) )
		{
			ssize_t bytes_read;
			int32_t buf;
			
			/* read the identifier off the fd and do nothing with it */
			bytes_read = read(store_notify_fd, &buf, sizeof(buf));
			
			/* pass the change notification off to another thread to handle */
			create_change_thread();
		}
		
		/* was a low disk notification received? */
		if ( FD_ISSET(lowdisk_notify_fd, &readfds) )
		{
			ssize_t bytes_read;
			
			/* read the token off the fd and do nothing with it */
			bytes_read = read(lowdisk_notify_fd, &out_token, sizeof(out_token));
			
			/* try to help the system out by purging any closed cache files */
			(void) requestqueue_purge_cache_files();
		}
	}
	
	syslog(LOG_INFO, "%s unmounted", mountPoint);
	
	/* attempt to delete the cache directory (if any) and the bound socket name */
	if (*gWebdavCachePath != '\0')
	{
		(void) rmdir(gWebdavCachePath);
	}
	
	/* clear the immutable flag and unlink the name from the listening socket */
	(void) chflags(un.sun_path, 0);
	(void) unlink(un.sun_path);

	exit(EXIT_SUCCESS);

error_exit:

	/* is we aren't mounted, return the set of error codes things expect mounts to return */
	if ( !isMounted )
	{
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
			
			/* Map everything else to a generic unexpected error */
			default:
				error = EINVAL;
				break;
		}
	}
	exit(error);
}

/*****************************************************************************/
