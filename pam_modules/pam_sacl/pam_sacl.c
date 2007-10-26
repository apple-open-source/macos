/*
 * Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
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
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <grp.h>
#include <pwd.h>
#include <syslog.h>
#include <membership.h>
#include <membershipPriv.h>

#define _PAM_EXTERN_FUNCTIONS
#include <pam/pam_modules.h>
#include <pam/pam_mod_misc.h>

#define MODULE_NAME "pam_sacl"

/* Note to self: To enable debug logging, we also have to make the syslog
 * *.debug level go somewhere.
 */
#define DEBUG_MESSAGE(format, ...) \
    if (options & PAM_OPT_DEBUG ) { \
	syslog(LOG_DEBUG, format, __VA_ARGS__); \
    }

bool match_option(const char *arg, const char * opt)
{
    size_t len = strlen(opt);

    if (strncasecmp(arg, opt, len) != 0) {
	return false;
    }

    /* At this point arg might be "option" or "option_blah". We will accept
     * either "option" or "option=foo".
     */
    if (arg[len] == '=' || arg[len] == '\0') {
	return true;
    }

    return false;
}

static const char * get_option_argument(const char * arg)
{
    const char * sep;

    sep = strchr(arg, '=');
    if (!sep) {
	    return NULL;
    }

    ++sep;
    if (*sep == '\0') {
	return NULL;
    }

    return sep;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t * pamh, int flags,
			int argc, const char ** argv)
{
	int		i, options = 0;
	const char *	service = NULL;
	const char *	username = NULL;
	bool		allow_trustacct = false;

	uuid_t	user_uuid;
	int	err;
	int	ismember;

	for (i = 0; (i < argc) && argv[i]; i++) {
		if (pam_std_option(&options, argv[i]) == 0) {
			continue;
		}

		if (match_option(argv[i], "sacl_service")) {
			service = get_option_argument(argv[i]);
		} else if (match_option(argv[i], "allow_trustacct")) {
			allow_trustacct = true;
		} else {
		    DEBUG_MESSAGE("%s: unrecognized option: %s",
				    MODULE_NAME, argv[i]);
		}
	}

	if (!service) {
		DEBUG_MESSAGE("%s: missing service option", MODULE_NAME);
		return PAM_IGNORE;
	}

	if (pam_get_user(pamh, &username, NULL) != PAM_SUCCESS ||
	    username == NULL || *username == '\0') {
		DEBUG_MESSAGE("%s: missing username", MODULE_NAME);
		return PAM_SYSTEM_ERR;
	}
 
	DEBUG_MESSAGE("%s: checking if account '%s' can access service '%s'",
		    MODULE_NAME, username, service);

	/* Since computer trust accounts in OD are not user accounts, you can't
	 * add them to a SACL, so we always let them through (if the option is
	 * set). A computer trust account has a username ending in '$' and no
	 * corresponding user account (ie. no passwd entry).
	 */
	if (allow_trustacct) {
		const char * c;

		c = strrchr(username, '$');
		if (c && *(c + 1) == '\0' && getpwnam(username) == NULL) {
			DEBUG_MESSAGE("%s: allowing '%s' because it is a "
				"computer trust account",
				MODULE_NAME, username);
			return PAM_SUCCESS;
		}
	}

	/* Get the UUID. This will fail if the user is is logging in over
	 * SMB, is specifed as DOMAIN\user or user@REALM and the directory
	 * does not have the aliases we need.
	 */
	if (mbr_user_name_to_uuid(username, user_uuid)) {
		char * sacl_group;

		/* We couldn't map the user to a UID, but we only care about
		 * this if the relevant SACL groups exist.
		 */

		if (asprintf(&sacl_group, "com.apple.access_%s\n",
							service) == -1) {
			return PAM_SYSTEM_ERR;
		}

		if (getgrnam(sacl_group) == NULL &&
		    getgrnam("com.apple.access_all_services") == NULL) {

			DEBUG_MESSAGE("%s: allowing '%s' "
				    "due to absence of service ACL",
				    MODULE_NAME, username);

			free(sacl_group);
			return PAM_SUCCESS;
		}

		DEBUG_MESSAGE("%s: denying '%s' due to missing UUID",
			MODULE_NAME, username);

		free(sacl_group);
		return PAM_PERM_DENIED;
	}

	err = mbr_check_service_membership(user_uuid, service, &ismember);
	if (err) {
	        if (err == ENOENT) {
	                /* Service ACLs not configured. */
			DEBUG_MESSAGE("%s: allowing '%s' "
				"due to unconfigured service ACLs",
				MODULE_NAME, username);
	                return PAM_SUCCESS;
	        }
	
		DEBUG_MESSAGE("%s: denying '%s' "
			"due to failed service ACL check (errno=%d)",
			MODULE_NAME, username, err);

	        return PAM_PERM_DENIED;
	}
	
        if (ismember) {
		DEBUG_MESSAGE("%s: allowing '%s'", MODULE_NAME);
		return PAM_SUCCESS;
	} else {
		DEBUG_MESSAGE("%s: denying '%s' "
			"due to failed service ACL check",
			MODULE_NAME, username);
		return PAM_PERM_DENIED;
	}
}

#ifdef PAM_STATIC
PAM_MODULE_ENTRY(MODULE_NAME);
#endif

