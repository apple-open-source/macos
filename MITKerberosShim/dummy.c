/*
 * Copyright (c) 2008-2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2008-2010 Apple Inc. All rights reserved.
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

#include <Heimdal/krb5_err.h>
#include <errno.h>

#include <asl.h>
#include <syslog.h>
#include <stdlib.h>

void
mshim_log_function_missing(const char *func)
{
    aslmsg m = asl_new(ASL_TYPE_MSG);
    asl_set(m, "com.apple.message.domain", "com.apple.kerberos.mshim.missing-function" );
    asl_set(m, "com.apple.message.signature", func);
    asl_set(m, "com.apple.message.signature2", getprogname());
    asl_log(NULL, m, ASL_LEVEL_NOTICE,
	    "function %s not implemented, but used by %s", func, getprogname());
    asl_free(m);

    syslog(LOG_ERR, "MITKerberosShim: function %s not implemented", func);
}

#define dummy(func, ret) int func() { mshim_log_function_missing(__func__); return (ret); }

#define quietdummy(func, ret) int func() { return (ret); }


dummy(add_error_table, 0);
dummy(cc_close, 0);
dummy(cc_create, 0);
dummy(cc_destroy, 0);
dummy(cc_free_NC_info, 0);
dummy(cc_free_creds, 0);
dummy(cc_free_name, 0);
dummy(cc_free_principal, 0);
dummy(cc_get_NC_info, 0);
dummy(cc_get_change_time, 0);
dummy(cc_get_cred_version, 0);
dummy(cc_get_name, 0);
dummy(cc_get_principal, 0);
dummy(cc_open, 0);
dummy(cc_remove_cred, 0);
dummy(cc_seq_fetch_NCs_begin, 0);
dummy(cc_seq_fetch_NCs_end, 0);
dummy(cc_seq_fetch_NCs_next, 0);
dummy(cc_seq_fetch_creds_begin, 0);
dummy(cc_seq_fetch_creds_end, 0);
dummy(cc_seq_fetch_creds_next, 0);
dummy(cc_set_principal, 0);
dummy(cc_shutdown, 0);
dummy(cc_store, 0);
dummy(encode_krb5_as_req, 0);
dummy(gss_krb5_ui, 0);
dummy(gss_str_to_oid, 0);
dummy(krb5_get_krbhst, KRB5_REALM_UNKNOWN);
dummy(krb5_free_krbhst, 0);
dummy(gss_krb5_get_tkt_flags, 0);
dummy(gss_sign, 0);
dummy(gss_inquire_mechs_for_name, 0);
dummy(gss_verify, 0);
dummy(kim_ccache_compare, 0);
dummy(kim_ccache_copy, 0);
dummy(kim_ccache_create_from_client_identity, 0);
dummy(kim_ccache_create_from_default, 0);
dummy(kim_ccache_create_from_display_name, 0);
dummy(kim_ccache_create_from_keytab, 0);
dummy(kim_ccache_create_from_krb5_ccache, 0);
dummy(kim_ccache_create_from_type_and_name, 0);
dummy(kim_ccache_create_new, 0);
dummy(kim_ccache_create_new_if_needed, 0);
dummy(kim_ccache_create_new_if_needed_with_password, 0);
dummy(kim_ccache_create_new_with_password, 0);
dummy(kim_ccache_destroy, 0);
dummy(kim_ccache_free, 0);
dummy(kim_ccache_get_client_identity, 0);
dummy(kim_ccache_get_display_name, 0);
dummy(kim_ccache_get_expiration_time, 0);
dummy(kim_ccache_get_krb5_ccache, 0);
dummy(kim_ccache_get_name, 0);
dummy(kim_ccache_get_options, 0);
dummy(kim_ccache_get_renewal_expiration_time, 0);
dummy(kim_ccache_get_start_time, 0);
dummy(kim_ccache_get_state, 0);
dummy(kim_ccache_get_type, 0);
dummy(kim_ccache_get_valid_credential, 0);
dummy(kim_ccache_iterator_create, 0);
dummy(kim_ccache_iterator_free, 0);
dummy(kim_ccache_iterator_next, 0);
dummy(kim_ccache_renew, 0);
dummy(kim_ccache_set_default, 0);
dummy(kim_ccache_validate, 0);
dummy(kim_ccache_verify, 0);
dummy(kim_credential_copy, 0);
dummy(kim_credential_create_from_keytab, 0);
dummy(kim_credential_create_from_krb5_creds, 0);
dummy(kim_credential_create_new, 0);
dummy(kim_credential_create_new_with_password, 0);
dummy(kim_credential_free, 0);
dummy(kim_credential_get_client_identity, 0);
dummy(kim_credential_get_expiration_time, 0);
dummy(kim_credential_get_krb5_creds, 0);
dummy(kim_credential_get_options, 0);
dummy(kim_credential_get_renewal_expiration_time, 0);
dummy(kim_credential_get_service_identity, 0);
dummy(kim_credential_get_start_time, 0);
dummy(kim_credential_get_state, 0);
dummy(kim_credential_is_tgt, 0);
dummy(kim_credential_iterator_create, 0);
dummy(kim_credential_iterator_free, 0);
dummy(kim_credential_iterator_next, 0);
dummy(kim_credential_renew, 0);
dummy(kim_credential_store, 0);
dummy(kim_credential_validate, 0);
dummy(kim_credential_verify, 0);
dummy(kim_identity_change_password, 0);
dummy(kim_identity_compare, 0);
dummy(kim_identity_copy, 0);
dummy(kim_identity_create_from_components, 0);
dummy(kim_identity_create_from_krb5_principal, 0);
dummy(kim_identity_create_from_string, 0);
dummy(kim_identity_free, 0);
dummy(kim_identity_get_component_at_index, 0);
dummy(kim_identity_get_components_string, 0);
dummy(kim_identity_get_display_string, 0);
dummy(kim_identity_get_krb5_principal, 0);
dummy(kim_identity_get_number_of_components, 0);
dummy(kim_identity_get_realm, 0);
dummy(kim_identity_get_string, 0);
dummy(kim_library_set_allow_automatic_prompting, 0);
dummy(kim_library_set_allow_home_directory_access, 0);
dummy(kim_library_set_application_name, 0);
dummy(kim_options_copy, 0);
dummy(kim_options_create, 0);
dummy(kim_options_create_from_stream, 0);
dummy(kim_options_free, 0);
dummy(kim_options_get_addressless, 0);
dummy(kim_options_get_forwardable, 0);
dummy(kim_options_get_lifetime, 0);
dummy(kim_options_get_proxiable, 0);
dummy(kim_options_get_renewable, 0);
dummy(kim_options_get_renewal_lifetime, 0);
dummy(kim_options_get_service_name, 0);
dummy(kim_options_get_start_time, 0);
dummy(kim_options_set_addressless, 0);
dummy(kim_options_set_forwardable, 0);
dummy(kim_options_set_lifetime, 0);
dummy(kim_options_set_proxiable, 0);
dummy(kim_options_set_renewable, 0);
dummy(kim_options_set_renewal_lifetime, 0);
dummy(kim_options_set_service_name, 0);
dummy(kim_options_set_start_time, 0);
dummy(kim_options_write_to_stream, 0);
dummy(kim_preferences_add_favorite_identity, 0);
dummy(kim_preferences_copy, 0);
dummy(kim_preferences_create, 0);
dummy(kim_preferences_free, 0);
dummy(kim_preferences_get_client_identity, 0);
dummy(kim_preferences_get_favorite_identity_at_index, 0);
dummy(kim_preferences_get_maximum_lifetime, 0);
dummy(kim_preferences_get_maximum_renewal_lifetime, 0);
dummy(kim_preferences_get_minimum_lifetime, 0);
dummy(kim_preferences_get_minimum_renewal_lifetime, 0);
dummy(kim_preferences_get_number_of_favorite_identities, 0);
dummy(kim_preferences_get_options, 0);
dummy(kim_preferences_get_remember_client_identity, 0);
dummy(kim_preferences_get_remember_options, 0);
dummy(kim_preferences_remove_all_favorite_identities, 0);
dummy(kim_preferences_remove_favorite_identity, 0);
dummy(kim_preferences_set_client_identity, 0);
dummy(kim_preferences_set_maximum_lifetime, 0);
dummy(kim_preferences_set_maximum_renewal_lifetime, 0);
dummy(kim_preferences_set_minimum_lifetime, 0);
dummy(kim_preferences_set_minimum_renewal_lifetime, 0);
dummy(kim_preferences_set_options, 0);
dummy(kim_preferences_set_remember_client_identity, 0);
dummy(kim_preferences_set_remember_options, 0);
dummy(kim_preferences_synchronize, 0);
dummy(kim_selection_hints_copy, 0);
dummy(kim_selection_hints_create, 0);
dummy(kim_selection_hints_create_from_stream, 0);
dummy(kim_selection_hints_forget_identity, 0);
dummy(kim_selection_hints_free, 0);
dummy(kim_selection_hints_get_allow_user_interaction, 0);
dummy(kim_selection_hints_get_explanation, 0);
dummy(kim_selection_hints_get_hint, 0);
dummy(kim_selection_hints_get_identity, 0);
dummy(kim_selection_hints_get_options, 0);
dummy(kim_selection_hints_get_remember_identity, 0);
dummy(kim_selection_hints_remember_identity, 0);
dummy(kim_selection_hints_set_allow_user_interaction, 0);
dummy(kim_selection_hints_set_explanation, 0);
dummy(kim_selection_hints_set_hint, 0);
dummy(kim_selection_hints_set_options, 0);
dummy(kim_selection_hints_set_remember_identity, 0);
dummy(kim_string_compare, 0);
dummy(kim_string_copy, 0);
dummy(kim_string_create_for_last_error, 0);
dummy(kim_string_free, 0);
dummy(krb524_convert_creds_kdc, 0);
dummy(krb5_425_conv_principal, 0);
dummy(krb5_524_conv_principal, 0);
dummy(krb5_524_convert_creds, 0);
dummy(krb5_address_compare, 0);
dummy(krb5_address_order, 0);
dummy(krb5_address_search, 0);
dummy(krb5_aname_to_localname, 0);
dummy(krb5_appdefault_boolean, 0);
dummy(krb5_appdefault_string, 0);
dummy(krb5_auth_con_get_checksum_func, 0);
dummy(krb5_auth_con_getrecvsubkey, 0);
dummy(krb5_auth_con_getsendsubkey, 0);
dummy(krb5_auth_con_initivector, 0);
dummy(krb5_auth_con_set_checksum_func, 0);
dummy(krb5_auth_con_setrecvsubkey, 0);
dummy(krb5_auth_con_setsendsubkey, 0);
dummy(krb5_auth_con_setuseruserkey, 0);
dummy(krb5_build_principal_alloc_va, 0);
dummy(krb5_build_principal_va, 0);
dummy(krb5_c_block_size, 0);
dummy(krb5_c_checksum_length, 0);
dummy(krb5_c_enctype_compare, 0);
dummy(krb5_c_free_state, 0);
dummy(krb5_c_init_state, 0);
dummy(krb5_c_is_coll_proof_cksum, 0);
dummy(krb5_c_is_keyed_cksum, 0);
dummy(krb5_c_keyed_checksum_types, 0);
dummy(krb5_c_make_checksum, 0);
dummy(krb5_c_make_random_key, 0);
dummy(krb5_c_random_add_entropy, 0);
dummy(krb5_c_random_make_octets, 0);
dummy(krb5_c_random_os_entropy, 0);
dummy(krb5_c_random_seed, 0);
dummy(krb5_c_string_to_key_with_params, 0);
dummy(krb5_c_valid_cksumtype, 0);
dummy(krb5_c_valid_enctype, 0);
dummy(krb5_c_verify_checksum, 0);
dummy(krb5_calculate_checksum, 0);
dummy(krb5_cc_copy_creds, 0);
dummy(krb5_cc_last_change_time, 0);
dummy(krb5_cc_lock, 0);
dummy(krb5_cc_move, 0);
dummy(krb5_cc_remove_cred, 0);
dummy(krb5_cc_set_config, 0);
dummy(krb5_cc_set_flags, 0);
dummy(krb5_cc_unlock, 0);
dummy(krb5_cccol_last_change_time, 0);
dummy(krb5_cccol_lock, 0);
dummy(krb5_cccol_unlock, 0);
dummy(krb5_checksum_size, 0);
dummy(krb5_cksumtype_to_string, 0);
dummy(krb5_copy_addresses, 0);
dummy(krb5_copy_authdata, 0);
dummy(krb5_copy_authenticator, 0);
dummy(krb5_copy_checksum, 0);
dummy(krb5_copy_context, 0);
dummy(krb5_copy_ticket, 0);
dummy(krb5_decrypt, 0);
dummy(krb5_deltat_to_string, 0);
dummy(krb5_eblock_enctype, 0);
dummy(krb5_encrypt, 0);
dummy(krb5_encrypt_size, 0);
dummy(krb5_finish_key, 0);
dummy(krb5_finish_random_key, 0);
dummy(krb5_free_authenticator, 0);
dummy(krb5_free_checksum, 0);
dummy(krb5_free_checksum_contents, 0);
dummy(krb5_free_cksumtypes, 0);
dummy(krb5_free_config_files, 0);
dummy(krb5_free_tgt_creds, 0);
dummy(krb5_fwd_tgt_creds, 0);
dummy(krb5_get_credentials_renew, 0);
dummy(krb5_get_credentials_validate, 0);
dummy(krb5_get_default_config_files, 0);
dummy(krb5_get_in_tkt, KRB5_KT_NOTFOUND);
dummy(krb5_get_in_tkt_with_keytab, KRB5_KT_NOTFOUND);
dummy(krb5_get_in_tkt_with_skey, KRB5_KT_NOTFOUND);
dummy(krb5_get_init_creds_opt_set_change_password_prompt,0);
dummy(krb5_get_init_creds_opt_set_pa,0);
dummy(krb5_get_permitted_enctypes, 0);
dummy(krb5_get_profile, 0);
dummy(krb5_get_prompt_types, 0);
dummy(krb5_get_time_offsets, 0);
dummy(krb5_gss_use_kdc_context, 0);
dummy(krb5_init_keyblock, 0);
dummy(krb5_init_random_key, 0);
quietdummy(krb5_ipc_client_clear_target, 0);
quietdummy(krb5_ipc_client_set_target_uid, 0);
dummy(krb5_is_config_principal, 0);
dummy(krb5_is_referral_realm, 0);
dummy(krb5_is_thread_safe, 0);
dummy(krb5_mk_1cred, 0);
dummy(krb5_mk_error, 0);
dummy(krb5_mk_ncred, 0);
dummy(krb5_mk_rep, 0);
dummy(krb5_parse_name_flags, 0);
dummy(krb5_pkinit_get_client_cert, 0);
dummy(krb5_pkinit_get_client_cert_db, 0);
dummy(krb5_pkinit_get_kdc_cert, 0);
dummy(krb5_pkinit_get_kdc_cert_db, 0);
dummy(krb5_pkinit_have_client_cert, 0);
dummy(krb5_pkinit_release_cert, 0);
dummy(krb5_pkinit_release_cert_db, 0);
dummy(krb5_pkinit_set_client_cert, 0);
dummy(krb5_process_key, 0);
dummy(krb5_random_key, 0);
dummy(krb5_rd_cred, 0);
dummy(krb5_rd_error, 0);
dummy(krb5_rd_rep, 0);
dummy(krb5_read_password, 0);
dummy(krb5_salttype_to_string, 0);
dummy(krb5_server_decrypt_ticket_keytab, 0);
dummy(krb5_set_password, 0);
dummy(krb5_set_principal_realm, 0);
dummy(krb5_string_to_cksumtype, 0);
dummy(krb5_string_to_enctype, 0);
dummy(krb5_string_to_salttype, 0);
dummy(krb5_timestamp_to_sfstring, 0);
dummy(krb5_timestamp_to_string, 0);
dummy(krb5_unparse_name_ext, 0);
dummy(krb5_unparse_name_flags, 0);
dummy(krb5_unparse_name_flags_ext, 0);
dummy(krb5_verify_checksum, 0);
dummy(krb5int_accessor, 0);
dummy(krb5int_freeaddrinfo, 0);
dummy(krb5int_gai_strerror, 0);
dummy(krb5int_getaddrinfo, 0);
dummy(krb5int_gmt_mktime, 0);
dummy(krb5int_init_context_kdc, 0);
dummy(krb5int_pkinit_auth_pack_decode, 0);
dummy(krb5int_pkinit_create_cms_msg, 0);
dummy(krb5int_pkinit_pa_pk_as_rep_encode, 0);
dummy(krb5int_pkinit_pa_pk_as_req_decode, 0);
dummy(krb5int_pkinit_parse_cms_msg, 0);
dummy(krb5int_pkinit_reply_key_pack_encode, 0);
dummy(remove_error_table, 0);
dummy(__KerberosInternal_krb5int_sendtokdc_debug_handler, 0);

