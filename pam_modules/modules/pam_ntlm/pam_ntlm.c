/*-
 * Copyright 2010 Apple Inc
 */

#include <sys/cdefs.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include <OpenDirectory/OpenDirectory.h>

#include <GSS/gssapi_ntlm.h>
#include <GSS/gssapi_spi.h>

#define	PAM_SM_AUTH
#define	PAM_SM_ACCOUNT
#define	PAM_SM_PASSWORD

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/openpam.h>

#include "Common.h"

#define PAM_OPT_DEBUG		"debug"

#define	PAM_LOG(...) \
	openpam_log(PAM_LOG_DEBUG, __VA_ARGS__)

#define	PAM_VERBOSE_ERROR(...) \
openpam_log(PAM_LOG_ERROR, __VA_ARGS__)

static const char *password_key = "NTLMPWD";


/*
 * authentication management
 */

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{
	int retval;
	const char *password;

	PAM_LOG("pam_sm_authenticate: ntlm");

	/* get password */
	retval = pam_get_authtok(pamh, PAM_AUTHTOK, &password, NULL);
	if (retval != PAM_SUCCESS)
		return retval;

	retval = pam_setenv(pamh, password_key, password, 1);
	if (retval != PAM_SUCCESS)
		return retval;

	return PAM_IGNORE;
}

static void
ac_complete(void *ctx, OM_uint32 major, gss_status_id_t status,
			gss_cred_id_t cred, gss_OID_set oids, OM_uint32 time_rec)
{
    OM_uint32 junk;
    gss_release_cred(&junk, &cred);
    gss_release_oid_set(&junk, &oids);
    PAM_LOG("ac_complete returned: %d for %d", major, geteuid());
}



PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags,
    int argc __unused, const char *argv[] __unused)
{
	gss_auth_identity_desc identity;
	const char *user, *password;
	struct passwd *pwd;
	struct passwd pwdbuf;
	char pwbuffer[2 * PATH_MAX];
	int retval;
	uid_t euid = geteuid();
	gid_t egid = getegid();
	ODRecordRef record = NULL;
	CFArrayRef array = NULL;
	CFIndex i, count;

	PAM_LOG("pam_sm_setcred: ntlm");

	memset(&identity, 0, sizeof(identity));

	/* Get username */
	retval = pam_get_item(pamh, PAM_USER, (const void **)&user);
	if (retval != PAM_SUCCESS) {
		PAM_LOG("pam_sm_setcred: ntlm user can't be found");
		goto cleanup;
	}

	if (getpwnam_r(user, &pwdbuf, pwbuffer, sizeof(pwbuffer), &pwd) != 0 || pwd == NULL) {
		PAM_LOG("pam_sm_setcred: ntlm user %s doesn't exists", user);
		retval = PAM_USER_UNKNOWN;
		goto cleanup;
	}

	password = pam_getenv(pamh, password_key);
	if (password == NULL) {
		PAM_LOG("pam_sm_setcred: ntlm user %s doesn't have a password", user);
		retval = PAM_IGNORE;
		goto cleanup;
	}

	retval = od_record_create_cstring(pamh, &record, user);
	if (retval || record == NULL) {
		retval = PAM_IGNORE;
		goto cleanup;
	}

	array = ODRecordCopyValues(record, kODAttributeTypeAuthenticationAuthority, NULL);
	if (array == NULL) {
		PAM_LOG("pam_sm_setcred: ntlm user %s doesn't have auth authority", user);
		retval = PAM_IGNORE;
		goto cleanup;
	}

	identity.username = (char *)user;
	identity.password = (char *)password;

	count = CFArrayGetCount(array);
	for (i = 0; i < count && identity.realm == NULL; i++) {
		CFStringRef val = CFArrayGetValueAtIndex(array, i);
		if (NULL == val || CFGetTypeID(val) != CFStringGetTypeID())
			break;

		if (!CFStringHasPrefix(val, CFSTR(";NetLogon;")))
			continue;

		CFArrayRef parts = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, val, CFSTR(";"));
		if (parts == NULL)
			continue;

		if (CFArrayGetCount(parts) < 4) {
			CFRelease(parts);
			continue;
		}
			
		CFStringRef domain = CFArrayGetValueAtIndex(parts, 3);

		retval = cfstring_to_cstring(domain, &identity.realm);
		CFRelease(parts);
		if (retval)
			goto cleanup;
	}

	if (identity.realm == NULL) {
		PAM_LOG("pam_sm_setcred: no domain found skipping");
		retval = PAM_IGNORE;
		goto cleanup;
	}

	if (euid == 0) {
		if (setegid(pwd->pw_gid) != 0) {
			retval = PAM_SERVICE_ERR;
			goto cleanup;
		}
		if (seteuid(pwd->pw_uid) != 0) {
			retval = PAM_SERVICE_ERR;
			goto cleanup;
		}
	}

	(void)gss_acquire_cred_ex_f(NULL,
				    GSS_C_NO_NAME,
				    0,
				    GSS_C_INDEFINITE,
				    GSS_NTLM_MECHANISM,
				    GSS_C_INITIATE,
				    &identity,
				    NULL,
				    ac_complete);

	if (euid == 0) {
		seteuid(euid);
		setegid(egid);
	}

	PAM_LOG("pam_sm_setcred: ntlm done, used domain: %s", identity.realm);

cleanup:
	if (record)
		CFRelease(record);
	if (array)
		CFRelease(array);
	if (identity.realm)
		free(identity.realm);
	pam_unsetenv(pamh, password_key);
	return retval;
}

/*
 * account management
 */
PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{
	return PAM_SUCCESS;
}

/*
 * password management
 */
PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh, int flags,
    int argc __unused, const char *argv[] __unused)
{
	return PAM_AUTHTOK_ERR;
}

PAM_MODULE_ENTRY("pam_ntlm");
