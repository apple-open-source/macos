/*
 *  webdavd.c
 *  webdavfs
 *
 *  Created by William Conway on 12/15/06.
 *  Copyright 2006 Apple Computer, Inc. All rights reserved.
 *
 */

#include "webdavd.h"
#include "LogMessage.h"

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

#include <mntopts.h>
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
int gSecureServerAuth = FALSE;		/* if TRUE, the authentication for server challenges must be sent securely (not clear-text) */
char gWebdavCachePath[MAXPATHLEN + 1] = ""; /* the current path to the cache directory */
int gSecureConnection = FALSE;	/* if TRUE, the connection is secure */
CFURLRef gBaseURL = NULL;		/* the base URL for this mount */

/*
 * mount_webdav.c file globals
 */
static int wakeupFDs[2] = { -1, -1 };	/* used by webdav_kill() to communicate with main select loop */
static char mountPoint[MAXPATHLEN];		/* path to our mount point */
static char mntfromname[MNAMELEN];		/* the mntfromname */

/*
 * We want getmntopts() to simply ignore
 * unrecognized mount options.
 */
int getmnt_silent = 1;

#define CFENVFORMATSTRING "__CF_USER_TEXT_ENCODING=0x%X:0:0"

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

static void usage(void)
{
	(void)fprintf(stderr,
		"usage: mount_webdav [-s] [-S] [-a<fd>] [-o options] [-v <volume name>]\n");
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
	
	*isHTTPS = (strncasecmp(arguri, "https://", strlen("https://")) == 0);
	
	/* if there's no scheme, we'll have to add "http://" */
	hasScheme = ((strncasecmp(arguri, "http://", strlen("http://")) == 0) || *isHTTPS);
	
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
		char CFUserTextEncodingEnvSetting[sizeof(CFENVFORMATSTRING) + 20]; 
		char *env[] = {CFUserTextEncodingEnvSetting, "", (char *) 0 };
		
		/* 
		 * Create a new environment with a definition of __CF_USER_TEXT_ENCODING to work 
		 * around CF's interest in the user's home directory (which could be networked, 
		 * causing recursive references through automount). Make sure we include the uid
		 * since CF will check for this when deciding if to look in the home directory.
		 */ 
		snprintf(CFUserTextEncodingEnvSetting, sizeof(CFUserTextEncodingEnvSetting), CFENVFORMATSTRING, getuid());
		
		result = execle(PRIVATE_UNMOUNT_COMMAND, PRIVATE_UNMOUNT_COMMAND,
			PRIVATE_UNMOUNT_FLAGS, mntpt,  (char *) 0, env);
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
		char CFUserTextEncodingEnvSetting[sizeof(CFENVFORMATSTRING) + 20]; 
		char *env[] = {CFUserTextEncodingEnvSetting, "", (char *) 0 };
		
		/* 
		 * Create a new environment with a definition of __CF_USER_TEXT_ENCODING to work 
		 * around CF's interest in the user's home directory (which could be networked, 
		 * causing recursive references through automount). Make sure we include the uid
		 * since CF will check for this when deciding if to look in the home directory.
		 */ 
		snprintf(CFUserTextEncodingEnvSetting, sizeof(CFUserTextEncodingEnvSetting), CFENVFORMATSTRING, getuid());
		
		result = execle(PRIVATE_LOAD_COMMAND, PRIVATE_LOAD_COMMAND, (char *) 0, env);
		
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
	struct statfs *buffer;
	int mntflags;
	int servermntflags;
	struct vfsconf vfc;
	mode_t mode_mask;
	int return_code;
	int listen_socket;
	int store_notify_fd;
	int lowdisk_notify_fd;
	int out_token;
	int error;
	int ch;
	int i;
	int count;
	unsigned int mntfromnameLength;
	struct rlimit rlp;
	struct node_entry *root_node;
	char volumeName[NAME_MAX + 1] = "";
	char *uri;
	int mirrored_mount;
	int isMounted = FALSE;			/* TRUE if we make it past mount(2) */
	mntoptparse_t mp;

	char user[WEBDAV_MAX_USERNAME_LEN];
	char pass[WEBDAV_MAX_PASSWORD_LEN];
	char domain[WEBDAV_MAX_DOMAIN_LEN];
	
	struct webdav_request_statfs request_statfs;
	struct webdav_reply_statfs reply_statfs;
	int tempError;
	struct statfs buf;
	
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
	while ((ch = getopt(argc, argv, "sSa:o:v:")) != -1)
	{
		switch (ch)
		{
			case 'a':	/* get the username and password from URLMount */
				{
					int fd = atoi(optarg), 		/* fd from URLMount */
					zero = 0;
					uint32_t be_len1, be_len2, be_len3;
					size_t len1, len2, len3;

					/* read the username length, the username, the password
					  length, and the password */
					if (fd >= 0 && lseek(fd, 0LL, SEEK_SET) != -1)
					{
						if (read(fd, &be_len1, sizeof be_len1) == sizeof be_len1 &&
						    (len1 = ntohl(be_len1)) > 0 &&
						    len1 < WEBDAV_MAX_USERNAME_LEN)
						{
							if (read(fd, user, len1) > 0)
							{
								user[len1] = '\0';
								if (read(fd, &be_len2, sizeof be_len2) == sizeof be_len2 &&
								    (len2 = ntohl(be_len2)) > 0 &&
								    len2 < WEBDAV_MAX_PASSWORD_LEN)
								{
									if (read(fd, pass, len2) > 0)
									{
										pass[len2] = '\0';
										if (read(fd, &be_len3, sizeof be_len3) == sizeof be_len3 &&
										    (len3 = ntohl(be_len3)) > 0 &&
										    len3 < WEBDAV_MAX_DOMAIN_LEN)
										{
											if (read(fd, domain, len3) > 0)
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
							struct stat statb;
							off_t bytes_to_overwrite;
							size_t bytes_to_write;

							if (fstat(fd, &statb) != -1) {
								bytes_to_overwrite = statb.st_size;
								(void)lseek(fd, 0LL, SEEK_SET);
								while (bytes_to_overwrite != 0) {
									bytes_to_write = bytes_to_overwrite;
									if (bytes_to_write > sizeof zero)
										bytes_to_write = sizeof zero;
									if (write(fd, (char *) & zero, bytes_to_write) < 0)
									{
										break;
									}
									bytes_to_overwrite -= bytes_to_write;
								}
								(void)fsync(fd);
							}
							(void)close(fd);
						}
					}
					break;
				}
			
			case 'S':	/* Suppress ALL dialogs and notifications */
				gSuppressAllUI = 1;
				break;
			
			case 's':	/* authentication to server must be secure */
				gSecureServerAuth = 1;
				break;
			
			case 'o':	/* Get the mount options */
				{
					const struct mntopt mopts[] = {
						MOPT_USERQUOTA,
						MOPT_GROUPQUOTA,
						MOPT_FSTAB_COMPAT,
						MOPT_NODEV,
						MOPT_NOEXEC,
						MOPT_NOSUID,
						MOPT_RDONLY,
						MOPT_UNION,
						MOPT_BROWSE,
						MOPT_AUTOMOUNTED,
						MOPT_QUARANTINE,
						{ NULL, 0, 0, 0 }
					};
					
					mp = getmntopts(optarg, mopts, &mntflags, 0);
					if (mp == NULL)
						error = 1;
					else
						freemntopts(mp);
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
	
	/* Check to see if this mntfromname is already used by a mount point by the
	 * current user. Sure, someone could mount using the DNS name one time and
	 * the IP address the next, or they could  munge the path with escaped characters,
	 * but this check will catch the obvious duplicates.
	 */
	count = getmntinfo(&buffer, MNT_NOWAIT);
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
			/* Handle MNT_DONTBROWSE separately per:
			 * <rdar://problem/5322867> iDisk gets mounted multiple timesif kMarkDontBrowse is set
			 */
			if(((buffer[i].f_flags & MNT_DONTBROWSE) && (mntflags & MNT_DONTBROWSE)) ||
				(!(buffer[i].f_flags & MNT_DONTBROWSE) && !(mntflags & MNT_DONTBROWSE)))
			{
				/* Yes, this mntfromname is in use - return EBUSY
				 * (the same error that you'd get if you tried mount a disk device twice).
				 */
				LogMessage(kSysLog | kError, "%s is already mounted: %s\n", mntfromname, strerror(EBUSY));
				error = EBUSY;
				goto error_exit;
			}
		}
	}
	
	/* is WEBDAVFS_DEBUG environment variable set? */
	gWebdavfsDebug = (getenv("WEBDAVFS_DEBUG") != NULL);
	
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
	openlog("webdavfs_agent", LOG_CONS | LOG_PID, LOG_DAEMON);
	
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
	free(uri);	/* all done with uri */
	uri = NULL;
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
	args.pa_version = kCurrentWebdavArgsVersion;
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
	
	/* need to get the statfs information before we can call mount */
	bzero(&reply_statfs, sizeof(struct webdav_reply_statfs));

	request_statfs.pcr.pcr_uid = getuid();
	request_statfs.pcr.pcr_ngroups = getgroups(NGROUPS_MAX, request_statfs.pcr.pcr_groups);
	
	request_statfs.root_obj_id = root_node->nodeid;

	tempError = filesystem_statfs(&request_statfs, &reply_statfs);
	
	memset (&args.pa_vfsstatfs, 0, sizeof (args.pa_vfsstatfs));

	if (tempError == 0) {
		args.pa_vfsstatfs.f_bsize = reply_statfs.fs_attr.f_bsize;
		args.pa_vfsstatfs.f_iosize = reply_statfs.fs_attr.f_iosize;
		args.pa_vfsstatfs.f_blocks = reply_statfs.fs_attr.f_blocks;
		args.pa_vfsstatfs.f_bfree = reply_statfs.fs_attr.f_bfree;
		args.pa_vfsstatfs.f_bavail = reply_statfs.fs_attr.f_bavail;
		args.pa_vfsstatfs.f_files = reply_statfs.fs_attr.f_files;
		args.pa_vfsstatfs.f_ffree = reply_statfs.fs_attr.f_ffree;
	}
	
	/* since we just read/write to a local cached file, set the iosize to be the same size as the local filesystem */
	tempError = statfs(_PATH_TMP, &buf);
	if ( tempError == 0 )
		args.pa_vfsstatfs.f_iosize = buf.f_iosize;

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
	tempError = notify_register_file_descriptor("com.apple.system.lowdiskspace.system", &lowdisk_notify_fd, 0, &out_token);
	if (tempError != NOTIFY_STATUS_OK)
	{
		syslog(LOG_ERR, "failed to register for low disk space notifications: err = %u\n", tempError);
		lowdisk_notify_fd = -1;
	}
	
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
			if (error != EINTR)
				LogMessage(kSysLog | kError, "webdavfs_agent exiting on select errno %d for %s\n", error, mountPoint);
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
					LogMessage(kSysLog | kError, "webdavfs_agent received signal: %d. Unmounting %s\n", message, mountPoint);
				}
				else
				{
					if ( message == -2 )
					{
						/* this is the normal way out of the select loop */
						LogMessage(kSysLog | kError, "webdavfs_agent received unmount message for %s\n", mountPoint);
						break;
					}
					else
					{
						LogMessage(kSysLog | kError, "webdavfs_agent received message: %d. Force unmounting %s\n", message, mountPoint);
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
		if ( lowdisk_notify_fd != -1 )
		{
			if ( FD_ISSET(lowdisk_notify_fd, &readfds) )
			{
				ssize_t bytes_read;
			
				/* read the token off the fd and do nothing with it */
				bytes_read = read(lowdisk_notify_fd, &out_token, sizeof(out_token));
			
				/* try to help the system out by purging any closed cache files */
				(void) requestqueue_purge_cache_files();
			}
		}
	}
	
	LogMessage(kTrace, "%s unmounted\n", mountPoint);

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
