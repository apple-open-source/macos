 /*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
  *
  * @APPLE_LICENSE_HEADER_START@
  *
  * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
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
/*      @(#)WebDAV_Mount.c      *
 *      (c) 2000   Apple Computer, Inc.  All Rights Reserved
 *
 *
 *      WebDAV_Mount.c -- Routine for mounting webdav volumes
 *
 *      MODIFICATION HISTORY:
 *              12-Feb-00	Clark Warner	File Creation
 *		23-Feb-00	Pat Dirks	Incorporated into URLMount framework
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/dirent.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <syslog.h>

#include <CoreServices/CoreServices.h>

#include <NetFS/NetFSPlugin.h>
#include <NetFS/NetFSUtil.h>
#include <NetFS/NetFSPrivate.h>
#include "webdavlib.h"

#define WebDAVMediaType ((VolumeType)('http'))
#define WEBDAVURLSCHEME "http"

#define UNIQUEDIGITS 5
#define MOUNT_COMMAND "/sbin/mount"
#define WEBDAV_MOUNT_TYPE "webdav"
#define HTTP_PREFIX "http://"
#define HTTPS_PREFIX "https://"

#define XXY(...) syslog(LOG_ERR, __VA_ARGS__)

#if PRAGMA_STRUCT_ALIGN
#pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
#pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
#pragma pack(2)
#endif

static CFURLRef copyStripUserPassFromCFURL(CFURLRef in_url);

/*
 * The session ref structure for webdav
 *
 * Note: According to Guy Harris, the first element of webdav_ctx does *NOT* have
 * to have the schema as a CFStringRef, despite comments in NFS and SMBs plugin.
 */
struct webdav_ctx {
    pthread_mutex_t mutex;
    CFStringRef     ct_user;
    CFStringRef     ct_pass;
	CFStringRef     ct_proxy_user;
	CFStringRef     ct_proxy_pass;
	CFURLRef		ct_url;
};

struct runloopInfo {
    CFHTTPMessageRef message;
    CFStringRef user;
    CFStringRef pass;
    int status;
};

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
#pragma pack(pop)
#elif PRAGMA_STRUCT_PACK
#pragma pack()
#endif

/* the host names of .Mac iDisk servers */
char *idisk_server_names[] = {"idisk.mac.com", "idisk.me.com", NULL};

/* Macro to simplify common CFRelease usage */
#define CFReleaseNull(obj) do { if(obj != NULL) { CFRelease(obj); obj = NULL; } } while (0)

/*
 * Undo the standard %-sign encoding in URIs (e.g., `%2f' -> `/'). This
 * must be done after the URI is parsed, since the principal purpose of
 * the encoding is to hide characters which would otherwise be significant
 * to the parser (like `/').
 */
void percent_decode_in_place(char *uri)
{
	char *s;
	
	s = uri;
	
	while (*uri)
	{
		if (*uri == '%' && uri[1] && isxdigit(uri[1]) && isxdigit(uri[2]))
		{
			int c;
			char buf[] = "xx";
			
			buf[0] = uri[1];
			buf[1] = uri[2];
			sscanf(buf, "%x", &c);
			uri += 3;
			*s++ = c;
		}
		else
		{
			*s++ = *uri++;
		}
	}
	*s = '\0';
}

static char* createUTF8CStringFromCFString(CFStringRef in_string)
{
	char* out_cstring = NULL;
	
	CFIndex bufSize;
	
	/* make sure we're not passed garbage */
	if ( in_string == NULL )
		return NULL;
	
	/* Add one to account for NULL termination. */
	bufSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(in_string) + 1, kCFStringEncodingUTF8);
	
	out_cstring = (char *)calloc(1, bufSize);
	
	/* Make sure malloc succeeded then convert cstring */
	if ( out_cstring == NULL ) 
		return NULL;
	
	if ( CFStringGetCString(in_string, out_cstring, bufSize, kCFStringEncodingUTF8) == FALSE ) {
		free(out_cstring);
		out_cstring = NULL;
	}
	
	return out_cstring;
}

static char *
SkipSeparators(char *bytes)
{
    /* skip over slash characters to find firstChar */
    while ( *bytes != '\0' ) {
		if ( *bytes == '/' ) {
			/* skip separator characters */
			++bytes;
		} else {
			/* not a separator character */
			break;
		}
    }
    return ( bytes );
}

static void
ParsePathSegment(char *bytes, char **firstChar, char **lastChar)
{
    /* skip over slash characters to find firstChar */
    *firstChar = bytes = SkipSeparators(bytes);
    
    /* skip over non-slash characters */
    while ( *bytes != '\0' ) {
		if ( *bytes == '/' ) {
			/* separator character */
			break;
		} else {
			/* skip non-separator characters */
			++bytes;
		}
    }
    *lastChar = bytes;
}

static char *
CopySegment(char *start, char *endPlusOne)
{
    int		length;
    char	*result;
    
    /* get length of segment and allocate space for string */
    length = endPlusOne - start;
    result = malloc(length + 1);
    if ( result != NULL ) {
		/* copy the segment and terminate the string */
		strncpy(result, start, length);
		result[length] = '\0';
    }
    return ( result );
}

/* basename_from_path() parses hostport_abs_path
 * to get name.
 * If there are no path segments in abs_path, name is the host.
 * Otherwise if the host is NOT idisk.mac.com OR abs_path is
 * only one segment, name is the last path segment.
 * Otherwise (host is idisk.mac.com AND there are multiple
 * path segements), name is the first path segment concatenated
 * with the last path segment separated with a dash character.
 * name is assumed to be char[MAXNAMLEN].
 */
static int
basename_from_path(const char *hostport_abs_path, char *name, size_t maxlength, int hostisidisk)
{
    int		error;
    char	*path;
    char	*host;
    char	*firstPathSegment;
    char	*lastPathSegment;
    int		length;
    char	*colon;
    char	*slash;
    char	*firstChar;
    char	*lastChar;
    
    error = 0;
    path = host = firstPathSegment = lastPathSegment = NULL;
    
    /* validate input parameters */
    if ( (hostport_abs_path == NULL) || (name == NULL) ) {
		error = EINVAL;
		goto exit;
    }
    
    /* no output name yet */
    *name = '\0';
	
    /* get length of input */
    length = strlen(hostport_abs_path);
    if ( length == 0 ) {
		error = EINVAL;
		goto exit;
    }
	
    /* allocate space for path */
    path = malloc(length + 2); /* one extra for slash if needed */
    if ( path == NULL ) {
		error = ENOMEM;
		goto exit;
    }
    /* and make a private copy of hostport_abs_path*/
    strlcpy(path, hostport_abs_path, length + 2);
	
    /* add a trailing slash if needed */
    if ( path[length] != '/' ) {
		strlcat(path, "/", length + 2);
    }
    
    /* find the first colon (if any) and the first slash */
    colon = strchr(path, ':');
    slash = strchr(path, '/');
    
    /* get the host name */
    if ( (colon == NULL) || (colon > slash) ) {
		/* if no colon, or the colon is after the slash,
		 * then there is no port so the host is everything
		 * up to the slash
		 */
		host = CopySegment(path, slash);
    } else {
		/* there is a port so the host is everything
		 * up to the colon
		 */
		host = CopySegment(path, colon);
    }
    
    /* find first path segment (if any) */
    lastChar = slash;
    ParsePathSegment(lastChar, &firstChar, &lastChar);
    if (firstChar != lastChar) {
		/* copy first path segment */
		firstPathSegment = CopySegment(firstChar, lastChar);
		percent_decode_in_place(firstPathSegment);
		
		/* find last path segment (if any) */
		while ( *lastChar != '\0' ) {
			ParsePathSegment(lastChar, &firstChar, &lastChar);
			if (firstChar != lastChar) {
				if  ( lastPathSegment != NULL ) {
					/* free up the previous lastPathSegment */
					free(lastPathSegment);
				}
				/* copy (new) last path segment */
				lastPathSegment = CopySegment(firstChar, lastChar);
			}
		}
		
		if ( lastPathSegment != NULL ) {
			percent_decode_in_place(lastPathSegment);
			/* is this the iDisk server? */
			if ( hostisidisk ) {
				/* iDisk server -- name is "firstPathSegment-lastPathSegment" */
				
				/* check length of strings */
				if ( (strlen(firstPathSegment) + strlen(lastPathSegment) + 2) > maxlength ) {
					error = ENAMETOOLONG;
					goto exit;
				}
				
				/* combine strings */
				strlcpy(name, firstPathSegment, maxlength);
				strlcat(name, "-", maxlength);
				strlcat(name, lastPathSegment, maxlength);
			} else {
				/* name is lastPathSegment */
				if ( (strlen(lastPathSegment) + 1) > MAXNAMLEN ) {
					error = ENAMETOOLONG;
					goto exit;
				}
				
				strlcpy(name, lastPathSegment, maxlength);
			}
		} else {
			/* no last path segment -- name is firstPathSegment*/
			if ( (strlen(firstPathSegment) + 1) > MAXNAMLEN ) {
				error = ENAMETOOLONG;
				goto exit;
			}
			
			strlcpy(name, firstPathSegment, maxlength);
		}
    } else {
		/* no path segments -- name is host */
		if ( (strlen(host) + 1) > MAXNAMLEN ) {
			error = ENAMETOOLONG;
			goto exit;
		}
		
		strlcpy(name, host, maxlength);
    }
	
exit:
    /* free up any memory and return any errors */
    if ( path != NULL ) {
		free(path);
    }
    if ( host != NULL ) {
		free(host);
    }
    if ( firstPathSegment != NULL ) {
		free(firstPathSegment);
    }
    if ( lastPathSegment != NULL ) {
		free(lastPathSegment);
    }
    return ( error );
}

static int
create_basename(const char *url, size_t maxlength, char *mountpoint,
				char *basename, int hostisidisk)
{
    const char *namestart;
    char *slash;
    char realmountdir[MAXPATHLEN];
    int error = 0;
    
    /* Find the real name of mountdir, by removing all ".", ".." and symlink components.
     * Any extra "/" characters (including any trailing "/") are removed.
     */
    if (realpath(mountpoint, realmountdir) == NULL) {
		error = errno;
		goto errorexit;
    }
	
    /* Check for obvious error cases first: */
    if ((strlen(realmountdir)) + 1 > maxlength) {
        return(ENAMETOOLONG);
    }
	
    namestart = url;
	
    /* First pick out a name we can use */
	
    if (strncasecmp (url, HTTP_PREFIX, sizeof(HTTP_PREFIX) - 1) == 0 ||
		strncasecmp (url, HTTPS_PREFIX, sizeof(HTTPS_PREFIX) - 1) == 0) {
        /* path starts with http or https so lets skip that */
#if DEBUG_TRACE
		syslog(LOG_DEBUG, "WebDAV create_basename: stripping off HTTP(S)_PREFIX...\n");
#endif
        slash = strchr(url,'/');
        if (slash) {
			namestart = slash + 1;
			while (*namestart == '/') {
				++namestart;
			}
        }
#if DEBUG_TRACE
        syslog(LOG_DEBUG, "WebDAV create_basename: resulting name = '%s'...\n", namestart);
#endif
    }
	
    /* create a basename from the path and check the host for idisk */
    error = basename_from_path(namestart, basename, maxlength, hostisidisk);
    if ( error != 0 ) {
		return ( error );
    }
    
#if DEBUG_TRACE
		syslog(LOG_DEBUG, "WebDAV create_basename: uniquename = '%s'...\n", basename);
#endif
	
errorexit:
	
    return(error);
} /* create_basename */

static int
GetMountURI(const char *arguri, char dst_buf[], size_t mntfrom_size)
{
	int hasScheme;
	int hasTrailingSlash;
	int isHTTPS;
	int result;
	size_t argURILength;
	
	result = 0;
	argURILength = strlen(arguri);
	
	if (argURILength + 1 >= mntfrom_size)
		return EINVAL;
	
	isHTTPS = (strncasecmp(arguri, HTTPS_PREFIX, strlen(HTTPS_PREFIX)) == 0);
	
	/* if there's no scheme, we'll have to add "http://" */
	hasScheme = ((strncasecmp(arguri, HTTP_PREFIX, strlen(HTTP_PREFIX)) == 0) || isHTTPS);
	
	/* if there's no trailing slash, we'll have to add one */
	hasTrailingSlash = arguri[argURILength - 1] == '/';
	
	/* copy arguri adding scheme and trailing slash if needed */ 
	if ( !hasScheme )
	{
		/* We're not worried about HTTP_PREFIX being bigger than MNAMELEN */
		(void)strlcpy(dst_buf, HTTP_PREFIX, mntfrom_size);
	}
	else
	{
		dst_buf[0] = '\0';
	}
	
	/* Use strlcat to verify dst_buf is large enough to hold */
	if ( strlcat(dst_buf, arguri, mntfrom_size) >= mntfrom_size )
	{
		result = EINVAL;
	}
	else if ( !hasTrailingSlash )
	{
		if ( strlcat(dst_buf, "/", mntfrom_size) >= mntfrom_size )
			result = EINVAL;
	}
	
	return ( result );
}

static int
FindActiveMountPointFromURL(const char* url, char* mountpoint, size_t mountpoint_len)
{
    struct statfs *buffer;
    int i, count, error;
    unsigned int mntfromnameLength;
    uid_t processUID;
    char  mntfromname[MNAMELEN];
    
    error = EBUSY;
    processUID = getuid();
    
    /* Copy URL to mntfromname and fix it up if needed */
    if ( GetMountURI(url, mntfromname, sizeof(mntfromname)) != 0 ) {
		syslog(LOG_ERR, "%s: url: %s is too long!", __FUNCTION__, url);
		return EINVAL;
	}
	
    mntfromnameLength = strlen(mntfromname);
    
    count = getmntinfo(&buffer, MNT_NOWAIT);
    for (i = 0; i < count; i++)
    {
		if ( (buffer[i].f_owner == processUID) &&
			(strcmp("webdav", buffer[i].f_fstypename) == 0) &&
			(strlen(buffer[i].f_mntfromname) == mntfromnameLength) &&
			(strncasecmp(buffer[i].f_mntfromname, mntfromname, mntfromnameLength) == 0) )
		{
			/* Found the active mount, copy over the mount point */
			strlcpy(mountpoint, buffer[i].f_mntonname, mountpoint_len);
			error = 0;	/* indicate success */
			break;
		}
    }
	
    return (error);
}

#define CFENVFORMATSTRING "__CF_USER_TEXT_ENCODING=0x%X:0:0"

static int
AttemptMount(const char *urlPtr, const char *mountPointPtr,  u_int32_t options, int fd, const char *volumename, int hostisidisk)
{
    pid_t pid, terminated_pid;
    int result;
    union wait status;
    char options_str[MAXNAMLEN + 21];	/* worst case is '-aXXXXXXXXXX,-S,-v"MAXNAMELEN"' as a c-string -- that's MAXNAMLEN + 21 */
    
    pid = fork();
    if (pid == 0) {
		uid_t effective_uid;
		uid_t real_uid;
		char CFUserTextEncodingEnvSetting[sizeof(CFENVFORMATSTRING) + 20]; 
		char *env[] = {CFUserTextEncodingEnvSetting, "", (char *) 0 };
		
		real_uid = getuid();		/* get the real user ID */
		effective_uid = geteuid();	/* get the effective user ID */
		/* If the real user ID is the super user and the effective user ID is not,
		 * then we need to set the real user ID to be the same as the effective user ID
		 * before executing mount. 
		 */
		if ( (real_uid == 0) && (effective_uid != 0) ) {
			setuid(real_uid);		/* set both real and effective user IDs to super user so the next call will work */ 
			setuid(effective_uid);	/* set both real and effective user IDs to the saved effective user ID */
		}
		/* 
		 * Create a new environment with a definition of __CF_USER_TEXT_ENCODING to work 
		 * around CF's interest in the user's home directory (which could be networked, 
		 * causing recursive references through automount). Make sure we include the uid
		 * since CF will check for this when deciding if to look in the home directory.
		 */ 
		snprintf(CFUserTextEncodingEnvSetting, sizeof(CFUserTextEncodingEnvSetting), CFENVFORMATSTRING, getuid());
		
		*options_str = '\0';
		if (fd != -1) {
			/* add the -a<fd> option */
			snprintf(options_str, MAXNAMLEN + 21, "-a%d", fd);
		}
#if 0 /* XXX */
		if (options & kSuppressAllUI) {
			/* add the -S option */
			if (*options_str != '\0') {
				/* add comma if there are already options */
				strcat(options_str, ",");
			}
			strcat(options_str, "-S");
		}
#endif

		if (volumename != NULL && *volumename != '\0') {
			/* add the -v<volumename> option */
			if (*options_str != '\0') {
				/* add comma if there are already options */
				strlcat(options_str, ",", MAXNAMLEN + 21);
			}
			strlcat(options_str, "-v", MAXNAMLEN + 21);
			strlcat(options_str, volumename, MAXNAMLEN + 21);
		}
		if (hostisidisk) {
			/* add the -s option */
			if (*options_str != '\0') {
				/* add comma if there are already options */
				strlcat(options_str, ",", MAXNAMLEN + 21);
			}
			strlcat(options_str, "-s", MAXNAMLEN + 21);
		}
		
		if (*options_str == '\0') {
			result = execle(MOUNT_COMMAND, MOUNT_COMMAND ,
							"-t", WEBDAV_MOUNT_TYPE, 
							"-o", (options & MNT_AUTOMOUNTED) ? "automounted" : "noautomounted",
							"-o", (options & MNT_DONTBROWSE) ? "nobrowse" : "browse",
							"-o", (options & MNT_RDONLY) ? "rdonly" : "nordonly",
							urlPtr, mountPointPtr, NULL, env);
		} else {
			result = execle(MOUNT_COMMAND, MOUNT_COMMAND ,
							"-t", WEBDAV_MOUNT_TYPE, 
							"-o", (options & MNT_AUTOMOUNTED) ? "automounted" : "noautomounted",
							"-o", (options & MNT_DONTBROWSE) ? "nobrowse" : "browse",
							"-o", (options & MNT_RDONLY) ? "rdonly" : "nordonly",
							"-o", options_str,
							urlPtr, mountPointPtr, NULL, env);
		}
		/* IF WE ARE HERE, WE WERE UNSUCCESSFUL */
        exit(result ? result : ECHILD);
    }
    
    if (pid == -1) {
        result = -1;
        goto Return;
    }
    
    /* No async mount was requested: wait for completion in-line here */
    while ( (terminated_pid = wait4(pid, (int *)&status, 0, NULL)) < 0 ) {
		/* retry if EINTR, else break out with error */
		if ( errno != EINTR ) {
			break;
		}
    }
    
    if ((terminated_pid == pid) && (WIFEXITED(status))) {
		result = WEXITSTATUS(status);
    } else {
		result = -1;
    }
    
Return:
    if (fd != -1)
        (void)close(fd);
    return result;
}

static int cfurl_is_idisk(CFURLRef url)
{
	int found_idisk;
	size_t len, host_len;
	char** idisk_server;
	char *cstr = NULL;
	CFStringRef host = NULL;
	
	found_idisk = FALSE;
	
	host = CFURLCopyHostName(url);
	
	if (host == NULL)
		return (found_idisk);
	
	cstr = createUTF8CStringFromCFString(host);
	CFRelease(host);
	
	if (cstr == NULL)
		return (found_idisk);

	host_len = strlen(cstr);
	idisk_server = idisk_server_names;
	
	while (*idisk_server) {
		len = strlen(*idisk_server);
		if (host_len >= len) {
			// check for match
			if ( strncasecmp(cstr, *idisk_server, len) == 0 ) {
				found_idisk = TRUE;
				break;
			}
		}
		idisk_server++;
	}
	
	free(cstr);
	
	return (found_idisk);
}

static int curl_is_idisk(const char* url)
{
	int found_idisk;
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


#define WEBDAV_TEMPLATE "/tmp/webdav.XXXXXX"

static int
WebDAVMountURL(const char *url,
			   const char *username,
			   const char *password,
			   const char *proxy_username,
			   const char *proxy_password,
			   char *mountpoint,
			   size_t mountpoint_len,
			   u_int32_t options)
{
    int fd = -1;
    char basename[MAXNAMLEN];
    int error;
    int hostisidisk = FALSE;
	
#if DEBUG_TRACE
    syslog(LOG_DEBUG, 
		   "WebDAV_MountURLWithAuthentication: Mounting '%s'...\n", 
		   url);
#endif
	
    hostisidisk = curl_is_idisk(url);
	
    if ((error = create_basename(url, MAXNAMLEN, mountpoint, basename, hostisidisk)) != 0)
        goto error;
	
    if ((username != NULL) || (proxy_username != NULL)) {
		size_t len = sizeof(WEBDAV_TEMPLATE) + 1;
        char template[len];
		
		strlcpy(template, WEBDAV_TEMPLATE, len);
        if ((fd = mkstemp(template)) != -1) {
			size_t len;
			uint32_t be_len;
			
			/*
			 * We write the lengths out big-endian, because we might be
			 * running on an x86 machine on behalf of a PPC application,
			 * and thus we might be running big-endian even on x86, but
			 * mount_webdav will always be native and thus little-endian
			 * on x86 machines.
			 */
			unlink(template);
			
			if (username != NULL) {
				len = strlen(username);
				be_len = htonl(len);
				
				if (write(fd, &be_len, sizeof be_len) > 0) {
					if (write(fd, username, len) > 0) {
						if (password)
							be_len = htonl(strlen(password));
						else
							be_len = 0;
						if ((write(fd, &be_len, sizeof be_len) > 0) && password)
							write(fd, password, strlen(password));
					}
				}
			}
			else {
				be_len = 0;
				
				// Write zeros for username len and password len
				write(fd, &be_len, sizeof be_len);
				write(fd, &be_len, sizeof be_len);
			}

			if (proxy_username != NULL) {
				len = strlen(proxy_username);
				be_len = htonl(len);
				if (write(fd, &be_len, sizeof be_len) > 0) {
					if (write(fd, proxy_username, len) > 0) {
						if (proxy_password)
							be_len = htonl(strlen(proxy_password));
						else
							be_len = 0;
						if ((write(fd, &be_len, sizeof be_len) > 0) && proxy_password)
							write(fd, proxy_password, strlen(proxy_password));
					}
				}
			}
			else {
				be_len = 0;
				
				// Write zeros for proxy username len and proxy password len
				write(fd, &be_len, sizeof be_len);
				write(fd, &be_len, sizeof be_len);				
			}
			
			(void)fsync(fd);
			/* fd will be closed by the mount_webdav that is execl()'ed
			 in AttemptMount */
		}
    }
	
#if DEBUG_TRACE
    syslog(LOG_DEBUG, 
		   "WebDAV_MountURLWithAuthentication: AttemptMount('%s', '%s')...\n",
		   url, mountpoint);
#endif
	
    if ((error = AttemptMount(url, mountpoint, options, fd, basename, hostisidisk))) {
#if DEBUG_TRACE
		syslog(LOG_DEBUG, 
			   "WebDAV_MountURLWithAuthentication: AttemptMount returned %d\n", 
			   error, mountpoint);
#endif
		if (error == EBUSY) {
			/*
			 * The mount attempt failed because this 'mntfromname' is already
			 * mounted by the current user.  Try and return the mountpoint
			 * that 'mntfromname' is currently mounted on.
			 */
			if (!FindActiveMountPointFromURL(url, mountpoint, mountpoint_len)) {
				/* Return EEXIST since we have mount info */
				error = EEXIST;
			}
		}
		goto error;
    };
	
    return(0);
	
error:	
	/* errorexit: */
    return(error);
}

static SInt32
GetHostAndPort(CFURLRef in_URL, CFStringRef *out_Host,
			   CFStringRef *out_Port)
{
    SInt32 error = 0;
    CFStringRef host = NULL;
    CFStringRef port = NULL;
    CFMutableStringRef hostM = NULL;
    SInt32 altPort;
    CFRange foundColon;
    
    if (out_Host != NULL)
		*out_Host = NULL;
    if (out_Port != NULL)
		*out_Port = NULL;
    
    /* Get the host address */
    host = CFURLCopyHostName(in_URL);
    if (host == NULL) {
		error = EINVAL;
		goto exit;
    }
    
    hostM = CFStringCreateMutableCopy(NULL, 0, host);
    CFRelease(host);
    if (hostM == NULL) {
		error = ENOMEM;
		goto exit;
    }
    
    /*
     * If it's an IPv6 address (i.e., it has : in the host), then I
     * need to put the [] back in.
     */
    foundColon = CFStringFind(hostM, CFSTR(":"), 0);
    if (foundColon.location != kCFNotFound) {
		CFStringInsert(hostM, 0, CFSTR("["));
		CFStringAppend(hostM, CFSTR("]"));
    }
    
    /* Check for alternate port number */
    altPort = CFURLGetPortNumber (in_URL);
    if (altPort != -1) {
		port = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"),
										altPort);
		if (port == NULL) {
			error = ENOMEM;
			goto exit;
		}
    }
    
    /*
     * Do they want a host:port string, or separate host and port
     * strings?
     */
    if (out_Port == NULL && port != NULL) {
		/* They want the port appended to the end of the host */
		CFStringAppend(hostM, CFSTR(":"));
		CFStringAppend(hostM, port);
    }
    
    /* No errors, so return the strings */
    if (out_Host != NULL)
		*out_Host = hostM;
    if (out_Port != NULL && port != NULL)
		*out_Port = port;
    
exit:
    if (error) {
		if (hostM != NULL)
			CFRelease(hostM);
		if (port != NULL)
			CFRelease(port);
    }
    
    return error;
}

/******************************************************************************/
/* Begin NetFS Implementation */
/******************************************************************************/


/*
 * WebDAV_CreateSessionRef
 *
 * Create a webdav_ctx and initialize the values.
 */
netfsError WebDAV_CreateSessionRef(void **out_SessionRef) 
{
    struct webdav_ctx *session_ref = NULL;
    
    /* if out_SessionRef is NULL, this is a void function, return an error */
    if (out_SessionRef == NULL)
		return EINVAL;
    
    session_ref = (struct webdav_ctx *)malloc(sizeof(struct webdav_ctx));
    /* if malloc fails, not much we can do */
    if (session_ref == NULL)
		return ENOMEM;
    
    /* zero out the region */
    memset(session_ref, '\0', sizeof(struct webdav_ctx));
    if (pthread_mutex_init(&session_ref->mutex, NULL) == -1) {
		syslog(LOG_ERR, "%s: pthread_mutex_init failed!", __FUNCTION__);
		free(session_ref);
		return EINVAL;
    }
    *out_SessionRef = session_ref;
    return 0;
}


/*
 * WebDAV_GetServerInfo
 *
 * Since CFHTTP doesn't have a way to get this information from the server, we
 * just set some basic dictionary keys to convey what we want NetAuth to show in
 * the connection dialog.
 */
netfsError WebDAV_GetServerInfo(CFURLRef in_URL,
								void *in_SessionRef, 
								CFDictionaryRef in_GetInfoOptions,
								CFDictionaryRef *out_ServerParms) 
{
#pragma unused(in_SessionRef, in_GetInfoOptions)
    CFMutableDictionaryRef mutableDict = NULL;
	CFStringRef serverName = NULL;
	CFMutableDictionaryRef proxyInfo;
	CFStringRef cf_str;
	CFURLRef a_url;
	enum WEBDAVLIBAuthStatus authStat;
	int error;
	
	/* verify we weren't passed in garbage */
	if ( in_URL == NULL || out_ServerParms == NULL )
		return EINVAL;    
    
    /* create and return the server parameters dictionary */
    mutableDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
											&kCFTypeDictionaryValueCallBacks);
    
    if (mutableDict == NULL) {
		return ENOMEM;
    }
	
	serverName = CFURLCopyHostName(in_URL);
    
    /* We have no way to change the password on the server */
    CFDictionarySetValue (mutableDict, kNetFSSupportsChangePasswordKey, kCFBooleanFalse);
    
	/* Get the serverDisplayName*/
	if(serverName != NULL) {
		CFDictionarySetValue(mutableDict, kNetFSServerDisplayNameKey, serverName);
		CFRelease(serverName);
	}
	
    CFDictionarySetValue (mutableDict, kNetFSSupportsKerberosKey, kCFBooleanFalse);
    
    /* WebDAV FS doesn't know anything until we connect, so we'll leave options open */
    CFDictionarySetValue (mutableDict, kNetFSSupportsGuestKey, kCFBooleanTrue);
    CFDictionarySetValue (mutableDict, kNetFSGuestOnlyKey, kCFBooleanFalse);
	
	/* Query for an authenticating proxy server */
	proxyInfo = CFDictionaryCreateMutable(NULL, 0,
										  &kCFTypeDictionaryKeyCallBacks, 
										  &kCFTypeDictionaryValueCallBacks);
	if (proxyInfo == NULL) {
		CFRelease(mutableDict);
		return ENOMEM;
	}
	
	// Remove user name and password from the URL
	// before passing to webdavlib
	a_url = copyStripUserPassFromCFURL(in_URL);
	
	authStat = queryForProxy(a_url, proxyInfo, &error);
	CFReleaseNull(a_url);
	
	// All we care about is whether or not there is a proxy server in the path
	if (authStat == WEBDAVLIB_ProxyAuth) {
		syslog(LOG_DEBUG, "%s: Proxy server found", __FUNCTION__);
		/* Fetch the proxy server name */
		cf_str = (CFStringRef) CFDictionaryGetValue(proxyInfo, kWebDAVLibProxyServerNameKey);
		
		if (cf_str != NULL) {
			CFDictionarySetValue(mutableDict, kNetFSProxyServerNameKey, cf_str);
		}
		else {
			syslog(LOG_ERR, "%s: Missing kNetFSProxyServerNameKey", __FUNCTION__);
		}

		/* Fetch the proxy server realm */
		cf_str = (CFStringRef) CFDictionaryGetValue(proxyInfo, kWebDAVLibProxyRealmKey);
		
		if (cf_str != NULL) {
			CFDictionarySetValue(mutableDict, kNetFSProxyServerRealmKey, cf_str);
		}
		else {
			syslog(LOG_ERR, "%s: Missing kNetFSProxyServerRealmKey", __FUNCTION__);
		}
	}
	
	CFReleaseNull(proxyInfo);
	
	// The error returned by queryForProxy() will be EAUTH for WEBDAVLIB_ProxyAuth
	// and WEBDAVLIB_ServerAuth.  This is not really an error since we were just
	// probing for a proxy server.  So return an error only if something bad
	// happened.
	if ((authStat == WEBDAVLIB_UnexpectedStatus) || (authStat == WEBDAVLIB_IOError)) {
		// We do not return a dictionary when there is an error
		CFRelease(mutableDict);
	}
	else {
		// Success
		*out_ServerParms = mutableDict;
		error = 0;
	}

    return error;
}

/*
 * WebDAV_ParseURL
 *
 * Parsing the URL is fairly simple, we're expecting RFC1808 style urls of the
 * form:
 * <scheme>://<net_loc>/<path>
 */
netfsError WebDAV_ParseURL(CFURLRef in_URL, CFDictionaryRef *out_URLParms) 
{
    CFMutableDictionaryRef mutableDict = NULL;
    CFStringRef scheme = NULL;
    CFStringRef host = NULL;
    CFStringRef port = NULL;
    CFStringRef path = NULL;
    CFStringRef unescaped_path = NULL;
    CFStringRef user = NULL;
    CFStringRef pass = NULL;
    netfsError error = 0;
    
    /* 
     * If in_URL is NULL, there's not much to do, if out_URLParms is NULL,
     * this is a void function, so return.
     */
    if (in_URL == NULL || out_URLParms == NULL)
		return EINVAL;
    
    *out_URLParms = NULL;
    
    /* check for a valid URL */
    if (!CFURLCanBeDecomposed(in_URL)) {
		error = EINVAL;
		goto exit;
    }
    
    /* create and return the parameters dictionary */
    mutableDict = CFDictionaryCreateMutable(NULL, 0,
											&kCFTypeDictionaryKeyCallBacks, 
											&kCFTypeDictionaryValueCallBacks);
    if (mutableDict == NULL) {
		error = ENOMEM;
		goto exit;
    }
    
    /* Get the scheme */
    scheme = CFURLCopyScheme(in_URL);
    if (scheme == NULL) {
		error = ENOMEM;
		goto exit;
    }
    CFDictionarySetValue(mutableDict, kNetFSSchemeKey, scheme);
    CFRelease(scheme);
    
    /* Get optional user */
    user = CFURLCopyUserName(in_URL);
    if (user != NULL) {
		CFDictionarySetValue(mutableDict, kNetFSUserNameKey, user);
		CFRelease(user);
		/* Since we have a user, check for optional password */
		pass = CFURLCopyPassword(in_URL);	
		if (pass != NULL) {
			CFDictionarySetValue(mutableDict, kNetFSPasswordKey, pass);
			CFRelease(pass);
		}
    }
    
    /* Get the host name or address and the port */
    error = GetHostAndPort(in_URL, &host, &port);
    if (error != 0)
		goto exit;
    
    CFDictionarySetValue(mutableDict, kNetFSHostKey, host);
    CFRelease(host);
    
    if (port != NULL) {
		CFDictionarySetValue(mutableDict, kNetFSAlternatePortKey, port);
		CFRelease(port);
    }
    
    /* Get optional path */
    path = CFURLCopyPath(in_URL);
    if (path != NULL) {
		unescaped_path = CFURLCreateStringByReplacingPercentEscapes(NULL,
			path, CFSTR(""));
		if (unescaped_path == NULL) {
			CFRelease(path);
			error = ENOMEM;
			goto exit;
		}
		CFRelease(path);
		CFDictionarySetValue(mutableDict, kNetFSPathKey, unescaped_path);
		CFRelease(unescaped_path);
    }
    
    /* If we reached this point we have no errors, 
     * so return the properties dictionary */
    *out_URLParms = mutableDict;
    
exit:
    if (error) {
		if (mutableDict != NULL)
			CFRelease (mutableDict);
    }
	
    return error;
}

/*
 * WebDAV_CreateURL
 *
 * Reverse the work done by WebDAV_ParseURL
 */
netfsError WebDAV_CreateURL(CFDictionaryRef in_URLParms, CFURLRef *out_URL)
{
    SInt32 error = 0;
    CFMutableStringRef urlStringM = NULL;
    CFURLRef myURL = NULL;
    CFStringRef scheme;
    CFStringRef host;
    CFStringRef port;
    CFStringRef path;
    CFStringRef user;
    CFStringRef pass;
    
    /*
     * If in_URLParms is NULL, there's not much we can do, if out_URL is NULL,
     * then this function is void.
     */
    if (in_URLParms == NULL || out_URL == NULL)
		return EINVAL;
    
    *out_URL = NULL;
    
    /* Get the required scheme */
    scheme = (CFStringRef) CFDictionaryGetValue(in_URLParms,
												kNetFSSchemeKey);	/* dont need to CFRelease this */
    if (scheme == NULL) {
		error = EINVAL;
		goto exit;
    }
    urlStringM = CFStringCreateMutableCopy(NULL, 0, scheme);
    if (urlStringM == NULL) {
		error = ENOMEM;
		goto exit;
    }
    CFStringAppend(urlStringM, CFSTR("://"));

    /* Check for optional user */
    user = (CFStringRef) CFDictionaryGetValue(in_URLParms, kNetFSUserNameKey);
    if (user != NULL) {
		user = CFURLCreateStringByAddingPercentEscapes(NULL, user, 
													   NULL, CFSTR("@:/?"), 
													   kCFStringEncodingUTF8);
		CFStringAppend(urlStringM, user);
		CFRelease(user);
		/* 
		 * Since we have a user, check for optional password.  We don't check
		 * this without user because you can't have a password without a 
		 * username in a url.
		 */
		pass = (CFStringRef) CFDictionaryGetValue(in_URLParms, kNetFSPasswordKey);
		if (pass != NULL) {
			pass = CFURLCreateStringByAddingPercentEscapes(NULL, pass, 
														   NULL, CFSTR("@:/?"), 
														   kCFStringEncodingUTF8);
			CFStringAppend(urlStringM, CFSTR(":"));
			CFStringAppend(urlStringM, pass);
			CFRelease(pass);
		}
		CFStringAppend(urlStringM, CFSTR("@"));
    }
    
    /* Get the required host */
    host = (CFStringRef) CFDictionaryGetValue(in_URLParms, kNetFSHostKey);
    if (host == NULL) {
		error = EINVAL;
		goto exit;
    }
    host = CFURLCreateStringByAddingPercentEscapes(NULL, host,
												   CFSTR("[]"), CFSTR("/@:,?=;&+$"), kCFStringEncodingUTF8);
    /* add the host */
    CFStringAppend(urlStringM, host);
    CFRelease(host);
    
    /* Check for optional alternate port */
    port = (CFStringRef) CFDictionaryGetValue(in_URLParms,
											  kNetFSAlternatePortKey);
    if (port != NULL) {
		port = CFURLCreateStringByAddingPercentEscapes(NULL, port,
													   NULL, NULL, kCFStringEncodingUTF8);
		CFStringAppend(urlStringM, CFSTR(":"));
		CFStringAppend(urlStringM, port);
		CFRelease(port);
    }
    
    /* Check for path.  Path is an optional field, but NetAuthAgent adds an
	 * Explicit "/" if there isn't one, so this should never be NULL
	 */
    path = (CFStringRef) CFDictionaryGetValue(in_URLParms, kNetFSPathKey);
    if (path != NULL) {
		/* Escape  */
		path = CFURLCreateStringByAddingPercentEscapes(NULL, path, 
													   NULL, CFSTR("?"), kCFStringEncodingUTF8);
		CFStringAppend(urlStringM, path);
		CFRelease(path);
    } 
	else {
		syslog(LOG_ERR, "%s: path is NULL!", __FUNCTION__);
		error = EINVAL;
		goto exit;
	}
    
    /* convert to CFURL */
    myURL = CFURLCreateWithString(NULL, urlStringM, NULL);
    CFRelease(urlStringM);
    urlStringM = NULL;
    if (myURL == NULL) {
		error = ENOMEM;
		goto exit;
    }
    
    /* if we got to this point, there are no errors, so return the CFURL */
    *out_URL = myURL;
    
exit:
    if (urlStringM != NULL)
		CFRelease(urlStringM);
    
    return error;
}


/*
 * copyStripUserPassFromCFURL
 *
 * Take in_url and create a new url with no user/pass components.
 * return NULL on failure.
 */
static CFURLRef copyStripUserPassFromCFURL(CFURLRef in_url)
{
	CFURLRef newurl = NULL;
	CFMutableDictionaryRef mutableDict = NULL;
	
	if (in_url == NULL)
		return NULL;
	
	if (WebDAV_ParseURL(in_url, (CFDictionaryRef *)&mutableDict) != 0) {
		syslog(LOG_ERR, "%s: WebDAV_ParseURL failed", __FUNCTION__);
		return NULL;
	}
	
	CFDictionaryRemoveValue(mutableDict, kNetFSUserNameKey);
	CFDictionaryRemoveValue(mutableDict, kNetFSPasswordKey);
		
	if(WebDAV_CreateURL(mutableDict, &newurl) != 0) {
		syslog(LOG_ERR, "%s: WebDAV_CreateURL failed", __FUNCTION__);
		return NULL;
	}
	CFRelease(mutableDict);
	
	return newurl;
}

/*
 * WebDAV_OpenSession
 *
 * WebDAV needs to keep the user/pass around in webdav_agent, so we save those 
 * here and pass them along.
 */
netfsError WebDAV_OpenSession(CFURLRef in_URL,
							  void *in_SessionRef,
							  CFDictionaryRef in_OpenOptions,
							  CFDictionaryRef *out_SessionInfo)
{
    int error = 0;
    struct webdav_ctx* ctx = in_SessionRef;
    int UseGuest = FALSE;
    CFMutableDictionaryRef mutableDict = NULL;
	boolean_t checkAuth, checkProxyServerOnly;
	enum WEBDAVLIBAuthStatus authStat;
	CFDictionaryRef proxyCredsDict;
	CFMutableDictionaryRef serverCredsDict;
	CFURLRef a_url;
    
    if (in_SessionRef == NULL) {
		syslog(LOG_ERR, "%s: in_SessionRef is NULL!", __FUNCTION__);
		return EINVAL;
    }
	
	if (out_SessionInfo != NULL) {
		*out_SessionInfo = NULL;
	}
    
    if (in_OpenOptions != NULL) {
		CFBooleanRef booleanRef = NULL;
		booleanRef = (CFBooleanRef)CFDictionaryGetValue(in_OpenOptions, kNetFSUseGuestKey);
		if (booleanRef != NULL)
			UseGuest = CFBooleanGetValue(booleanRef);
    }
	
	/* Check if we have proxy server credentials */
	proxyCredsDict = (CFDictionaryRef) CFDictionaryGetValue(in_OpenOptions, kNetFSProxyServerCredentialsKey);
	
	if (proxyCredsDict == NULL) {
		/*
		 * NetAuth is not checking proxy server credentials.
		 * If this is not a Guest mount request, then we have 
		 * valid server credentials in the in_URL.
		 */

		/*
		 * We need to save the username/pass credentials here since NetAuth currently
		 * strips them out before it passes the components of the URL to CreateURL.
		 * We'll need to use the username/pass credentials several times for authentication.
		 */
		if (in_URL != NULL) {
			if (ctx->ct_url != NULL)
				CFRelease(ctx->ct_url);
			
			ctx->ct_url = CFURLCopyAbsoluteURL(in_URL);
			if (ctx->ct_url != NULL) {
				ctx->ct_user = CFURLCopyUserName(ctx->ct_url);
				ctx->ct_pass = CFURLCopyPassword(ctx->ct_url);
			}
		}
		
		if (out_SessionInfo != NULL) {
			mutableDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			if (mutableDict == NULL) {
				syslog(LOG_ERR, "%s: mutableDict is NULL!", __FUNCTION__);
				return ENOMEM;
			}
			
			if (ctx->ct_user != NULL) {
				CFDictionarySetValue(mutableDict, kNetFSMountedByUserKey, ctx->ct_user);
			}
			else if (UseGuest) {
				CFDictionarySetValue(mutableDict, kNetFSMountedByGuestKey, kCFBooleanTrue);
			}
			else
				syslog(LOG_ERR, "%s: not set to guest and user is NULL!", __FUNCTION__);
		}		
	}

	// We need a dictionary to pass credentials to the webdav lib.
	serverCredsDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (serverCredsDict == NULL) {
		syslog(LOG_ERR, "%s: No mem for serverCredsDictionary", __FUNCTION__);
		error = ENOMEM;
		goto out;
	}
	
	checkAuth = FALSE;
	checkProxyServerOnly = FALSE;
	if (proxyCredsDict != NULL ) {
		/* Authenticate with a proxy server */
		syslog(LOG_DEBUG, "%s: authenticating with proxy server", __FUNCTION__);
		
		/* release old proxy credentials */
		CFReleaseNull(ctx->ct_proxy_user);
		CFReleaseNull(ctx->ct_proxy_pass);
		
		/* Fetch the new proxy credentials */
		ctx->ct_proxy_user = CFDictionaryGetValue(proxyCredsDict, kNetFSUserNameKey);
		ctx->ct_proxy_pass = CFDictionaryGetValue(proxyCredsDict, kNetFSPasswordKey);
		
		if (ctx->ct_proxy_user != NULL)
			CFRetain(ctx->ct_proxy_user);
		if (ctx->ct_proxy_pass != NULL)
			CFRetain(ctx->ct_proxy_pass);
		
		if (ctx->ct_proxy_user == NULL || ctx->ct_proxy_pass == NULL) {
			syslog(LOG_ERR, "%s: missing proxy credentials", __FUNCTION__);
		}
		else {
			CFDictionarySetValue(serverCredsDict, kWebDAVLibProxyUserNameKey, ctx->ct_proxy_user);
			CFDictionarySetValue(serverCredsDict, kWebDAVLibProxyPasswordKey, ctx->ct_proxy_pass);
			checkProxyServerOnly = TRUE;
			checkAuth = TRUE;
		}
	}
	else if (UseGuest == FALSE) {
		/* Authenticate with the server */
		syslog(LOG_DEBUG, "%s: authenticating with destination server", __FUNCTION__);
		if (ctx->ct_user == NULL || ctx->ct_pass == NULL) {
			syslog(LOG_ERR, "%s: missing server credentials", __FUNCTION__);
		}
		else {
			CFDictionarySetValue(serverCredsDict, kWebDAVLibUserNameKey, ctx->ct_user);
			CFDictionarySetValue(serverCredsDict, kWebDAVLibPasswordKey, ctx->ct_pass);
			
			/*
			 * If a proxy server is in place, we must add proxy server credentials
			 * from the previous call
			 */
			if (ctx->ct_proxy_user != NULL && ctx->ct_proxy_pass != NULL) {
				CFDictionarySetValue(serverCredsDict, kWebDAVLibProxyUserNameKey, ctx->ct_proxy_user);
				CFDictionarySetValue(serverCredsDict, kWebDAVLibProxyPasswordKey, ctx->ct_proxy_pass);				
			}
			checkAuth = TRUE;
		}
	}
	else
		syslog(LOG_DEBUG, "%s: guest authentication", __FUNCTION__);

	if (checkAuth == TRUE) {
		// Remove user name and password from the URL
		// before passing to webdavlib
		a_url = copyStripUserPassFromCFURL(in_URL);
		
		if (cfurl_is_idisk(a_url)) {
			// iDisk server - we require credentials to be sent securely
			authStat = connectToServer(a_url, serverCredsDict, TRUE, &error);
		}
		else
			authStat = connectToServer(a_url, serverCredsDict, FALSE, &error);

		CFReleaseNull(a_url);
		
		switch (authStat) {
			case WEBDAVLIB_Success:
				error = 0;
				break;
			case WEBDAVLIB_ProxyAuth:
				if (checkProxyServerOnly != TRUE) {
					// This is unexpected because we already authenticated with
					// the proxy server on the previous call
					syslog(LOG_ERR, "%s: Unexpected 407 status when checking server", __FUNCTION__);
				}
				break;
			case WEBDAVLIB_ServerAuth:
				if (checkProxyServerOnly == TRUE) {
					// Success, we got through the proxy server
					syslog(LOG_DEBUG, "%s: Success, we got through the proxy server", __FUNCTION__);
					error = 0;
				}
				break;
			default:
				syslog(LOG_DEBUG, "%s: connectToServer returned %d, error %d", __FUNCTION__, authStat, error);
				break;
		}
	}

	CFReleaseNull(serverCredsDict);
out:
	if (mutableDict != NULL) {
		if (error == 0) {
			// Only return mutableDict if there wasn't an error
			*out_SessionInfo = mutableDict;
		}
		else
			CFRelease(mutableDict);
	}

    return error;
}


/*
 * WebDAV_EnumerateShares
 *
 * WebDAV doesn't have a concept of "shares", so just return ENOTSUP. 
 */
netfsError WebDAV_EnumerateShares(void *in_SessionRef,
								  CFDictionaryRef in_EnumerateOptions,
								  CFDictionaryRef *out_Sharepoints)
{
#pragma unused(in_SessionRef, in_EnumerateOptions, out_Sharepoints)
    return ENOTSUP;
}

/*
 * WebDAV_Mount
 *
 * Convert CF* into c strings and pass onto WebDAVMountURL
 */
netfsError WebDAV_Mount(void *in_SessionRef,
						CFURLRef in_URL,
						CFStringRef in_Mountpoint,
						CFDictionaryRef in_MountOptions,
						CFDictionaryRef *out_MountInfo)
{
    struct webdav_ctx* ctx = in_SessionRef;
    char mountpoint[MAXPATHLEN];
    char* url = NULL;
    char* username = NULL;
    char* password = NULL;
    char* proxy_username = NULL;
    char* proxy_password = NULL;	
	CFURLRef tmpurl = NULL;
    CFNumberRef numRef = NULL;
	CFStringRef cfpassword = NULL;
	CFDataRef dataref = NULL;
    CFMutableDictionaryRef mutableDict = NULL;
    u_int32_t mntflags;
    int error = 0;
    
    *out_MountInfo = NULL;
    
    if (in_SessionRef == NULL || in_URL == NULL || in_Mountpoint == NULL) {
		syslog(LOG_ERR, "%s: invalid arguments!", __FUNCTION__);
		return EINVAL;
    }
	
	/*
	 * webdavfs_agent rejects any url with a user or pass.
	 */
	tmpurl = copyStripUserPassFromCFURL(in_URL);
	if (tmpurl == NULL) {
		syslog(LOG_ERR, "%s: copyStripUserPassFromCFURL failed!", __FUNCTION__);
		return EIO;
	}

	/*
     * Normally you would have to retain a CFString returned from CFURLGetString,
     * but we don't here since it's copied out immediately.
     */
    url = NetFSCFStringtoCString(CFURLGetString(tmpurl));
	CFRelease(tmpurl);
    if (url == NULL) {
		syslog(LOG_ERR, "%s:  NetFSCFStringtoCString failed!", __FUNCTION__);
		return ENOMEM;
    }
    
	if (CFStringGetCString(in_Mountpoint, mountpoint, sizeof(mountpoint), kCFStringEncodingUTF8) == false) {
		syslog(LOG_ERR, "%s:  CFStringGetCString failed!", __FUNCTION__);
		error = EINVAL;
		goto out;
    }
	
    if (ctx->ct_user != NULL) {
		username = NetFSCFStringtoCString(ctx->ct_user);
		if (username == NULL) {
			error = ENOMEM;
			goto out;
		}
		/*
		 * Only need to check for password if username is valid.  You can't have
		 * a password without a username.
		 */
		
		/*
		 * First see if they passed in a password in the mount options dictionary.
		 */
		if (in_MountOptions) {
			dataref = (CFDataRef)CFDictionaryGetValue(in_MountOptions, kNetFSPasswordKey);
			if (dataref != NULL) {
				cfpassword = CFStringCreateFromExternalRepresentation(NULL, dataref, kCFStringEncodingUTF8);
			}
		}

		/* 
		 * if cfpassword is still NULL, there was no password stored in the mount
		 * options dictionary or there was a failure during conversion.
		 */
		if (cfpassword == NULL && ctx->ct_pass != NULL) {
			/* We create a copy so that the this and the above code path
			 * and this path both allocate memory.
			 */
			cfpassword =  CFStringCreateCopy(NULL, ctx->ct_pass);
		}
		/* convert cfpassword if we have it */
		if (cfpassword != NULL) {
			password = NetFSCFStringtoCString(cfpassword);
			if (password == NULL) {
				error = ENOMEM;
				goto out;
			}
		}
    }
	
	// Check if we have proxy server credentials
    if (ctx->ct_proxy_user != NULL) {
		proxy_username = NetFSCFStringtoCString(ctx->ct_proxy_user);

		if (ctx->ct_proxy_pass != NULL)
			proxy_password = NetFSCFStringtoCString(ctx->ct_proxy_pass);
		
		if (proxy_username == NULL || proxy_password == NULL) {
			syslog(LOG_ERR, "%s:  No mem for proxy credentials", __FUNCTION__);

			if (proxy_username != NULL)
				free(proxy_username);
			if (proxy_password != NULL)
				free(proxy_password);
			
			proxy_username = NULL;
			proxy_password = NULL;
		}
	}
	
    mutableDict = CFDictionaryCreateMutable(NULL, 0, 
											&kCFTypeDictionaryKeyCallBacks, 
											&kCFTypeDictionaryValueCallBacks);
    if (mutableDict == NULL) {
		error = EIO;
		goto out;
    }
    
    mntflags = 0;
    if (in_MountOptions) {
		numRef = (CFNumberRef)CFDictionaryGetValue(in_MountOptions, kNetFSMountFlagsKey);
		if (numRef)
			(void)CFNumberGetValue(numRef, kCFNumberSInt32Type, &mntflags);
    }

	/*
	 * Do the work of mounting.
	 */ 
    error = WebDAVMountURL(url, username, password, proxy_username, proxy_password, mountpoint, sizeof(mountpoint), mntflags);
    
#if DEBUG_TRACE
    if (error)
		syslog(LOG_ERR, "%s: MountURLWithAuthentication exited with error: %d", __FUNCTION__, error);
#endif 
    
    if (error == 0 || error == EEXIST) {
		/* 
		 * Mount succeeded or was already mounted.
		 *
		 * Create a CFString of the mountpoint, this could be the mountpoint passed
		 * in or WebDAVMountURL may have modified mountpoint with the existing location.
		 */
		if (error == EEXIST) {
			CFStringRef mountPath = NULL;
			mountPath = CFStringCreateWithCString(NULL, mountpoint, kCFStringEncodingUTF8);
			if (mountPath == NULL) {
				/*
				 * If CFStringCreateWithCString fails, we don't umount(2)
				 * since the volume was already mounted before we were called.
				 */
				syslog(LOG_ERR, "%s:  CFStringCreateWithCString failed!", __FUNCTION__);
				error = ENOMEM;
				goto out;
			}
			CFDictionarySetValue(mutableDict, kNetFSMountPathKey, mountPath);
			CFRelease(mountPath);
 		}
		else {
			CFDictionarySetValue(mutableDict, kNetFSMountPathKey, in_Mountpoint);
		}

		if (ctx->ct_user != NULL) {
			CFDictionarySetValue(mutableDict, kNetFSMountedByUserKey, ctx->ct_user);
		}
		*out_MountInfo = mutableDict;
    }

out:
    if (url)
		free(url);
    if (username)
		free(username);
    if (password)
		free(password);
    if (proxy_username)
		free(proxy_username);
    if (proxy_password)
		free(proxy_password);	
	if (cfpassword) {
		CFRelease(cfpassword);
		cfpassword = NULL;
	}
    
    return error;
}


/*
 * WebDAV_Cancel
 *
 * Anything we could cancel is handled in webdavfs_agent.
 */
netfsError WebDAV_Cancel(void *in_SessionRef)
{
#pragma unused(in_SessionRef)
    return 0;
}

/*
 * WebDAV_CloseSession
 *
 * Free all context information.
 */
netfsError WebDAV_CloseSession(void *in_SessionRef)
{
    struct webdav_ctx* ctx = in_SessionRef;
	
	if (in_SessionRef == NULL) {
		/* XXX ERR or DEBUG? */
		syslog(LOG_ERR, "%s: in_SessionRef is NULL", __FUNCTION__);
		return EINVAL;
	}
    
    WebDAV_Cancel(in_SessionRef);
    pthread_mutex_destroy(&ctx->mutex);
	CFReleaseNull(ctx->ct_user);
	CFReleaseNull(ctx->ct_pass);
	CFReleaseNull(ctx->ct_proxy_user);
	CFReleaseNull(ctx->ct_proxy_pass);
	CFReleaseNull(ctx->ct_url);

	free(in_SessionRef);
    return 0;
}

/*
 * WebDAV_GetMountInfo
 *
 * stat the mount point and return relevant info.
 */
netfsError WebDAV_GetMountInfo(CFStringRef in_mountpoint, CFDictionaryRef *out_MountInfo)
{
    CFStringRef urlString = NULL;
    CFMutableDictionaryRef	mutableDict = NULL;
    char *mountpath = NULL;
    struct statfs statbuf;
    int error = 0;
    
    *out_MountInfo = NULL;
    mutableDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
											&kCFTypeDictionaryValueCallBacks);
    if (mutableDict == NULL)
		return ENOMEM;
    
    mountpath = NetFSCFStringtoCString(in_mountpoint);
    if (mountpath == NULL) {
		CFRelease(mutableDict);
		return ENOMEM;
    }
    
    if (statfs(mountpath, &statbuf) == -1) {
		error = errno;
		CFRelease(mutableDict);
		free(mountpath);
		return error;
    }
    free(mountpath);
    
    urlString = CFStringCreateWithCString(NULL, statbuf.f_mntfromname, kCFStringEncodingUTF8);
    if (urlString == NULL) {
		CFRelease(mutableDict);
		return ENOMEM;
    }
    
    /* kNetFSMountedURLKey is the only key listed By Guy, though SMB sets
     * kNetFSMountPathKey as well as kNetFSMountedby*Key.
     *
     * Since we don't really change behavior based on MountedBy*, we're not
     * going to worry about that value here.
     */
    CFDictionarySetValue(mutableDict, kNetFSMountedURLKey, urlString);
    
    *out_MountInfo = mutableDict;
    CFRelease(urlString);
    return 0;
}

/******************************************************************************/
/* End NetFS Implementation */
/******************************************************************************/


static NetFSMountInterface_V1 gWebDAVNetFSInterfaceFTbl = {
	NULL,
	NetFSQueryInterface,		/* IUNKNOWN_C_GUTS: QueryInterface */
	NetFSInterface_AddRef,		/* IUNKNOWN_C_GUTS: AddRef */
	NetFSInterface_Release,		/* IUNKNOWN_C_GUTS: Release */
	WebDAV_CreateSessionRef,		/* CreateSessionRef */
	WebDAV_GetServerInfo,		/* GetServerInfo */
	WebDAV_ParseURL,			/* ParseURL */
	WebDAV_CreateURL,			/* CreateURL */
	WebDAV_OpenSession,			/* OpenSession */
	WebDAV_EnumerateShares,		/* EnumerateShares */
	WebDAV_Mount,			/* Mount */
	WebDAV_Cancel,			/* Cancel */
	WebDAV_CloseSession,		/* CloseSession */
	WebDAV_GetMountInfo, 		/* GetMountInfo */
};


/* WebDAV URLMounter factory ID: F1821BFB-F659-11D5-AB4B-003065A0E6DE */
#define kWebDAVURLMounterFactoryID CFUUIDGetConstantUUIDWithBytes(NULL, 0xf1, 0x82, 0x1b, 0xfb, 0xf6, 0x59, 0x11, 0xd5, 0xab, 0x4b, 0x00, 0x30, 0x65, 0xa0, 0xe6, 0xde)

void *
WebDAVURLMounterFactory(__unused CFAllocatorRef allocator, CFUUIDRef typeID)
{
    if (CFEqual(typeID, kNetFSTypeID)) {
		NetFSInterface *result = NetFS_CreateInterface(kWebDAVURLMounterFactoryID, &gWebDAVNetFSInterfaceFTbl);
		return result;
    } else {
		return NULL;
    }
}
