/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 * $Header: /cvs/krbdev/krb5/src/lib/kadm5/srv/svr_misc_free.c,v 1.2 1996/10/18 19:45:53 bjaspan Exp $
 * 
 */

#if !defined(lint) && !defined(__CODECENTER__)
static char *rcsid = "$Header: /cvs/krbdev/krb5/src/lib/kadm5/srv/svr_misc_free.c,v 1.2 1996/10/18 19:45:53 bjaspan Exp $";
#endif
#include	<kadm5/admin.h>
#include	<stdlib.h>
#include	"server_internal.h"

kadm5_ret_t
kadm5_free_principal_ent(void *server_handle,
			      kadm5_principal_ent_t val)
{
    kadm5_server_handle_t	handle = server_handle;

    CHECK_HANDLE(server_handle);

    if(val) {
	if(val->principal) 
	    krb5_free_principal(handle->context, val->principal);
	if(val->mod_name)
	    krb5_free_principal(handle->context, val->mod_name);
	if(val->policy)
	    free(val->policy);

	/* XXX free key_data and tl_data */

	if (handle->api_version == KADM5_API_VERSION_1)
	     free(val);
    }
    return KADM5_OK;
}
