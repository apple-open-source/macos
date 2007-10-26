/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#define _PAM_EXTERN_FUNCTIONS
#define PAM_SM_SESSION
#include <pam/pam_modules.h>
#include <pam/pam_mod_misc.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vproc.h>
#include <vproc_priv.h>
#include <pwd.h>
#include <sys/syslimits.h>

#define SESSION_TYPE_OPT "launchd_session_type"
#define DEFAULT_SESSION_TYPE VPROCMGR_SESSION_STANDARDIO
#define NULL_SESSION_TYPE "NullSession"

/* An application may modify the default behavior as follows:
 * (1) Choose a specific session type:
 *     pam_putenv(pamh, "launchd_session_type=Aqua");
 * (2) Choose to not start a new session:
 *     pam_putenv(pamh, "launchd_session_type=NullSession");
 * Otherwise, if launchd_session_type is not set, a new session of the
 * default type will be created.
 */


PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	char buffer[2*PATH_MAX];
	int result;
	const char* default_session_type = DEFAULT_SESSION_TYPE;
	const char* session_type = pam_getenv(pamh, SESSION_TYPE_OPT);
	const char* username;
	struct passwd *pwd;
	struct passwd pwdbuf;
	uid_t uid;

	int options = 0;
	int i;

	for(i = 0; (i < argc) && argv[i]; i++) {
		pam_std_option(&options, argv[i]);
		if (strlen(argv[i]) >= sizeof(SESSION_TYPE_OPT) &&
		    argv[i][sizeof(SESSION_TYPE_OPT)-1] == '=' &&
		    memcmp(argv[i], SESSION_TYPE_OPT, sizeof(SESSION_TYPE_OPT)-1) == 0) {
			default_session_type = &argv[i][sizeof(SESSION_TYPE_OPT)];
		}
	}
	options |= PAM_OPT_TRY_FIRST_PASS;

	if (NULL == session_type) {
		session_type = default_session_type;
	} else if (0 == strcmp(session_type, NULL_SESSION_TYPE)) {
		return PAM_IGNORE;
	}

        result = pam_get_item(pamh, PAM_USER, (void *)&username);
        if (result != PAM_SUCCESS || username == NULL) {
                return PAM_IGNORE;
        }

	result = getpwnam_r(username, &pwdbuf, buffer, sizeof(buffer), &pwd);
	if (0 != result || pwd == NULL) {
		return PAM_IGNORE;
	}

	uid = pwd->pw_uid;

	if (_vprocmgr_move_subset_to_user(uid, (char*)session_type) == NULL) {
		result = PAM_SUCCESS;
	} else {
		result = PAM_SESSION_ERR;
	}

	return result;
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}
