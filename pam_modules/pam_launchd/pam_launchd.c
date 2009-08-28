/*
 * Copyright (c) 2007-2009 Apple Inc. All rights reserved.
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
#include <security/pam_modules.h>
#include <security/pam_appl.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vproc.h>
#include <vproc_priv.h>
#include <servers/bootstrap.h>
#include <bootstrap_priv.h>
#include <pwd.h>
#include <sys/syslimits.h>

#include <Security/AuthSession.h>
#include <Security/AuthorizationPriv.h>

#define SESSION_TYPE_OPT "launchd_session_type"
#define DEFAULT_SESSION_TYPE VPROCMGR_SESSION_BACKGROUND
#define NULL_SESSION_TYPE "NullSession"
#define SESSION_ATTR_HAS_TTY "has_tty"
#define SESSION_ATTR_HAS_GUI "has_gui"
#define SESSION_ATTR_IS_REMOTE "is_remote"

/* An application may modify the default behavior as follows:
 * (1) Choose a specific session type:
 *     pam_putenv(pamh, "launchd_session_type=Aqua");
 * (2) Choose to not start a new session:
 *     pam_putenv(pamh, "launchd_session_type=NullSession");
 * Otherwise, if launchd_session_type is not set, a new session of the
 * default type will be created.
 */

extern vproc_err_t _vproc_post_fork_ping(void);

static mach_port_t
get_root_bootstrap_port(void)
{
	mach_port_t parent_port = MACH_PORT_NULL;
	mach_port_t previous_port = MACH_PORT_NULL;
	do {
		if (previous_port) {
			if (previous_port != bootstrap_port) {
				mach_port_deallocate(mach_task_self(), previous_port);
			}
			previous_port = parent_port;
		} else {
			previous_port = bootstrap_port;
		}
		if (bootstrap_parent(previous_port, &parent_port) != 0) {
			return MACH_PORT_NULL;
		}
	} while (parent_port != previous_port);
	
	return parent_port;
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	char buffer[2*PATH_MAX];
	const char* default_session_type = DEFAULT_SESSION_TYPE;
	const char* session_type = pam_getenv(pamh, SESSION_TYPE_OPT);
	const char* username;
	struct passwd *pwd;
	struct passwd pwdbuf;
	uid_t uid, suid;

	/* Deterine the launchd session type. */
	if (NULL == (default_session_type = openpam_get_option(pamh, SESSION_TYPE_OPT))) {
		default_session_type = DEFAULT_SESSION_TYPE;
	}
	if (NULL == session_type) {
		session_type = default_session_type;
	} else if (0 == strcmp(session_type, NULL_SESSION_TYPE)) {
		return PAM_IGNORE;
	}
	
	SessionAttributeBits attrs = 0;
 	if( NULL != openpam_get_option(pamh, SESSION_ATTR_IS_REMOTE) ) {
		attrs |= sessionIsRemote;
	}
 	if( NULL != openpam_get_option(pamh, SESSION_ATTR_HAS_GUI) ) {
		attrs |= sessionHasGraphicAccess;
	} else {
		attrs |= sessionHasTTY;
	}

	/* Get the username (and UID). */
	if (PAM_SUCCESS != pam_get_item(pamh, PAM_USER, (void *)&username) || NULL == username) {
		return PAM_IGNORE;
	}
	if (0 != getpwnam_r(username, &pwdbuf, buffer, sizeof(buffer), &pwd) || NULL == pwd) {
		return PAM_IGNORE;
	}
	uid = pwd->pw_uid;
	openpam_log(PAM_LOG_DEBUG, "Going to switch to (%s) %u's %s session", username, uid, session_type);
	
	/* If we're running as root, set the root Mach bootstrap as our bootstrap port. If not, we fail. */
	if (geteuid() == 0) {
		mach_port_t rbs = get_root_bootstrap_port();
		if (rbs) {
			mach_port_mod_refs(mach_task_self(), bootstrap_port, MACH_PORT_RIGHT_SEND, -1);
			task_set_bootstrap_port(mach_task_self(), rbs);
			bootstrap_port = rbs;
		}
	} else {
		return PAM_IGNORE;
	}

	/* We need to set the UID to appease launchd, then lookup the per-user bootstrap. */
	suid = getuid();
	setreuid(0, 0);
	mach_port_t puc = MACH_PORT_NULL;
	kern_return_t kr = bootstrap_look_up_per_user(bootstrap_port, NULL, uid, &puc);
	setreuid(suid, 0);
	if (BOOTSTRAP_SUCCESS != kr) {
		openpam_log(PAM_LOG_ERROR, "Could not look up per-user bootstrap for UID %u.", uid);
		return PAM_IGNORE;
	} else if (BOOTSTRAP_NOT_PRIVILEGED == kr) {
		openpam_log(PAM_LOG_ERROR, "Permission denied to look up per-user bootstrap for UID %u.", uid);
		/* If this happens, bootstrap_port is probably already set appropriately anyway. */
		return PAM_IGNORE;
	}

	/* Set our bootstrap port to be that of the Background session of the per-user launchd. */
	mach_port_mod_refs(mach_task_self(), bootstrap_port, MACH_PORT_RIGHT_SEND, -1);
	task_set_bootstrap_port(mach_task_self(), puc);
	bootstrap_port = puc;
	
	/* Now move ourselves into the appropriate session. */
	if (strncmp(session_type, VPROCMGR_SESSION_BACKGROUND, sizeof(VPROCMGR_SESSION_BACKGROUND)) != 0) {
		vproc_err_t verr = NULL;
		if (NULL != (verr = _vprocmgr_switch_to_session(session_type, 0))) {
			openpam_log(PAM_LOG_ERROR, "Unable to switch to %u's %s session (0x%p).", uid, session_type, verr);
			return PAM_SESSION_ERR;
		}
	}

	/* Create a new security session. */
	if (noErr != SessionCreate(0, attrs)) {
		return PAM_SESSION_ERR;
	}
	if (NULL != _vproc_post_fork_ping()) {
		return PAM_SESSION_ERR;
	}
	SessionSetDistinguishedUser(callerSecuritySession, uid);
	
	return PAM_SUCCESS;
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}
