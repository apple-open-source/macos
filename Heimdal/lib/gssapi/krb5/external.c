/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 - 2010 Apple Inc. All rights reserved.
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

#include "gsskrb5_locl.h"
#include <gssapi_mech.h>

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 *              "\x01\x02\x01\x01"},
 * corresponding to an object-identifier value of
 * {iso(1) member-body(2) United States(840) mit(113554)
 *  infosys(1) gssapi(2) generic(1) user_name(1)}.  The constant
 * GSS_C_NT_USER_NAME should be initialized to point
 * to that gss_OID_desc.
 */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_nt_user_name_oid_desc =
    {10, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12" "\x01\x02\x01\x01")};

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 *              "\x01\x02\x01\x02"},
 * corresponding to an object-identifier value of
 * {iso(1) member-body(2) United States(840) mit(113554)
 *  infosys(1) gssapi(2) generic(1) machine_uid_name(2)}.
 * The constant GSS_C_NT_MACHINE_UID_NAME should be
 * initialized to point to that gss_OID_desc.
 */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_nt_machine_uid_name_oid_desc =
    {10, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12" "\x01\x02\x01\x02")};

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 *              "\x01\x02\x01\x03"},
 * corresponding to an object-identifier value of
 * {iso(1) member-body(2) United States(840) mit(113554)
 *  infosys(1) gssapi(2) generic(1) string_uid_name(3)}.
 * The constant GSS_C_NT_STRING_UID_NAME should be
 * initialized to point to that gss_OID_desc.
 */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_nt_string_uid_name_oid_desc =
    {10, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12" "\x01\x02\x01\x03")};

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {6, (void *)"\x2b\x06\x01\x05\x06\x02"},
 * corresponding to an object-identifier value of
 * {iso(1) org(3) dod(6) internet(1) security(5)
 * nametypes(6) gss-host-based-services(2)).  The constant
 * GSS_C_NT_HOSTBASED_SERVICE_X should be initialized to point
 * to that gss_OID_desc.  This is a deprecated OID value, and
 * implementations wishing to support hostbased-service names
 * should instead use the GSS_C_NT_HOSTBASED_SERVICE OID,
 * defined below, to identify such names;
 * GSS_C_NT_HOSTBASED_SERVICE_X should be accepted a synonym
 * for GSS_C_NT_HOSTBASED_SERVICE when presented as an input
 * parameter, but should not be emitted by GSS-API
 * implementations
 */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_nt_hostbased_service_x_oid_desc =
    {6, rk_UNCONST("\x2b\x06\x01\x05\x06\x02")};

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 *              "\x01\x02\x01\x04"}, corresponding to an
 * object-identifier value of {iso(1) member-body(2)
 * Unites States(840) mit(113554) infosys(1) gssapi(2)
 * generic(1) service_name(4)}.  The constant
 * GSS_C_NT_HOSTBASED_SERVICE should be initialized
 * to point to that gss_OID_desc.
 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_nt_hostbased_service_oid_desc =
    {10, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12" "\x01\x02\x01\x04")};

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {6, (void *)"\x2b\x06\01\x05\x06\x03"},
 * corresponding to an object identifier value of
 * {1(iso), 3(org), 6(dod), 1(internet), 5(security),
 * 6(nametypes), 3(gss-anonymous-name)}.  The constant
 * and GSS_C_NT_ANONYMOUS should be initialized to point
 * to that gss_OID_desc.
 */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_nt_anonymous_oid_desc =
    {6, rk_UNCONST("\x2b\x06\01\x05\x06\x03")};

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {6, (void *)"\x2b\x06\x01\x05\x06\x04"},
 * corresponding to an object-identifier value of
 * {1(iso), 3(org), 6(dod), 1(internet), 5(security),
 * 6(nametypes), 4(gss-api-exported-name)}.  The constant
 * GSS_C_NT_EXPORT_NAME should be initialized to point
 * to that gss_OID_desc.
 */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_nt_export_name_oid_desc =
    {6, rk_UNCONST("\x2b\x06\x01\x05\x06\x04") };

/*
 *
 */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_nt_dn_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x16")}; /* 1.2.752.43.13.22 */

/*
 *   This name form shall be represented by the Object Identifier {iso(1)
 *   member-body(2) United States(840) mit(113554) infosys(1) gssapi(2)
 *   krb5(2) krb5_name(1)}.  The recommended symbolic name for this type
 *   is "GSS_KRB5_NT_PRINCIPAL_NAME".
 */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_nt_principal_name_oid_desc =
    {10, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x01") };

/*
 * Do not use
 */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_nt_principal_oid_desc =
    {10, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x02") };

/*
 *   This name form shall be represented by the Object Identifier {iso(1)
 *   member-body(2) United States(840) mit(113554) infosys(1) gssapi(2)
 *   generic(1) user_name(1)}.  The recommended symbolic name for this
 *   type is "GSS_KRB5_NT_USER_NAME".
 */

/*
 *   This name form shall be represented by the Object Identifier {iso(1)
 *   member-body(2) United States(840) mit(113554) infosys(1) gssapi(2)
 *   generic(1) machine_uid_name(2)}.  The recommended symbolic name for
 *   this type is "GSS_KRB5_NT_MACHINE_UID_NAME".
 */

/*
 *   This name form shall be represented by the Object Identifier {iso(1)
 *   member-body(2) United States(840) mit(113554) infosys(1) gssapi(2)
 *   generic(1) string_uid_name(3)}.  The recommended symbolic name for
 *   this type is "GSS_KRB5_NT_STRING_UID_NAME".
 */

/*
 *   To support ongoing experimentation, testing, and evolution of the
 *   specification, the Kerberos V5 GSS-API mechanism as defined in this
 *   and any successor memos will be identified with the following Object
 *   Identifier, as defined in RFC-1510, until the specification is
 *   advanced to the level of Proposed Standard RFC:
 *
 *   {iso(1), org(3), dod(5), internet(1), security(5), kerberosv5(2)}
 *
 *   Upon advancement to the level of Proposed Standard RFC, the Kerberos
 *   V5 GSS-API mechanism will be identified by an Object Identifier
 *   having the value:
 *
 *   {iso(1) member-body(2) United States(840) mit(113554) infosys(1)
 *   gssapi(2) krb5(2)}
 */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_mechanism_oid_desc =
    {9, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12\x01\x02\x02") };

/*
 * IAKERB
 * { iso(1) org(3) dod(6) internet(1) security(5) kerberosV5(2)
 *   iakerb(5) }
 */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_iakerb_mechanism_oid_desc =
    {6, rk_UNCONST("\x2b\x06\x01\x05\x02\x05")};

/*
 * PK-U2U
 * { iso(1) org(3) dod(6) internet(1) security(5) kerberosV5(2) pku2u(7) }
 */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_pku2u_mechanism_oid_desc =
    {6, rk_UNCONST("\x2b\x06\x01\x05\x02\x07")};

/*
 *
 */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_peer_has_updated_spnego_oid_desc =
    {9, (void *)"\x2b\x06\x01\x04\x01\xa9\x4a\x13\x05"};

/*
 * 1.2.752.43.13 Heimdal GSS-API Extentions
 */

/* 1.2.752.43.13.1 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_copy_ccache_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x01")};

/* 1.2.752.43.13.2 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_get_tkt_flags_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x02")};

/* 1.2.752.43.13.3 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_extract_authz_data_from_sec_context_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x03")};

/* 1.2.752.43.13.4 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_compat_des3_mic_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x04")};

/* 1.2.752.43.13.5 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_register_acceptor_identity_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x05")};

/* 1.2.752.43.13.6 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_export_lucid_context_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x06")};

/* 1.2.752.43.13.6.1 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_export_lucid_context_v1_x_oid_desc =
    {7, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x06\x01")};

/* 1.2.752.43.13.7 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_set_dns_canonicalize_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x07")};

/* 1.2.752.43.13.8 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_get_subkey_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x08")};

/* 1.2.752.43.13.9 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_get_initiator_subkey_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x09")};

/* 1.2.752.43.13.10 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_get_acceptor_subkey_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x0a")};

/* 1.2.752.43.13.11 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_send_to_kdc_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x0b")};

/* 1.2.752.43.13.12 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_get_authtime_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x0c")};

/* 1.2.752.43.13.13 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_get_service_keyblock_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x0d")};

/* 1.2.752.43.13.14 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_set_allowable_enctypes_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x0e")};

/* 1.2.752.43.13.15 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_set_default_realm_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x0f")};

/* 1.2.752.43.13.16 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_ccache_name_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x10")};

/* 1.2.752.43.13.17 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_set_time_offset_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x11")};

/* 1.2.752.43.13.18 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_get_time_offset_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x12")};

/* 1.2.752.43.13.19 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_plugin_register_x_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x13")};

/* 1.2.752.43.13.20 GSS_NTLM_GET_SESSION_KEY_X */
/* 1.2.752.43.13.21 GSS_C_NT_NTLM */
/* 1.2.752.43.13.22 GSS_C_NT_DN */
/* 1.2.752.43.13.23 GSS_KRB5_NT_PRINCIPAL_NAME_REFERRAL */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_krb5_nt_principal_name_referral_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x17")};

/* 1.2.752.43.13.24 GSS_C_NTLM_AVGUEST */
/* 1.2.752.43.13.25 GSS_C_NTLM_V1 */
/* 1.2.752.43.13.26 GSS_C_NTLM_V2 */
/* 1.2.752.43.13.27 GSS_C_NTLM_SESSION_KEY */
/* 1.2.752.43.13.28 GSS_C_NTLM_FORCE_V1 */
/* 1.2.752.43.13.29 GSS_C_NTLM_SUPPORT_CHANNELBINDINGS */

/* 1.2.752.43.13.30 GSS_C_NT_UUID */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_nt_uuid_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x1e")};

/* 1.2.752.43.13.31 GSS_C_NTLM_SUPPORT_LM2 */

/* 1.2.752.43.14.1 */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_sasl_digest_md5_mechanism_oid_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0e\x01") };

/* 1.2.752.43.14.2 - netlogon ssp -- replaced by msft oid */

gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_inq_sspi_session_key_oid_desc =
    {11, (void *)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x05"};

/* 1.2.752.43.14.3 supports LKDC */
gss_OID_desc GSSAPI_LIB_VARIABLE __gss_appl_lkdc_supported_desc =
    {6, rk_UNCONST("\x2a\x85\x70\x2b\x0e\x03") };



/*
 * Context for krb5 calls.
 */

/*
 *
 */

static gssapi_mech_interface_desc krb5_mech = {
    GMI_VERSION,
    "kerberos 5",
    {9, "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02" },
    0,
    _gsskrb5_acquire_cred,
    _gsskrb5_release_cred,
    _gsskrb5_init_sec_context,
    _gsskrb5_accept_sec_context,
    _gsskrb5_process_context_token,
    _gsskrb5_delete_sec_context,
    _gsskrb5_context_time,
    _gsskrb5_get_mic,
    _gsskrb5_verify_mic,
    _gsskrb5_wrap,
    _gsskrb5_unwrap,
    _gsskrb5_display_status,
    NULL,
    _gsskrb5_compare_name,
    _gsskrb5_display_name,
    _gsskrb5_import_name,
    _gsskrb5_export_name,
    _gsskrb5_release_name,
    _gsskrb5_inquire_cred,
    _gsskrb5_inquire_context,
    _gsskrb5_wrap_size_limit,
    _gsskrb5_add_cred,
    _gsskrb5_inquire_cred_by_mech,
    _gsskrb5_export_sec_context,
    _gsskrb5_import_sec_context,
    _gsskrb5_inquire_names_for_mech,
    _gsskrb5_inquire_mechs_for_name,
    _gsskrb5_canonicalize_name,
    _gsskrb5_duplicate_name,
    _gsskrb5_inquire_sec_context_by_oid,
    _gsskrb5_inquire_cred_by_oid,
    _gsskrb5_set_sec_context_option,
    _gsskrb5_set_cred_option,
    _gsskrb5_pseudo_random,
    _gk_wrap_iov,
    _gk_unwrap_iov,
    _gk_wrap_iov_length,
    _gsskrb5_store_cred,
    _gsskrb5_export_cred,
    _gsskrb5_import_cred,
    _gss_krb5_acquire_cred_ex,
    _gss_krb5_iter_creds_f,
    _gsskrb5_destroy_cred,
    _gsskrb5_cred_hold,
    _gsskrb5_cred_unhold,
    _gsskrb5_cred_label_get,
    _gsskrb5_cred_label_set,
    NULL,
    0
};

static gssapi_mech_interface_desc iakerb_mech = {
    GMI_VERSION,
    "iakerb",
    {6, "\x2b\x06\x01\x05\x02\x05" },
    0,
    _gssiakerb_acquire_cred,
    _gsskrb5_release_cred,
    _gsskrb5_init_sec_context,
    _gssiakerb_accept_sec_context,
    _gsskrb5_process_context_token,
    _gsskrb5_delete_sec_context,
    _gsskrb5_context_time,
    _gsskrb5_get_mic,
    _gsskrb5_verify_mic,
    _gsskrb5_wrap,
    _gsskrb5_unwrap,
    _gsskrb5_display_status,
    NULL,
    _gsskrb5_compare_name,
    _gsskrb5_display_name,
    _gssiakerb_import_name,
    _gssiakerb_export_name,
    _gsskrb5_release_name,
    _gsskrb5_inquire_cred,
    _gsskrb5_inquire_context,
    _gsskrb5_wrap_size_limit,
    _gsskrb5_add_cred,
    _gsskrb5_inquire_cred_by_mech,
    _gsskrb5_export_sec_context,
    _gsskrb5_import_sec_context,
    _gssiakerb_inquire_names_for_mech,
    _gsskrb5_inquire_mechs_for_name,
    _gsskrb5_canonicalize_name,
    _gsskrb5_duplicate_name,
    _gsskrb5_inquire_sec_context_by_oid,
    _gsskrb5_inquire_cred_by_oid,
    _gsskrb5_set_sec_context_option,
    _gsskrb5_set_cred_option,
    _gsskrb5_pseudo_random,
    _gk_wrap_iov,
    _gk_unwrap_iov,
    _gk_wrap_iov_length,
    _gsskrb5_store_cred,
    _gsskrb5_export_cred,
    _gsskrb5_import_cred,
    _gss_iakerb_acquire_cred_ex,
    _gss_iakerb_iter_creds_f,
    _gsskrb5_destroy_cred,
    _gsskrb5_cred_hold,
    _gsskrb5_cred_unhold,
    _gsskrb5_cred_label_get,
    _gsskrb5_cred_label_set,
    NULL,
    0
};


#ifdef PKINIT

static gssapi_mech_interface_desc pku2u_mech = {
    GMI_VERSION,
    "pku2u",
    {6, "\x2b\x05\x01\x05\x02\x07" },
    0,
    _gsspku2u_acquire_cred,
    _gsskrb5_release_cred,
    _gsskrb5_init_sec_context,
    _gsspku2u_accept_sec_context,
    _gsskrb5_process_context_token,
    _gsskrb5_delete_sec_context,
    _gsskrb5_context_time,
    _gsskrb5_get_mic,
    _gsskrb5_verify_mic,
    _gsskrb5_wrap,
    _gsskrb5_unwrap,
    _gsskrb5_display_status,
    NULL,
    _gsskrb5_compare_name,
    _gsskrb5_display_name,
    _gsspku2u_import_name,
    _gsspku2u_export_name,
    _gsskrb5_release_name,
    _gsskrb5_inquire_cred,
    _gsskrb5_inquire_context,
    _gsskrb5_wrap_size_limit,
    _gsskrb5_add_cred,
    _gsskrb5_inquire_cred_by_mech,
    _gsskrb5_export_sec_context,
    _gsskrb5_import_sec_context,
    _gsspku2u_inquire_names_for_mech,
    _gsskrb5_inquire_mechs_for_name,
    _gsskrb5_canonicalize_name,
    _gsskrb5_duplicate_name,
    _gsskrb5_inquire_sec_context_by_oid,
    _gsskrb5_inquire_cred_by_oid,
    _gsskrb5_set_sec_context_option,
    _gsskrb5_set_cred_option,
    _gsskrb5_pseudo_random,
    _gk_wrap_iov,
    _gk_unwrap_iov,
    _gk_wrap_iov_length,
    _gsskrb5_store_cred,
    _gsskrb5_export_cred,
    _gsskrb5_import_cred,
    _gss_krb5_acquire_cred_ex,
    _gss_pku2u_iter_creds_f,
    _gsskrb5_destroy_cred,
    _gsskrb5_cred_hold,
    _gsskrb5_cred_unhold,
    _gsskrb5_cred_label_get,
    _gsskrb5_cred_label_set,
    NULL,
    0
};

#endif

gssapi_mech_interface
__gss_krb5_initialize(void)
{
    return &krb5_mech;
}

gssapi_mech_interface
__gss_pku2u_initialize(void)
{
    return &iakerb_mech;
}

gssapi_mech_interface
__gss_iakerb_initialize(void)
{
#ifdef PKINIT
    return &pku2u_mech;
#else
    return NULL;
#endif
}

/*
 * compat glue
 */

#undef GSS_C_INQ_WIN2K_PAC_X
GSSAPI_LIB_VARIABLE gss_OID GSS_C_INQ_WIN2K_PAC_X =
    &__gss_c_inq_win2k_pac_x_oid_desc;

#undef GSS_C_NT_ANONYMOUS
GSSAPI_LIB_VARIABLE gss_OID GSS_C_NT_ANONYMOUS =
    &__gss_c_nt_anonymous_oid_desc;

#undef GSS_C_NT_DN
GSSAPI_LIB_VARIABLE gss_OID GSS_C_NT_DN =
    &__gss_c_nt_dn_oid_desc;

#undef GSS_C_NT_EXPORT_NAME
GSSAPI_LIB_VARIABLE gss_OID GSS_C_NT_EXPORT_NAME =
    &__gss_c_nt_export_name_oid_desc;

#undef GSS_C_NT_HOSTBASED_SERVICE
GSSAPI_LIB_VARIABLE gss_OID GSS_C_NT_HOSTBASED_SERVICE =
    &__gss_c_nt_hostbased_service_oid_desc;

#undef GSS_C_NT_HOSTBASED_SERVICE_X
GSSAPI_LIB_VARIABLE gss_OID GSS_C_NT_HOSTBASED_SERVICE_X =
    &__gss_c_nt_hostbased_service_x_oid_desc;

#undef GSS_C_NT_MACHINE_UID_NAME
GSSAPI_LIB_VARIABLE gss_OID GSS_C_NT_MACHINE_UID_NAME =
    &__gss_c_nt_machine_uid_name_oid_desc;

#undef GSS_C_NT_STRING_UID_NAME
GSSAPI_LIB_VARIABLE gss_OID GSS_C_NT_STRING_UID_NAME =
    &__gss_c_nt_string_uid_name_oid_desc;

#undef GSS_C_NT_USER_NAME
GSSAPI_LIB_VARIABLE gss_OID GSS_C_NT_USER_NAME =
    &__gss_c_nt_user_name_oid_desc;

#undef GSS_C_PEER_HAS_UPDATED_SPNEGO
GSSAPI_LIB_VARIABLE gss_OID GSS_C_PEER_HAS_UPDATED_SPNEGO =
    &__gss_c_peer_has_updated_spnego_oid_desc;

#undef GSS_IAKERB_MECHANISM
GSSAPI_LIB_VARIABLE gss_OID GSS_IAKERB_MECHANISM =
    &__gss_iakerb_mechanism_oid_desc;

#undef GSS_KRB5_CCACHE_NAME_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_CCACHE_NAME_X =
    &__gss_krb5_ccache_name_x_oid_desc;

#undef GSS_KRB5_COMPAT_DES3_MIC_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_COMPAT_DES3_MIC_X =
    &__gss_krb5_compat_des3_mic_x_oid_desc;

#undef GSS_KRB5_COPY_CCACHE_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_COPY_CCACHE_X =
    &__gss_krb5_copy_ccache_x_oid_desc;

#undef GSS_KRB5_CRED_NO_CI_FLAGS_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_CRED_NO_CI_FLAGS_X =
    &__gss_krb5_cred_no_ci_flags_x_oid_desc;

#undef GSS_KRB5_EXPORT_LUCID_CONTEXT_V1_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_EXPORT_LUCID_CONTEXT_V1_X =
    &__gss_krb5_export_lucid_context_v1_x_oid_desc;

#undef GSS_KRB5_EXPORT_LUCID_CONTEXT_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_EXPORT_LUCID_CONTEXT_X =
    &__gss_krb5_export_lucid_context_x_oid_desc;

#undef GSS_KRB5_EXTRACT_AUTHZ_DATA_FROM_SEC_CONTEXT_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_EXTRACT_AUTHZ_DATA_FROM_SEC_CONTEXT_X =
    &__gss_krb5_extract_authz_data_from_sec_context_x_oid_desc;

#undef GSS_KRB5_GET_ACCEPTOR_SUBKEY_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_GET_ACCEPTOR_SUBKEY_X =
    &__gss_krb5_get_acceptor_subkey_x_oid_desc;

#undef GSS_KRB5_GET_AUTHTIME_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_GET_AUTHTIME_X =
    &__gss_krb5_get_authtime_x_oid_desc;

#undef GSS_KRB5_GET_INITIATOR_SUBKEY_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_GET_INITIATOR_SUBKEY_X =
    &__gss_krb5_get_initiator_subkey_x_oid_desc;

#undef GSS_KRB5_GET_SERVICE_KEYBLOCK_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_GET_SERVICE_KEYBLOCK_X =
    &__gss_krb5_get_service_keyblock_x_oid_desc;

#undef GSS_KRB5_GET_SUBKEY_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_GET_SUBKEY_X =
    &__gss_krb5_get_subkey_x_oid_desc;

#undef GSS_KRB5_GET_TIME_OFFSET_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_GET_TIME_OFFSET_X =
    &__gss_krb5_get_time_offset_x_oid_desc;

#undef GSS_KRB5_GET_TKT_FLAGS_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_GET_TKT_FLAGS_X =
    &__gss_krb5_get_tkt_flags_x_oid_desc;

#undef GSS_KRB5_IMPORT_CRED_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_IMPORT_CRED_X =
    &__gss_krb5_import_cred_x_oid_desc;

#undef GSS_KRB5_MECHANISM
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_MECHANISM =
    &__gss_krb5_mechanism_oid_desc;

#undef GSS_KRB5_NT_MACHINE_UID_NAME
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_NT_MACHINE_UID_NAME =
    &__gss_c_nt_machine_uid_name_oid_desc;

#undef GSS_KRB5_NT_PRINCIPAL
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_NT_PRINCIPAL =
    &__gss_krb5_nt_principal_oid_desc;

#undef GSS_KRB5_NT_PRINCIPAL_NAME
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_NT_PRINCIPAL_NAME =
    &__gss_krb5_nt_principal_name_oid_desc;

#undef GSS_KRB5_NT_PRINCIPAL_NAME_REFERRAL
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_NT_PRINCIPAL_NAME_REFERRAL =
    &__gss_krb5_nt_principal_name_referral_oid_desc;

#undef GSS_KRB5_NT_STRING_UID_NAME
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_NT_STRING_UID_NAME =
    &__gss_c_nt_string_uid_name_oid_desc;

#undef GSS_KRB5_NT_USER_NAME
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_NT_USER_NAME =
    &__gss_c_nt_user_name_oid_desc;

#undef GSS_KRB5_PLUGIN_REGISTER_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_PLUGIN_REGISTER_X =
    &__gss_krb5_plugin_register_x_oid_desc;

#undef GSS_KRB5_REGISTER_ACCEPTOR_IDENTITY_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_REGISTER_ACCEPTOR_IDENTITY_X =
    &__gss_krb5_register_acceptor_identity_x_oid_desc;

#undef GSS_KRB5_SEND_TO_KDC_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_SEND_TO_KDC_X =
    &__gss_krb5_send_to_kdc_x_oid_desc;

#undef GSS_KRB5_SET_ALLOWABLE_ENCTYPES_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_SET_ALLOWABLE_ENCTYPES_X =
    &__gss_krb5_set_allowable_enctypes_x_oid_desc;

#undef GSS_KRB5_SET_DEFAULT_REALM_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_SET_DEFAULT_REALM_X =
    &__gss_krb5_set_default_realm_x_oid_desc;

#undef GSS_KRB5_SET_DNS_CANONICALIZE_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_SET_DNS_CANONICALIZE_X =
    &__gss_krb5_set_dns_canonicalize_x_oid_desc;

#undef GSS_KRB5_SET_TIME_OFFSET_X
GSSAPI_LIB_VARIABLE gss_OID GSS_KRB5_SET_TIME_OFFSET_X =
    &__gss_krb5_set_time_offset_x_oid_desc;

#undef GSS_PKU2U_MECHANISM
GSSAPI_LIB_VARIABLE gss_OID GSS_PKU2U_MECHANISM =
    &__gss_pku2u_mechanism_oid_desc;

#undef GSS_SASL_DIGEST_MD5_MECHANISM
GSSAPI_LIB_VARIABLE gss_OID GSS_SASL_DIGEST_MD5_MECHANISM =
    &__gss_sasl_digest_md5_mechanism_oid_desc;

#undef GSS_C_INQ_SSPI_SESSION_KEY
gss_OID GSSAPI_LIB_VARIABLE GSS_C_INQ_SSPI_SESSION_KEY =
    &__gss_c_inq_sspi_session_key_oid_desc;
