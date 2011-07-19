/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/openpam.h>

#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <pwd.h>
#include <utmpx.h>

#include <CoreFoundation/CoreFoundation.h>
#include <OpenDirectory/OpenDirectory.h>
#include <NetFS/URLMount.h>
#include <DiskImages/DIHLFileVaultInterface.h>
#include <DiskImages/DIFrameworkUtilities.h>

#include "Common.h"

#define PM_DISPLAY_NAME "mount"

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	static const char password_prompt[] = "Password:";
	char *authenticator = NULL;

	if (PAM_SUCCESS != pam_get_authtok(pamh, PAM_AUTHTOK, (void *)&authenticator, password_prompt)) {
		openpam_log(PAM_LOG_DEBUG, "Unable to obtain the authtok.");
		return PAM_IGNORE;
	}
	if (PAM_SUCCESS != pam_setenv(pamh, "mount_authenticator", authenticator, 1)) {
		openpam_log(PAM_LOG_DEBUG, "Unable to set the authtok in the environment.");
		return PAM_IGNORE;
	}

	return PAM_SUCCESS;
}


PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}


PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	int retval = PAM_SUCCESS;
	char *server_URL = NULL;
	char *path = NULL;
	char *homedir = NULL;
	int mountdirlen = PATH_MAX;
	char *mountdir = NULL;
	const char *username = NULL;
	const char *authenticator = NULL;
	struct passwd *pwd = NULL;
	struct passwd pwdbuf;
	char pwbuffer[2 * PATH_MAX];
	unsigned int was_remounted = 0;

	/* get the username */
	if (PAM_SUCCESS != (retval = pam_get_user(pamh, &username, NULL))) {
		openpam_log(PAM_LOG_ERROR, "Unable to get the username: %s", pam_strerror(pamh, retval));
		goto fin;
	}
	if (username == NULL || *username == '\0') {
		openpam_log(PAM_LOG_ERROR, "Username is invalid.");
		retval = PAM_PERM_DENIED;
		goto fin;
	}

	/* get the uid */
	if (0 != getpwnam_r(username, &pwdbuf, pwbuffer, sizeof(pwbuffer), &pwd) || NULL == pwd) {
		openpam_log(PAM_LOG_ERROR, "Unknown user \"%s\".", username);
		retval = PAM_SYSTEM_ERR;
		goto fin;
	}

	/* get the authenticator */
	if (NULL == (authenticator = pam_getenv(pamh, "mount_authenticator"))) {
		openpam_log(PAM_LOG_DEBUG, "Unable to retrieve the authenticator.");
		retval = PAM_IGNORE;
		goto fin;
	}

	/* get the server_URL, path and homedir from OD */
	if (PAM_SUCCESS != (retval = od_extract_home(username, &server_URL, &path, &homedir))) {		
		openpam_log(PAM_LOG_ERROR, "Error retrieve data from OpenDirectory: %s", pam_strerror(pamh, retval));
		goto fin;
	}

	openpam_log(PAM_LOG_DEBUG, "           UID: %d", pwd->pw_uid);
	openpam_log(PAM_LOG_DEBUG, "    server_URL: %s", server_URL);
	openpam_log(PAM_LOG_DEBUG, "          path: %s", path);
	openpam_log(PAM_LOG_DEBUG, "       homedir: %s", homedir);
	openpam_log(PAM_LOG_DEBUG, "      username: %s", username);
	//openpam_log(PAM_LOG_DEBUG, " authenticator: %s", authenticator);  // We don't want to log user's passwords.

	/* determine if we need to mount the home folder */
	// this triggers the automounting for nfs
	if (0 == access(homedir, F_OK) || EACCES == errno) {
		openpam_log(PAM_LOG_DEBUG, "The home folder share is already mounted.");
	}
	if (NULL == (mountdir = malloc(mountdirlen+1))) {
		openpam_log(PAM_LOG_DEBUG, "Failed to malloc the mountdir.");
		retval = PAM_IGNORE;
		goto fin;
	}

	/* mount the home folder */
	if (NULL != server_URL && retval == PAM_SUCCESS) {
		// for an afp or smb home folder
		if (NULL != path) {
			if (0 != NetFSMountHomeDirectoryWithAuthentication(server_URL,
									   homedir,
									   path,
									   pwd->pw_uid,
									   mountdirlen,
									   mountdir,
									   username,
									   authenticator,
									   kNetFSAllowKerberos,
									   &was_remounted)) {
				openpam_log(PAM_LOG_DEBUG, "Unable to mount the home folder.");
				retval = PAM_SESSION_ERR;
				goto fin;
			}
			else {
				if (0 != was_remounted) {
					openpam_log(PAM_LOG_DEBUG, "Remounted home folder.");
				}
				else {
					openpam_log(PAM_LOG_DEBUG, "Mounted home folder.");
				}
				/* cache the homedir and path for close_session */
				pam_set_data(pamh, "homedir", homedir, openpam_free_data);
				pam_set_data(pamh, "path", path, openpam_free_data);
				homedir = NULL;
				path = NULL;
			}
		}
		// for a FileVault home folder
		if (NULL == path && NULL != homedir) {
			CFStringRef password = CFStringCreateWithCString(NULL, authenticator, kCFStringEncodingUTF8);
			CFStringRef url = CFStringCreateWithCString(NULL, server_URL, kCFStringEncodingUTF8);
			CFURLRef dmgin = CFURLCreateWithString(NULL, url, NULL);
			CFStringRef mountpoint = CFStringCreateWithCString(NULL, homedir, kCFStringEncodingUTF8);
			CFStringRef devicepath = NULL;
			CFStringRef mountpath = NULL;

			if (0 != DIHLFVMount(dmgin, kDIHLFVCredUserPasswordType, password, mountpoint, NULL, NULL, NULL, &mountpath, &devicepath)) {
				openpam_log(PAM_LOG_ERROR, "Unable to mount the FileVault home folder.");
				retval = PAM_SESSION_ERR;
			}
			else {
				openpam_log(PAM_LOG_DEBUG, "Mounted FileVault home folder.");
				pam_set_data(pamh, "devicepath", (void *)devicepath, pam_cf_cleanup);
			}
			CFRelease(url);			
			CFRelease(password);
			CFRelease(dmgin);
			CFRelease(mountpoint);
		}
	}
	else {
		// skip unmount for local homes
		pam_set_data(pamh, "path", strdup(""), openpam_free_data);
	}


fin:
	pam_unsetenv(pamh, "mount_authenticator");
	free(homedir);
	free(mountdir);
	free(server_URL);
	free(path);

	return retval;
}


PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	int retval = PAM_SUCCESS;
	const char *username;
	char *server_URL = NULL;
	char *path = NULL;
	char *homedir = NULL;
	CFStringRef devicepath = NULL;
	struct passwd *pwd = NULL;
	struct passwd pwdbuf;
	char pwbuffer[2 * PATH_MAX];
	CFStringRef cfhomedir = NULL;
	CFURLRef home_url = NULL;
	DASessionRef dasession = NULL;
	DADiskRef da = NULL;
	const char *devnode = NULL;
	
	/* get the username */
	retval = pam_get_user(pamh, &username, NULL);
	if (retval != PAM_SUCCESS) {
		openpam_log(PAM_LOG_ERROR, "Unable to get the username: %s", pam_strerror(pamh, retval));
		return retval;
	}
	if (username == NULL || *username == '\0') {
		openpam_log(PAM_LOG_ERROR, "Username is invalid.");
		return PAM_PERM_DENIED;
	}

	/* determine if we need to unmount the home folder */
	struct utmpx *x;
	setutxent();
	while (NULL != (x = getutxent())) {
		if (USER_PROCESS == x->ut_type && 0 == strcmp(x->ut_user, username) && x->ut_pid != getpid()) {
			openpam_log(PAM_LOG_DEBUG, "User is still logged in elsewhere (%s), skipping home folder unmount.", x->ut_line);
			return PAM_IGNORE;
		}
	}
	endutxent();

	/* try to retrieve the cached devicepath */
	if (PAM_SUCCESS != pam_get_data(pamh, "devicepath", (void *)&devicepath)) {
		openpam_log(PAM_LOG_DEBUG, "No cached devicepath in the PAM context.");
	}
	if (NULL != devicepath) {
		if (NULL == (devicepath = CFStringCreateCopy(kCFAllocatorDefault, devicepath))) {
			openpam_log(PAM_LOG_ERROR, "Failed to duplicate the devicepath.");
			retval = PAM_BUF_ERR;
			goto fin;
		}
	}

	/* try to retrieve the cached homedir */
	if (PAM_SUCCESS != pam_get_data(pamh, "homedir", (void *)&homedir)) {
		openpam_log(PAM_LOG_DEBUG, "No cached homedir in the PAM context.");
	}
	if (NULL != homedir) {
		if (NULL == (homedir = strdup(homedir))) {
			openpam_log(PAM_LOG_ERROR, "Failed to duplicate the homedir.");
			retval = PAM_BUF_ERR;
			goto fin;
		}
	}

	/* try to retrieve the cached path */
	if (PAM_SUCCESS != pam_get_data(pamh, "path", (void *)&path)) {
		openpam_log(PAM_LOG_DEBUG, "No cached path in the PAM context.");
	}
	if (NULL != path) {
		if (NULL == (path = strdup(path))) {
			openpam_log(PAM_LOG_ERROR, "Failed to duplicate the path.");
			retval = PAM_BUF_ERR;
			goto fin;
		}
	}

	/* skip unmount for local homes */
	if (NULL != path && 0 == strcmp("", path)) {
		openpam_log(PAM_LOG_DEBUG, "Skipping unmount.");
		goto fin;
	}

	/* get the homedir and path or devicepath if needed */
	if ((NULL == homedir || NULL == path) && NULL == devicepath) {
		if (PAM_SUCCESS != (retval = od_extract_home(username, &server_URL, &path, &homedir))) {
			openpam_log(PAM_LOG_ERROR, "Error retrieve data from OpenDirectory: %s", pam_strerror(pamh, retval));
			goto fin;
		}
		if (NULL != server_URL && NULL == path && NULL != homedir) {
			openpam_log(PAM_LOG_DEBUG, "Constructing the FileVault home path.");
			if ((NULL == (cfhomedir = CFStringCreateWithCString(kCFAllocatorDefault, homedir, kCFStringEncodingUTF8))) ||
				(NULL == (home_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfhomedir, kCFURLPOSIXPathStyle, true))) ||
				(NULL == (dasession = DASessionCreate(kCFAllocatorDefault))) ||
				(NULL == (da = DADiskCreateFromVolumePath(kCFAllocatorDefault, dasession, home_url))) ||
				(NULL == (devnode = DADiskGetBSDName(da))) ||
				(NULL == (devicepath = CFStringCreateWithCString(kCFAllocatorDefault, devnode, kCFStringEncodingUTF8)))) {
				openpam_log(PAM_LOG_ERROR, "Unable to to construct the FileVault home path.");
				retval = PAM_SYSTEM_ERR;
				goto fin;
			}
		}
	}

	/* attempt to unmount the home folder */
	if (NULL == devicepath && NULL != homedir && NULL != path) {
		// not FileVault
		if (0 != getpwnam_r(username, &pwdbuf, pwbuffer, sizeof(pwbuffer), &pwd) || NULL == pwd) {
			openpam_log(PAM_LOG_ERROR, "Unknown user \"%s\".", username);
			retval = PAM_SYSTEM_ERR;
			goto fin;
		}
		if (0 != NetFSUnmountHomeDirectory(homedir, path, pwd->pw_uid, 0)) {
			openpam_log(PAM_LOG_DEBUG, "Unable to unmount the home folder: %s.", strerror(errno));
			retval = PAM_IGNORE;
		}
		else {
			openpam_log(PAM_LOG_DEBUG, "Unmounted home folder.");
		}
	}
	else if (NULL != devicepath && NULL != homedir && NULL == path) {
		// FileVault
		if (0 != (retval = DIHLFVUnmount(devicepath, kDIHLFVUnmountNormally, kDIHLFVUnmountNoTimeout))) {
			openpam_log(PAM_LOG_DEBUG, "Unable to unmount the FileVault home folder: %s.", DIStrError(retval));
			retval = PAM_IGNORE;
		}
		else {
			openpam_log(PAM_LOG_DEBUG, "Unmounted FileVault home folder.");
		}
	}
	else {
		// nothing
		openpam_log(PAM_LOG_DEBUG, "There is nothing to unmount.");
		retval = PAM_IGNORE;
	}

fin:
	free(server_URL);
	free(path);
	free(homedir);

	if (home_url)
		CFRelease(home_url);
	if (cfhomedir)
		CFRelease(cfhomedir);
	if (dasession)
		CFRelease(dasession);
	if (da)
		CFRelease(da);
	if (devicepath)
		CFRelease(devicepath);

	return retval;
}
