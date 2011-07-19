/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
**
**  NAME
**
**      gssauth.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**      Client-side support of kerberos module.
**
**
*/

#include <config.h>

#if defined(AUTH_GSS_NEGOTIATE) && AUTH_GSS_NEGOTIATE

#include <gssauth.h>

INTERNAL unsigned32 rpc_g_gssauth_alloc_count = 0;
INTERNAL unsigned32 rpc_g_gssauth_free_count = 0;

INTERNAL rpc_auth_rpc_prot_epv_p_t rpc_g_gssauth_negotiate_rpc_prot_epv[RPC_C_PROTOCOL_ID_MAX];
INTERNAL rpc_auth_rpc_prot_epv_p_t rpc_g_gssauth_mskrb_rpc_prot_epv[RPC_C_PROTOCOL_ID_MAX];
INTERNAL rpc_auth_rpc_prot_epv_p_t rpc_g_gssauth_winnt_rpc_prot_epv[RPC_C_PROTOCOL_ID_MAX];
INTERNAL rpc_auth_rpc_prot_epv_p_t rpc_g_gssauth_netlogon_rpc_prot_epv[RPC_C_PROTOCOL_ID_MAX];

INTERNAL void rpc__gssauth_negotiate_bnd_set_auth (
	unsigned_char_p_t		/* in  */    /*server_princ_name*/,
	rpc_authn_level_t		/* in  */    /*authn_level*/,
	rpc_auth_identity_handle_t	/* in  */    /*auth_identity*/,
	rpc_authz_protocol_id_t		/* in  */    /*authz_protocol*/,
	rpc_binding_handle_t		/* in  */    /*binding_h*/,
	rpc_auth_info_p_t		/* out */    * /*auth_info*/,
	unsigned32			/* out */    * /*st*/
    );

INTERNAL void rpc__gssauth_mskrb_bnd_set_auth (
	unsigned_char_p_t		/* in  */    /*server_princ_name*/,
	rpc_authn_level_t		/* in  */    /*authn_level*/,
	rpc_auth_identity_handle_t	/* in  */    /*auth_identity*/,
	rpc_authz_protocol_id_t		/* in  */    /*authz_protocol*/,
	rpc_binding_handle_t		/* in  */    /*binding_h*/,
	rpc_auth_info_p_t		/* out */    * /*auth_info*/,
	unsigned32			/* out */    * /*st*/
    );

INTERNAL void rpc__gssauth_winnt_bnd_set_auth (
	unsigned_char_p_t		/* in  */    /*server_princ_name*/,
	rpc_authn_level_t		/* in  */    /*authn_level*/,
	rpc_auth_identity_handle_t	/* in  */    /*auth_identity*/,
	rpc_authz_protocol_id_t		/* in  */    /*authz_protocol*/,
	rpc_binding_handle_t		/* in  */    /*binding_h*/,
	rpc_auth_info_p_t		/* out */    * /*auth_info*/,
	unsigned32			/* out */    * /*st*/
    );

INTERNAL void rpc__gssauth_netlogon_bnd_set_auth (
	unsigned_char_p_t		/* in  */    /*server_princ_name*/,
	rpc_authn_level_t		/* in  */    /*authn_level*/,
	rpc_auth_identity_handle_t	/* in  */    /*auth_identity*/,
	rpc_authz_protocol_id_t		/* in  */    /*authz_protocol*/,
	rpc_binding_handle_t		/* in  */    /*binding_h*/,
	rpc_auth_info_p_t		/* out */    * /*auth_info*/,
	unsigned32			/* out */    * /*st*/
    );

INTERNAL void rpc__gssauth_srv_reg_auth (
	unsigned_char_p_t		/* in  */    /*server_princ_name*/,
	rpc_auth_key_retrieval_fn_t	/* in  */    /*get_key_func*/,
	dce_pointer_t			/* in  */    /*arg*/,
	unsigned32			/* out */    * /*st*/
    );

INTERNAL void rpc__gssauth_mgt_inq_def (
	unsigned32			/* out */    * /*authn_level*/,
	unsigned32			/* out */    * /*st*/
    );

INTERNAL void rpc__gssauth_inq_my_princ_name (
	unsigned32			/* in */     /*princ_name_size*/,
	unsigned_char_p_t		/* out */    /*princ_name*/,
	unsigned32			/* out */    * /*st*/
    );

INTERNAL void rpc__gssauth_free_info (
	rpc_auth_info_p_t		/* in/out */ * /*info*/
    );

INTERNAL void rpc__gssauth_free_key (
	rpc_key_info_p_t		/* in/out */ * /*info*/
    );

INTERNAL error_status_t rpc__gssauth_resolve_identity (
	rpc_auth_identity_handle_t	/* in */     /* in_identity*/,
	rpc_auth_identity_handle_t	/* out */    * /*out_identity*/
    );

INTERNAL void rpc__gssauth_release_identity (
	rpc_auth_identity_handle_t	/* in/out */ * /*identity*/
    );

INTERNAL void rpc__gssauth_inq_sec_context (
	rpc_auth_info_p_t		/* in */     /*auth_info*/,
	void				/* out */    ** /*mech_context*/,
	unsigned32			/* out */    * /*stp*/
    );

INTERNAL void rpc__gssauth_inq_access_token(
    rpc_auth_info_p_t auth_info,
    rpc_access_token_p_t* token,
    unsigned32 *stp
    );

INTERNAL rpc_auth_epv_t rpc_g_gssauth_negotiate_epv =
{
	rpc__gssauth_negotiate_bnd_set_auth,
	rpc__gssauth_srv_reg_auth,
	rpc__gssauth_mgt_inq_def,
	rpc__gssauth_inq_my_princ_name,
	rpc__gssauth_free_info,
	rpc__gssauth_free_key,
	rpc__gssauth_resolve_identity,
	rpc__gssauth_release_identity,
	rpc__gssauth_inq_sec_context,
        rpc__gssauth_inq_access_token
};

INTERNAL rpc_auth_epv_t rpc_g_gssauth_mskrb_epv =
{
	rpc__gssauth_mskrb_bnd_set_auth,
	rpc__gssauth_srv_reg_auth,
	rpc__gssauth_mgt_inq_def,
	rpc__gssauth_inq_my_princ_name,
	rpc__gssauth_free_info,
	rpc__gssauth_free_key,
	rpc__gssauth_resolve_identity,
	rpc__gssauth_release_identity,
	rpc__gssauth_inq_sec_context,
        rpc__gssauth_inq_access_token
};

INTERNAL rpc_auth_epv_t rpc_g_gssauth_winnt_epv =
{
	rpc__gssauth_winnt_bnd_set_auth,
	rpc__gssauth_srv_reg_auth,
	rpc__gssauth_mgt_inq_def,
	rpc__gssauth_inq_my_princ_name,
	rpc__gssauth_free_info,
	rpc__gssauth_free_key,
	rpc__gssauth_resolve_identity,
	rpc__gssauth_release_identity,
	rpc__gssauth_inq_sec_context,
        rpc__gssauth_inq_access_token
};

INTERNAL rpc_auth_epv_t rpc_g_gssauth_netlogon_epv =
{
	rpc__gssauth_netlogon_bnd_set_auth,
	rpc__gssauth_srv_reg_auth,
	rpc__gssauth_mgt_inq_def,
	rpc__gssauth_inq_my_princ_name,
	rpc__gssauth_free_info,
	rpc__gssauth_free_key,
	rpc__gssauth_resolve_identity,
	rpc__gssauth_release_identity,
	rpc__gssauth_inq_sec_context,
        rpc__gssauth_inq_access_token
};

/*
 * R P C _ _ G S S A U T H _ B N D _ S E T _ A U T H
 *
 */

INTERNAL void rpc__gssauth_bnd_set_auth
(
	unsigned_char_p_t server_name,
	rpc_authn_level_t level,
	rpc_authn_protocol_id_t authn_protocol,
	rpc_auth_identity_handle_t auth_ident,
	rpc_authz_protocol_id_t authz_prot,
	rpc_binding_handle_t binding_h,
	rpc_auth_info_p_t *infop,
	unsigned32 *stp
)
{
	unsigned32 st;
	rpc_gssauth_info_p_t gssauth_info;
	unsigned_char_p_t str_server_name;
	gss_name_t gss_server_name;
	OM_uint32 maj_stat;
	OM_uint32 min_stat;

	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_bnd_set_auth)\n"));

	rpc_g_gssauth_alloc_count++;
	RPC_MEM_ALLOC(gssauth_info,
		      rpc_gssauth_info_p_t,
		      sizeof (*gssauth_info),
		      RPC_C_MEM_GSSAUTH_INFO,
		      RPC_C_MEM_WAITOK);
	memset (gssauth_info, 0, sizeof(*gssauth_info));

	if ((authz_prot != rpc_c_authz_name) &&
	    (authz_prot != rpc_c_authz_gss_name)) {
		st = rpc_s_authn_authz_mismatch;
		goto poison;
	}

	if ((level != rpc_c_authn_level_connect) &&
	    (level != rpc_c_authn_level_pkt_integrity) &&
	    (level != rpc_c_authn_level_pkt_privacy)) {
		st = rpc_s_unsupported_authn_level;
		goto poison;
	}

	/*
	 * If no server principal name was specified, go ask for it.
	 */
	if (authz_prot == rpc_c_authz_name) {
		gss_buffer_desc input_name;

		if (server_name == NULL) {
			rpc_mgmt_inq_server_princ_name(binding_h,
						       authn_protocol,
						       &str_server_name,
						       &st);
			if (st != rpc_s_ok) {
				goto poison;
			}
		} else {
			str_server_name = rpc_stralloc(server_name);
		}

		input_name.value = (void *)str_server_name;
		input_name.length = strlen((char *)str_server_name);

		maj_stat = gss_import_name(&min_stat,
					   &input_name,
					   GSS_KRB5_NT_PRINCIPAL_NAME,
					   &gss_server_name);
		if (GSS_ERROR(maj_stat)) {
			char msg[256];
			rpc__gssauth_error_map(maj_stat, min_stat, GSS_C_NO_OID,
					       msg, sizeof(msg), &st);
			RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_GENERAL,
				("(rpc__gssauth_bnd_set_auth): import: %s\n", msg));
			goto poison;
		}
	} else if (authz_prot == rpc_c_authz_gss_name) {
		gss_buffer_desc output_name;

		gss_server_name = (gss_name_t)server_name;
		server_name = NULL;

		if (gss_server_name == GSS_C_NO_NAME) {
			/*
			 * the caller passes GSS_C_NO_NAME, we'll pass it down
			 * later, if the caller wants an autolookup
			 * rpc_c_authz_name should be used
			 */
			gss_server_name = GSS_C_NO_NAME;
			str_server_name = NULL;
		} else {
			maj_stat = gss_duplicate_name(&min_stat,
						      gss_server_name,
						      &gss_server_name);
			if (maj_stat != GSS_S_COMPLETE) {
				char msg[256];
				rpc__gssauth_error_map(maj_stat, min_stat, GSS_C_NO_OID,
						       msg, sizeof(msg), &st);
				RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_GENERAL,
					("(rpc__gssauth_bnd_set_auth): duplicate: %s\n", msg));
				goto poison;
			}

			maj_stat = gss_display_name(&min_stat,
						    gss_server_name,
						    &output_name,
						    NULL);
			if (maj_stat != GSS_S_COMPLETE) {
				char msg[256];
				rpc__gssauth_error_map(maj_stat, min_stat, GSS_C_NO_OID,
						       msg, sizeof(msg), &st);
				RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_GENERAL,
					("(rpc__gssauth_bnd_set_auth): display: %s\n", msg));
				goto poison;
			}

			RPC_MEM_ALLOC(str_server_name,
				      unsigned_char_p_t,
				      output_name.length + 1,
				      RPC_C_MEM_STRING,
				      RPC_C_MEM_WAITOK);
			rpc__strncpy(str_server_name,
				     output_name.value,
				     output_name.length);

			gss_release_buffer(&min_stat, &output_name);
		}
	}

	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_GENERAL,
		("(rpc__gssauth_bnd_set_auth) %p created (now %d active)\n",
		gssauth_info, rpc_g_gssauth_alloc_count - rpc_g_gssauth_free_count));

	if (auth_ident != NULL) {
		gss_OID desired_mech = GSS_C_NO_OID;

		rpc__gssauth_select_mech(&min_stat, authn_protocol, &desired_mech);

		assert(gssauth_info->gss_creds == GSS_C_NO_CREDENTIAL);

		maj_stat = gss_add_cred(&min_stat,
					(const gss_cred_id_t)auth_ident,
					GSS_C_NO_NAME,
					desired_mech,
					GSS_C_INITIATE,
					GSS_C_INDEFINITE,
					GSS_C_INDEFINITE,
					&gssauth_info->gss_creds,
					NULL,
					NULL,
					NULL);
		if (GSS_ERROR(maj_stat)) {
			char msg[256];
			rpc__gssauth_error_map(maj_stat, min_stat, GSS_C_NO_OID,
					       msg, sizeof(msg), &st);
			RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_GENERAL,
				("(rpc__gssauth_bnd_set_auth): add_cred: %s\n", msg));
			goto poison;
		}
	}

	gssauth_info->gss_server_name = gss_server_name;
	gssauth_info->auth_info.server_princ_name = str_server_name;
	gssauth_info->auth_info.authn_level = level;
	gssauth_info->auth_info.authn_protocol = authn_protocol;
	gssauth_info->auth_info.authz_protocol = authz_prot;
	gssauth_info->auth_info.is_server = false;
	gssauth_info->auth_info.u.auth_identity = (rpc_auth_identity_handle_t)gssauth_info->gss_creds;

	gssauth_info->auth_info.refcount = 1;

	*infop = &gssauth_info->auth_info;
	*stp = rpc_s_ok;
	return;

poison:
	*infop = &gssauth_info->auth_info;
	*stp = st;
	return;
}

INTERNAL void rpc__gssauth_negotiate_bnd_set_auth
(
	unsigned_char_p_t server_name,
	rpc_authn_level_t level,
	rpc_auth_identity_handle_t auth_ident,
	rpc_authz_protocol_id_t authz_prot,
	rpc_binding_handle_t binding_h,
	rpc_auth_info_p_t *infop,
	unsigned32 *stp
)
{
	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_negotiate_bnd_set_auth)\n"));

	rpc__gssauth_bnd_set_auth(server_name,
				  level,
				  rpc_c_authn_gss_negotiate,
				  auth_ident,
				  authz_prot,
				  binding_h,
				  infop,
				  stp);
}

INTERNAL void rpc__gssauth_mskrb_bnd_set_auth
(
	unsigned_char_p_t server_name,
	rpc_authn_level_t level,
	rpc_auth_identity_handle_t auth_ident,
	rpc_authz_protocol_id_t authz_prot,
	rpc_binding_handle_t binding_h,
	rpc_auth_info_p_t *infop,
	unsigned32 *stp
)
{
	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_mskrb_bnd_set_auth)\n"));

	rpc__gssauth_bnd_set_auth(server_name,
				  level,
				  rpc_c_authn_gss_mskrb,
				  auth_ident,
				  authz_prot,
				  binding_h,
				  infop,
				  stp);
}

INTERNAL void rpc__gssauth_winnt_bnd_set_auth
(
	unsigned_char_p_t server_name,
	rpc_authn_level_t level,
	rpc_auth_identity_handle_t auth_ident,
	rpc_authz_protocol_id_t authz_prot,
	rpc_binding_handle_t binding_h,
	rpc_auth_info_p_t *infop,
	unsigned32 *stp
)
{
	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_winntmskrb_bnd_set_auth)\n"));

	rpc__gssauth_bnd_set_auth(server_name,
				  level,
				  rpc_c_authn_winnt,
				  auth_ident,
				  authz_prot,
				  binding_h,
				  infop,
				  stp);
}

INTERNAL void rpc__gssauth_netlogon_bnd_set_auth
(
	unsigned_char_p_t server_name,
	rpc_authn_level_t level,
	rpc_auth_identity_handle_t auth_ident,
	rpc_authz_protocol_id_t authz_prot,
	rpc_binding_handle_t binding_h,
	rpc_auth_info_p_t *infop,
	unsigned32 *stp
)
{
	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_netlogon_bnd_set_auth)\n"));

	rpc__gssauth_bnd_set_auth(server_name,
				  level,
				  rpc_c_authn_netlogon,
				  auth_ident,
				  authz_prot,
				  binding_h,
				  infop,
				  stp);
}

INTERNAL void rpc__gssauth_negotiate_init
(
	rpc_auth_epv_p_t *epv,
	rpc_auth_rpc_prot_epv_tbl_t *rpc_prot_epv,
	unsigned32 *st
)
{
	unsigned32		prot_id;
	rpc_auth_rpc_prot_epv_t *prot_epv;

	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_negotiate_init)\n"));

	/*
	 * Initialize the RPC-protocol-specific EPVs for the RPC protocols
	 * we work with (ncacn).
	 */
	/* for now only ncacn, that's what windows uses */
	prot_id = rpc__gssauth_negotiate_cn_init (&prot_epv, st);
	if (*st == rpc_s_ok) {
		rpc_g_gssauth_negotiate_rpc_prot_epv[prot_id] = prot_epv;
	}

	/*
	 * Return information for this gss_negotiate (SPNEGO) authentication service.
	 */
	*epv = &rpc_g_gssauth_negotiate_epv;
	*rpc_prot_epv = rpc_g_gssauth_negotiate_rpc_prot_epv;

	*st = 0;
}

INTERNAL void rpc__gssauth_mskrb_init
(
	rpc_auth_epv_p_t *epv,
	rpc_auth_rpc_prot_epv_tbl_t *rpc_prot_epv,
	unsigned32 *st
)
{
	unsigned32		prot_id;
	rpc_auth_rpc_prot_epv_t *prot_epv;

	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_mskrb_init)\n"));

	/*
	 * Initialize the RPC-protocol-specific EPVs for the RPC protocols
	 * we work with (ncacn).
	 */
	/* for now only ncacn, that's what windows uses */
	prot_id = rpc__gssauth_mskrb_cn_init (&prot_epv, st);
	if (*st == rpc_s_ok) {
		rpc_g_gssauth_mskrb_rpc_prot_epv[prot_id] = prot_epv;
	}

	/*
	 * Return information for this (KRB5) authentication service.
	 */
	*epv = &rpc_g_gssauth_mskrb_epv;
	*rpc_prot_epv = rpc_g_gssauth_mskrb_rpc_prot_epv;

	*st = 0;
}

INTERNAL void rpc__gssauth_winnt_init
(
	rpc_auth_epv_p_t *epv,
	rpc_auth_rpc_prot_epv_tbl_t *rpc_prot_epv,
	unsigned32 *st
)
{
	unsigned32		prot_id;
	rpc_auth_rpc_prot_epv_t *prot_epv;

	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_winnt_init)\n"));

	/*
	 * Initialize the RPC-protocol-specific EPVs for the RPC protocols
	 * we work with (ncacn).
	 */
	/* for now only ncacn, that's what windows uses */
	prot_id = rpc__gssauth_winnt_cn_init (&prot_epv, st);
	if (*st == rpc_s_ok) {
		rpc_g_gssauth_winnt_rpc_prot_epv[prot_id] = prot_epv;
	}

	/*
	 * Return information for this (KRB5) authentication service.
	 */
	*epv = &rpc_g_gssauth_winnt_epv;
	*rpc_prot_epv = rpc_g_gssauth_winnt_rpc_prot_epv;

	*st = 0;
}

INTERNAL void rpc__gssauth_netlogon_init
(
	rpc_auth_epv_p_t *epv,
	rpc_auth_rpc_prot_epv_tbl_t *rpc_prot_epv,
	unsigned32 *st
)
{
	unsigned32		prot_id;
	rpc_auth_rpc_prot_epv_t *prot_epv;

	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_netlogon_init)\n"));

	/*
	 * Initialize the RPC-protocol-specific EPVs for the RPC protocols
	 * we work with (ncacn).
	 */
	/* for now only ncacn, that's what windows uses */
	prot_id = rpc__gssauth_netlogon_cn_init (&prot_epv, st);
	if (*st == rpc_s_ok) {
		rpc_g_gssauth_netlogon_rpc_prot_epv[prot_id] = prot_epv;
	}

	/*
	 * Return information for this (KRB5) authentication service.
	 */
	*epv = &rpc_g_gssauth_netlogon_epv;
	*rpc_prot_epv = rpc_g_gssauth_netlogon_rpc_prot_epv;

	*st = 0;
}
/*
 * R P C _ _ G S S A U T H _ F R E E _ I N F O
 *
 * Free info.
 */

INTERNAL void rpc__gssauth_free_info
(
	rpc_auth_info_p_t *info
)
{
	rpc_gssauth_info_p_t gssauth_info = (rpc_gssauth_info_p_t)*info ;
	const char *info_type;

	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_free_info)\n"));

	info_type = (*info)->is_server?"server":"client";

	if (gssauth_info->auth_info.server_princ_name != NULL) {
		unsigned32 st;
		rpc_string_free(&gssauth_info->auth_info.server_princ_name, &st);
	}

	if (gssauth_info->gss_server_name != GSS_C_NO_NAME) {
		OM_uint32 min_stat;
		gss_release_name(&min_stat, &gssauth_info->gss_server_name);
		gssauth_info->gss_server_name = GSS_C_NO_NAME;
	}

	if (gssauth_info->gss_creds != GSS_C_NO_CREDENTIAL) {
		OM_uint32 min_stat;
		gss_release_cred(&min_stat, &gssauth_info->gss_creds);
		gssauth_info->gss_creds = GSS_C_NO_CREDENTIAL;
	}

	memset(gssauth_info, 0x69, sizeof(*gssauth_info));
	RPC_MEM_FREE(gssauth_info, RPC_C_MEM_GSSAUTH_INFO);

	rpc_g_gssauth_free_count++;

	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_GENERAL,
		("(rpc__gssauth_free_info) freeing %s auth_info (now %d active).\n",
		info_type, rpc_g_gssauth_alloc_count - rpc_g_gssauth_free_count));

	*info = NULL;
}

/*
 * R P C _ _ G S S A U T H _ M G T _ I N Q _ D E F
 *
 * Return default authentication level
 *
 * !!! should read this from a config file.
 */

INTERNAL void rpc__gssauth_mgt_inq_def
(
	unsigned32 *authn_level,
	unsigned32 *stp
)
{
	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_mgt_inq_def)\n"));

	*authn_level = rpc_c_authn_level_pkt_privacy;
	*stp = rpc_s_ok;
}

/*
 * R P C _ _ G S S A U T H _ S R V _ R E G _ A U T H
 *
 */

INTERNAL void rpc__gssauth_srv_reg_auth
(
	unsigned_char_p_t server_name ATTRIBUTE_UNUSED,
	rpc_auth_key_retrieval_fn_t get_key_func ATTRIBUTE_UNUSED,
	dce_pointer_t arg ATTRIBUTE_UNUSED,
	unsigned32 *stp
)
{
	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_srv_reg_auth)\n"));

	*stp = rpc_s_ok;
}

/*
 * R P C _ _ G S S A U T H _ I N Q _ M Y _ P R I N C _ N A M E
 *
 * All this doesn't matter for this module, but we need the placebo.
 */

INTERNAL void rpc__gssauth_inq_my_princ_name
(
	unsigned32 name_size,
	unsigned_char_p_t name,
	unsigned32 *stp
)
{
	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_inq_my_princ_name)\n"));

	if (name_size > 0) {
		rpc__strncpy(name, (unsigned char *)"", name_size - 1);
	}
	*stp = rpc_s_ok;
}

/*
 * R P C _ _ G S S A U T H _ F R E E _ KEY
 *
 * Free key.
 */

INTERNAL void rpc__gssauth_free_key
(
	rpc_key_info_p_t *info ATTRIBUTE_UNUSED
)
{
	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_free_key)\n"));
}

/*
 * R P C _ _ G S S A U T H _ R E S O L V E _ I D E N T I T Y
 *
 * Resolve identity.
 */

INTERNAL error_status_t rpc__gssauth_resolve_identity
(
	rpc_auth_identity_handle_t in_identity,
	rpc_auth_identity_handle_t *out_identity
)
{
	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_resolve_identity)\n"));

	*out_identity = in_identity;
	return 0;
}

/*
 * R P C _ _ G S S A U T H _ R E L E A S E _ I D E N T I T Y
 *
 * Release identity.
 */

INTERNAL void rpc__gssauth_release_identity
(
	rpc_auth_identity_handle_t *identity ATTRIBUTE_UNUSED
)
{
	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_release_identity)\n"));
}

void rpc__gssauth_init_func(void)
{
	static rpc_authn_protocol_id_elt_t auth[4] = {
	{ /* 0 */
		rpc__gssauth_negotiate_init,
		rpc_c_authn_gss_negotiate,
		dce_c_rpc_authn_protocol_gss_negotiate,
		NULL,
		rpc_g_gssauth_negotiate_rpc_prot_epv
	},
	{ /* 1 */
		rpc__gssauth_mskrb_init,
		rpc_c_authn_gss_mskrb,
		dce_c_rpc_authn_protocol_gss_mskrb,
		NULL,
		rpc_g_gssauth_mskrb_rpc_prot_epv
	},
	{ /* 2 */
		rpc__gssauth_winnt_init,
		rpc_c_authn_winnt,
		dce_c_rpc_authn_protocol_winnt,
		NULL,
		rpc_g_gssauth_winnt_rpc_prot_epv
	},
	{ /* 3 */
		rpc__gssauth_netlogon_init,
		rpc_c_authn_netlogon,
		dce_c_rpc_authn_protocol_netlogon,
		NULL,
		rpc_g_gssauth_netlogon_rpc_prot_epv
	}
	};

	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__module_init_func)\n"));

	rpc__register_authn_protocol(auth, sizeof(auth)/sizeof(auth[0]));
}

/*
 * R P C _ _ G S S A U T H _ I N Q _ S E C _ C O N T E X T
 *
 * Inq sec context.
 */

INTERNAL void rpc__gssauth_inq_sec_context
(
	rpc_auth_info_p_t auth_info,
	void **mech_context,
	unsigned32 *stp
)
{
	rpc_gssauth_info_p_t gssauth_info = NULL;
	rpc_gssauth_cn_info_p_t gssauth_cn_info = NULL;

	RPC_DBG_PRINTF(rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
		("(rpc__gssauth_inq_sec_context)\n"));

	gssauth_info = (rpc_gssauth_info_p_t)auth_info;
	gssauth_cn_info = gssauth_info->cn_info;

	*mech_context = (void*)gssauth_cn_info->gss_ctx;
	*stp = rpc_s_ok;
}

INTERNAL void rpc__gssauth_inq_access_token(
    rpc_auth_info_p_t auth_info ATTRIBUTE_UNUSED,
    rpc_access_token_p_t* token,
    unsigned32 *stp
    )
{
	*token = NULL;
	*stp = rpc_s_not_supported;
}

static struct {
	rpc_authn_protocol_id_t authn_protocol;
	gss_OID_desc gss_oid;
} rpc__gssauth_mechanisms[] = {
	{	/* SPNEGO mechanism */
		rpc_c_authn_gss_negotiate,
		{ 6, (void *)"\053\006\001\005\005\002" },
	},
	{	/* Kerberos mechanism */
		rpc_c_authn_gss_mskrb,
		{ 9, (void *)"\052\206\110\206\367\022\001\002\002" },
	},
	{	/* NTLM mechanism */
		rpc_c_authn_winnt,
		{ 10, (void *)"\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a" },
	},
	{	/* NetLogon secure channel mechanism (private) */
		rpc_c_authn_netlogon,
		{ 6, (void *)"\x2a\x85\x70\x2b\x0e\x02" },
	},
};

PRIVATE OM_uint32 rpc__gssauth_select_mech
(
	OM_uint32		*min_stat,
	rpc_authn_protocol_id_t	authn_protocol,
	gss_OID			*req_mech
)
{
	gss_OID selected_mech = GSS_C_NO_OID;
	size_t i;

	*min_stat = 0;

	for (i = 0;
	     i < sizeof(rpc__gssauth_mechanisms)/sizeof(rpc__gssauth_mechanisms[0]);
	     i++)
	{
		if (rpc__gssauth_mechanisms[i].authn_protocol == authn_protocol) {
			selected_mech = &rpc__gssauth_mechanisms[i].gss_oid;
			break;
		}
	}

	if (selected_mech == GSS_C_NO_OID)
		return GSS_S_UNAVAILABLE;

	*req_mech = selected_mech;
	return GSS_S_COMPLETE;
}

#endif /* defined(AUTH_GSS_NEGOTIATE) && AUTH_GSS_NEGOTIATE */
