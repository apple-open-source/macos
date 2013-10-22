/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ntlm.h"

#ifdef ENABLE_NTLM

int __gss_ntlm_is_digest_service = 0;

static int
get_signing_supported(gss_const_OID mech, gss_mo_desc *mo, gss_buffer_t value)
{
    OM_uint32 major, minor;
    ntlm_ctx ctx;
    int def = 0;

    if (!__gss_ntlm_is_digest_service) {

	major = _gss_ntlm_allocate_ctx(&minor, NULL, &ctx);
	if (major == GSS_S_COMPLETE) {
	    gss_ctx_id_t gctx = (gss_ctx_id_t)ctx;
	    
	    if ((ctx->probe_flags & NSI_NO_SIGNING) == 0)
		def = 1;
	    
	    _gss_ntlm_delete_sec_context(&minor, &gctx, NULL);
	}
    }
    if (!def)
	return _gss_mo_get_option_0(mech, mo, value);

    return _gss_mo_get_option_1(mech, mo, value);
}



static gss_mo_desc _gssntlm_mech_options[] = {
    {
	GSS_C_NTLM_V1,
	GSS_MO_MA,
	"NTLMv1",
	NULL,
	_gss_mo_get_option_1
    },
    {
	GSS_C_NTLM_FORCE_V1,
	GSS_MO_MA,
	"Force NTLMv1",
	NULL,
	_gss_mo_get_option_0
    },
    {
	GSS_C_NTLM_V2,
	GSS_MO_MA,
	"NTLMv2",
	NULL,
	_gss_mo_get_option_1
    },
    {
	GSS_C_NTLM_SESSION_KEY,
	GSS_MO_MA,
	"NTLM session key",
	NULL,
	get_signing_supported
    },
    {
	GSS_C_NTLM_SUPPORT_CHANNELBINDINGS,
	GSS_MO_MA,
	"NTLM support channel bindings",
	NULL,
	_gss_mo_get_option_1
    },
    {
	GSS_C_NTLM_SUPPORT_LM2,
	GSS_MO_MA,
	"NTLM support LM2",
	NULL,
	_gss_mo_get_option_1
    }
};

static gssapi_mech_interface_desc ntlm_mech = {
    GMI_VERSION,
    "ntlm",
    {10, rk_UNCONST("\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a") },
    0,
    _gss_ntlm_acquire_cred,
    _gss_ntlm_release_cred,
    _gss_ntlm_init_sec_context,
    _gss_ntlm_accept_sec_context,
    _gss_ntlm_process_context_token,
    _gss_ntlm_delete_sec_context,
    _gss_ntlm_context_time,
    _gss_ntlm_get_mic,
    _gss_ntlm_verify_mic,
    _gss_ntlm_wrap,
    _gss_ntlm_unwrap,
    NULL,
    NULL,
    _gss_ntlm_compare_name,
    _gss_ntlm_display_name,
    _gss_ntlm_import_name,
    _gss_ntlm_export_name,
    _gss_ntlm_release_name,
    _gss_ntlm_inquire_cred,
    _gss_ntlm_inquire_context,
    _gss_ntlm_wrap_size_limit,
    _gss_ntlm_add_cred,
    _gss_ntlm_inquire_cred_by_mech,
    _gss_ntlm_export_sec_context,
    _gss_ntlm_import_sec_context,
    _gss_ntlm_inquire_names_for_mech,
    _gss_ntlm_inquire_mechs_for_name,
    _gss_ntlm_canonicalize_name,
    _gss_ntlm_duplicate_name,
    _gss_ntlm_inquire_sec_context_by_oid,
    NULL,
    NULL,
    NULL,
    NULL,
    _gss_ntlm_wrap_iov,
    _gss_ntlm_unwrap_iov,
    _gss_ntlm_wrap_iov_length,
    NULL,
    NULL,
    NULL,
    _gss_ntlm_acquire_cred_ext,
    _gss_ntlm_iter_creds_f,
    _gss_ntlm_destroy_cred,
    _gss_ntlm_cred_hold,
    _gss_ntlm_cred_unhold,
    _gss_ntlm_cred_label_get,
    _gss_ntlm_cred_label_set,
    _gssntlm_mech_options,
    sizeof(_gssntlm_mech_options) / sizeof(_gssntlm_mech_options[0])
};

#endif

gssapi_mech_interface
__gss_ntlm_initialize(void)
{
#ifdef ENABLE_NTLM
	return &ntlm_mech;
#else
	return NULL;
#endif
}

/*
 * Binary compat, thse version are missing the trailer "_oid_desc"
 * that the autogenerged version have.
 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_ntlm_v1 =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x19")};
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_ntlm_v2 =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x1a")};
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_ntlm_session_key =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x1b")};
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_ntlm_force_v1 =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x1c")};
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_ntlm_support_channelbindings =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x1d")};
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_ntlm_support_lm2 =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x1f")};

