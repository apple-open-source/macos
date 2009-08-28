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
#include <OpenDirectory/OpenDirectoryPriv.h>
#include <DirectoryService/DirectoryService.h>
#include <NetFS/URLMount.h>
#include <DiskImages/DIHLFileVaultInterface.h>
#include <DiskImages/DIFrameworkUtilities.h>


static int
extract_homemount(CFStringRef val, char **in_server_URL, char **in_path)
{
	int retval = -1;
	CFIndex maxlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(val), kCFStringEncodingUTF8);
	char *buffer = malloc(maxlen+1);

	if (NULL == buffer)
		goto fin;
	if (!CFStringGetCString(val, buffer, maxlen+1, kCFStringEncodingUTF8))
		goto fin;

	// Directory Services people have assured me that this won't change
	static const char URL_OPEN[] = "<url>";
	static const char URL_CLOSE[] = "</url>";
	static const char PATH_OPEN[] = "<path>";
	static const char PATH_CLOSE[] = "</path>";

	char *server_URL;
	char *path;
	char *record_start = NULL;
	char *record_end = NULL;

	record_start = buffer;
	server_URL = strstr(record_start, URL_OPEN);
	if (NULL == server_URL)
		goto fin;
	server_URL += sizeof(URL_OPEN)-1;
	while ('\0' != *server_URL && isspace(*server_URL))
		server_URL++;
	record_end = strstr(server_URL, URL_CLOSE);
	if (NULL == record_end)
		goto fin;
	while (record_end >= server_URL && '\0' != *record_end && isspace(*(record_end-1)))
		record_end--;
	if (NULL == record_end)
		goto fin;
	*record_end = '\0';
	if (NULL == (*in_server_URL = strdup(server_URL)))
		goto fin;

	record_start = record_end+1;
	path = strstr(record_start, PATH_OPEN);
	if (NULL == path)
		goto ok;
	path += sizeof(PATH_OPEN)-1;
	while ('\0' != *path && isspace(*path))
		path++;
	record_end = strstr(path, PATH_CLOSE);
	if (NULL == record_end)
		goto fin;
	while (record_end >= path && '\0' != *record_end && isspace(*(record_end-1)))
		record_end--;
	if (NULL == record_end)
		goto fin;
	*record_end = '\0';
	if (NULL == (*in_path = strdup(path)))
		goto fin;

ok:
	retval = 0;
fin:
	free(buffer);
	return retval;
}


static int
extract_homedir(CFStringRef val, char **in_homedir)
{
	CFIndex maxlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(val), kCFStringEncodingUTF8);
	char *buffer = malloc(maxlen+1);

	if (NULL != buffer && CFStringGetCString(val, buffer, maxlen+1, kCFStringEncodingUTF8)) {
		*in_homedir = buffer;
		return 0;
	} else {
		free(buffer);
		return -1;
	}
}


static int
extract_od_values(const char *username, char **server_URL, char **path, char **homedir)
{
	CFArrayRef vals;
	int retval = PAM_SUCCESS;

	ODNodeRef cfNodeRef = ODNodeCreateWithNodeType(kCFAllocatorDefault, kODSessionDefault, eDSAuthenticationSearchNodeName, NULL);
	if (cfNodeRef != NULL) {
		CFStringRef cfUser = CFStringCreateWithCString(NULL, username, kCFStringEncodingUTF8);
		if (cfUser != NULL) {
			ODRecordRef cfRecord = ODNodeCopyRecord(cfNodeRef, CFSTR(kDSStdRecordTypeUsers), cfUser, NULL, NULL);
			if (cfRecord != NULL) {
				/* get the server_URL and path */
				if (retval == PAM_SUCCESS) {
					vals = ODRecordCopyValues(cfRecord, CFSTR(kDSNAttrHomeDirectory), NULL);
					if (vals != NULL && CFArrayGetCount(vals) > 0) {
						const void *val = CFArrayGetValueAtIndex(vals, 0);
						if (val == NULL || CFGetTypeID(val) != CFStringGetTypeID()) {
							openpam_log(PAM_LOG_ERROR, "The user's kDSNAttrHomeDirectory is not a string.");
							retval = PAM_SERVICE_ERR;
						} else if (0 != extract_homemount(val, server_URL, path)) {
							openpam_log(PAM_LOG_ERROR, "Unable to extract the server_URL or path.");
							retval = PAM_SERVICE_ERR;
						}
						CFRelease(vals);
					}
				}
				/* get the homedir */
				if (retval == PAM_SUCCESS) {
					vals = ODRecordCopyValues(cfRecord, CFSTR(kDS1AttrNFSHomeDirectory), NULL);
					if (vals != NULL && CFArrayGetCount(vals) > 0) {
						const void *val = CFArrayGetValueAtIndex(vals, 0);
						if (val == NULL || CFGetTypeID(val) != CFStringGetTypeID()) {
							openpam_log(PAM_LOG_ERROR, "This user's kDS1AttrNFSHomeDirectory is not a string.");
							retval = PAM_SERVICE_ERR;
						} else if (0 != extract_homedir(val, homedir)) {
							openpam_log(PAM_LOG_ERROR, "Unable to extract the homedir.");
							retval = PAM_SERVICE_ERR;
						}
						CFRelease(vals);
					}
				}				
				CFRelease(cfRecord);
			}
			CFRelease(cfUser);
		}
		CFRelease(cfNodeRef);
	}

	return retval;
}


static void
pam_malloc_cleanup(__unused pam_handle_t *pamh, void *data, __unused int pam_end_status)
{
	free(data);
}


static void
pam_cf_cleanup(__unused pam_handle_t *pamh, void *data, __unused int pam_end_status)
{
	CFStringRef *cfstring = data;
	CFRelease(*cfstring);
}


PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	static const char password_prompt[] = "Password:";
	char *authenticator = NULL;

	if (PAM_SUCCESS != pam_get_item(pamh, PAM_AUTHTOK, (void *)&authenticator))
		return PAM_IGNORE;
	if (NULL == authenticator && PAM_SUCCESS != pam_get_authtok(pamh, PAM_AUTHTOK, (void *)&authenticator, password_prompt))
		return PAM_IGNORE;
	if (PAM_SUCCESS != pam_setenv(pamh, "mount_authenticator", authenticator, 1)) {
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
	const char *username;
	const char *authenticator = NULL;
	struct passwd *pwd = NULL;
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
	if (NULL == (pwd = getpwnam(username))) {
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
	if (PAM_SUCCESS != (retval = extract_od_values(username, &server_URL, &path, &homedir))) {		
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
				/* cache the mountdir for close_session */
				pam_set_data(pamh, "mountdir", mountdir, pam_malloc_cleanup);
				mountdir = NULL;
			}
		}
		// for a FileVault home folder
		if (NULL == path && NULL != homedir) {
			CFStringRef password = CFStringCreateWithCString(NULL, authenticator, kCFStringEncodingUTF8);
			CFStringRef url = CFStringCreateWithCString(NULL, server_URL, kCFStringEncodingUTF8);
			CFURLRef dmgin = CFURLCreateWithString(NULL, url, NULL);
			CFStringRef mountpoint = CFStringCreateWithCString(NULL, homedir, kCFStringEncodingUTF8);
			CFStringRef devicepath;
			CFStringRef mountpath;

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

fin:
	if (NULL != authenticator)
		pam_setenv(pamh, "mount_authenticator", "", 1);
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
	char *mountdir = NULL;
	CFStringRef devicepath = NULL;
	
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

	/* try to retrieve the cached devicepath */
	if (PAM_SUCCESS != pam_get_data(pamh, "devicepath", (void *)&devicepath)) {
		openpam_log(PAM_LOG_DEBUG, "No cached devicepath in the PAM context.");
	}

	/* try to retrieve the cached mountdir */
	if (PAM_SUCCESS != pam_get_data(pamh, "mountdir", (void *)&mountdir)) {
		openpam_log(PAM_LOG_DEBUG, "No cached mountdir in the PAM context.");
	}

	/* attempt to unmount the home folder */
	if (NULL == devicepath && NULL != mountdir) {
		// not FileVault
		if (0 != UnmountServerURL(mountdir, 0)) {
			openpam_log(PAM_LOG_DEBUG, "Unable to unmount the home folder: %s.", strerror(errno));
			retval = PAM_IGNORE;
		}
		else {
			openpam_log(PAM_LOG_DEBUG, "Unmounted home folder.");
		}
	}

	if (NULL != devicepath) {
		// FileVault
		if (0 != (retval = DIHLFVUnmount(devicepath, kDIHLFVUnmountNormally, kDIHLFVUnmountNoTimeout))) {
			openpam_log(PAM_LOG_DEBUG, "Unable to unmount the FileVault home folder: %s.", DIStrError(retval));
			retval = PAM_IGNORE;
		}
		else {
			openpam_log(PAM_LOG_DEBUG, "Unmounted FileVault home folder.");
		}
	}

	return retval;
}
