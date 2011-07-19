#include <stdlib.h>
#include <string.h>

#include <security/pam_appl.h>

#include "openpam_impl.h"

/*
 * OpenPAM extension
 *
 * Unset an environment variable
 * Mirrors unsetenv(3)
 */

int
pam_unsetenv(pam_handle_t *pamh, const char *name)
{
	int i;

	ENTER();
	if (pamh == NULL)
		RETURNC(PAM_SYSTEM_ERR);

	/* sanity checks */
	if (name == NULL)
		RETURNC(PAM_SYSTEM_ERR);

	/* find and remove the variable from the environment */
	if ((i = openpam_findenv(pamh, name, strlen(name))) >= 0) {
		memset(pamh->env[i], 0, strlen(pamh->env[i]));
		FREE(pamh->env[i]);
		pamh->env[i] = pamh->env[pamh->env_count-1];
		pamh->env[pamh->env_count-1] = NULL;
		pamh->env_count--;
		RETURNC(PAM_SUCCESS);
	}

	RETURNC(PAM_SYSTEM_ERR);
}

/*
 * Error codes:
 *
 *	=pam_unsetenv
 *	PAM_SYSTEM_ERR
 */

/**
 * The =pam_unsetenv function unsets a environment variable.
 * Its semantics are similar to those of =unsetenv, but it modifies the PAM
 * context's environment list instead of the application's.
 *
 * >pam_getenv
 * >pam_getenvlist
 * >pam_putenv
 */
