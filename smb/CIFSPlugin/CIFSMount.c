/*
 * Copyright (c) 1999 - 2007 Apple Inc. All rights reserved.
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

#include <CoreServices/CoreServices.h>

#include <URLMount/URLMount.h>
#include <URLMount/URLMountPrivate.h>
#include <URLMount/URLMountPlugin.h>
#include "smb_netfs.h"

#include <syslog.h>

#define CIFSMediaType ((VolumeType)('cifs'))
#define CIFSURLSCHEME "cifs"

static int CIFS_AttemptMount(const char *urlPtr,
			     const char *mountPointPtr,
			     const char *username,
			     const void *authenticator,
			     size_t authenticatorlength,
			     MountProgressHandle **progressHandle,
			     u_int32_t options,
			     u_int32_t internalFlags);
static int writestring(int fd, const char *string, size_t stringlen);
static size_t CIFS_RemountInfoSize(struct statfs64 *mountpointinfo);

#define CIFS_MOUNT_COMMAND "/sbin/mount_cifs"
#define SMBFS_MOUNT_COMMAND "/sbin/mount_smbfs"
#define CIFS_PREFIX "cifs://"
#define SMB_PREFIX "smb://"

#define UNIQUEDIGITS 5
#define SLASH '/'
#define BACKSLASH '\\'

#define REMOUNTINFOMAJORVERSION 1
#define REMOUNTINFOMINORVERSION 0

enum {
	kMountWithoutUI = 0x00000001
};

#if PRAGMA_STRUCT_ALIGN
    #pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
    #pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
    #pragma pack(2)
#endif

struct CIFSRemountInfoHeader {
	unsigned char majorVersion;
	unsigned char minorVersion;
	unsigned char reserved;
	unsigned char flags;
};

struct CIFSRemountInfoRecord {
	struct CIFSRemountInfoHeader header;
	char urlstring[0];
};

#if PRAGMA_STRUCT_ALIGN
    #pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
    #pragma pack(pop)
#elif PRAGMA_STRUCT_PACK
    #pragma pack()
#endif

static char gTargetPrefix[] = "//";

static URLScheme gCIFSURLScheme = CIFSURLSCHEME;

/* CIFS URLMount factory ID: 92D4EFEF-F5AA-11D5-A1EE-003065A0E6DE */
#define kCIFSURLMounterFactoryID (CFUUIDGetConstantUUIDWithBytes(NULL, 0x92, 0xd4, 0xef, 0xef, 0xf5, 0xaa, 0x11, 0xd5, 0xa1, 0xee, 0x00, 0x30, 0x65, 0xa0, 0xe6, 0xde))



static int
CIFS_MountURL (void *this,
	       const char *url,
	       const char *mountdir,
	       size_t maxlength,
	       char *mountpoint,
	       const char *username,
	       const void *authenticator,
	       size_t authenticatorlength,
	       MountProgressHandle **progressHandle,
	       u_int32_t options,
	       u_int32_t internalFlags)
{
	#pragma unused(this)
	const char * namestart;
	const char * urlstring;
	const char * lastnamestart;
	char * slash;
	char *at;
	char basename[MAXNAMLEN];
	char uniquename[MAXNAMLEN];
	char realmountdir[MAXPATHLEN];
	int error;
	size_t namelen;

#if DEBUG_TRACE
	syslog(LOG_INFO, "CIFS_MountURL: Mounting '%s' in '%s' (uid = %ld)...\n", url, mountdir, geteuid());
#endif

	/* Find the real name of mountdir, by removing all ".", ".."
	 * and symlink components.
	 * Any extra "/" characters (including any trailing "/") are removed.
	 */
	if (realpath(mountdir, realmountdir) == NULL) {
		error = errno;
		goto errorexit;
	}

	/* Check for obvious error cases first: */
	if ((strlen(realmountdir)) + 1 > maxlength) {
		error = ENAMETOOLONG;
		goto errorexit;
	}
		
	/* First pick out a name we can use */

	namestart = url;

	if ((strncasecmp (url, CIFS_PREFIX, sizeof(CIFS_PREFIX)-1) == 0) ||
		(strncasecmp (url, SMB_PREFIX, sizeof(SMB_PREFIX)-1) == 0)) {
		/* path starts with "cifs://" so lets skip that */
#if DEBUG_TRACE
		syslog(LOG_INFO, "CIFS_MountURL: stripping off URL scheme prefix...\n");
#endif
		slash = strchr(url, SLASH);
		if (slash) {
			namestart = slash + 1;
			while (*namestart == SLASH) {
				++namestart;
			}
		}
#if DEBUG_TRACE
		syslog(LOG_INFO, "CIFS_MountURL: resulting name = '%s'...\n", namestart);
#endif
	}

	/* Save the server URL: */
	urlstring = namestart;

	if (options & kMountAtMountdir) {
		/* Use the path specified */
		strcpy(mountpoint, realmountdir);	/* Copy the string now that the length's been checked */
	} else {
		/* Pick a suitable basename: the last name element that's not a slash: */
		
		lastnamestart = namestart;
		namelen = strlen(namestart);
		while ((namelen > 0) && (slash = strchr (namestart, SLASH))) {
#if DEBUG_TRACE
			syslog(LOG_DEBUG, "CIFS_MountURL: found name component starting with '/': %s...", slash);
#endif
			namelen -= (slash - namestart) + 1;
			namestart = slash + 1;
			if ((namelen > 0) && (*namestart != SLASH))
				lastnamestart = namestart;
#if DEBUG_TRACE
			syslog(LOG_DEBUG, "CIFS_MountURL: namestart = '%s'; lastnamestart = '%s'; namelen = %zu...", namestart, lastnamestart, namelen);
#endif
		}
		namestart = lastnamestart;
		
		/* Pick out the part up to, but not including, any trailing slash: */
		if (slash = strchr (namestart, SLASH)) {
			namelen = slash - namestart;
		} else {
			namelen = strlen(namestart);
		}
		
		/* Skip over any username:password@hostname strings that might be embedded: */
		at = strchr(namestart, '@');
		if (at && (at < (namestart + namelen))) {
			namelen -= (at - namestart) + 1;
			namestart = at + 1;
#if DEBUG_TRACE
			syslog(LOG_DEBUG, "CIFS_MountURL: removed auth. info; trimmed name = '%s', namelen = %zu...", namestart, namelen);
#endif
		}
		
		if (namelen + 1 > MAXNAMLEN) {
			return (ENAMETOOLONG);
		}

		(void) strncpy(basename, namestart, namelen);
		basename[namelen] = (char)0;
#if DEBUG_TRACE
		syslog(LOG_INFO, "CIFS_MountURL: basename is '%s' (length = %zu)...\n", basename, namelen);
#endif
	
		/* Now make the directory */
		if (error = URLM_CreateUniqueMountpointFromEscapedName(realmountdir, basename, 0700,
				MAXNAMLEN, uniquename, maxlength, mountpoint)) {
			return (error);
		}
#if DEBUG_TRACE
		syslog(LOG_INFO, "CIFS_MountURL: uniquename = '%s'...\n", uniquename);
#endif
	}
	
#if DEBUG_TRACE
	syslog(LOG_INFO, "CIFS_MountURL: CIFS_AttemptMount('%s', '%s')...\n", urlstring, mountpoint);
#endif
	if (error = CIFS_AttemptMount(urlstring, mountpoint, username,
	    authenticator, authenticatorlength, progressHandle, options,
	    internalFlags)) {
#if DEBUG_TRACE
		syslog(LOG_INFO, "CIFS_MountURL: CIFS_AttemptMount returned %d; deleting '%s'...\n", error, mountpoint);
#endif
		goto delete;
	};

	return(0);

delete:
	if ((options & kMountAtMountdir) == 0) {
		/* Delete the mountpoint if it was just created */
		URLM_RemoveMountpoint(mountpoint);
	}
		
errorexit:
	return(error);
}


static int
CIFS_MountCompleteURL (void *this,
		       const char* url,
		       const char* mountdir,
		       size_t maxlength,
		       char* mountpoint,
		       MountProgressHandle **progressHandle,
		       u_int32_t options)
{
	return CIFS_MountURL(this, url, mountdir, maxlength, mountpoint,
	    NULL, NULL, 0, progressHandle, options, kMountWithoutUI);
}


static int
CIFS_MountServerURL (void *this,
		     const char * url,
		     const char * mountdir,
		     size_t maxlength,
		     char * mountpoint,
		     MountProgressHandle **progressHandle,
		     u_int32_t options)
{
	return (CIFS_MountURL(this, url, mountdir, maxlength, mountpoint,
	    NULL, NULL, 0, progressHandle, options, 0));
}


static int
CIFS_MountURLWithAuthentication(void *this,
				const char *url,
				const char *mountdir,
				size_t maxlength,
				char *mountpoint,
				const char *authenticationdomain,
				const char *username,
				const char *authenticationmethod,
				const void *authenticator,
				size_t authenticatorlength,
				MountProgressHandle **progressHandle,
				u_int32_t options)
{
	#pragma unused(authenticationdomain, authenticationmethod)

	const char *url_scheme_end, *url_end;
	const char *password_begin, *password_end, *p;
	char c;
	ptrdiff_t password_len;
	size_t url_len;
	char *stripped_url;
	int ret;
	
	/*
	 * Ick.  Some callers might have included the user name and
	 * password in the URL, as well as in the arguments, because
	 * of 3906547.  They don't want the password showing up in
	 * the output of mount (or, I suspect, on the command line),
	 * so we'll strip out the password if it was also supplied
	 * as an argument.
	 */
	if (authenticator == NULL) {
		/*
		 * The password wasn't supplied as an argument; don't
		 * munge the URL.
		 */
		goto no_transform;
	}
	url_scheme_end = strchr(url, ':');
	if (url_scheme_end == NULL || url_scheme_end[1] != '/' ||
	    url_scheme_end[2] != '/') {
		/*
		 * The "URL" doesn't begin with "smb://" or "cifs://".
		 * Don't munge it.
		 */
		goto no_transform;
	}
	p = &url_scheme_end[3];
	password_begin = NULL;
	password_end = NULL;
	while ((c = *p) != '\0' && c != '/') {
		/*
		 * We're in the "authority" portion of the URL.
		 */
		if (c == ';') {
			/*
			 * We treat everything up to the last ';'
			 * in the authority portion as a domain
			 * name.  That means that nothing we've
			 * seen before this ';' is a password.
			 */
			password_begin = NULL;
			password_end = NULL;
		} else if (c == ':') {
			/*
			 * Anything after a ':' is presumably a password.
			 * We include the ':' in the password field,
			 * as we'll be stripping it out too.
			 */
			password_begin = p;
		} else if (c == '@') {
			/*
			 * We're at the end of the userinfo field.
			 * If we saw the beginning of a password,
			 * this is the end of the password.
			 */
			if (password_begin != NULL)
				password_end = p;

			/*
			 * In any case, there's nothing more to do;
			 * either we found the end of the password,
			 * or there is no password.
			 */
			break;
		}
		p++;
	}
	if (password_begin == NULL || password_end == NULL) {
		/*
		 * We didn't find a password, so there's nothing
		 * to remove.
		 */
		goto no_transform;
	}

	/*
	 * Make a copy of the URL, with the ':' and password stripped out.
	 */
	password_len = password_end - password_begin;
	url_len = strlen(url);
	url_end = url + strlen(url) + 1;	/* include the trailing '\0' */
	stripped_url = malloc(url_len - password_len + 1);
	memcpy(stripped_url, url, password_begin - url);
	memcpy(stripped_url + (password_begin - url), password_end,
	    url_end - password_end);
	ret = CIFS_MountURL(this, stripped_url, mountdir, maxlength, mountpoint,
	    username, authenticator, authenticatorlength, progressHandle,
	    options, kMountWithoutUI);
	free(stripped_url);
	return ret;

no_transform:

	return (CIFS_MountURL(this, url, mountdir, maxlength, mountpoint,
	    username, authenticator, authenticatorlength, progressHandle,
	    options, kMountWithoutUI));
}



static int
CIFS_UnmountServerURL(void *this, const char *mountpoint, u_int32_t options)
{
	#pragma unused(this, mountpoint, options)
	return 0;
}



static int
CIFS_GetCompleteMountURL (void *this, const char * mountpath, size_t maxlength, char * full_url, u_int32_t options)
{
	#pragma unused(this, options)
	struct statfs64 statbuf;
	int error;
	char *sharepointname;

	if (error = statfs64 (mountpath, &statbuf) == -1) {
		return(error);
	}

	sharepointname = statbuf.f_mntfromname;
	/* Skip all leading slashes and backslashes*/
	while ((*sharepointname == SLASH) || (*sharepointname == BACKSLASH)) {
		++sharepointname;
	}
	
	if (maxlength <= (sizeof(SMB_PREFIX) + strlen(sharepointname))) {
		return ENAMETOOLONG;
	}
	
	/* assert(strlen(sharepointname) + sizeof(CIFS_PREFIX) <= maxlength) */
	strcpy(full_url, SMB_PREFIX);
	full_url[maxlength - 1] = (char)0;
	strcat(full_url, sharepointname);

	return(0);
}


static int
CIFS_GetURLFromURLRemountInfo(void *this, URLRemountInfo *remountinfo,
								  size_t maxURLlength,
								  char *complete_URL,
								  u_int32_t options)
{
	#pragma unused(this, options)
	struct CIFSRemountInfoRecord *fsremountinfo = (struct CIFSRemountInfoRecord *)(&remountinfo->media_specific);

	/* make sure the CIFSRemountInfoRecord is what we expect */
	if ((remountinfo->media != CIFSMediaType) ||
	    (fsremountinfo->header.majorVersion > REMOUNTINFOMAJORVERSION) ||
	    (fsremountinfo->header.reserved != 0) ||
	    (fsremountinfo->header.flags != 0)) {
		return ( EINVAL );
	}
	
	/* make sure the complete_URL buffer is large enough */
	if (strlen(fsremountinfo->urlstring) >= maxURLlength) {
		return ( ENAMETOOLONG );
	}

	/* copy the urlstring */
	strcpy(complete_URL, fsremountinfo->urlstring);

	return ( 0 );
}

#define CFENVFORMATSTRING "__CF_USER_TEXT_ENCODING=0x%X:0:0"

static int
CIFS_AttemptMount(const char *urlPtr,
		  const char *mountPointPtr,
		  const char *username,
		  const void *authenticator,
		  size_t authenticatorlength,
		  MountProgressHandle **progressHandle,
		  u_int32_t options,
		  u_int32_t internalFlags)
{
	pid_t pid, terminated_pid;
	int result;
	union wait status;
	char *mounttarget;
	struct stat mntcmdinfo;
	const char *mount_command;
	char fdnum[11+1];
	int p_option = FALSE;
	int sp[2];
	
	if (progressHandle)
		*progressHandle = (MountProgressHandle)NULL;
	
	/* Skip any leading slashes and backslashes: */
	while ((*urlPtr == SLASH) || (*urlPtr == BACKSLASH)) {
		++urlPtr;
	}
	mounttarget = malloc(strlen(urlPtr) + sizeof(gTargetPrefix));	/* sizeof(gTargetPrefix) is 3 (includes terminating zero) */
	if (mounttarget == NULL) return (errno ? errno : ENOMEM);
	
	strcpy(mounttarget, gTargetPrefix);
	strcat(mounttarget, urlPtr);	/* assert(sizeof(mounttarget) - 2 == strlen(urlPtr)) */

	result = stat(CIFS_MOUNT_COMMAND, &mntcmdinfo);
	if (result == 0)
		mount_command = CIFS_MOUNT_COMMAND;
	else
		mount_command = SMBFS_MOUNT_COMMAND;

	if (username || authenticator) {
		/*
		 * Create a socket pair over which to pass the user name
		 * and/or password.
		 * We use socketpair() so we can set the SO_NOSIGPIPE
		 * option; we don't want whatever program is calling us
		 * to get a SIGPIPE if, for example, mount_smbfs exits
		 * before we can finish writing to the pipe.
		 */
		if (socketpair(PF_UNIX, SOCK_STREAM, 0, sp) == -1) {
			result = errno;
			goto Return;
		}
	}
	pid = fork();
	if (pid == 0) { 
		uid_t effective_uid;
		uid_t real_uid;
		char CFUserTextEncodingEnvSetting[sizeof(CFENVFORMATSTRING) + 20]; /* Extra bytes for expansion of %X uid field */
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
		 * Create a new environment with a definition of __CF_USER_TEXT_ENCODING to work around CF's interest in
		 * the user's home directory (which could be networked, causing recursive references through automount): 
		 * Make sure we include the uid since CF will check for this when deciding if to look in the home directory.
		 */
		snprintf(CFUserTextEncodingEnvSetting, sizeof(CFUserTextEncodingEnvSetting), CFENVFORMATSTRING, geteuid());
		
		if (username || authenticator) {
			p_option = TRUE;
			snprintf(fdnum, sizeof fdnum, "%d", sp[0]);
			/*
			 * Close the side of the pipe to which writes will
			 * be done.
			 */
			close(sp[1]);
		}
		
		if ((internalFlags & kMountWithoutUI) && p_option) {
			result = execle(mount_command, mount_command,
							"-o", (options & kMarkAutomounted) ? "automounted" : "noautomounted",
							"-o", (options & kMarkDontBrowse) ? "nobrowse" : "browse",
							"-N",
							"-p", fdnum,
							mounttarget, mountPointPtr, NULL, env);			
		} else if (internalFlags & kMountWithoutUI) {
			result = execle(mount_command, mount_command,
							"-o", (options & kMarkAutomounted) ? "automounted" : "noautomounted",
							"-o", (options & kMarkDontBrowse) ? "nobrowse" : "browse",
							"-N",
							mounttarget, mountPointPtr, NULL, env);			
		} else if (p_option) {
			result = execle(mount_command, mount_command,
							"-o", (options & kMarkAutomounted) ? "automounted" : "noautomounted",
							"-o", (options & kMarkDontBrowse) ? "nobrowse" : "browse",
							"-p", fdnum,
							mounttarget, mountPointPtr, NULL, env);			
		} else {
			result = execle(mount_command, mount_command,
							"-o", (options & kMarkAutomounted) ? "automounted" : "noautomounted",
							"-o", (options & kMarkDontBrowse) ? "nobrowse" : "browse",
							mounttarget, mountPointPtr, NULL, env);			
		}
		
		/* IF WE ARE HERE, WE WERE UNSUCCESSFUL */
		exit(result ? result : ECHILD);
	}

	if (pid == -1) {
		result = -1;
		close(sp[0]);
		close(sp[1]);
		goto Return;
	}

	/*
	 * Success!
	 * If we were give a user name or password, send them down the
	 * pipe.
	 */
	if (username || authenticator) {
		int on = 1;

		/*
		 * Close the side of the pipe from which reads will be done.
		 */
		close(sp[0]);

		/*
		 * Set SO_NOSIGPIPE on the side of the pipe to which
		 * we'll be writing.
		 */
		if (setsockopt(sp[1], SOL_SOCKET, SO_NOSIGPIPE, &on,
		    sizeof (on)) == -1) {
			result = errno;
			goto Return;
		}

		result = writestring(sp[1], username,
		    username == NULL ? 0 : strlen(username));
		if (result) {
			close(sp[1]);
			goto Return;
		}
		result = writestring(sp[1], authenticator, authenticatorlength);
		if (result) {
			close(sp[1]);
			goto Return;
		}

		/*
		 * Close the side to which we've written, as there's
		 * nothing more to write.
		 */
		close(sp[1]);
	}

	/*
	 * If a progress handle was given, initialize it and return;
	 * this is an async mount.
	 */
	if (progressHandle) {
		result = URLM_InitStdProgressHandle(progressHandle, pid, &gCIFSURLScheme, mounttarget, mountPointPtr, options);
		goto Return;
	}

#if DEBUG_TRACE
	syslog(LOG_INFO, "CIFS_AttemptMount: waiting for exit of process %ld...\n", pid);
#endif
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
	if (mounttarget) free(mounttarget);
	
	return result;
}

static int
writestring(int fd, const char *string, size_t stringlen)
{
	uint32_t stringlen_be;
	ssize_t bytes_written;

	if (string == NULL) {
		/*
		 * Write a special "no such string" marker.
		 */
		stringlen_be = 0xFFFFFFFF;
		bytes_written = write(fd, &stringlen_be, sizeof stringlen_be);
		if (bytes_written == -1)
			return (errno);
	} else {
		stringlen_be = htonl(stringlen);
		bytes_written = write(fd, &stringlen_be, sizeof stringlen_be);
		if (bytes_written == -1)
			return (errno);
		bytes_written = write(fd, string, stringlen);
		if (bytes_written == -1)
			return (errno);
	}
	return (0);
}

static int
CIFS_GetURLRemountInfoSize(void *this, const char *mountpoint, size_t *remountinfosize, u_int32_t options)
{
	#pragma unused(this, options)
	struct statfs64 mountinfo;
	int result;
	
	if ((result = statfs64 (mountpoint, &mountinfo)) != 0) return errno;
	
	*remountinfosize = CIFS_RemountInfoSize(&mountinfo);
		
	return 0;
}


static int
CIFS_GetURLRemountInfo(void *this, const char *mountpoint, size_t maxinfosize, URLRemountInfo *remountinfo, u_int32_t options)
{
	#pragma unused(this, options)
	struct statfs64 mountinfo;
	int result;
	struct CIFSRemountInfoRecord *fsremountinfo;
	size_t spaceremaining;
	char *sharepointname;
	
	if ((result = statfs64 (mountpoint, &mountinfo)) != 0) return errno;
	if (CIFS_RemountInfoSize(&mountinfo) > maxinfosize) return EOVERFLOW;
	
	spaceremaining = maxinfosize;
	
	if ( spaceremaining < offsetof(URLRemountInfo, media_specific) )
		return EOVERFLOW;
	
	remountinfo->length = CIFS_RemountInfoSize(&mountinfo);
	remountinfo->media = CIFSMediaType;
	remountinfo->flags = 0;
	fsremountinfo = (struct CIFSRemountInfoRecord *)(&remountinfo->media_specific);
	spaceremaining -= offsetof(URLRemountInfo, media_specific);
	
	if (spaceremaining < sizeof(fsremountinfo->header)) return EOVERFLOW;
	fsremountinfo->header.majorVersion = REMOUNTINFOMAJORVERSION;
	fsremountinfo->header.minorVersion = REMOUNTINFOMINORVERSION;
	fsremountinfo->header.reserved = 0;
	fsremountinfo->header.flags = 0;
	spaceremaining -= sizeof(fsremountinfo->header);
	
	if (spaceremaining < sizeof(CIFS_PREFIX)) return EOVERFLOW;
	strncpy(fsremountinfo->urlstring, CIFS_PREFIX, spaceremaining - 1);
	fsremountinfo->urlstring[spaceremaining - 1] = (char)0;
	spaceremaining -= sizeof(CIFS_PREFIX);
	
	sharepointname = mountinfo.f_mntfromname;
	/* Skip all leading slashes and backslashes*/
	while ((*sharepointname == SLASH) || (*sharepointname == BACKSLASH)) {
		++sharepointname;
	}
	if (spaceremaining <= strlen(sharepointname)) return EOVERFLOW;
	strncat(fsremountinfo->urlstring, sharepointname, spaceremaining);
	
	return 0;
}


static int
CIFS_RemountServerURL(void *this, URLRemountInfo *remountinfo, const char *mountdir, size_t maxlength, char *mountpoint, MountProgressHandle **progressHandle, u_int32_t options) {
	struct CIFSRemountInfoRecord *fsremountinfo = (struct CIFSRemountInfoRecord *)(&remountinfo->media_specific);

	if ((remountinfo->media != CIFSMediaType) ||
	    (fsremountinfo->header.majorVersion > REMOUNTINFOMAJORVERSION) ||
	    (fsremountinfo->header.reserved != 0) ||
	    (fsremountinfo->header.flags != 0)) {
		return EINVAL;
	}
	
	return CIFS_MountServerURL(this, fsremountinfo->urlstring, mountdir, maxlength, mountpoint, progressHandle, options);
}


static size_t
CIFS_RemountInfoSize(struct statfs64 *mountpointinfo)
{
	char *sharepointname;
	
	sharepointname = mountpointinfo->f_mntfromname;
	/* Skip all leading slashes and backslashes*/
	while ((*sharepointname == SLASH) || (*sharepointname == BACKSLASH)) {
		++sharepointname;
	}
		
	return sizeof(URLRemountInfo) +
	       sizeof(struct CIFSRemountInfoRecord) +
	       sizeof(CIFS_PREFIX) +
	       strlen(sharepointname) + 1;	/* Add 1 for terminating null */
};



/*
 *  GeneralURLMounter Type implementation:
 */
static URLMountGeneralInterface gCIFSGeneralInterfaceFTbl = {
	NULL,				/* IUNKNOWN_C_GUTS: _reserved */
	URLM_GeneralQueryInterface,	/* IUNKNOWN_C_GUTS: QueryInterface */
	URLM_URLMounter_AddRef,		/* IUNKNOWN_C_GUTS: AddRef */
	URLM_URLMounter_Release,	/* IUNKNOWN_C_GUTS: Release */
	CIFS_MountServerURL,		/* MountServerURL */
	CIFS_RemountServerURL		/* RemountServerURL */
};



/*
 *  DirectURLMounter Type implementation:
 */
static URLMountDirectInterface gCIFSDirectInterfaceFTbl = {
	NULL,				/* IUNKNOWN_C_GUTS: _reserved */
	URLM_DirectQueryInterface,	/* IUNKNOWN_C_GUTS: QueryInterface */
	URLM_URLMounter_AddRef,		/* IUNKNOWN_C_GUTS: AddRef */
	URLM_URLMounter_Release,	/* IUNKNOWN_C_GUTS: Release */
	CIFS_MountCompleteURL,		/* MountCompleteURL */
	CIFS_MountURLWithAuthentication,/* MountURLWithAuthentication */
	URLM_StdGetURLMountProgressInfo,/* GetURLMountProgressInfo */
	URLM_StdCancelURLMount,		/* CancelURLMount */
	URLM_StdCompleteURLMount,	/* CompleteURLMount */
	CIFS_UnmountServerURL,		/* UnmountServerURL */
	CIFS_GetURLRemountInfoSize,	/* GetURLRemountInfoSize */
	CIFS_GetURLRemountInfo,		/* GetURLRemountInfo */
	CIFS_GetCompleteMountURL,	/* GetCompleteMountURL */
	CIFS_GetURLFromURLRemountInfo	/* GetURLFromURLRemountInfo */
};


/*
 *  NetFS Type implementation:
 */
static NetFSInterface gCIFSNetFSInterfaceFTbl = {
    NULL,				/* IUNKNOWN_C_GUTS: _reserved */
    URLM_NetFSQueryInterface,		/* IUNKNOWN_C_GUTS: QueryInterface */
    URLM_URLMounter_AddRef,		/* IUNKNOWN_C_GUTS: AddRef */
    URLM_URLMounter_Release,		/* IUNKNOWN_C_GUTS: Release */
    SMB_CreateSessionRef,		/* CreateSessionRef */
    SMB_GetServerInfo,			/* GetServerInfo */
    SMB_ParseURL,			/* ParseURL */
    SMB_CreateURL,			/* CreateURL */
    SMB_OpenSession,			/* OpenSession */
    SMB_EnumerateShares,		/* EnumerateShares */
    SMB_Mount,				/* Mount */
    SMB_Cancel,				/* Cancel */
    SMB_CloseSession,			/* CloseSession */
};


void *
CIFSURLMounterFactory(CFAllocatorRef allocator, CFUUIDRef typeID)
{
	#pragma unused(allocator)
	if (CFEqual(typeID, kGeneralURLMounterTypeID)) {
		URLMounter *result = URLM_CreateURLMounter(kCIFSURLMounterFactoryID, &gCIFSGeneralInterfaceFTbl);
		return result;
	} else if (CFEqual(typeID, kDirectURLMounterTypeID)) {
		URLMounter *result = URLM_CreateURLMounter(kCIFSURLMounterFactoryID, &gCIFSDirectInterfaceFTbl);
		return result;
	} else if (CFEqual(typeID, kNetFSTypeID)) {
		URLMounter *result = URLM_CreateURLMounter(kCIFSURLMounterFactoryID, &gCIFSNetFSInterfaceFTbl);
		return result;
	} else {
		return NULL;
	}
}
