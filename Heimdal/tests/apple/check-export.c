/*
 * Copyright (c) 2011 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2011 Apple Inc. All rights reserved.
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

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

static const char *exported_10_7[] = {
    "gss_aapl_initial_cred",
    "gss_accept_sec_context",
    "gss_acquire_cred",
    "gss_add_buffer_set_member",
    "gss_add_cred",
    "gss_add_oid_set_member",
    "gss_canonicalize_name",
    "gss_compare_name",
    "gss_context_time",
    "gss_create_empty_buffer_set",
    "gss_create_empty_oid_set",
    "gss_decapsulate_token",
    "gss_delete_sec_context",
    "gss_destroy_cred",
    "gss_display_name",
    "gss_display_status",
    "gss_duplicate_name",
    "gss_duplicate_oid",
    "gss_encapsulate_token",
    "gss_export_cred",
    "gss_export_name",
    "gss_export_sec_context",
    "gss_get_mic",
    "gss_import_cred",
    "gss_import_name",
    "gss_import_sec_context",
    "gss_indicate_mechs",
    "gss_init_sec_context",
    "gss_inquire_context",
    "gss_inquire_cred",
    "gss_inquire_cred_by_mech",
    "gss_inquire_cred_by_oid",
    "gss_inquire_mechs_for_name",
    "gss_inquire_names_for_mech",
    "gss_inquire_sec_context_by_oid",
    "gss_iter_creds",
    "gss_iter_creds_f",
    "gss_krb5_ccache_name",
    "gss_krb5_copy_ccache",
    "gss_krb5_export_lucid_sec_context",
    "gss_krb5_free_lucid_sec_context",
    "gss_krb5_set_allowable_enctypes",
    "gss_oid_equal",
    "gss_oid_to_str",
    "gss_process_context_token",
    "gss_pseudo_random",
    "gss_release_buffer",
    "gss_release_buffer_set",
    "gss_release_cred",
    "gss_release_name",
    "gss_release_oid",
    "gss_release_oid_set",
    "gss_seal",
    "gss_set_cred_option",
    "gss_set_sec_context_option",
    "gss_sign",
    "gss_test_oid_set_member",
    "gss_unseal",
    "gss_unwrap",
    "gss_verify",
    "gss_verify_mic",
    "gss_wrap",
    "gss_wrap_size_limit",
    "gsskrb5_extract_authz_data_from_sec_context",
    "gsskrb5_register_acceptor_identity",
    "krb5_gss_register_acceptor_identity"
};

static const char *import_mkshim_gss[] = {
    "gss_accept_sec_context",
    "gss_acquire_cred",
    "gss_add_cred",
    "gss_add_oid_set_member",
    "gss_canonicalize_name",
    "gss_compare_name",
    "gss_context_time",
    "gss_create_empty_oid_set",
    "gss_delete_sec_context",
    "gss_display_name",
    "gss_display_status",
    "gss_duplicate_name",
    "gss_export_name",
    "gss_export_sec_context",
    "gss_get_mic",
    "gss_import_sec_context",
    "gss_indicate_mechs",
    "gss_init_sec_context",
    "gss_inquire_context",
    "gss_inquire_cred",
    "gss_inquire_cred_by_mech",
    "gss_inquire_names_for_mech",
    "gss_krb5_ccache_name",
    "gss_krb5_copy_ccache",
    "gss_krb5_export_lucid_sec_context",
    "gss_krb5_free_lucid_sec_context",
    "gss_krb5_set_allowable_enctypes",
    "gss_oid_equal",
    "gss_oid_to_str",
    "gss_process_context_token",
    "gss_release_buffer",
    "gss_release_cred",
    "gss_release_name",
    "gss_release_oid",
    "gss_release_oid_set",
    "gss_seal",
    "gss_test_oid_set_member",
    "gss_unseal",
    "gss_unwrap",
    "gss_verify_mic",
    "gss_wrap",
    "gss_wrap_size_limit",
    "gss_import_name",
    "gsskrb5_extract_authz_data_from_sec_context",
    "krb5_gss_register_acceptor_identity"
};

static const char *import_mkshim_heimdal[] = {
    "krb5_cc_start_seq_get",
    "krb5_cc_default_name",
    "krb5_cc_destroy",
    "krb5_cccol_cursor_free",
    "krb5_cccol_cursor_new",
    "krb5_cccol_cursor_next",
    "krb5_free_host_realm",
    "krb5_get_default_realm",
    "krb5_get_host_realm",
    "krb5_cc_set_default_name",
    "krb5_kt_resolve",
    "krb5_kt_default",
    "krb5_kt_default_name",
    "krb5_kt_close",
    "krb5_kt_destroy",
    "krb5_auth_con_free",
    "krb5_auth_con_init",
    "krb5_auth_con_genaddrs",
    "krb5_auth_con_getlocalseqnumber",
    "krb5_auth_con_getremoteseqnumber",
    "krb5_auth_con_setflags",
    "krb5_auth_con_getflags",
    "krb5_clear_error_message",
    "krb5_free_error_message",
    "krb5_set_default_realm",
    "krb5_set_error_message",
    "krb5_vset_error_message",
    "com_err",
    "com_err_va",
    "reset_com_err_hook",
    "set_com_err_hook",
    "krb5_cc_end_seq_get",
    "krb5_config_get_string",
    "krb5_set_default_in_tkt_etypes",
    "krb5_get_pw_salt",
    "krb5_free_salt",
    "krb5_string_to_key_data_salt",
    "krb5_free_keyblock_contents",
    "krb5_set_real_time",
    "krb5_mk_req_extended",
    "krb5_free_keyblock",
    "krb5_auth_con_getremotesubkey",
    "krb5_auth_con_getlocalsubkey",
    "krb5_set_password",
    "krb5_set_password_using_ccache",
    "krb5_realm_compare",
    "krb5_get_renewed_creds",
    "krb5_get_validated_creds",
    "krb5_get_init_creds_keytab",
    "krb5_prompter_posix",
    "krb5_string_to_deltat",
    "krb5_get_all_client_addrs",
    "krb5_kt_get_type",
    "krb5_kt_add_entry",
    "krb5_kt_remove_entry",
    "krb5_mk_req",
    "krb5_kt_get_name",
    "krb5_rd_req",
    "krb5_free_ticket",
    "krb5_build_principal_va",
    "krb5_build_principal_va_ext",
    "krb5_cc_cache_match",
    "krb5_cc_close",
    "krb5_cc_default",
    "krb5_cc_get_config",
    "krb5_cc_get_full_name",
    "krb5_cc_get_name",
    "krb5_cc_get_principal",
    "krb5_cc_get_type",
    "krb5_cc_initialize",
    "krb5_cc_move",
    "krb5_cc_new_unique",
    "krb5_cc_resolve",
    "krb5_cc_store_cred",
    "krb5_cc_switch",
    "krb5_cc_retrieve_cred",
    "krb5_cc_remove_cred",
    "krb5_cc_get_kdc_offset",
    "krb5_cc_set_kdc_offset",
    "krb5_cc_next_cred",
    "krb5_cccol_last_change_time",
    "krb5_crypto_init",
    "krb5_crypto_getblocksize",
    "krb5_crypto_destroy",
    "krb5_decrypt_ivec",
    "krb5_encrypt_ivec",
    "krb5_crypto_getenctype",
    "krb5_generate_random_keyblock",
    "krb5_get_wrapped_length",
    "krb5_copy_creds_contents",
    "krb5_copy_data",
    "krb5_copy_principal",
    "krb5_data_copy",
    "krb5_data_free",
    "krb5_data_zero",
    "krb5_free_context",
    "krb5_free_cred_contents",
    "krb5_free_creds",
    "krb5_free_principal",
    "krb5_sname_to_principal",
    "krb5_get_credentials",
    "krb5_get_error_string",
    "krb5_get_default_principal",
    "krb5_get_error_message",
    "krb5_get_init_creds_opt_alloc",
    "krb5_get_init_creds_opt_free",
    "krb5_get_init_creds_opt_set_canonicalize",
    "krb5_get_init_creds_opt_set_forwardable",
    "krb5_get_init_creds_opt_set_proxiable",
    "krb5_get_init_creds_opt_set_renew_life",
    "krb5_get_init_creds_opt_set_tkt_life",
    "krb5_get_init_creds_password",
    "krb5_get_kdc_cred",
    "krb5_get_kdc_sec_offset",
    "krb5_init_context",
    "krb5_make_principal",
    "krb5_parse_name",
    "krb5_principal_compare",
    "krb5_principal_get_realm",
    "krb5_timeofday",
    "krb5_unparse_name",
    "krb5_us_timeofday",
    "krb5_kt_start_seq_get",
    "krb5_kt_end_seq_get",
    "krb5_xfree",
    "krb5_kt_next_entry",
    "krb5_kt_free_entry",
    "krb5_sendauth",
    "krb5_free_ap_rep_enc_part",
    "krb5_free_error",
    "krb5_recvauth",
    "krb5_recvauth_match_version",
    "krb5_mk_priv",
    "krb5_rd_priv",
    "krb5_mk_safe",
    "krb5_rd_safe",
    "krb5_set_home_dir_access",
    "krb5_verify_init_creds",
    "krb5_verify_init_creds_opt_init",
    "krb5_verify_init_creds_opt_set_ap_req_nofail",
    "krb5_kuserok",
    "com_right",
    "com_right_r",
    "krb5_appdefault_boolean",
    "krb5_appdefault_string",

};

int
main(int argc, char **argv)
{
    unsigned int n;
    void *syms;

    /* check gss */

    syms = dlopen("/System/Library/Framework/GSS.framework/GSS", RTLD_NOW | RTLD_LOCAL);
    if (syms == NULL)
	err(1, "dlopen(GSS.framework)");
    
    for (n = 0; n < sizeof(exported_10_7) / sizeof(exported_10_7[0]); n++)
	if (dlsym(syms, exported_10_7[n]) == NULL)
	    err(1, "symbol: %s missing", exported_10_7[0]);

    for (n = 0; n < sizeof(import_mkshim_gss) / sizeof(import_mkshim_gss[0]); n++)
	if (dlsym(syms, import_mkshim_gss[n]) == NULL)
	    err(1, "symbol: %s missing", import_mkshim_gss[0]);

    dlclose(syms);

    /* check heimdal */

    syms = dlopen("/System/Library/PrivateFrameworks/Heimdal.framework/Heimdal", RTLD_NOW | RTLD_LOCAL);
    if (syms == NULL)
	err(1, "dlopen(Heimdal.framework)");

    for (n = 0; n < sizeof(import_mkshim_heimdal) / sizeof(import_mkshim_heimdal[0]); n++)
	if (dlsym(syms, import_mkshim_heimdal[n]) == NULL)
	    err(1, "symbol: %s missing", import_mkshim_heimdal[0]);

    dlclose(syms);

    return 0;
}
