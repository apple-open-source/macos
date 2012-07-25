/*-
 * Copyright (c) 2003 Networks Associates Technology, Inc.
 * All rights reserved.
 * Copyright (c) 2008-2009 Apple Inc. All rights reserved.
 *
 * Portions of this software were developed for the FreeBSD Project by
 * ThinkSec AS and NAI Labs, the Security Research Division of Network
 * Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/types.h>
#include <sys/syslimits.h>

#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef __APPLE__
#include <membership.h>
#include <stdlib.h>
#endif /* __APPLE__ */

#define PAM_SM_AUTH

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/openpam.h>

PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{
	const char *group, *user;
	const void *ruser;
#ifndef __APPLE__
	char *const *list;
#endif /* !__APPLE__ */
	struct passwd *pwd;
	struct passwd pwdbuf;
	char pwbuffer[2 * PATH_MAX];
	struct group *grp;
#ifdef __APPLE__
	char *str1, *str, *p;
	int found_group = 0;
	uuid_t u_uuid, g_uuid;
	int ismember;
#endif /* __APPLE__ */

	/* get target account */
	if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS ||
	    user == NULL || getpwnam_r(user, &pwdbuf, pwbuffer, sizeof(pwbuffer), &pwd) != 0 || pwd == NULL) {
		openpam_log(PAM_LOG_ERROR, "Unable to obtain the username.");
		return (PAM_AUTH_ERR);
	}
	if (pwd->pw_uid != 0 && openpam_get_option(pamh, "root_only")) {
		openpam_log(PAM_LOG_DEBUG, "The root_only option means root only.");
		return (PAM_IGNORE);
	}

	/* get applicant */
	if (openpam_get_option(pamh, "ruser") &&
		(pam_get_item(pamh, PAM_RUSER, &ruser) != PAM_SUCCESS || ruser == NULL || 
		 getpwnam_r(ruser, &pwdbuf, pwbuffer, sizeof(pwbuffer), &pwd) != 0 || pwd == NULL)) {
		openpam_log(PAM_LOG_ERROR, "Unable to obtain the remote username.");
		return (PAM_AUTH_ERR);
	}

	/* get regulating group */
	if ((group = openpam_get_option(pamh, "group")) == NULL) {
		group = "wheel";
		openpam_log(PAM_LOG_DEBUG, "With no group specfified, I am defaulting to wheel.");
	}
#ifdef __APPLE__
	str1 = str = strdup(group);
	while ((p = strsep(&str, ",")) != NULL) {
		if ((grp = getgrnam(p)) == NULL || grp->gr_mem == NULL)
			continue;

		/* check if the group is empty */
		if (*grp->gr_mem == NULL)
			continue;

		found_group = 1;

		/* check membership */
		if (mbr_uid_to_uuid(pwd->pw_uid, u_uuid) != 0)
			continue;
		if (mbr_gid_to_uuid(grp->gr_gid, g_uuid) != 0)
			continue;
		if (mbr_check_membership(u_uuid, g_uuid, &ismember) != 0)
			continue;
		if (ismember)
			goto found;
	}
	if (!found_group) {
		openpam_log(PAM_LOG_DEBUG, "The specified group (%s) could not be found.", group);
		goto failed;
	}
#else /* !__APPLE__ */
	if ((grp = getgrnam(group)) == NULL || grp->gr_mem == NULL) {
		openpam_log(PAM_LOG_DEBUG, "The specified group (%s) is NULL.", group);
		goto failed;
	}

	/* check if the group is empty */
	if (*grp->gr_mem == NULL) {
		openpam_log(PAM_LOG_DEBUG, "The specified group (%s) is empty.", group);
		goto failed;
	}

	/* check membership */
	if (pwd->pw_gid == grp->gr_gid)
		goto found;
	for (list = grp->gr_mem; *list != NULL; ++list)
		if (strcmp(*list, pwd->pw_name) == 0)
			goto found;
#endif /* __APPLE__ */

 not_found:
	openpam_log(PAM_LOG_DEBUG, "The group check failed.");
#ifdef __APPLE__
	free(str1);
#endif /* __APPLE__ */
	if (openpam_get_option(pamh, "deny"))
		return (PAM_SUCCESS);
	return (PAM_AUTH_ERR);
 found:
	openpam_log(PAM_LOG_DEBUG, "The group check succeeded.");
#ifdef __APPLE__
	free(str1);
#endif /* __APPLE__ */
	if (openpam_get_option(pamh, "deny"))
		return (PAM_AUTH_ERR);
	return (PAM_SUCCESS);
 failed:
	if (openpam_get_option(pamh, "fail_safe"))
		goto found;
	else
		goto not_found;
}


PAM_MODULE_ENTRY("pam_group");
