/* generated file, no dont edit */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <dispatch/dispatch.h>
#include "heim-sym.h"

static void *hf = NULL;
static void *gf = NULL;
void heim_load_frameworks(void) {
hf = dlopen("/System/Library/PrivateFrameworks/Heimdal.framework/Heimdal", RTLD_LAZY|RTLD_LOCAL);
gf = dlopen("/System/Library/Frameworks/GSS.framework/GSS", RTLD_LAZY|RTLD_LOCAL);
}

void load_functions(void) {
fun_krb5_cc_end_seq_get = dlsym(hf, "krb5_cc_end_seq_get");
if (!fun_krb5_cc_end_seq_get) { syslog(LOG_ERR, "krb5_cc_end_seq_get failed loading"); }
fun_krb5_config_get_string = dlsym(hf, "krb5_config_get_string");
if (!fun_krb5_config_get_string) { syslog(LOG_ERR, "krb5_config_get_string failed loading"); }
fun_krb5_set_default_in_tkt_etypes = dlsym(hf, "krb5_set_default_in_tkt_etypes");
if (!fun_krb5_set_default_in_tkt_etypes) { syslog(LOG_ERR, "krb5_set_default_in_tkt_etypes failed loading"); }
fun_krb5_get_pw_salt = dlsym(hf, "krb5_get_pw_salt");
if (!fun_krb5_get_pw_salt) { syslog(LOG_ERR, "krb5_get_pw_salt failed loading"); }
fun_krb5_free_salt = dlsym(hf, "krb5_free_salt");
if (!fun_krb5_free_salt) { syslog(LOG_ERR, "krb5_free_salt failed loading"); }
fun_krb5_string_to_key_data_salt = dlsym(hf, "krb5_string_to_key_data_salt");
if (!fun_krb5_string_to_key_data_salt) { syslog(LOG_ERR, "krb5_string_to_key_data_salt failed loading"); }
fun_krb5_free_keyblock_contents = dlsym(hf, "krb5_free_keyblock_contents");
if (!fun_krb5_free_keyblock_contents) { syslog(LOG_ERR, "krb5_free_keyblock_contents failed loading"); }
fun_krb5_set_real_time = dlsym(hf, "krb5_set_real_time");
if (!fun_krb5_set_real_time) { syslog(LOG_ERR, "krb5_set_real_time failed loading"); }
fun_krb5_mk_req_extended = dlsym(hf, "krb5_mk_req_extended");
if (!fun_krb5_mk_req_extended) { syslog(LOG_ERR, "krb5_mk_req_extended failed loading"); }
fun_krb5_free_keyblock = dlsym(hf, "krb5_free_keyblock");
if (!fun_krb5_free_keyblock) { syslog(LOG_ERR, "krb5_free_keyblock failed loading"); }
fun_krb5_auth_con_getremotesubkey = dlsym(hf, "krb5_auth_con_getremotesubkey");
if (!fun_krb5_auth_con_getremotesubkey) { syslog(LOG_ERR, "krb5_auth_con_getremotesubkey failed loading"); }
fun_krb5_auth_con_getlocalsubkey = dlsym(hf, "krb5_auth_con_getlocalsubkey");
if (!fun_krb5_auth_con_getlocalsubkey) { syslog(LOG_ERR, "krb5_auth_con_getlocalsubkey failed loading"); }
fun_krb5_set_password = dlsym(hf, "krb5_set_password");
if (!fun_krb5_set_password) { syslog(LOG_ERR, "krb5_set_password failed loading"); }
fun_krb5_set_password_using_ccache = dlsym(hf, "krb5_set_password_using_ccache");
if (!fun_krb5_set_password_using_ccache) { syslog(LOG_ERR, "krb5_set_password_using_ccache failed loading"); }
fun_krb5_realm_compare = dlsym(hf, "krb5_realm_compare");
if (!fun_krb5_realm_compare) { syslog(LOG_ERR, "krb5_realm_compare failed loading"); }
fun_krb5_get_renewed_creds = dlsym(hf, "krb5_get_renewed_creds");
if (!fun_krb5_get_renewed_creds) { syslog(LOG_ERR, "krb5_get_renewed_creds failed loading"); }
fun_krb5_get_validated_creds = dlsym(hf, "krb5_get_validated_creds");
if (!fun_krb5_get_validated_creds) { syslog(LOG_ERR, "krb5_get_validated_creds failed loading"); }
fun_krb5_get_init_creds_keytab = dlsym(hf, "krb5_get_init_creds_keytab");
if (!fun_krb5_get_init_creds_keytab) { syslog(LOG_ERR, "krb5_get_init_creds_keytab failed loading"); }
fun_krb5_prompter_posix = dlsym(hf, "krb5_prompter_posix");
if (!fun_krb5_prompter_posix) { syslog(LOG_ERR, "krb5_prompter_posix failed loading"); }
fun_krb5_string_to_deltat = dlsym(hf, "krb5_string_to_deltat");
if (!fun_krb5_string_to_deltat) { syslog(LOG_ERR, "krb5_string_to_deltat failed loading"); }
fun_krb5_get_all_client_addrs = dlsym(hf, "krb5_get_all_client_addrs");
if (!fun_krb5_get_all_client_addrs) { syslog(LOG_ERR, "krb5_get_all_client_addrs failed loading"); }
fun_krb5_kt_get_type = dlsym(hf, "krb5_kt_get_type");
if (!fun_krb5_kt_get_type) { syslog(LOG_ERR, "krb5_kt_get_type failed loading"); }
fun_krb5_kt_add_entry = dlsym(hf, "krb5_kt_add_entry");
if (!fun_krb5_kt_add_entry) { syslog(LOG_ERR, "krb5_kt_add_entry failed loading"); }
fun_krb5_kt_remove_entry = dlsym(hf, "krb5_kt_remove_entry");
if (!fun_krb5_kt_remove_entry) { syslog(LOG_ERR, "krb5_kt_remove_entry failed loading"); }
fun_krb5_mk_req = dlsym(hf, "krb5_mk_req");
if (!fun_krb5_mk_req) { syslog(LOG_ERR, "krb5_mk_req failed loading"); }
fun_krb5_kt_get_name = dlsym(hf, "krb5_kt_get_name");
if (!fun_krb5_kt_get_name) { syslog(LOG_ERR, "krb5_kt_get_name failed loading"); }
fun_krb5_rd_req = dlsym(hf, "krb5_rd_req");
if (!fun_krb5_rd_req) { syslog(LOG_ERR, "krb5_rd_req failed loading"); }
fun_krb5_free_ticket = dlsym(hf, "krb5_free_ticket");
if (!fun_krb5_free_ticket) { syslog(LOG_ERR, "krb5_free_ticket failed loading"); }
fun_krb5_build_principal_va = dlsym(hf, "krb5_build_principal_va");
if (!fun_krb5_build_principal_va) { syslog(LOG_ERR, "krb5_build_principal_va failed loading"); }
fun_krb5_build_principal_va_ext = dlsym(hf, "krb5_build_principal_va_ext");
if (!fun_krb5_build_principal_va_ext) { syslog(LOG_ERR, "krb5_build_principal_va_ext failed loading"); }
fun_krb5_cc_cache_match = dlsym(hf, "krb5_cc_cache_match");
if (!fun_krb5_cc_cache_match) { syslog(LOG_ERR, "krb5_cc_cache_match failed loading"); }
fun_krb5_cc_close = dlsym(hf, "krb5_cc_close");
if (!fun_krb5_cc_close) { syslog(LOG_ERR, "krb5_cc_close failed loading"); }
fun_krb5_cc_default = dlsym(hf, "krb5_cc_default");
if (!fun_krb5_cc_default) { syslog(LOG_ERR, "krb5_cc_default failed loading"); }
fun_krb5_cc_get_config = dlsym(hf, "krb5_cc_get_config");
if (!fun_krb5_cc_get_config) { syslog(LOG_ERR, "krb5_cc_get_config failed loading"); }
fun_krb5_cc_get_full_name = dlsym(hf, "krb5_cc_get_full_name");
if (!fun_krb5_cc_get_full_name) { syslog(LOG_ERR, "krb5_cc_get_full_name failed loading"); }
fun_krb5_cc_get_name = dlsym(hf, "krb5_cc_get_name");
if (!fun_krb5_cc_get_name) { syslog(LOG_ERR, "krb5_cc_get_name failed loading"); }
fun_krb5_cc_get_principal = dlsym(hf, "krb5_cc_get_principal");
if (!fun_krb5_cc_get_principal) { syslog(LOG_ERR, "krb5_cc_get_principal failed loading"); }
fun_krb5_cc_get_type = dlsym(hf, "krb5_cc_get_type");
if (!fun_krb5_cc_get_type) { syslog(LOG_ERR, "krb5_cc_get_type failed loading"); }
fun_krb5_cc_initialize = dlsym(hf, "krb5_cc_initialize");
if (!fun_krb5_cc_initialize) { syslog(LOG_ERR, "krb5_cc_initialize failed loading"); }
fun_krb5_cc_move = dlsym(hf, "krb5_cc_move");
if (!fun_krb5_cc_move) { syslog(LOG_ERR, "krb5_cc_move failed loading"); }
fun_krb5_cc_new_unique = dlsym(hf, "krb5_cc_new_unique");
if (!fun_krb5_cc_new_unique) { syslog(LOG_ERR, "krb5_cc_new_unique failed loading"); }
fun_krb5_cc_resolve = dlsym(hf, "krb5_cc_resolve");
if (!fun_krb5_cc_resolve) { syslog(LOG_ERR, "krb5_cc_resolve failed loading"); }
fun_krb5_cc_store_cred = dlsym(hf, "krb5_cc_store_cred");
if (!fun_krb5_cc_store_cred) { syslog(LOG_ERR, "krb5_cc_store_cred failed loading"); }
fun_krb5_cc_switch = dlsym(hf, "krb5_cc_switch");
if (!fun_krb5_cc_switch) { syslog(LOG_ERR, "krb5_cc_switch failed loading"); }
fun_krb5_cc_retrieve_cred = dlsym(hf, "krb5_cc_retrieve_cred");
if (!fun_krb5_cc_retrieve_cred) { syslog(LOG_ERR, "krb5_cc_retrieve_cred failed loading"); }
fun_krb5_cc_remove_cred = dlsym(hf, "krb5_cc_remove_cred");
if (!fun_krb5_cc_remove_cred) { syslog(LOG_ERR, "krb5_cc_remove_cred failed loading"); }
fun_krb5_cc_get_kdc_offset = dlsym(hf, "krb5_cc_get_kdc_offset");
if (!fun_krb5_cc_get_kdc_offset) { syslog(LOG_ERR, "krb5_cc_get_kdc_offset failed loading"); }
fun_krb5_cc_set_kdc_offset = dlsym(hf, "krb5_cc_set_kdc_offset");
if (!fun_krb5_cc_set_kdc_offset) { syslog(LOG_ERR, "krb5_cc_set_kdc_offset failed loading"); }
fun_krb5_cc_next_cred = dlsym(hf, "krb5_cc_next_cred");
if (!fun_krb5_cc_next_cred) { syslog(LOG_ERR, "krb5_cc_next_cred failed loading"); }
fun_krb5_cccol_last_change_time = dlsym(hf, "krb5_cccol_last_change_time");
if (!fun_krb5_cccol_last_change_time) { syslog(LOG_ERR, "krb5_cccol_last_change_time failed loading"); }
fun_krb5_crypto_init = dlsym(hf, "krb5_crypto_init");
if (!fun_krb5_crypto_init) { syslog(LOG_ERR, "krb5_crypto_init failed loading"); }
fun_krb5_crypto_getblocksize = dlsym(hf, "krb5_crypto_getblocksize");
if (!fun_krb5_crypto_getblocksize) { syslog(LOG_ERR, "krb5_crypto_getblocksize failed loading"); }
fun_krb5_crypto_destroy = dlsym(hf, "krb5_crypto_destroy");
if (!fun_krb5_crypto_destroy) { syslog(LOG_ERR, "krb5_crypto_destroy failed loading"); }
fun_krb5_decrypt_ivec = dlsym(hf, "krb5_decrypt_ivec");
if (!fun_krb5_decrypt_ivec) { syslog(LOG_ERR, "krb5_decrypt_ivec failed loading"); }
fun_krb5_encrypt_ivec = dlsym(hf, "krb5_encrypt_ivec");
if (!fun_krb5_encrypt_ivec) { syslog(LOG_ERR, "krb5_encrypt_ivec failed loading"); }
fun_krb5_crypto_getenctype = dlsym(hf, "krb5_crypto_getenctype");
if (!fun_krb5_crypto_getenctype) { syslog(LOG_ERR, "krb5_crypto_getenctype failed loading"); }
fun_krb5_generate_random_keyblock = dlsym(hf, "krb5_generate_random_keyblock");
if (!fun_krb5_generate_random_keyblock) { syslog(LOG_ERR, "krb5_generate_random_keyblock failed loading"); }
fun_krb5_get_wrapped_length = dlsym(hf, "krb5_get_wrapped_length");
if (!fun_krb5_get_wrapped_length) { syslog(LOG_ERR, "krb5_get_wrapped_length failed loading"); }
fun_krb5_copy_creds_contents = dlsym(hf, "krb5_copy_creds_contents");
if (!fun_krb5_copy_creds_contents) { syslog(LOG_ERR, "krb5_copy_creds_contents failed loading"); }
fun_krb5_copy_data = dlsym(hf, "krb5_copy_data");
if (!fun_krb5_copy_data) { syslog(LOG_ERR, "krb5_copy_data failed loading"); }
fun_krb5_copy_principal = dlsym(hf, "krb5_copy_principal");
if (!fun_krb5_copy_principal) { syslog(LOG_ERR, "krb5_copy_principal failed loading"); }
fun_krb5_data_copy = dlsym(hf, "krb5_data_copy");
if (!fun_krb5_data_copy) { syslog(LOG_ERR, "krb5_data_copy failed loading"); }
fun_krb5_data_free = dlsym(hf, "krb5_data_free");
if (!fun_krb5_data_free) { syslog(LOG_ERR, "krb5_data_free failed loading"); }
fun_krb5_data_zero = dlsym(hf, "krb5_data_zero");
if (!fun_krb5_data_zero) { syslog(LOG_ERR, "krb5_data_zero failed loading"); }
fun_krb5_free_context = dlsym(hf, "krb5_free_context");
if (!fun_krb5_free_context) { syslog(LOG_ERR, "krb5_free_context failed loading"); }
fun_krb5_free_cred_contents = dlsym(hf, "krb5_free_cred_contents");
if (!fun_krb5_free_cred_contents) { syslog(LOG_ERR, "krb5_free_cred_contents failed loading"); }
fun_krb5_free_creds = dlsym(hf, "krb5_free_creds");
if (!fun_krb5_free_creds) { syslog(LOG_ERR, "krb5_free_creds failed loading"); }
fun_krb5_free_principal = dlsym(hf, "krb5_free_principal");
if (!fun_krb5_free_principal) { syslog(LOG_ERR, "krb5_free_principal failed loading"); }
fun_krb5_sname_to_principal = dlsym(hf, "krb5_sname_to_principal");
if (!fun_krb5_sname_to_principal) { syslog(LOG_ERR, "krb5_sname_to_principal failed loading"); }
fun_krb5_get_credentials = dlsym(hf, "krb5_get_credentials");
if (!fun_krb5_get_credentials) { syslog(LOG_ERR, "krb5_get_credentials failed loading"); }
fun_krb5_get_error_string = dlsym(hf, "krb5_get_error_string");
if (!fun_krb5_get_error_string) { syslog(LOG_ERR, "krb5_get_error_string failed loading"); }
fun_krb5_get_default_principal = dlsym(hf, "krb5_get_default_principal");
if (!fun_krb5_get_default_principal) { syslog(LOG_ERR, "krb5_get_default_principal failed loading"); }
fun_krb5_get_init_creds_opt_alloc = dlsym(hf, "krb5_get_init_creds_opt_alloc");
if (!fun_krb5_get_init_creds_opt_alloc) { syslog(LOG_ERR, "krb5_get_init_creds_opt_alloc failed loading"); }
fun_krb5_get_init_creds_opt_free = dlsym(hf, "krb5_get_init_creds_opt_free");
if (!fun_krb5_get_init_creds_opt_free) { syslog(LOG_ERR, "krb5_get_init_creds_opt_free failed loading"); }
fun_krb5_get_init_creds_opt_set_canonicalize = dlsym(hf, "krb5_get_init_creds_opt_set_canonicalize");
if (!fun_krb5_get_init_creds_opt_set_canonicalize) { syslog(LOG_ERR, "krb5_get_init_creds_opt_set_canonicalize failed loading"); }
fun_krb5_get_init_creds_opt_set_forwardable = dlsym(hf, "krb5_get_init_creds_opt_set_forwardable");
if (!fun_krb5_get_init_creds_opt_set_forwardable) { syslog(LOG_ERR, "krb5_get_init_creds_opt_set_forwardable failed loading"); }
fun_krb5_get_init_creds_opt_set_proxiable = dlsym(hf, "krb5_get_init_creds_opt_set_proxiable");
if (!fun_krb5_get_init_creds_opt_set_proxiable) { syslog(LOG_ERR, "krb5_get_init_creds_opt_set_proxiable failed loading"); }
fun_krb5_get_init_creds_opt_set_renew_life = dlsym(hf, "krb5_get_init_creds_opt_set_renew_life");
if (!fun_krb5_get_init_creds_opt_set_renew_life) { syslog(LOG_ERR, "krb5_get_init_creds_opt_set_renew_life failed loading"); }
fun_krb5_get_init_creds_opt_set_tkt_life = dlsym(hf, "krb5_get_init_creds_opt_set_tkt_life");
if (!fun_krb5_get_init_creds_opt_set_tkt_life) { syslog(LOG_ERR, "krb5_get_init_creds_opt_set_tkt_life failed loading"); }
fun_krb5_get_init_creds_password = dlsym(hf, "krb5_get_init_creds_password");
if (!fun_krb5_get_init_creds_password) { syslog(LOG_ERR, "krb5_get_init_creds_password failed loading"); }
fun_krb5_get_kdc_cred = dlsym(hf, "krb5_get_kdc_cred");
if (!fun_krb5_get_kdc_cred) { syslog(LOG_ERR, "krb5_get_kdc_cred failed loading"); }
fun_krb5_get_kdc_sec_offset = dlsym(hf, "krb5_get_kdc_sec_offset");
if (!fun_krb5_get_kdc_sec_offset) { syslog(LOG_ERR, "krb5_get_kdc_sec_offset failed loading"); }
fun_krb5_init_context = dlsym(hf, "krb5_init_context");
if (!fun_krb5_init_context) { syslog(LOG_ERR, "krb5_init_context failed loading"); }
fun_krb5_make_principal = dlsym(hf, "krb5_make_principal");
if (!fun_krb5_make_principal) { syslog(LOG_ERR, "krb5_make_principal failed loading"); }
fun_krb5_parse_name = dlsym(hf, "krb5_parse_name");
if (!fun_krb5_parse_name) { syslog(LOG_ERR, "krb5_parse_name failed loading"); }
fun_krb5_principal_compare = dlsym(hf, "krb5_principal_compare");
if (!fun_krb5_principal_compare) { syslog(LOG_ERR, "krb5_principal_compare failed loading"); }
fun_krb5_principal_get_realm = dlsym(hf, "krb5_principal_get_realm");
if (!fun_krb5_principal_get_realm) { syslog(LOG_ERR, "krb5_principal_get_realm failed loading"); }
fun_krb5_timeofday = dlsym(hf, "krb5_timeofday");
if (!fun_krb5_timeofday) { syslog(LOG_ERR, "krb5_timeofday failed loading"); }
fun_krb5_unparse_name = dlsym(hf, "krb5_unparse_name");
if (!fun_krb5_unparse_name) { syslog(LOG_ERR, "krb5_unparse_name failed loading"); }
fun_krb5_us_timeofday = dlsym(hf, "krb5_us_timeofday");
if (!fun_krb5_us_timeofday) { syslog(LOG_ERR, "krb5_us_timeofday failed loading"); }
fun_krb5_kt_start_seq_get = dlsym(hf, "krb5_kt_start_seq_get");
if (!fun_krb5_kt_start_seq_get) { syslog(LOG_ERR, "krb5_kt_start_seq_get failed loading"); }
fun_krb5_kt_end_seq_get = dlsym(hf, "krb5_kt_end_seq_get");
if (!fun_krb5_kt_end_seq_get) { syslog(LOG_ERR, "krb5_kt_end_seq_get failed loading"); }
fun_krb5_xfree = dlsym(hf, "krb5_xfree");
if (!fun_krb5_xfree) { syslog(LOG_ERR, "krb5_xfree failed loading"); }
fun_krb5_kt_next_entry = dlsym(hf, "krb5_kt_next_entry");
if (!fun_krb5_kt_next_entry) { syslog(LOG_ERR, "krb5_kt_next_entry failed loading"); }
fun_krb5_kt_free_entry = dlsym(hf, "krb5_kt_free_entry");
if (!fun_krb5_kt_free_entry) { syslog(LOG_ERR, "krb5_kt_free_entry failed loading"); }
fun_gsskrb5_extract_authz_data_from_sec_context = dlsym(gf, "gsskrb5_extract_authz_data_from_sec_context");
if (!fun_gsskrb5_extract_authz_data_from_sec_context) { syslog(LOG_ERR, "gsskrb5_extract_authz_data_from_sec_context failed loading"); }
fun_krb5_sendauth = dlsym(hf, "krb5_sendauth");
if (!fun_krb5_sendauth) { syslog(LOG_ERR, "krb5_sendauth failed loading"); }
fun_krb5_free_ap_rep_enc_part = dlsym(hf, "krb5_free_ap_rep_enc_part");
if (!fun_krb5_free_ap_rep_enc_part) { syslog(LOG_ERR, "krb5_free_ap_rep_enc_part failed loading"); }
fun_krb5_free_error = dlsym(hf, "krb5_free_error");
if (!fun_krb5_free_error) { syslog(LOG_ERR, "krb5_free_error failed loading"); }
fun_krb5_recvauth = dlsym(hf, "krb5_recvauth");
if (!fun_krb5_recvauth) { syslog(LOG_ERR, "krb5_recvauth failed loading"); }
fun_krb5_recvauth_match_version = dlsym(hf, "krb5_recvauth_match_version");
if (!fun_krb5_recvauth_match_version) { syslog(LOG_ERR, "krb5_recvauth_match_version failed loading"); }
fun_krb5_mk_priv = dlsym(hf, "krb5_mk_priv");
if (!fun_krb5_mk_priv) { syslog(LOG_ERR, "krb5_mk_priv failed loading"); }
fun_krb5_rd_priv = dlsym(hf, "krb5_rd_priv");
if (!fun_krb5_rd_priv) { syslog(LOG_ERR, "krb5_rd_priv failed loading"); }
fun_krb5_mk_safe = dlsym(hf, "krb5_mk_safe");
if (!fun_krb5_mk_safe) { syslog(LOG_ERR, "krb5_mk_safe failed loading"); }
fun_krb5_rd_safe = dlsym(hf, "krb5_rd_safe");
if (!fun_krb5_rd_safe) { syslog(LOG_ERR, "krb5_rd_safe failed loading"); }
fun_krb5_set_home_dir_access = dlsym(hf, "krb5_set_home_dir_access");
if (!fun_krb5_set_home_dir_access) { syslog(LOG_ERR, "krb5_set_home_dir_access failed loading"); }
fun_krb5_verify_init_creds = dlsym(hf, "krb5_verify_init_creds");
if (!fun_krb5_verify_init_creds) { syslog(LOG_ERR, "krb5_verify_init_creds failed loading"); }
fun_krb5_verify_init_creds_opt_init = dlsym(hf, "krb5_verify_init_creds_opt_init");
if (!fun_krb5_verify_init_creds_opt_init) { syslog(LOG_ERR, "krb5_verify_init_creds_opt_init failed loading"); }
fun_krb5_verify_init_creds_opt_set_ap_req_nofail = dlsym(hf, "krb5_verify_init_creds_opt_set_ap_req_nofail");
if (!fun_krb5_verify_init_creds_opt_set_ap_req_nofail) { syslog(LOG_ERR, "krb5_verify_init_creds_opt_set_ap_req_nofail failed loading"); }
fun_krb5_kuserok = dlsym(hf, "krb5_kuserok");
if (!fun_krb5_kuserok) { syslog(LOG_ERR, "krb5_kuserok failed loading"); }
fun_com_right = dlsym(hf, "com_right");
if (!fun_com_right) { syslog(LOG_ERR, "com_right failed loading"); }
fun_com_right_r = dlsym(hf, "com_right_r");
if (!fun_com_right_r) { syslog(LOG_ERR, "com_right_r failed loading"); }
fun_gss_import_name = dlsym(gf, "gss_import_name");
if (!fun_gss_import_name) { syslog(LOG_ERR, "gss_import_name failed loading"); }
fun_krb5_appdefault_boolean = dlsym(hf, "krb5_appdefault_boolean");
if (!fun_krb5_appdefault_boolean) { syslog(LOG_ERR, "krb5_appdefault_boolean failed loading"); }
fun_krb5_appdefault_string = dlsym(hf, "krb5_appdefault_string");
if (!fun_krb5_appdefault_string) { syslog(LOG_ERR, "krb5_appdefault_string failed loading"); }
fun_gss_accept_sec_context = dlsym(gf, "gss_accept_sec_context");
if (!fun_gss_accept_sec_context) { syslog(LOG_ERR, "gss_accept_sec_context failed loading"); }
fun_gss_acquire_cred = dlsym(gf, "gss_acquire_cred");
if (!fun_gss_acquire_cred) { syslog(LOG_ERR, "gss_acquire_cred failed loading"); }
fun_gss_add_cred = dlsym(gf, "gss_add_cred");
if (!fun_gss_add_cred) { syslog(LOG_ERR, "gss_add_cred failed loading"); }
fun_gss_add_oid_set_member = dlsym(gf, "gss_add_oid_set_member");
if (!fun_gss_add_oid_set_member) { syslog(LOG_ERR, "gss_add_oid_set_member failed loading"); }
fun_gss_canonicalize_name = dlsym(gf, "gss_canonicalize_name");
if (!fun_gss_canonicalize_name) { syslog(LOG_ERR, "gss_canonicalize_name failed loading"); }
fun_gss_compare_name = dlsym(gf, "gss_compare_name");
if (!fun_gss_compare_name) { syslog(LOG_ERR, "gss_compare_name failed loading"); }
fun_gss_context_time = dlsym(gf, "gss_context_time");
if (!fun_gss_context_time) { syslog(LOG_ERR, "gss_context_time failed loading"); }
fun_gss_create_empty_oid_set = dlsym(gf, "gss_create_empty_oid_set");
if (!fun_gss_create_empty_oid_set) { syslog(LOG_ERR, "gss_create_empty_oid_set failed loading"); }
fun_gss_delete_sec_context = dlsym(gf, "gss_delete_sec_context");
if (!fun_gss_delete_sec_context) { syslog(LOG_ERR, "gss_delete_sec_context failed loading"); }
fun_gss_display_name = dlsym(gf, "gss_display_name");
if (!fun_gss_display_name) { syslog(LOG_ERR, "gss_display_name failed loading"); }
fun_gss_display_status = dlsym(gf, "gss_display_status");
if (!fun_gss_display_status) { syslog(LOG_ERR, "gss_display_status failed loading"); }
fun_gss_duplicate_name = dlsym(gf, "gss_duplicate_name");
if (!fun_gss_duplicate_name) { syslog(LOG_ERR, "gss_duplicate_name failed loading"); }
fun_gss_export_name = dlsym(gf, "gss_export_name");
if (!fun_gss_export_name) { syslog(LOG_ERR, "gss_export_name failed loading"); }
fun_gss_export_sec_context = dlsym(gf, "gss_export_sec_context");
if (!fun_gss_export_sec_context) { syslog(LOG_ERR, "gss_export_sec_context failed loading"); }
fun_gss_get_mic = dlsym(gf, "gss_get_mic");
if (!fun_gss_get_mic) { syslog(LOG_ERR, "gss_get_mic failed loading"); }
fun_gss_import_sec_context = dlsym(gf, "gss_import_sec_context");
if (!fun_gss_import_sec_context) { syslog(LOG_ERR, "gss_import_sec_context failed loading"); }
fun_gss_indicate_mechs = dlsym(gf, "gss_indicate_mechs");
if (!fun_gss_indicate_mechs) { syslog(LOG_ERR, "gss_indicate_mechs failed loading"); }
fun_gss_init_sec_context = dlsym(gf, "gss_init_sec_context");
if (!fun_gss_init_sec_context) { syslog(LOG_ERR, "gss_init_sec_context failed loading"); }
fun_gss_inquire_context = dlsym(gf, "gss_inquire_context");
if (!fun_gss_inquire_context) { syslog(LOG_ERR, "gss_inquire_context failed loading"); }
fun_gss_inquire_cred = dlsym(gf, "gss_inquire_cred");
if (!fun_gss_inquire_cred) { syslog(LOG_ERR, "gss_inquire_cred failed loading"); }
fun_gss_inquire_cred_by_mech = dlsym(gf, "gss_inquire_cred_by_mech");
if (!fun_gss_inquire_cred_by_mech) { syslog(LOG_ERR, "gss_inquire_cred_by_mech failed loading"); }
fun_gss_inquire_names_for_mech = dlsym(gf, "gss_inquire_names_for_mech");
if (!fun_gss_inquire_names_for_mech) { syslog(LOG_ERR, "gss_inquire_names_for_mech failed loading"); }
fun_gss_krb5_ccache_name = dlsym(gf, "gss_krb5_ccache_name");
if (!fun_gss_krb5_ccache_name) { syslog(LOG_ERR, "gss_krb5_ccache_name failed loading"); }
fun_gss_krb5_copy_ccache = dlsym(gf, "gss_krb5_copy_ccache");
if (!fun_gss_krb5_copy_ccache) { syslog(LOG_ERR, "gss_krb5_copy_ccache failed loading"); }
fun_gss_krb5_export_lucid_sec_context = dlsym(gf, "gss_krb5_export_lucid_sec_context");
if (!fun_gss_krb5_export_lucid_sec_context) { syslog(LOG_ERR, "gss_krb5_export_lucid_sec_context failed loading"); }
fun_gss_krb5_free_lucid_sec_context = dlsym(gf, "gss_krb5_free_lucid_sec_context");
if (!fun_gss_krb5_free_lucid_sec_context) { syslog(LOG_ERR, "gss_krb5_free_lucid_sec_context failed loading"); }
fun_gss_krb5_set_allowable_enctypes = dlsym(gf, "gss_krb5_set_allowable_enctypes");
if (!fun_gss_krb5_set_allowable_enctypes) { syslog(LOG_ERR, "gss_krb5_set_allowable_enctypes failed loading"); }
fun_gss_oid_equal = dlsym(gf, "gss_oid_equal");
if (!fun_gss_oid_equal) { syslog(LOG_ERR, "gss_oid_equal failed loading"); }
fun_gss_oid_to_str = dlsym(gf, "gss_oid_to_str");
if (!fun_gss_oid_to_str) { syslog(LOG_ERR, "gss_oid_to_str failed loading"); }
fun_gss_process_context_token = dlsym(gf, "gss_process_context_token");
if (!fun_gss_process_context_token) { syslog(LOG_ERR, "gss_process_context_token failed loading"); }
fun_gss_release_buffer = dlsym(gf, "gss_release_buffer");
if (!fun_gss_release_buffer) { syslog(LOG_ERR, "gss_release_buffer failed loading"); }
fun_gss_release_cred = dlsym(gf, "gss_release_cred");
if (!fun_gss_release_cred) { syslog(LOG_ERR, "gss_release_cred failed loading"); }
fun_gss_release_name = dlsym(gf, "gss_release_name");
if (!fun_gss_release_name) { syslog(LOG_ERR, "gss_release_name failed loading"); }
fun_gss_release_oid = dlsym(gf, "gss_release_oid");
if (!fun_gss_release_oid) { syslog(LOG_ERR, "gss_release_oid failed loading"); }
fun_gss_release_oid_set = dlsym(gf, "gss_release_oid_set");
if (!fun_gss_release_oid_set) { syslog(LOG_ERR, "gss_release_oid_set failed loading"); }
fun_gss_seal = dlsym(gf, "gss_seal");
if (!fun_gss_seal) { syslog(LOG_ERR, "gss_seal failed loading"); }
fun_gss_test_oid_set_member = dlsym(gf, "gss_test_oid_set_member");
if (!fun_gss_test_oid_set_member) { syslog(LOG_ERR, "gss_test_oid_set_member failed loading"); }
fun_gss_unseal = dlsym(gf, "gss_unseal");
if (!fun_gss_unseal) { syslog(LOG_ERR, "gss_unseal failed loading"); }
fun_gss_unwrap = dlsym(gf, "gss_unwrap");
if (!fun_gss_unwrap) { syslog(LOG_ERR, "gss_unwrap failed loading"); }
fun_gss_verify_mic = dlsym(gf, "gss_verify_mic");
if (!fun_gss_verify_mic) { syslog(LOG_ERR, "gss_verify_mic failed loading"); }
fun_gss_wrap = dlsym(gf, "gss_wrap");
if (!fun_gss_wrap) { syslog(LOG_ERR, "gss_wrap failed loading"); }
fun_gss_wrap_size_limit = dlsym(gf, "gss_wrap_size_limit");
if (!fun_gss_wrap_size_limit) { syslog(LOG_ERR, "gss_wrap_size_limit failed loading"); }
fun_krb5_cc_start_seq_get = dlsym(hf, "krb5_cc_start_seq_get");
if (!fun_krb5_cc_start_seq_get) { syslog(LOG_ERR, "krb5_cc_start_seq_get failed loading"); }
fun_krb5_cc_default_name = dlsym(hf, "krb5_cc_default_name");
if (!fun_krb5_cc_default_name) { syslog(LOG_ERR, "krb5_cc_default_name failed loading"); }
fun_krb5_cc_destroy = dlsym(hf, "krb5_cc_destroy");
if (!fun_krb5_cc_destroy) { syslog(LOG_ERR, "krb5_cc_destroy failed loading"); }
fun_krb5_cccol_cursor_free = dlsym(hf, "krb5_cccol_cursor_free");
if (!fun_krb5_cccol_cursor_free) { syslog(LOG_ERR, "krb5_cccol_cursor_free failed loading"); }
fun_krb5_cccol_cursor_new = dlsym(hf, "krb5_cccol_cursor_new");
if (!fun_krb5_cccol_cursor_new) { syslog(LOG_ERR, "krb5_cccol_cursor_new failed loading"); }
fun_krb5_cccol_cursor_next = dlsym(hf, "krb5_cccol_cursor_next");
if (!fun_krb5_cccol_cursor_next) { syslog(LOG_ERR, "krb5_cccol_cursor_next failed loading"); }
fun_krb5_free_host_realm = dlsym(hf, "krb5_free_host_realm");
if (!fun_krb5_free_host_realm) { syslog(LOG_ERR, "krb5_free_host_realm failed loading"); }
fun_krb5_get_default_realm = dlsym(hf, "krb5_get_default_realm");
if (!fun_krb5_get_default_realm) { syslog(LOG_ERR, "krb5_get_default_realm failed loading"); }
fun_krb5_get_host_realm = dlsym(hf, "krb5_get_host_realm");
if (!fun_krb5_get_host_realm) { syslog(LOG_ERR, "krb5_get_host_realm failed loading"); }
fun_krb5_gss_register_acceptor_identity = dlsym(gf, "krb5_gss_register_acceptor_identity");
if (!fun_krb5_gss_register_acceptor_identity) { syslog(LOG_ERR, "krb5_gss_register_acceptor_identity failed loading"); }
fun_krb5_cc_set_default_name = dlsym(hf, "krb5_cc_set_default_name");
if (!fun_krb5_cc_set_default_name) { syslog(LOG_ERR, "krb5_cc_set_default_name failed loading"); }
fun_krb5_kt_resolve = dlsym(hf, "krb5_kt_resolve");
if (!fun_krb5_kt_resolve) { syslog(LOG_ERR, "krb5_kt_resolve failed loading"); }
fun_krb5_kt_default = dlsym(hf, "krb5_kt_default");
if (!fun_krb5_kt_default) { syslog(LOG_ERR, "krb5_kt_default failed loading"); }
fun_krb5_kt_default_name = dlsym(hf, "krb5_kt_default_name");
if (!fun_krb5_kt_default_name) { syslog(LOG_ERR, "krb5_kt_default_name failed loading"); }
fun_krb5_kt_close = dlsym(hf, "krb5_kt_close");
if (!fun_krb5_kt_close) { syslog(LOG_ERR, "krb5_kt_close failed loading"); }
fun_krb5_kt_destroy = dlsym(hf, "krb5_kt_destroy");
if (!fun_krb5_kt_destroy) { syslog(LOG_ERR, "krb5_kt_destroy failed loading"); }
fun_krb5_auth_con_free = dlsym(hf, "krb5_auth_con_free");
if (!fun_krb5_auth_con_free) { syslog(LOG_ERR, "krb5_auth_con_free failed loading"); }
fun_krb5_auth_con_init = dlsym(hf, "krb5_auth_con_init");
if (!fun_krb5_auth_con_init) { syslog(LOG_ERR, "krb5_auth_con_init failed loading"); }
fun_krb5_auth_con_genaddrs = dlsym(hf, "krb5_auth_con_genaddrs");
if (!fun_krb5_auth_con_genaddrs) { syslog(LOG_ERR, "krb5_auth_con_genaddrs failed loading"); }
fun_krb5_auth_con_getlocalseqnumber = dlsym(hf, "krb5_auth_con_getlocalseqnumber");
if (!fun_krb5_auth_con_getlocalseqnumber) { syslog(LOG_ERR, "krb5_auth_con_getlocalseqnumber failed loading"); }
fun_krb5_auth_con_getremoteseqnumber = dlsym(hf, "krb5_auth_con_getremoteseqnumber");
if (!fun_krb5_auth_con_getremoteseqnumber) { syslog(LOG_ERR, "krb5_auth_con_getremoteseqnumber failed loading"); }
fun_krb5_auth_con_setflags = dlsym(hf, "krb5_auth_con_setflags");
if (!fun_krb5_auth_con_setflags) { syslog(LOG_ERR, "krb5_auth_con_setflags failed loading"); }
fun_krb5_auth_con_getflags = dlsym(hf, "krb5_auth_con_getflags");
if (!fun_krb5_auth_con_getflags) { syslog(LOG_ERR, "krb5_auth_con_getflags failed loading"); }
fun_krb5_clear_error_message = dlsym(hf, "krb5_clear_error_message");
if (!fun_krb5_clear_error_message) { syslog(LOG_ERR, "krb5_clear_error_message failed loading"); }
fun_krb5_free_error_message = dlsym(hf, "krb5_free_error_message");
if (!fun_krb5_free_error_message) { syslog(LOG_ERR, "krb5_free_error_message failed loading"); }
fun_krb5_get_error_message = dlsym(hf, "krb5_get_error_message");
if (!fun_krb5_get_error_message) { syslog(LOG_ERR, "krb5_get_error_message failed loading"); }
fun_krb5_set_default_realm = dlsym(hf, "krb5_set_default_realm");
if (!fun_krb5_set_default_realm) { syslog(LOG_ERR, "krb5_set_default_realm failed loading"); }
fun_krb5_set_error_message = dlsym(hf, "krb5_set_error_message");
if (!fun_krb5_set_error_message) { syslog(LOG_ERR, "krb5_set_error_message failed loading"); }
fun_krb5_vset_error_message = dlsym(hf, "krb5_vset_error_message");
if (!fun_krb5_vset_error_message) { syslog(LOG_ERR, "krb5_vset_error_message failed loading"); }
fun_com_err = dlsym(hf, "com_err");
if (!fun_com_err) { syslog(LOG_ERR, "com_err failed loading"); }
fun_com_err_va = dlsym(hf, "com_err_va");
if (!fun_com_err_va) { syslog(LOG_ERR, "com_err_va failed loading"); }
fun_reset_com_err_hook = dlsym(hf, "reset_com_err_hook");
if (!fun_reset_com_err_hook) { syslog(LOG_ERR, "reset_com_err_hook failed loading"); }
fun_set_com_err_hook = dlsym(hf, "set_com_err_hook");
if (!fun_set_com_err_hook) { syslog(LOG_ERR, "set_com_err_hook failed loading"); }
}

void heim_load_functions(void) {
static dispatch_once_t once = 0;
dispatch_once(&once, ^{ load_functions(); });
}

