/*
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mount_webdav.c	8.6 (Berkeley) 4/26/95";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <sys/syslog.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/resource.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <kvm.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <mach/mach_error.h>
#include <DiskArbitration/DiskArbitration.h>

#include <ctype.h>
#include <CoreFoundation/CFString.h>
/* Needed to read the HTTP proxy configuration from the configuration 
   database */
#define USE_SYSTEMCONFIGURATION_PUBLIC_APIS
#include <SystemConfiguration/SystemConfiguration.h>

#include "fetch.h"
#include "mntopts.h"
#include "pathnames.h"
#include "webdavd.h"
#include "webdav_mount.h"
#include "webdav_memcache.h"
#include "webdav_authcache.h"
#include "webdav_requestqueue.h"
#include "webdav_inode.h"
#include "../webdav_fs.kextproj/webdav_fs.kmodproj/webdav.h"

/* to keep builds working with older errno.h where ECANCELED wasn't defined */
#ifndef ECANCELED
	#define	ECANCELED	89		/* Operation canceled */
#endif

/*****************************************************************************/

/* Local Definitions */

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	{ NULL }
};

struct file_array_element gfile_array[WEBDAV_MAX_OPEN_FILES];

int glast_array_element =0;
FILE * logfile = 0;

/* Globel locks used by the various cachees 
 * Note that the lock order is garray_lock followed
 * by ginode_lock.  That is if both are required the
 * garray_lock must be acquired first to avoid deadlocks
 */
 
pthread_mutex_t garray_lock;
pthread_mutex_t ginode_lock;

int gtimeout_val;
char * gtimeout_string;
webdav_memcache_header_t gmemcache_header;
webdav_file_record_t * ginode_hashtbl[WEBDAV_FILE_RECORD_HASH_BUCKETS];
u_int32_t ginode_cntr = WEBDAV_ROOTFILEID + 1;
char gmountpt[MAXPATHLEN];
webdav_requestqueue_header_t gwaiting_requests;
/* A ThreadSocket for each thread that might need a socket.
 * Access to this array is protected by grequests_lock.
 */
ThreadSocket webdav_threadsockets[WEBDAV_REQUEST_THREADS];
pthread_mutex_t grequests_lock;
pthread_cond_t gcondvar;
struct statfs gstatfsbuf;
time_t gstatfstime;

int proxy_ok = 0, proxy_exception = 0;
char *dest_server = NULL,
	*proxy_server = NULL,
	*http_hostname = NULL; 		/* name of the host we're sending packets to;
									will be set to dest_server or proxy_server */
struct sockaddr_in http_sin;	/* corresponds to http_hostname */
int dest_port = HTTP_PORT, proxy_port = HTTP_PORT, http_port = HTTP_PORT;

char *append_to_file = NULL;  /* IANA character set string to append to file 
								 names, e.g. "?charset=x-mac-japanese" */

int mygetvfsbyname __P((const char *, struct vfsconf *));

mach_port_t diskArbitrationPort;

int webdav_first_read_len;	/* bytes.  Amount to download at open so first read at offset 0 doesn't stall */

char *gUserAgentHeader = NULL;	/* The User-Agent request-header field */

/*****************************************************************************/

static void usage()
{
	(void)fprintf(stderr,
		"usage: mount_webdav [-o options] dav-enabled-uri mount-point\n");
	exit(1);
}

/*****************************************************************************/

static void stop_proxy_update()
{
	/* *** close socket on which webdav received 
		proxy configuration update notifications *** */
}

/*****************************************************************************/

static int webdav_unmount(mntpt)
	char *mntpt;
{
	int pid;
	int result = -1;

	pid = fork();
	if (pid == 0)
	{
		result = execl(PRIVATE_UNMOUNT_COMMAND, PRIVATE_UNMOUNT_COMMAND,
			PRIVATE_UNMOUNT_FLAGS, mntpt, NULL);
		/* We can only get here if the exec failed */
		goto Return;
	}

	if (pid == -1)
	{
		result = errno;
		goto Return;
	}

	/* assume success.  We can't wait for the child process because waiting
	 * will suspend us making us unable to handle the few system calls the
	 * unmount process will do, thus causing a hang.
	 */
	result = 0;

Return:

	stop_proxy_update();

	return result;
}

/*****************************************************************************/

static void sighdlr(sig)
	int sig;
{
	if (sig == SIGHUP)
	{
		if (logfile == 0)
		{
			logfile = fopen("/tmp/webdavlog", "a");
		}
		else
		{
			(void)fclose(logfile);
			(void)fflush(logfile);
			logfile = 0;
		}
	}
	else
	{
		/* disable all other signals so that more signals don't try to unmount */

		signal(SIGUSR1, SIG_IGN);
		signal(SIGUSR2, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGILL, SIG_IGN);
		signal(SIGTRAP, SIG_IGN);
		signal(SIGABRT, SIG_IGN);
		signal(SIGEMT, SIG_IGN);
		signal(SIGFPE, SIG_IGN);
		signal(SIGBUS, SIG_IGN);
		signal(SIGSEGV, SIG_IGN);
		signal(SIGSYS, SIG_IGN);
		signal(SIGALRM, SIG_IGN);
		signal(SIGTERM, SIG_IGN);
		signal(SIGTSTP, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		signal(SIGTTOU, SIG_IGN);
		signal(SIGXCPU, SIG_IGN);
		signal(SIGXFSZ, SIG_IGN);
		signal(SIGVTALRM, SIG_IGN);
		signal(SIGPROF, SIG_IGN);

		syslog(LOG_CRIT, "mount_webdav recieved signal: %d. Unmounting %s", sig, gmountpt);
		(void *)webdav_unmount(gmountpt);
	}

	return;
}

/*****************************************************************************/

static int attempt_webdav_load()
{
	int pid;
	int result = -1;
	union wait status;

	pid = fork();
	if (pid == 0)
	{
		result = execl(PRIVATE_LOAD_COMMAND, PRIVATE_LOAD_COMMAND, NULL);
		/* We can only get here if the exec failed */
		goto Return;
	}

	if (pid == -1)
	{
		result = errno;
		goto Return;
	}

	/* Success! */
	if ((wait4(pid, (int *) & status, 0, NULL) == pid) && (WIFEXITED(status)))
	{
		result = status.w_retcode;
	}
	else
	{
		result = -1;
	}

Return:
	
	return result;
}

/*****************************************************************************/

int resolve_http_hostaddr()
{
	memset(&http_sin, 0, sizeof sin);
	http_sin.sin_family = AF_INET;
	http_sin.sin_len = sizeof sin;
	http_sin.sin_port = htons(http_port);

	if (inet_aton(http_hostname, &http_sin.sin_addr) == 0)
	{
		struct hostent *hp;

		/* XXX - do timeouts for name resolution? */
		hp = gethostbyname(http_hostname);
		if (hp == 0)
		{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			fprintf(stderr, "resolve_http_hostaddr resolving %s\n", http_hostname);
#endif
			warnx("`%s': cannot resolve: %s", http_hostname, hstrerror(h_errno));
			return ENOENT;
		}
		memcpy(&http_sin.sin_addr, hp->h_addr_list[0], sizeof http_sin.sin_addr);
	}
	return (0);
}

/*****************************************************************************/

#define ENCODING_KEY "?charset="

static int update_text_encoding()
{
	CFStringRef cf_encoding = NULL;
	char encoding[100];
	CFStringEncoding str_encoding;

	if (append_to_file)
	{
		free(append_to_file);
		append_to_file = NULL;
	}

	str_encoding = CFStringGetSystemEncoding();
	if (str_encoding != kCFStringEncodingMacRoman)
		/* the default encoding */
	{
		cf_encoding = CFStringConvertEncodingToIANACharSetName(str_encoding);
		if (cf_encoding)
		{
			if (CFStringGetCString(cf_encoding, encoding, sizeof(encoding), kCFStringEncodingMacRoman))
			{
				append_to_file = malloc(strlen(ENCODING_KEY) + strlen(encoding) + 1);
				if ( append_to_file )
				{
					sprintf(append_to_file, "%s%s", ENCODING_KEY, encoding);
				}
			}
			CFRelease(cf_encoding);
		}
	}
	return (0);
}

/*****************************************************************************/

static int proxy_update()
{
	int rv = 0;
	char host[MAXHOSTNAMELEN], ehost[MAXHOSTNAMELEN];
	int port = 0, enabled = 0;
	SCDynamicStoreRef store = NULL;
	CFStringRef cf_host = NULL;

	CFNumberRef cf_port = NULL, cf_enabled = NULL;
	CFArrayRef cf_list = NULL;
	CFDictionaryRef dict = NULL;

	host[0] = '\0';

	/* Get the dictionary for the proxy information */
	store = SCDynamicStoreCreate(NULL, CFSTR("WebDAV"), NULL, NULL);
	if (!store)
	{
#ifdef DEBUG
		syslog(LOG_INFO, "proxy_update SCDynamicStoreCreate failed: %s", SCErrorString(SCError()));
#endif

		return (ENODEV);
	}
	dict = SCDynamicStoreCopyProxies(store);
	if (!dict)
	{
#ifdef DEBUG
		syslog(LOG_INFO, "proxy_update No proxy information");
#endif

		goto free_data;
	}
	cf_enabled = CFDictionaryGetValue(dict, kSCPropNetProxiesHTTPEnable);
	if (cf_enabled == NULL)
	{
#ifdef DEBUG
		syslog(LOG_INFO, "proxy_update CFDictionaryGetValue cf_enabled failed");
#endif

		goto free_data;
	}
	if (!CFNumberGetValue(cf_enabled, kCFNumberIntType, &enabled))
	{
#ifdef DEBUG
		syslog(LOG_INFO, "proxy_update CFNumberGetValue cf_enabled failed");
#endif

		goto free_data;
	}
	if (enabled)
	{
		cf_host = CFDictionaryGetValue(dict, kSCPropNetProxiesHTTPProxy);
		if (cf_host == NULL)
		{
#ifdef DEBUG
			syslog(LOG_INFO, "proxy_update CFDictionaryGetValue cf_host failed");
#endif

			goto free_data;
		}
		if (!CFStringGetCString(cf_host, host, sizeof(host), kCFStringEncodingMacRoman))
		{
#ifdef DEBUG
			syslog(LOG_INFO, "proxy_update CFStringGetCString cf_host failed");
#endif

			goto free_data;
		}
#ifdef DEBUG
		syslog(LOG_INFO, "proxy_update read host %s", host);
#endif

		cf_port = CFDictionaryGetValue(dict, kSCPropNetProxiesHTTPPort);
		if (cf_port == NULL)
		{
#ifdef DEBUG
			syslog(LOG_INFO, "proxy_update CFDictionaryGetValue cf_port failed");
#endif

			goto free_data;
		}
		if (!CFNumberGetValue(cf_port, kCFNumberIntType, &port))
		{
#ifdef DEBUG
			syslog(LOG_INFO, "proxy_update CFNumberGetValue cf_port failed");
#endif

			goto free_data;
		}
#ifdef DEBUG
		syslog(LOG_INFO, "proxy_update read port %d", port);
#endif

		/* Read the proxy exceptions list */
		proxy_exception = 0;
		cf_list = CFDictionaryGetValue(dict, kSCPropNetProxiesExceptionsList);
		if (cf_list)
		{
			CFIndex len = CFArrayGetCount(cf_list), idx;
			CFStringRef cf_ehost;
			int start;

			for (idx = (CFIndex)0; idx < len; idx++)
			{
				/* Find out whether dest_server is on it */
				cf_ehost = CFArrayGetValueAtIndex(cf_list, idx);
				if (cf_ehost)
				{
					if (!CFStringGetCString(cf_ehost, ehost, sizeof(ehost), kCFStringEncodingMacRoman))
					{
#ifdef DEBUG
						syslog(LOG_INFO, "proxy_update CFStringGetCString cf_ehost failed");
#endif

						goto free_data;
					}
#ifdef DEBUG
					syslog(LOG_INFO, "proxy_update read ehost %s", ehost);
#endif

					start = strlen(dest_server) - strlen(ehost);
					if (start > 0)
					{
						if ((strcmp(&dest_server[start], ehost)) == 0)
						{
							/* last part of dest_server matches ehost */
							proxy_exception = 1;
							break;
						}
					}
				}
			}
		}
	}

free_data:

	if (store)
	{
		CFRelease(store);
	}

	if (dict)
	{
		CFRelease(dict);
	}

	if (!strlen(host))
	{
		proxy_ok = 0;
	}
	else
	{
		char *old_server = proxy_server;

		proxy_server = malloc(strlen(host) + 1);
		if (proxy_server)
		{
			(void)strcpy(proxy_server, host);
			proxy_port = (port) ? port : HTTP_PORT;
			if (old_server)
				free(old_server);
			proxy_ok = 1;
		}
		else
		{
			return (ENOMEM);
		}
	}

	if ((!proxy_ok) || proxy_exception)
	{
		http_hostname = dest_server;
		http_port = dest_port;
	}
	else
	{
		/* for proxy, put the proxy server name in http_hostname */
		http_hostname = proxy_server;
		http_port = proxy_port;
#ifdef DEBUG
		syslog(LOG_INFO, "proxy_update set http_hostname %s http_port %d", http_hostname, http_port);
#endif

	}
	rv = resolve_http_hostaddr();
	
	return (rv);
}												/* proxy_update */

/*****************************************************************************/

static int reg_proxy_update()
{

	/* *** register for proxy configuration update notifications *** */

	return (proxy_update());
}

/*****************************************************************************/

/*
	The InitUserAgentHeader initializes the string gUserAgentHeader which is
	sent with every request to the server. The User-Agent request-header field
	is defined in RFC 2616, section 14.43 as:
		User-Agent		= "User-Agent" ":" 1*( product | comment )
	section 3.8 defines product as:
		product			= token ["/" product-version]
		product-version	= token
	section 2.2 defines comment as:
		comment			= "(" *( ctext | quoted-pair | comment ) ")"
		ctext			= <any TEXT excluding "(" and ")">
		quoted-pair		= "\" CHAR

	We want our User-Agent request-header field to look something like:
		"User-Agent: WebDAVFS/1.1 (0110800000) Darwin/5.3 (Power Macintosh)"
	where:
		1.1	= the CFBundleShortVersionString from webdavfs.bundle
		0110800000 = webdavfs.bundle's numeric version
		Darwin = CTL_KERN/KERN_OSTYPE
		5.3 = CTL_KERN/KERN_OSRELEASE
		Power Macintosh = CTL_HW/HW_MACHINE
	
	webdavfs.bundle is located at:
		/System/Library/CoreServices/webdavfs.bundle
	
	If the data from webdavfs.bundle could not be obtained, then we'll
	fall back to the generic User-Agent request-header string WebDAV FS
	used to use.
	
	Added with PR-2797472.
*/

static int InitUserAgentHeader(void)
{
	char				buf[128];
	int					mib[2];
	char				ostype[128];
	char				osrelease[128];
	char				machine[128];
	size_t				len;
	CFURLRef			url;
	CFBundleRef			bundle;
	CFDictionaryRef		dict;
	CFStringRef			shortVersion;
	CFIndex				shortVersionLen;
	char				*webdavfsVersionStr;
	UInt32				webdavfsVersion;
	int					result;
	
	result = 0;	/* assume things will work til they don't */
	
	/* Have we built the string yet? */
	if ( gUserAgentHeader == NULL )
	{
		/* Get the ostype, osrelease, and machine strings using sysctl*/
		mib[0] = CTL_KERN;
		mib[1] = KERN_OSTYPE;
		len = sizeof ostype;
		if (sysctl(mib, 2, ostype, &len, 0, 0) < 0)
		{
			warn("sysctl");
			ostype[0] = '\0';
		}
		mib[1] = KERN_OSRELEASE;
		len = sizeof osrelease;
		if (sysctl(mib, 2, osrelease, &len, 0, 0) < 0)
		{
			warn("sysctl");
			osrelease[0] = '\0';
		}
		mib[0] = CTL_HW;
		mib[1] = HW_MACHINE;
		len = sizeof machine;
		if (sysctl(mib, 2, machine, &len, 0, 0) < 0)
		{
			warn("sysctl");
			machine[0] = '\0';
		}
		
		/* We don't have it yet */
		webdavfsVersionStr = NULL;
		webdavfsVersion = 0x010080000; /* 1.0 final */
		
		/* Create the CFURLRef to the webdavfs.bundle's version.plist */
		url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
			CFSTR("/System/Library/CoreServices/webdavfs.bundle"),
			kCFURLPOSIXPathStyle, true );
		if ( url != NULL )
		{
			/* Create the bundle */
			bundle = CFBundleCreate(kCFAllocatorDefault, url);
			if ( bundle != NULL )
			{
				/* Get the bundle's numeric version */
				webdavfsVersion = CFBundleGetVersionNumber(bundle);
				
				/* Get the Info dictionary */ 
				dict = CFBundleGetInfoDictionary(bundle);
				if ( dict != NULL )
				{
					/* Get the CFBundleShortVersionString (display string) */
					shortVersion = CFDictionaryGetValue(dict, CFSTR("CFBundleShortVersionString"));
					if ( shortVersion != NULL )
					{
						/* Get the bundleVersionStr */
						shortVersionLen = CFStringGetLength(shortVersion) + 1;
						webdavfsVersionStr = malloc(shortVersionLen);
						if ( webdavfsVersionStr != NULL )
						{
							/* Convert it to a C string */
							if ( !CFStringGetCString(shortVersion, webdavfsVersionStr, shortVersionLen, kCFStringEncodingMacRoman) )
							{
								/* If we can't get it, free the memory */
								free(webdavfsVersionStr);
								webdavfsVersionStr = NULL;
							}
						}
					}
				}
				/* release created bundle */
				CFRelease(bundle);
			}
			/* release created url */
			CFRelease(url);
		}
		
		if ( webdavfsVersionStr != NULL )
		{
			/* if everything worked, use the new format User-Agent request-header string */
			snprintf(buf, sizeof(buf), USER_AGENT_HEADER_PREFIX "%s (%.8lx) %s/%s (%s)\r\n",
				webdavfsVersionStr, webdavfsVersion, ostype, osrelease, machine);
			free(webdavfsVersionStr);
		}
		else
		{
			/* create the generic User-Agent request-header string WebDAV FS used to use */
			snprintf(buf, sizeof(buf), USER_AGENT_HEADER_PREFIX "1.0 %s/%s (%s)\r\n",
				ostype, osrelease, machine);
		}
				
		/* save it in a global */
		gUserAgentHeader = malloc(strlen(buf) + 1);
		if ( gUserAgentHeader != NULL )
		{
			strcpy(gUserAgentHeader, buf);
		}
		else
		{
			result = ENOMEM;
		}
	}
	
	return ( result );
}

/*****************************************************************************/

/*
 * The get_webdav_first_read_len function sets the global
 * webdav_first_read_len. It is set to the system's page size so that if the
 * first read after an open starts at offset 0, that page will already be
 * downloaded into the cache file.
 */
static void get_webdav_first_read_len(void)
{
	int		mib[2];
	size_t	len;
	int		result;
	
	/* get the hardware page size */
	mib[0] = CTL_HW;
	mib[1] = HW_PAGESIZE;
	len = sizeof(int);
	result = sysctl(mib, 2, &webdav_first_read_len, &len, 0, 0);
	if ( 0 > result )
	{
		warn("sysctl");
		/* set webdav_first_read_len to PowerPC page size */
		webdav_first_read_len = 4096;
	}
}

/*****************************************************************************/

int main(argc, argv)
	int argc;
	char *argv[];
{
	struct webdav_args args;
	struct sockaddr_un un;
	int mntflags = 0;
	int servermntflags = 0;
	char arguri[MAXPATHLEN + 1];
	char *uri;
	int urilen;
	struct vfsconf vfc;
	mode_t um;
	char *colon,  *slash,  *ep;					/* used to parse uri */
	int len;
	unsigned long ul;
	DiskArbDiskIdentifier diskIdentifier;

	int rc;
	int so;
	int error = 0;
	kern_return_t daconnect_status;

	int ch;
	int i;
	pthread_mutexattr_t mutexattr;
	pthread_t pulse_thread;
	pthread_attr_t pulse_thread_attr;
	pthread_t request_thread;
	pthread_attr_t request_thread_attr;
	int mysocket;

	char user[WEBDAV_MAX_USERNAME_LEN];
	char pass[WEBDAV_MAX_PASSWORD_LEN];

	struct rlimit rlp;
	
	/* initialize webdav_first_read_len variable */
	get_webdav_first_read_len();
	
	/* initialize gUserAgentHeader */
	error = InitUserAgentHeader();
	if ( error )
	{
		/* not likely to fail, but just in case */
		errx(error, "InitUserAgentHeader: %s", strerror(error));
	}
	
	user[0] = '\0';
	pass[0] = '\0';

	/* used to format time */
	setenv("TZ", "UTC0", 1);
	tzset();

	/* *** The Finder understands the exit code ENODEV. "item could not be found",
	  but not much else. *** */

	/* zero out the global stuff */

	bzero(&gfile_array, sizeof(gfile_array));

	/* initialize the memory cache, authcache & thread queues
	  XXX initialization has been delayed until after
	  daemonization.  The authcache could not be moved without
	  changing webdav mount. Chances of contention are low so
	  we'll hopefully get a fix for the problem of pthread mutex's
	  not working accross daemon before we need to address that*/

	error = webdav_authcache_init();
	if (error)
	{
		errx(error, "webdav_authcache_init: %s", strerror(error));
	}

	/*
	 * Set the default timeout value
	 */
	gtimeout_string = WEBDAV_PULSE_TIMEOUT;
	gtimeout_val = atoi(gtimeout_string);

	/* Set up the stafs timeout & buffer */

	bzero(&gstatfsbuf, sizeof(gstatfsbuf));
	gstatfstime = 0;

	/*
	 * Crack command line args
	 */

	while ((ch = getopt(argc, argv, "a:o:")) != -1)
	{
		switch (ch)
		{
			case 'a':							/* get the username and password from URLMount */
				{
					int fd = atoi(optarg), 		/* fd from URLMount */
					zero = 0, i = 0, len1 = 0, len2 = 0;

					/* read the username length, the username, the password
					  length, and the password */
					if (fd >= 0)
					{
						(void)lseek(fd, 0, SEEK_SET);
						if (read(fd, &len1,
							sizeof(int)) > 0 && len1 > 0 && len1 < WEBDAV_MAX_USERNAME_LEN)
						{
							if (read(fd, user, len1) > 0)
							{
								user[len1] = '\0';
								if (read(fd, &len2,
									sizeof(int)) > 0 && len2 > 0 && len2 < WEBDAV_MAX_PASSWORD_LEN)
								{
									if (read(fd, pass, len2) > 0)
										pass[len2] = '\0';
								}
							}
						}

						/* zero the contents of the file */
						(void)lseek(fd, 0, SEEK_SET);
						for (i = 0; i < (((len1 + len2) / sizeof(int)) + 3); i++)
						{
							if (write(fd, (char *) & zero, sizeof(int)) < 0)
							{
								break;
							}
						}
						(void)fsync(fd);
						(void)close(fd);
					}
					break;
				}

			case 'o':
				error = getmntopts(optarg, mopts, &mntflags, 0);
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

	if (error)
	{
		usage();
	}

	/* cache the username and password from the tmp-file who's fd was
	  passed from URLMount on the command line */
	if (strlen(user))
	{
		int uid = getuid();
		WebdavAuthcacheInsertRec auth_insert =
		{
			uid, NULL, 0, FALSE, user, pass
		};
		/* Challenge string will be filled in when OPTIONS
		  request is challenged.  */

		(void)webdav_authcache_insert(&auth_insert);

		/* if not "root", make an entry for root (uid 0) as well */
		if (uid != 0)
		{
			auth_insert.uid = 0;
			(void)webdav_authcache_insert(&auth_insert);
		}

		/* if not "daemon", make an entry for daemon (uid 1) as well */
		if (uid != 1)
		{
			auth_insert.uid = 1;
			(void)webdav_authcache_insert(&auth_insert);
		}
	}
	bzero(user, sizeof(user));
	bzero(pass, sizeof(pass));

	/* get the current maximum number of open files for this process */
	error = getrlimit(RLIMIT_NOFILE, &rlp);
	if (error)
	{
		err(ENODEV, "getrlimit");
	}

	/* Close any open file descriptors we may have inherited from our
	 * parent caller.  This excludes the first three.  We don't close
	 * stdin, stdout or stdio. Note, this has to be done before we
	 * open any other file descriptors, but after we check for a file
	 * containing authentication in /tmp.
	 */
	for (i = 3; i < rlp.rlim_cur; ++i)
	{
		(void)close(i);
	}

	/* raise the maximum number of open files for this process if needed */
	if ( rlp.rlim_cur < WEBDAV_RLIMIT_NOFILE )
	{
		rlp.rlim_cur = WEBDAV_RLIMIT_NOFILE;
		error = setrlimit(RLIMIT_NOFILE, &rlp);
		if (error)
		{
			err(ENODEV, "setrlimit");
		}
	}

	/* the socket is initially closed */
	mysocket = -1;

	/*
	 * Get uri and mount point
	 */

	(void)strncpy(arguri, argv[optind], sizeof(arguri) - 1);
	if ( realpath(argv[optind + 1], gmountpt) == NULL )
	{
		err(ENOENT, "realpath");
	}

	/* If they gave us a full uri, blow off the scheme */

	if (strncmp(arguri, _WEBDAVPREFIX, strlen(_WEBDAVPREFIX)) == 0)
	{
		uri = &arguri[strlen(_WEBDAVPREFIX)];
	}
	else
	{
		uri = arguri;
	}

	/*
	 * If there is no trailing '/' in the uri, add a trailing one to
	 * keep the finicky uri parsing stuff from blowing up.
	 */
	urilen = strlen(uri);
	if (uri[urilen-1] != '/')
	{
		uri[urilen] = '/';
		++urilen;
		/*
		 * Note: it is safe to slam in the null because we refused to
		 * Copy more than 1 fewer bytes than the size of the buffer.
		 */

		uri[urilen] = '\0';
	}
	
	/* cache the destination host name and port */
	colon = strchr(uri, ':');
	slash = strchr(uri, '/');
	if (colon != 0)
	{
		errno = 0;
		ul = strtoul(colon + 1, &ep, 10);
		if (errno != 0 || ep != slash || colon[1] == '\0' || ul < 1 || ul > 65534)
		{
			err(ENODEV, "`%s': invalid port number", colon + 1);
		}
		len = colon - uri;
		dest_port = (int)ul;
	}
	else
	{
		len = slash - uri;
		dest_port = HTTP_PORT;
	}
	dest_server = malloc(len + 1);
	if (dest_server)
	{
		(void)strncpy(dest_server, uri, len);
		dest_server[len] = '\0';
	}
	else
	{
		err(ENOMEM, "error allocating dest_server");
	}

	/* Set global signal handling to protect us from SIGPIPE */

	signal(SIGPIPE, SIG_IGN);

	/* get the default text encoding */
	update_text_encoding();

	/* Determine if we can proxy */
	rc = reg_proxy_update();
	if (rc)
	{
		if (rc == ENOMEM)
		{
			err(ENOMEM, "reading proxy from configuration database");
		}
		else
		{
			warn("error reading proxy from configuration database");
			/* *** Would it be better to exit at this point? *** */
		}
	}
	
	/* Create a DiskArbDiskIdentifier from the uri. Because the
	 * DiskArbDiskIdentifier is also returned as f_mntfromname by
	 * statfs(), because f_mntfromname must match the DiskArbDiskIdentifier,
	 * and because f_mntfromname and DiskArbDiskIdentifier are currently
	 * different lengths, make sure the string is no longer than the
	 * smaller of the two.
	 */
	strncpy(diskIdentifier, uri, MIN(MNAMELEN, sizeof(DiskArbDiskIdentifier)));
	diskIdentifier[MIN(MNAMELEN, sizeof(DiskArbDiskIdentifier))] = '\0';
	
	/* Check to see if this DiskArbDiskIdentifier is already used by a mount point.
	 * The DiskArbDiskIdentifier must be unique. Sure, someone could mount using
	 * the DNS name one time and the IP address the next, or they could
	 * munge the path with escaped characters, but at least the DiskArbDiskIdentifier
	 * will still be unique.
	 */
	{
		struct statfs *	buffer;
		SInt32			count = getmntinfo(&buffer, MNT_NOWAIT);
		SInt32          i;
		int				identifierLength;
		
		identifierLength = strlen(diskIdentifier);
		for (i = 0; i < count; i++)
		{
			/* Is diskIdentifier already being used as a DiskArbDiskIdentifier?
			 * Note: DiskArbDiskIdentifiers are case-insensitive.
			 */
			if ( (strcmp("webdav", buffer[i].f_fstypename) == 0) &&
				 (strlen(buffer[i].f_mntfromname) == identifierLength) &&
				 (strncasecmp(buffer[i].f_mntfromname, diskIdentifier, identifierLength) == 0) )
			{
				/* Yes, this DiskArbDiskIdentifier is in use - return EBUSY
				 * (the same error that you'd get if you tried mount a disk device twice).
				 */
				errx(EBUSY, "%s is already mounted: %s", diskIdentifier, strerror(EBUSY));
			}
		}
	}
	
	/* Create the temporary directory to hold cache files
	  We need to do this now so that the webdav_mount call
	  (which will do a lookup) will suceed.  It may need
	  to cache some data in a file */

	um = umask(0);
	error = mkdir(_PATH_TMPWEBDAVDIR, 0777);
	if (error)
	{
		if (errno != EEXIST)
		{

			/* we got an error and it wasn't the one we 	 */
			/* could take so barf  				 */

			err(errno, "making cache directory in tmp");
		}
	}
	(void)umask(um);

	/*
	 * Check out the server and get the mount flags
	 */
	error = webdav_mount(proxy_ok, uri, &mysocket, &servermntflags);
	/* if a socket was opened, close it */
	if (mysocket >= 0)
	{
		(void)close(mysocket);
	}
	if (error)
	{
		/* If EACCES, then the user canceled when asked to authenticate.
		 * In this case, we want to return ECANCELED so that Carbon will
		 * translate our error result to userCanceledErr.
		 */
		if ( EACCES == error )
		{
			error = ECANCELED;
		}
		errx(error, "checking server URL: %s", strerror(error));
	}

	/*
	 * Or in the mnt flags forced on us by the server
	 */

	mntflags |= servermntflags;

	/*
	 * Construct the listening socket
	 */
	un.sun_family = AF_UNIX;
	if (sizeof(_PATH_TMPWEBDAV) >= sizeof(un.sun_path))
	{
		errx(EINVAL, "webdav socket name too long");
	}
	strcpy(un.sun_path, _PATH_TMPWEBDAV);
	mktemp(un.sun_path);
	un.sun_len = strlen(un.sun_path);

	so = socket(AF_UNIX, SOCK_STREAM, 0);
	if (so < 0)
	{
		err(errno, "socket");
	}

	um = umask(077);
	(void)unlink(un.sun_path);
	if (bind(so, (struct sockaddr *) & un, sizeof(un)) < 0)
	{
		err(errno, "bind");
	}

	(void)unlink(un.sun_path);
	(void)umask(um);

	(void)listen(so, 5);

	args.pa_socket = so;
	args.pa_config = diskIdentifier;
	args.pa_uri = uri;

	error = mygetvfsbyname("webdav", &vfc);
	if (error)
	{
		error = attempt_webdav_load();
		if (!error)
		{
			error = mygetvfsbyname("webdav", &vfc);
		}
	}

	if (error)
	{
		errx(errno, "webdav filesystem is not available");
	}

#if 0
	/* The old way of loading, now superseeded */

	if (error && vfsisloadable("webdav"))
	{
		if (vfsload("webdav"))
		{
			err(errno, "vfsload(webdav)");
		}
		endvfsent();
		error = getvfsbyname("webdav", &vfc);
	}

#endif

	/*
	 * Ok, we are about to set up the mount point so stop
	 * the signals and set up our mach exception task
	 * so that we know if someone is trying to kill us.
	 */

	signal(SIGUSR1, sighdlr);
	signal(SIGUSR2, sighdlr);
	signal(SIGHUP, sighdlr);
	signal(SIGINT, sighdlr);
	signal(SIGQUIT, sighdlr);
	signal(SIGILL, sighdlr);
	signal(SIGTRAP, sighdlr);
	signal(SIGABRT, sighdlr);
	signal(SIGEMT, sighdlr);
	signal(SIGFPE, sighdlr);
	signal(SIGBUS, sighdlr);
	signal(SIGSEGV, sighdlr);
	signal(SIGSYS, sighdlr);
	signal(SIGALRM, sighdlr);
	signal(SIGTERM, sighdlr);
	signal(SIGTSTP, sighdlr);
	signal(SIGTTIN, sighdlr);
	signal(SIGTTOU, sighdlr);
	signal(SIGXCPU, sighdlr);
	signal(SIGXFSZ, sighdlr);
	signal(SIGVTALRM, sighdlr);
	signal(SIGPROF, sighdlr);


	rc = mount(vfc.vfc_name, gmountpt, mntflags, &args);
	if (rc < 0)
	{
		err(errno, "mount");
	}

#ifndef DEBUG       
	/* Connect to the AutoDiskMount server */
	daconnect_status = DiskArbStart(&diskArbitrationPort);
	if (daconnect_status != KERN_SUCCESS)
	{
#if 0
		err(errno, "Couldn't connect to DiskArbitration server\n");
#endif

	}
	else
	{
		daconnect_status = DiskArbDiskAppearedWithMountpointPing_auto(diskIdentifier,
			kDiskArbDiskAppearedNetworkDiskMask | kDiskArbDiskAppearedEjectableMask, gmountpt);
	}

	/*
	 * Everything is ready to go - now is a good time to fork
	 * Note, forking seems to kill all the threads so make sure we
	 * daemonize before creating our threads.
	 */
	daemon(0, 0);
#endif

	/* Until pthread can handle locks accrss deamonization
	 * we need to delay mutex initialization to here.
	 */


	/* set up the lock on the file arrary and socket */
	error = pthread_mutexattr_init(&mutexattr);
	if (error)
	{
		errx(error, "mutex atrribute init: %s", strerror(error));
	}

	error = pthread_mutex_init(&garray_lock, &mutexattr);
	if (error)
	{
		errx(error, "garray mutex lock: %s", strerror(error));
	}

	/* Init the stat cache */
	error = webdav_memcache_init(&gmemcache_header);
	if (error)
	{
		errx(error, "webdav_memcache_init: %s", strerror(error));
	}

	/* Init the inode hash table */
	error = webdav_inode_init(uri, urilen);
	if (error)
	{
		errx(error, "webdav_inode_init: %s", strerror(error));
	}

	/* Start up the request threads */

	webdav_requestqueue_init();

	for (i = 0; i < WEBDAV_REQUEST_THREADS; ++i)
	{
		error = pthread_attr_init(&request_thread_attr);
		if (error)
		{
			errx(error, "pthread_attr_init: %s", strerror(error));
		}

		error = pthread_attr_setdetachstate(&request_thread_attr, PTHREAD_CREATE_DETACHED);
		if (error)
		{
			errx(error, "pthread_attr_setdetachstate: %s", strerror(error));
		}

		error = pthread_create(&request_thread, &request_thread_attr,
			(void *)webdav_request_thread, (void *)NULL);
		if (error)
		{
			errx(error, "pthread_create request thread: %s", strerror(error));
		}

	}


	/*
	 * Start logging (and change name)
	 */
	openlog("webdavd", LOG_CONS | LOG_PID, LOG_DAEMON);

	/*
	 * Start the pulse thread
	 */
	error = pthread_attr_init(&pulse_thread_attr);
	if (error)
	{
		errx(error, "pthread_attr_init: %s", strerror(error));
	}

	error = pthread_attr_setdetachstate(&pulse_thread_attr, PTHREAD_CREATE_DETACHED);
	if (error)
	{
		errx(error, "pthread_attr_setdetachstate: %s", strerror(error));
	}

	error = pthread_create(&pulse_thread, &pulse_thread_attr, (void *)webdav_pulse_thread,
		(void *) & proxy_ok);
	if (error)
	{
		errx(error, "pthread_create: %s", strerror(error));
	}


	/*
	 * Just loop waiting for new connections and activating them
	 */
	for (;;)
	{
		struct sockaddr_un un2;
		int len2 = sizeof(un2);
		int so2;
		fd_set fdset;
		int rc;
		/*
		 * Accept a new connection
		 * Will get EINTR if a signal has arrived, so just
		 * ignore that error code
		 */
		FD_ZERO(&fdset);
		FD_SET(so, &fdset);
		rc = select(so + 1, &fdset, (fd_set *)0, (fd_set *)0, (struct timeval *)0);
		if (rc < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			syslog(LOG_ERR, "select: %s", strerror(errno));
			err(errno, "select");
		}
		if (rc == 0)
		{
			break;
		}
		so2 = accept(so, (struct sockaddr *) & un2, &len2);
		if (so2 < 0)
		{
			/*
			 * The unmount function does a shutdown on the socket
			 * which will generated ECONNABORTED on the accept.
			 */
#ifdef DEBUG
			printf(" Got error from accept %d/n", errno);
#endif

			if (errno == ECONNABORTED)
			{
				break;
			}
			if (errno != EINTR)
			{
				syslog(LOG_ERR, "accept: %s", strerror(errno));
				err(errno, "accept");
			}
			continue;
		}

		/*
		 * Now put a new element on the thread queue so that a thread
		 *  will handle this.
		 */
		error = webdav_requestqueue_enqueue_request(proxy_ok, so2);
		if (error)
		{
			errx(error, "webdav_requestqueue_enqueue: %s", strerror(error));
		}
	}

	syslog(LOG_INFO, "%s unmounted", gmountpt);
#ifndef DEBUG
	/* Notify AutoDiskMount of the disappearance of this volume: */
	DiskArbDiskDisappearedPing_auto(diskIdentifier, 0);
#endif

	exit(0);
}

/*****************************************************************************/

/*
 * Given a filesystem name, determine if it is resident in the kernel,
 * and if it is resident, return its vfsconf structure.
 *
 * Returns 0 on success; returns -1 on error and sets errno.
 */
int mygetvfsbyname(fsname, vfcp) const
	char *fsname;
	struct vfsconf *vfcp;
{
	int name[4], maxtypenum, cnt;
	size_t buflen;

	name[0] = CTL_VFS;
	name[1] = VFS_GENERIC;
	name[2] = VFS_MAXTYPENUM;
	buflen = 4;
	if (sysctl(name, 3, &maxtypenum, &buflen, (void *)0, (size_t)0) < 0)
	{
		return (-1);
	}
	name[2] = VFS_CONF;
	buflen = sizeof * vfcp;
	for (cnt = 0; cnt < maxtypenum; cnt++)
	{
		name[3] = cnt;
		if (sysctl(name, 4, vfcp, &buflen, (void *)0, (size_t)0) < 0)
		{
			if (errno != EOPNOTSUPP && errno != ENOENT)
			{
				return (-1);
			}
			continue;
		}
		if (!strcmp(fsname, vfcp->vfc_name))
		{
			return (0);
		}
	}
	errno = ENOENT;
	return (-1);
}

/*****************************************************************************/
