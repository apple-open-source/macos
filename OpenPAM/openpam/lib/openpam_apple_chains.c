/*
 * openpam_apple_chains.c
 *
 * Import of all stock macOS PAM chains into this file.
 */

#include <assert.h>		/* assert() */
#include <errno.h>
#include <stdio.h>		/* fmemopen() */
#include <stdlib.h>		/* calloc() */
#include <string.h>		/* mempcy(), strcmp() */

#include <security/pam_appl.h>

#include "openpam_impl.h"


#pragma mark - Apple Chains

struct openpam_chain {
	char *service;
	char *definition;
};

static const struct openpam_chain apple_chains[] = {
	/* /etc/pam.d/authorization */
	{
		"authorization",
		"auth       optional       pam_krb5.so use_first_pass no_ccache\n"
		"auth       optional       pam_ntlm.so use_first_pass\n"
		"auth       required       pam_opendirectory.so use_first_pass nullok\n"
		"account    required       pam_opendirectory.so\n"
	},
	/* /etc/pam.d/authorization_aks */
	{
		"authorization_aks",
		"auth       required       pam_aks.so\n"
		"account    required       pam_opendirectory.so\n"
	},
	/* /etc/pam.d/authorization_ctk */
	{
		"authorization_ctk",
		"auth       required       pam_smartcard.so 		use_first_pass pkinit\n"
		"account    required       pam_opendirectory.so\n"
	},
	/* /etc/pam.d/authorization_la */
	{
		"authorization_la",
		"auth       required       pam_localauthentication.so\n"
		"auth       required       pam_aks.so\n"
		"account    required       pam_opendirectory.so\n"
	},
	/* /etc/pam.d/authorization_lacont */
	{
		"authorization_lacont",
		"auth       required       pam_localauthentication.so continuityunlock\n"
		"auth       required       pam_aks.so\n"
		"account    required       pam_opendirectory.so\n"
	},
	/* /etc/pam.d/checkpw */
	{
		"checkpw",
		"auth       required       pam_opendirectory.so use_first_pass nullok\n"
		"account    required       pam_opendirectory.so no_check_home no_check_shell\n"
	},
	/* /etc/pam.d/chkpasswd */
	{
		"chkpasswd",
		"auth       required       pam_opendirectory.so\n"
		"account    required       pam_opendirectory.so\n"
		"password   required       pam_permit.so\n"
		"session    required       pam_permit.so\n"
	},
	/* /etc/pam.d/cups */
	{
		"cups",
		"auth       required       pam_opendirectory.so\n"
		"account    required       pam_permit.so\n"
		"password   required       pam_deny.so\n"
		"session    required       pam_permit.so\n"
	},
	/* /etc/pam.d/login */
	{
		"login",
		"auth       optional       pam_krb5.so use_kcminit\n"
		"auth       optional       pam_ntlm.so try_first_pass\n"
		"auth       optional       pam_mount.so try_first_pass\n"
		"auth       required       pam_opendirectory.so try_first_pass\n"
		"account    required       pam_nologin.so\n"
		"account    required       pam_opendirectory.so\n"
		"password   required       pam_opendirectory.so\n"
		"session    required       pam_launchd.so\n"
		"session    required       pam_uwtmp.so\n"
		"session    optional       pam_mount.so\n"
	},
	/* /etc/pam.d/login.term */
	{
		"login.term",
		"account    required       pam_nologin.so\n"
		"account    required       pam_opendirectory.so\n"
		"session    required       pam_uwtmp.so\n"
	},
	/* /etc/pam.d/other */
	{
		"other",
		"auth       required       pam_deny.so\n"
		"account    required       pam_deny.so\n"
		"password   required       pam_deny.so\n"
		"session    required       pam_deny.so\n"
	},
	/* /etc/pam.d/passwd */
	{
		"passwd",
		"auth       required       pam_permit.so\n"
		"account    required       pam_opendirectory.so\n"
		"password   required       pam_opendirectory.so\n"
		"session    required       pam_permit.so\n"
	},
	/* /etc/pam.d/screensaver */
	{
		"screensaver",
		"auth       optional       pam_krb5.so use_first_pass use_kcminit\n"
		"auth       required       pam_opendirectory.so use_first_pass nullok\n"
		"account    required       pam_opendirectory.so\n"
		"account    sufficient     pam_self.so\n"
		"account    required       pam_group.so no_warn group=admin,wheel fail_safe\n"
		"account    required       pam_group.so no_warn deny group=admin,wheel ruser fail_safe\n"
	},
	/* /etc/pam.d/screensaver_aks */
	{
		"screensaver_aks",
		"auth       required       pam_aks.so\n"
		"account    required       pam_opendirectory.so\n"
		"account    sufficient     pam_self.so\n"
		"account    required       pam_group.so no_warn group=admin,wheel fail_safe\n"
		"account    required       pam_group.so no_warn deny group=admin,wheel ruser fail_safe\n"
	},
	/* /etc/pam.d/screensaver_ctk */
	{
		"screensaver_ctk",
		"auth       required       pam_smartcard.so			use_first_pass\n"
		"account    required       pam_opendirectory.so\n"
		"account    sufficient     pam_self.so\n"
		"account    required       pam_group.so no_warn group=admin,wheel fail_safe\n"
		"account    required       pam_group.so no_warn deny group=admin,wheel ruser fail_safe\n"
	},
	/* /etc/pam.d/screensaver_la */
	{
		"screensaver_la",
		"auth       required       pam_localauthentication.so\n"
		"auth       required       pam_aks.so\n"
		"account    required       pam_opendirectory.so\n"
		"account    sufficient     pam_self.so\n"
		"account    required       pam_group.so no_warn group=admin,wheel fail_safe\n"
		"account    required       pam_group.so no_warn deny group=admin,wheel ruser fail_safe\n"
	},
	/* /etc/pam.d/smbd */
	{
		"smbd",
		"account required	pam_sacl.so sacl_service=smb allow_trustacct\n"
		"session required	pam_permit.so\n"
	},
	/* /etc/pam.d/sshd */
	{
		"sshd",
		"auth       optional       pam_krb5.so use_kcminit\n"
		"auth       optional       pam_ntlm.so try_first_pass\n"
		"auth       optional       pam_mount.so try_first_pass\n"
		"auth       required       pam_opendirectory.so try_first_pass\n"
		"account    required       pam_nologin.so\n"
		"account    required       pam_sacl.so sacl_service=ssh\n"
		"account    required       pam_opendirectory.so\n"
		"password   required       pam_opendirectory.so\n"
		"session    required       pam_launchd.so\n"
		"session    optional       pam_mount.so\n"
	},
	/* /etc/pam.d/su */
	{
		"su",
		"auth       sufficient     pam_rootok.so\n"
		"auth       required       pam_opendirectory.so\n"
		"account    required       pam_group.so no_warn group=admin,wheel ruser root_only fail_safe\n"
		"account    required       pam_opendirectory.so no_check_shell\n"
		"password   required       pam_opendirectory.so\n"
		"session    required       pam_launchd.so\n"
	},
	/* /etc/pam.d/sudo */
	{
		"sudo",
		"auth       sufficient     pam_smartcard.so\n"
		"auth       required       pam_opendirectory.so\n"
		"account    required       pam_permit.so\n"
		"password   required       pam_deny.so\n"
		"session    required       pam_permit.so\n"
	}
};

static const int num_apple_chains = (int)sizeof(apple_chains) / sizeof(apple_chains[0]);


#pragma mark - openpam_read_chain

extern int
openpam_read_chain_from_filehandle(pam_handle_t *pamh,
	const char *service,
	pam_facility_t facility,
	FILE *f,
	const char *filename,
	openpam_style_t style);

static int
openpam_read_chain_from_memory(pam_handle_t *pamh,
	const struct openpam_chain *chain,
	pam_facility_t facility)
{
	FILE *f	= fmemopen(chain->definition, strlen(chain->definition), "r");
	if (f == NULL)
		return -1;

	return openpam_read_chain_from_filehandle(pamh, chain->service, facility, f, chain->service, pam_d_style);
}

static int
openpam_read_apple_chain(pam_handle_t *pamh,
	const char *service,
	pam_facility_t facility)
{
	int i = 0;
	for (i = 0; i < num_apple_chains; i++)
		if (strcmp(service, apple_chains[i].service) == 0)
			return openpam_read_chain_from_memory(pamh, &apple_chains[i], facility);

	return -1;
}


#pragma mark - openpam_configure

int
openpam_configure_apple(pam_handle_t *pamh,
	const char *service)
{
	pam_facility_t fclt;

	if (openpam_read_apple_chain(pamh, service, PAM_FACILITY_ANY) < 0)
		goto load_err;

	for (fclt = 0; fclt < PAM_NUM_FACILITIES; ++fclt) {
		if (pamh->chains[fclt] != NULL)
			continue;
		if (openpam_read_apple_chain(pamh, PAM_OTHER, fclt) < 0)
			goto load_err;
	}
	if (openpam_configure_default(pamh))
		goto load_err;
	return (PAM_SUCCESS);

load_err:
	openpam_clear_chains(pamh->chains);
	return (PAM_SYSTEM_ERR);
}
