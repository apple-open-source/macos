#include <sys/types.h>
#include <utmp.h>
#include <util.h>

#define PAM_SM_SESSION
#include <pam/pam_modules.h>

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	int status;
	char *tty;
	char *user;
	char *remhost;
	struct utmp ut;

	status = pam_get_item(pamh, PAM_USER, (void *)&user);
	if( status != PAM_SUCCESS )
		return status;

	status = pam_get_item(pamh, PAM_TTY, (void *)&tty);
	if( status != PAM_SUCCESS )
		return status;

	memset(ut.ut_host, 0, UT_HOSTSIZE);
	status = pam_get_item(pamh, PAM_RHOST, (void *)&remhost);
	if( (status == PAM_SUCCESS) && (remhost != NULL) )
		strncpy(ut.ut_host, remhost, UT_HOSTSIZE);
	
	strncpy(ut.ut_line, tty, UT_LINESIZE);
	strncpy(ut.ut_name, user, UT_NAMESIZE);
	ut.ut_time = time(NULL);

	login(&ut);

	return PAM_SUCCESS;
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	int status;
	char *tty;

	status = pam_get_item(pamh, PAM_TTY, (void *)&tty);
	if( status != PAM_SUCCESS )
		return status;
	if( logout(tty) != 0 )
		return PAM_SYSTEM_ERR;

	return PAM_SUCCESS;
}
