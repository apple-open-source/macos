/* pam_session.c - PAM Session Management */

/*
 * $Id: pam_session.c,v 1.2 2002/03/27 02:36:10 bbraun Exp $
 */

#include <stdio.h>

#include "pam_private.h"

int pam_open_session(pam_handle_t *pamh, int flags)
{
    D(("called"));

    IF_NO_PAMH("pam_open_session", pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_MODULE(pamh)) {
	D(("called from module!?"));
	return PAM_SYSTEM_ERR;
    }

    return _pam_dispatch(pamh, flags, PAM_OPEN_SESSION);
}

int pam_close_session(pam_handle_t *pamh, int flags)
{
    D(("called"));

    IF_NO_PAMH("pam_close_session", pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_MODULE(pamh)) {
	D(("called from module!?"));
	return PAM_SYSTEM_ERR;
    }

    return _pam_dispatch(pamh, flags, PAM_CLOSE_SESSION);
}
