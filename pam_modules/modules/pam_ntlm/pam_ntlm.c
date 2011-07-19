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

#include <GSS/gssapi_ntlm.h>
#include <GSS/gssapi_spi.h>

#include <dispatch/dispatch.h>

#define	PAM_SM_AUTH
#define	PAM_SM_ACCOUNT
#define	PAM_SM_PASSWORD

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/openpam.h>

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
    dispatch_semaphore_signal((dispatch_semaphore_t)ctx);
}



PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags,
    int argc __unused, const char *argv[] __unused)
{
    gss_auth_identity_desc identity;
	const char *user, *password;
	dispatch_semaphore_t sema;
	struct passwd *pwd;
	struct passwd pwdbuf;
	char pwbuffer[2 * PATH_MAX];
	int retval;
	uid_t euid = geteuid();
	gid_t egid = getegid();
	char hostname[_POSIX_HOST_NAME_MAX + 1];

	PAM_LOG("pam_sm_setcred: ntlm");

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

   	sema = dispatch_semaphore_create(0);

	gethostname(hostname, sizeof(hostname));
	hostname[sizeof(hostname) - 1] = '\0';

	identity.username = (char *)user;
	identity.realm = hostname;
	identity.password = (char *)password;

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
				    sema,
				    ac_complete);
	dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
	dispatch_release(sema);

	if (euid == 0) {
		seteuid(euid);
		setegid(egid);
	}

	PAM_LOG("pam_sm_setcred: ntlm done");

cleanup:
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
