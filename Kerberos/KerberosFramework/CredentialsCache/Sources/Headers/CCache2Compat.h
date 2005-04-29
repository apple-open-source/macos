#pragma once

/* Internal functions of the compatibility layer */
	
void
cci_compat_deep_free_NC_info (
	apiCB*			inContext,
	infoNC**				data);

cc_bool
cci_compat_credentials_equal (
	const cred_union*				inOldCreds,
	const cc_credentials_union*		inNewCreds);
