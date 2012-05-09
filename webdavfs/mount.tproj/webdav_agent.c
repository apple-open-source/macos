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

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <sys/types.h>
#include <sys/sysctl.h>
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
#include <readpassphrase.h>
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
#include "load_webdavfs.h"
#include "webdavfs_load_kext.h"
#include "webdav_cookie.h"
#include "webdav_utils.h"

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
CFStringRef gBasePath = NULL;	/* the base path (from gBaseURL) for this mount */
char gBasePathStr[MAXPATHLEN];	/* gBasePath as a c-string */
uint32_t gServerIdent = 0;		/* identifies some (not all) types of servers we are connected to (i.e. WEBDAV_IDISK_SERVER) */
fsid_t	g_fsid;					/* file system id */
boolean_t gUrlIsIdisk = FALSE;	/* True if URL host domain is of form idisk.mac.com or idisk.me.com */
char g_mountPoint[MAXPATHLEN];	/* path to our mount point */

/*
 * mount_webdav.c file globals
 */
static int wakeupFDs[2] = { -1, -1 };	/* used by webdav_kill() to communicate with main select loop */
static char mntfromname[MNAMELEN];		/* the mntfromname */

/* host names of .Mac iDisk servers */
char *idisk_server_names[] = {"idisk.mac.com", "idisk.me.com", NULL};

/*
 * We want getmntopts() to simply ignore
 * unrecognized mount options.
 */
int getmnt_silent = 1;

// The maximum size of an upload or download to allow the
// system to cache.
uint64_t webdavCacheMaximumSize = WEBDAV_DEFAULT_CACHE_MAX_SIZE;

// Sets the maximum size of an upload or download to allow the
// system to cache, based on the amount of physical memory in
// the system.
static void setCacheMaximumSize(void);

#define CFENVFORMATSTRING "__CF_USER_TEXT_ENCODING=0x%X:0:0"

/*****************************************************************************/

void webdav_debug_assert(const char *componentNameString, const char *assertionString, 
	const char *exceptionLabelString, const char *errorString, 
	const char *fileName, long lineNumber, uint64_t errorCode)
{
	#pragma unused(componentNameString)

	if ( (assertionString != NULL) && (*assertionString != '\0') )
	{
		if ( errorCode != 0 )
		{
			syslog(WEBDAV_LOG_LEVEL, "(%s) failed with %d%s%s%s%s; file: %s; line: %ld", 
			assertionString,
			(int)errorCode,
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
		"usage: mount_webdav [-i] [-s] [-S] [-o options] [-v <volume name>]\n");
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

static boolean_t urlIsIdisk(const char *url) {
	boolean_t found_idisk;
	size_t len, shortest_len, url_len;
	char*  colon;
	char** idisk_server;
	
	found_idisk = FALSE;
	
	if (url == NULL)
		return (found_idisk);
	
	colon = strchr(url, ':');
	if (colon == NULL)
		return (found_idisk);
	
	// First, find the length of the shortest idisk server name
	idisk_server = idisk_server_names;
	shortest_len = strlen(*idisk_server);
	while (*idisk_server != NULL) {
		len = strlen(*idisk_server);
		if (len < shortest_len)
			shortest_len = len;
		idisk_server++;
	}
	
	if (strlen(colon) < shortest_len)
		return (found_idisk);	// too short, not an idisk server name
	
    /*
     * Move colon past "://".  We've already verified that
     * colon is at least as long as the shortest iDisk server name
     * so not worried about buffer overflows
     */	
	colon += 3;
	url_len = strlen(colon);
	idisk_server = idisk_server_names;
	while (*idisk_server) {
		len = strlen(*idisk_server);
		if (url_len >= len) {
			// check for match
			if ( strncasecmp(colon, *idisk_server, len) == 0 ) {
				found_idisk = TRUE;
				break;
			}
		}
		idisk_server++;
	}
	
	return (found_idisk);
}

/*****************************************************************************/

static OSStatus KeychainItemCopyAccountPassword(SecKeychainItemRef itemRef,
                                                char *username,
                                                size_t user_size,
                                                char *password,
                                                size_t pass_size)
{
    OSStatus                            result;
    SecKeychainAttribute                attr;
    SecKeychainAttributeList            attrList;
    UInt32                              length;
    void                                *outData;
	
    /* the attribute we want is the account name */
    attr.tag = kSecAccountItemAttr;
    attr.length = 0;
    attr.data = NULL;
	
    attrList.count = 1;
    attrList.attr = &attr;
	
    result = SecKeychainItemCopyContent(itemRef, NULL, &attrList, &length, &outData);
    if ( result == noErr )
	{
		/* attr.data is the account (username) and outdata is the password */
		if ( attr.length >= user_size || length >= pass_size ) {
			syslog(LOG_ERR, "%s: keychain username or password is too long!", __FUNCTION__);
			result = ENAMETOOLONG;
		} else {
			(void)strlcpy(username, attr.data, user_size);
			(void)strlcpy(password, outData, pass_size);
		}
		
		(void) SecKeychainItemFreeContent(&attrList, outData);
	}
    return ( result );
}

/*****************************************************************************/

static SecProtocolType getSecurityProtocol(CFStringRef str)
{
	if ( CFStringCompare(str, CFSTR("http"), kCFCompareCaseInsensitive) == 0 )
		return kSecProtocolTypeHTTP;
	else if ( CFStringCompare(str, CFSTR("https"), kCFCompareCaseInsensitive) == 0 )
		return kSecProtocolTypeHTTPS;
	else 
		return kSecProtocolTypeAny;
}

/*****************************************************************************/

static int get_keychain_credentials(char* in_url,
									char* out_user,
									size_t out_user_size,
									char* out_pass,
									size_t out_pass_size)

{
	CFStringRef        cfhostName = NULL;
	CFStringRef        cfpath = NULL;
	CFStringRef        cfscheme = NULL;
	CFURLRef           cf_url = NULL;
	SecKeychainItemRef itemRef = NULL;
	SecProtocolType    protocol;
	char*              path = NULL;
	char*              hostName = NULL;
	int	               result;
	
	/* Validate input */
	if ( in_url == NULL|| out_user == NULL || out_pass == NULL)
		return EINVAL;
	
	cf_url = CFURLCreateWithBytes(NULL, (const UInt8 *)in_url, strlen(in_url), kCFStringEncodingUTF8, NULL);
	if ( cf_url == NULL )
		return ENOMEM;
	
	cfscheme = CFURLCopyScheme(cf_url);
	if ( cfscheme == NULL ) {
		result = ENOMEM;
		goto cleanup_exit;
	}
	protocol = getSecurityProtocol(cfscheme);
	
	cfhostName = CFURLCopyHostName(cf_url);
	if ( cfhostName == NULL || (hostName = createUTF8CStringFromCFString(cfhostName)) == NULL) {
		result = ENOMEM;
		goto cleanup_exit;
	}
	
	cfpath = CFURLCopyPath(cf_url);
	if ( cfpath == NULL || (path = createUTF8CStringFromCFString(cfpath)) == NULL) {
		result = ENOMEM;
		goto cleanup_exit;
	}
	
	result = SecKeychainFindInternetPassword(NULL,                                                      /* default keychain */
                                             (UInt32)strlen(hostName), hostName,                        /* serverName */
                                             0, NULL,                                                   /* no securityDomain */
                                             0, NULL,													/* no accountName */
                                             (UInt32)strlen(path), path,                                /* path */
                                             0,                                                         /* port */
                                             protocol,                                                  /* protocol */
                                             kSecAuthenticationTypeAny,                                 /* authenticationType */
                                             0, NULL,                                                   /* no password */
                                             &itemRef);
	
	/* if it doesn't work the first time, try with a NULL path. This is what NetAuth does. */
	if ( result != noErr ) {
		result = SecKeychainFindInternetPassword(NULL,                                                      /* default keychain */
												 (UInt32)strlen(hostName), hostName,                        /* serverName */
												 0, NULL,                                                   /* no securityDomain */
												 0, NULL,													/* no accountName */
												 0, NULL,													/* no path */
												 0,                                                         /* port */
												 protocol,                                                  /* protocol */
												 kSecAuthenticationTypeAny,                                 /* authenticationType */
												 0, NULL,                                                   /* no password */
												 &itemRef);
		
	}
	
	if ( result == noErr ) {
		/* Success, copy the user & pass out */
		result = KeychainItemCopyAccountPassword(itemRef, out_user, out_user_size, out_pass, out_pass_size);
	}
	
cleanup_exit:
	
	CFReleaseNull(cf_url);
	CFReleaseNull(cfscheme);
	CFReleaseNull(cfhostName);
	CFReleaseNull(cfpath);
	CFReleaseNull(itemRef);
	if (path)     free(path);
	if (hostName) free(hostName);

	return result;
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
	int error = 0;
	mach_port_t mp = MACH_PORT_NULL;
	
	// Find the mach port
	error = bootstrap_look_up(bootstrap_port, (char *)WEBDAVFS_LOAD_KEXT_BOOTSTRAP_NAME, &mp);

	if (error != KERN_SUCCESS) {
		syslog(LOG_ERR, "%s: bootstrap_look_up: %s", __FUNCTION__, bootstrap_strerror(error));
		return error;
	}

	error = load_kext(mp, (string_t)WEBDAVFS_VFSNAME);
	if (error != KERN_SUCCESS)
			syslog(LOG_ERR, "%s: load_kext: %s", __FUNCTION__, bootstrap_strerror(error));

	return error;	
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

// determine the cacheMaximumSize based on the physical memory size of the system
static void setCacheMaximumSize(void)
{
	int		mib[2];
	size_t	len;
	int		result;
	uint64_t	memsize;
    
	/* get the physical ram size */
	mib[0] = CTL_HW;
	mib[1] = HW_MEMSIZE;
	len = sizeof(uint64_t);
	result = sysctl(mib, 2, &memsize, &len, 0, 0);
	if (result == 0) {
		if (memsize <= WEBDAV_ONE_GIGABYTE) {
			// limit to 1/8 physical ram for systems with 1G or less
			webdavCacheMaximumSize = memsize / 8;
		}
		else {
			// limit to 1/4 physical ram for larger systems
			webdavCacheMaximumSize = memsize / 4;
		}
	}
	else {
		// limit to kDefaultCacheMaximumSize if for some bizarre reason the physical ram size cannot be determined
		webdavCacheMaximumSize = WEBDAV_DEFAULT_CACHE_MAX_SIZE;
	}
}

/*****************************************************************************/

#define TMP_WEBDAV_UDS _PATH_TMP ".webdavUDS.XXXXXX"	/* Scratch socket name */

/* maximum length of username and password */
#define WEBDAV_MAX_USERNAME_LEN 256
#define WEBDAV_MAX_PASSWORD_LEN 256
#define PASS_PROMPT "Password: "
#define USER_PROMPT "Username: "

/*****************************************************************************/

static boolean_t readCredentialsFromFile(int fd, char *userName, char *userPassword,
										 char *proxyUserName, char *proxyUserPassword)
{
	boolean_t success;
	uint32_t be_len;
	size_t len1;
	ssize_t rlen;
	
	success = TRUE;
	
	if (fd < 0) {
		syslog(LOG_ERR, "%s: invalid file descriptor arg", __FUNCTION__);
		return FALSE;
	}

	// seek to beginnning
	if (lseek(fd, 0LL, SEEK_SET) == -1) {
		return FALSE;
	}
			
	// read username length
	rlen = read(fd, &be_len, sizeof(be_len));
	if (rlen != sizeof(be_len)) {
		return FALSE;
	}
	len1 = ntohl(be_len);
	if (len1 >= WEBDAV_MAX_USERNAME_LEN) {
		return FALSE;
	}
			
	// read username (if length not zero)
	if (len1) {
		rlen = read(fd, userName, len1);
		if (rlen < 0) {
			return FALSE;
		}
	}
	
	// read password length
	rlen = read(fd, &be_len, sizeof(be_len));
	if (rlen != sizeof(be_len)) {
		return FALSE;
	}
	len1 = ntohl(be_len);
	if (len1 >= WEBDAV_MAX_USERNAME_LEN) {
		return FALSE;
	}
	
	// read password (if length not zero)
	if (len1) {
		rlen = read(fd, userPassword, len1);
		if (rlen < 0) {
			return FALSE;
		}
	}
	
	// read proxy username length
	rlen = read(fd, &be_len, sizeof(be_len));
	if (rlen != sizeof(be_len)) {
		return FALSE;
	}
	len1 = ntohl(be_len);
	if (len1 >= WEBDAV_MAX_USERNAME_LEN) {
		return FALSE;
	}
	
	// read proxy username (if length not zero)
	if (len1) {
		rlen = read(fd, proxyUserName, len1);
		if (rlen < 0) {
			return FALSE;
		}
	}	
	
	// read proxy password length
	rlen = read(fd, &be_len, sizeof(be_len));
	if (rlen != sizeof(be_len)) {
		return FALSE;
	}
	len1 = ntohl(be_len);
	if (len1 >= WEBDAV_MAX_USERNAME_LEN) {
		return FALSE;
	}
	
	// read proxy password (if length not zero)
	if (len1) {
		rlen = read(fd, proxyUserPassword, len1);
		if (rlen < 0) {
			return FALSE;
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
		int zero;
		
		zero = 0;
		
		if (fstat(fd, &statb) != -1) {
			bytes_to_overwrite = statb.st_size;
			(void)lseek(fd, 0LL, SEEK_SET);
			while (bytes_to_overwrite != 0) {
				if (bytes_to_overwrite > (off_t)sizeof(zero))
					bytes_to_write = sizeof(zero);
				else
					bytes_to_write = (size_t)bytes_to_overwrite;
				if (write(fd, &zero, bytes_to_write) < 0)
				{
					break;
				}
				bytes_to_overwrite -= bytes_to_write;
			}
			(void)fsync(fd);
		}
		(void)close(fd);
	}
	
	return TRUE;
}
		
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
	int checkKeychain = TRUE;       /* set to false for -i and -a */
	mntoptparse_t mp;

	char user[WEBDAV_MAX_USERNAME_LEN];
	char pass[WEBDAV_MAX_PASSWORD_LEN];
	char proxy_user[WEBDAV_MAX_USERNAME_LEN];
	char proxy_pass[WEBDAV_MAX_PASSWORD_LEN];
	
	struct webdav_request_statfs request_statfs;
	struct webdav_reply_statfs reply_statfs;
	int tempError;
	struct statfs buf;
	boolean_t result;
	
	error = 0;
	g_fsid.val[0] = -1;
	g_fsid.val[1] = -1;
	
	/* store our UID */
	gProcessUID = getuid();

	// init auth arrays
	memset(user, 0, sizeof(user));
	memset(pass, 0, sizeof(pass));
	memset(proxy_user, 0, sizeof(proxy_user));
	memset(proxy_pass, 0, sizeof(proxy_pass));
		
	mntflags = 0;
	/*
	 * Crack command line args
	 */
	while ((ch = getopt(argc, argv, "sSa:io:v:")) != -1)
	{
		switch (ch)
		{
			case 'a':	/* get user and password credentials from URLMount */
				{
					int fd = atoi(optarg); 		/* fd from URLMount */

					/* we're reading in from URLMOUNT, ignore keychain */
					checkKeychain = FALSE;
					
					result = readCredentialsFromFile(fd, user, pass,
													 proxy_user, proxy_pass);
					if (result == FALSE) {
						syslog(LOG_DEBUG, "%s: readCredentials returned FALSE", __FUNCTION__);
						user[0] = '\0';
						pass[0] = '\0';
						proxy_user[0] = '\0';
						proxy_pass[0] = '\0';
					}
					break;
				}
			
			case 'i': /* called from mount_webdav, get user/pass interactively */
				/* Ignore keychain when entering user/pass */
				checkKeychain = FALSE;
				/* Set RPP_REQUIRE_TTY so that we get an error if this is called 
				 * from the NetFS plugin.
				 */
				if (readpassphrase(USER_PROMPT, user, sizeof(user), RPP_REQUIRE_TTY | RPP_ECHO_ON) == NULL) {
					user[0] = '\0';
					error = errno;
				}
				else if (readpassphrase(PASS_PROMPT, pass, sizeof(pass), RPP_ECHO_OFF) == NULL) {
					pass[0] = '\0';
					error = errno;
				}
				break;
				
			case 'S':	/* Suppress ALL dialogs and notifications */
				gSuppressAllUI = 1;
				break;
			
			case 's':	/* authentication to server must be secure */
				gSecureServerAuth = TRUE;
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
	else if (error == 1) {
		/* Generic error was set, map to EINVAL.  Otherwise should be set to useful errno */
		error = EINVAL;
	}

	require_noerr_action_quiet(error, error_exit, usage());
	
	/* does this look like a mirrored mount (UI suppressed and not browseable) */
	mirrored_mount = gSuppressAllUI && (mntflags & MNT_DONTBROWSE);
	
	/* get the mount point */
	require_action(realpath(argv[optind + 1], g_mountPoint) != NULL, error_exit, error = ENOENT);
	
	/* derive the volume name from the mount point if needed */
	if ( *volumeName == '\0' )
	{
		/* the volume name wasn't passed on the command line so use the
		 * last path segment of mountPoint
		 */
		strcpy(volumeName, strrchr(g_mountPoint, '/') + 1);
	}
	
	/* Get uri (fix it up if needed) */
	uri = GetMountURI(argv[optind], &gSecureConnection);
	require_action_quiet(uri != NULL, error_exit, error = EINVAL);
	
	// check if this is an iDisk URL
	gUrlIsIdisk = urlIsIdisk(uri);
	
	// Credentials must always be sent securely to iDisk servers
	if (gUrlIsIdisk == TRUE)
		gSecureServerAuth = TRUE;	

	/* Create a mntfromname from the uri. Make sure the string is no longer than MNAMELEN */
	strncpy(mntfromname, uri , MNAMELEN);
	mntfromname[MNAMELEN - 1] = '\0';
	
	/* Check to see if this mntfromname is already used by a mount point by the
	 * current user. Sure, someone could mount using the DNS name one time and
	 * the IP address the next, or they could  munge the path with escaped characters,
	 * but this check will catch the obvious duplicates.
	 */
	count = getmntinfo(&buffer, MNT_NOWAIT);
	mntfromnameLength = (unsigned int)strlen(mntfromname);
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

	// Set default maximum file upload or download size to allow
	// the file system to cache
	setCacheMaximumSize();
	
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
	
	if ( checkKeychain == TRUE ) {
		error = get_keychain_credentials(argv[optind], user, sizeof(user), pass, sizeof(pass));
		if ( error != 0 )
			syslog(LOG_INFO, "%s: get_keychain_credentials exited with result: %d", __FUNCTION__, error);
	}
	
	error = authcache_init(user, pass, proxy_user, proxy_pass, NULL);
	require_noerr_action_quiet(error, error_exit, error = EINVAL);
	
	cookies_init();
	
	bzero(user, sizeof(user));
	bzero(pass, sizeof(pass));
	bzero(proxy_user, sizeof(proxy_user));
	bzero(proxy_pass, sizeof(proxy_pass));	
	
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

	/* bind socket with owner write-only access (S_IWUSR) which is all that's needed for the kext to connect */
	mode_mask = umask(0577);
	require_action(bind(listen_socket, (struct sockaddr *)&un, (socklen_t)sizeof(un)) >= 0, error_exit, error = EINVAL);
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

	/* 
	 * Catch signals which suggest we shut down cleanly.  Other signals
	 * will kill webdavfs_agent and will be appropriately handled by 
	 * CrashReporter.
	 * 
	 * SIGTERM is set to webdav_kill after we've successfully mounted.
	 */
	signal(SIGHUP, webdav_kill);
	signal(SIGINT, webdav_kill);
	signal(SIGQUIT, webdav_kill);

	/* prepare mount args */
	args.pa_mntfromname = mntfromname;
	args.pa_version = kCurrentWebdavArgsVersion;
	args.pa_socket_namelen = (int)sizeof(un);
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
	
	args.pa_server_ident = gServerIdent;	/* gServerIdent is set in filesytem_mount() */
	args.pa_root_id = root_node->nodeid;
	args.pa_root_fileid = WEBDAV_ROOTFILEID;
	args.pa_uid = geteuid();	/* effective uid of user mounting this volume */
	args.pa_gid = getegid();	/* effective gid of user mounting this volume */	
	args.pa_dir_size = WEBDAV_DIR_SIZE;
	/* pathconf values: >=0 to return value; -1 if not supported */
	args.pa_link_max = 1;			/* 1 for file systems that do not support link counts */
	args.pa_name_max = NAME_MAX;	/* The maximum number of bytes in a file name */
	args.pa_path_max = PATH_MAX;	/* The maximum number of bytes in a pathname */
	args.pa_pipe_buf = -1;			/* no support for pipes */
	args.pa_chown_restricted = (int)_POSIX_CHOWN_RESTRICTED; /* appropriate privileges are required for the chown(2) */
	args.pa_no_trunc = (int)_POSIX_NO_TRUNC; /* file names longer than KERN_NAME_MAX are truncated */
	
	/* need to get the statfs information before we can call mount */
	bzero(&reply_statfs, sizeof(struct webdav_reply_statfs));

	request_statfs.pcr.pcr_uid = getuid();
	
	request_statfs.root_obj_id = root_node->nodeid;

	tempError = filesystem_statfs(&request_statfs, &reply_statfs);
	
	memset (&args.pa_vfsstatfs, 0, sizeof (args.pa_vfsstatfs));

	if (tempError == 0) {
		args.pa_vfsstatfs.f_bsize = (uint32_t)reply_statfs.fs_attr.f_bsize;
		args.pa_vfsstatfs.f_iosize = (uint32_t)reply_statfs.fs_attr.f_iosize;
		args.pa_vfsstatfs.f_blocks = reply_statfs.fs_attr.f_blocks;
		args.pa_vfsstatfs.f_bfree = reply_statfs.fs_attr.f_bfree;
		args.pa_vfsstatfs.f_bavail = reply_statfs.fs_attr.f_bavail;
		args.pa_vfsstatfs.f_files = reply_statfs.fs_attr.f_files;
		args.pa_vfsstatfs.f_ffree = reply_statfs.fs_attr.f_ffree;
	}
	
	/* since we just read/write to a local cached file, set the iosize to be the same size as the local filesystem */
	tempError = statfs(_PATH_TMP, &buf);
	if ( tempError == 0 )
		args.pa_vfsstatfs.f_iosize = (uint32_t)buf.f_iosize;

	/* mount the volume */
	return_code = mount(vfc.vfc_name, g_mountPoint, mntflags, &args);
	require_noerr_action(return_code, error_exit, error = errno);
		
	/* we're mounted so kill our parent so it will call parentexit and exit with success */
 	kill(getppid(), SIGTERM);
	
	isMounted = TRUE;

	/* and set SIGTERM to webdav_kill */
	signal(SIGTERM, webdav_kill);
	
	syslog(LOG_INFO, "%s mounted", g_mountPoint);
	
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
				syslog(LOG_ERR, "%s: exiting on select errno %d for %s\n",
					   __FUNCTION__, error, g_mountPoint);
			require(error == EINTR, error_exit); /* EINTR is OK */
			continue;
		}
		
		/* Did we receive (SIGINT | SIGTERM | SIGHUP | SIGQUIT)? */
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
					syslog(LOG_ERR, "%s: received signal: %d. Unmounting %s\n",
						   __FUNCTION__, message, g_mountPoint);
				}
				else
				{
					if ( message == -2 )
					{
						/* this is the normal way out of the select loop */
						syslog(LOG_DEBUG, "%s: received unmount message for %s\n",
							   __FUNCTION__, g_mountPoint);
						break;
					}
					else
					{
						syslog(LOG_ERR, "%s: received message: %d. Force unmounting %s\n",
							   __FUNCTION__, message, g_mountPoint);
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
			
			addrlen = (socklen_t)sizeof(addr);
			accept_socket = accept(listen_socket, (struct sockaddr *)&addr, &addrlen);
			if (accept_socket < 0)
			{
				error = errno;
				if (error != EINTR)
				syslog(LOG_ERR, "%s: exiting on select errno %d for %s\n",
						__FUNCTION__, error, g_mountPoint);
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
	
	syslog(LOG_DEBUG, "%s unmounted\n", g_mountPoint);

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
				
			/* Could not connect to the server because the name or password is not correct */
			case EAUTH:
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
