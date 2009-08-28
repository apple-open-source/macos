#include <TargetConditionals.h>

#define KRB5_SUPRESS_DEPRECATED_WARNINGS 1
#define USE_PASSWORD_SERVER              1
#define USE_CFBUNDLE                     1
#define BIG_ENDIAN_MASTER_KEY            1
#define DISABLE_PKINIT                   1 // disables UMich pkinit
#define DISABLE_IPROP                    1

#if KERBEROS_LITE
#define LEAN_CLIENT                      1
#define NO_FILE_CCACHE                   1
#else
#define KIM_BUILTIN_UI                   1
#define KIM_TO_KLL_SHIM                  1
#define USE_CCAPI                        1
#define USE_CCAPI_V3                     1
#define USE_KIM                          1
#define APPLE_PKINIT                     1
#endif

#define KRB5_PLUGIN_BUNDLE_DIR          "/System/Library/KerberosPlugins/KerberosFrameworkPlugins"
#define KDB5_PLUGIN_BUNDLE_DIR          "/System/Library/KerberosPlugins/KerberosDatabasePlugins"
#define KRB5_AUTHDATA_PLUGIN_BUNDLE_DIR "/System/Library/KerberosPlugins/KerberosAuthDataPlugins"
#define KRB5_KIM_UI_PLUGIN_BUNDLE_DIR   "/System/Library/KerberosPlugins/KerberosUIPlugins"

#define NDEBUG                           1
#define CONFIG_SMALL_NO_CRYPTO           1

#define SHARED                           1

#define KRB5_PRIVATE                     1
#define KRB5_DEPRECATED                  1

#define krb5_decrypt_tkt_part                __KerberosInternal_krb5_decrypt_tkt_part
#define krb5_free_enc_tkt_part               __KerberosInternal_krb5_free_enc_tkt_part
#define decode_krb5_ticket                   __KerberosInternal_decode_krb5_ticket
#define encode_krb5_enc_data                 __KerberosInternal_encode_krb5_enc_data
#define krb5_kt_free_entry                   __KerberosInternal_krb5_kt_free_entry
#define decode_krb5_as_req                   __KerberosInternal_decode_krb5_as_req
#define krb5_crypto_us_timeofday             __KerberosInternal_krb5_crypto_us_timeofday
#define krb5_free_kdc_req                    __KerberosInternal_krb5_free_kdc_req
#define krb5_encode_kdc_rep                  __KerberosInternal_krb5_encode_kdc_rep
#define krb5_encrypt_tkt_part                __KerberosInternal_krb5_encrypt_tkt_part
#define krb5_free_pa_data                    __KerberosInternal_krb5_free_pa_data
#define decode_krb5_authdata                 __KerberosInternal_decode_krb5_authdata
#define decode_krb5_tgs_req                  __KerberosInternal_decode_krb5_tgs_req
#define krb5_check_transited_list            __KerberosInternal_krb5_check_transited_list
#define krb5_free_realm_tree                 __KerberosInternal_krb5_free_realm_tree
#define krb5_walk_realm_tree                 __KerberosInternal_krb5_walk_realm_tree
#define decode_krb5_enc_data                 __KerberosInternal_decode_krb5_enc_data
#define decode_krb5_enc_sam_response_enc     __KerberosInternal_decode_krb5_enc_sam_response_enc
#define decode_krb5_pa_enc_ts                __KerberosInternal_decode_krb5_pa_enc_ts
#define decode_krb5_predicted_sam_response   __KerberosInternal_decode_krb5_predicted_sam_response
#define decode_krb5_sam_response             __KerberosInternal_decode_krb5_sam_response
#define encode_krb5_etype_info               __KerberosInternal_encode_krb5_etype_info
#define encode_krb5_etype_info2              __KerberosInternal_encode_krb5_etype_info2
#define encode_krb5_padata_sequence          __KerberosInternal_encode_krb5_padata_sequence
#define encode_krb5_predicted_sam_response   __KerberosInternal_encode_krb5_predicted_sam_response
#define encode_krb5_sam_challenge            __KerberosInternal_encode_krb5_sam_challenge
#define krb5_free_etype_info                 __KerberosInternal_krb5_free_etype_info
#define krb5_free_predicted_sam_response     __KerberosInternal_krb5_free_predicted_sam_response
#define krb5_free_sam_response               __KerberosInternal_krb5_free_sam_response
#define krb5_rc_store                        __KerberosInternal_krb5_rc_store
#define mit_des_fixup_key_parity             __KerberosInternal_mit_des_fixup_key_parity
#define mit_des_is_weak_key                  __KerberosInternal_mit_des_is_weak_key
#define decode_krb5_ap_req                   __KerberosInternal_decode_krb5_ap_req
#define encode_krb5_kdc_req_body             __KerberosInternal_encode_krb5_kdc_req_body
#define krb5_free_ap_req                     __KerberosInternal_krb5_free_ap_req
#define krb5_rc_close                        __KerberosInternal_krb5_rc_close
#define krb5_rc_expunge                      __KerberosInternal_krb5_rc_expunge
#define krb5_rc_initialize                   __KerberosInternal_krb5_rc_initialize
#define krb5_rc_recover                      __KerberosInternal_krb5_rc_recover
#define krb5_rc_resolve_full                 __KerberosInternal_krb5_rc_resolve_full
#define krb5_rd_req_decoded_anyflag          __KerberosInternal_krb5_rd_req_decoded_anyflag
#define krb5int_cm_call_select               __KerberosInternal_krb5int_cm_call_select
#define krb5int_foreach_localaddr            __KerberosInternal_krb5int_foreach_localaddr
#define krb5int_getnameinfo                  __KerberosInternal_krb5int_getnameinfo
#define krb5int_sendtokdc_debug_handler      __KerberosInternal_krb5int_sendtokdc_debug_handler
#define krb5_copy_addr                       __KerberosInternal_krb5_copy_addr
#define krb5_free_address                    __KerberosInternal_krb5_free_address
#define krb5int_getspecific                  __KerberosInternal_krb5int_getspecific
#define krb5int_key_register                 __KerberosInternal_krb5int_key_register
#define krb5int_setspecific                  __KerberosInternal_krb5int_setspecific
#define krb5int_mutex_lock                   __KerberosInternal_krb5int_mutex_lock
#define krb5int_mutex_unlock                 __KerberosInternal_krb5int_mutex_unlock
#define krb5int_mutex_alloc                  __KerberosInternal_krb5int_mutex_alloc
#define krb5int_mutex_free                   __KerberosInternal_krb5int_mutex_free
#define krb5_net_write                       __KerberosInternal_krb5_net_write
#define krb5_defkeyname                      __KerberosInternal_krb5_defkeyname
#define krb5_lock_file                       __KerberosInternal_krb5_lock_file
#define krb5_kt_register                     __KerberosInternal_krb5_kt_register
#define krb5_read_message                    __KerberosInternal_krb5_read_message
#define krb5_write_message                   __KerberosInternal_krb5_write_message

#define krb5_is_permitted_enctype            __KerberosInternal_krb5_is_permitted_enctype
#define krb5_principal2salt_norealm          __KerberosInternal_krb5_principal2salt_norealm

#define krb5int_open_plugin_dirs             __KerberosInternal_krb5int_open_plugin_dirs
#define krb5int_close_plugin_dirs            __KerberosInternal_krb5int_close_plugin_dirs
#define krb5int_get_plugin_dir_func          __KerberosInternal_krb5int_get_plugin_dir_func
#define krb5int_free_plugin_dir_func         __KerberosInternal_krb5int_free_plugin_dir_func
#define krb5int_get_plugin_dir_data          __KerberosInternal_krb5int_get_plugin_dir_data
#define krb5int_free_plugin_dir_data         __KerberosInternal_krb5int_free_plugin_dir_data

#define krb5int_buf_init_fixed               __KerberosInternal_krb5int_buf_init_fixed
#define krb5int_buf_init_dynamic             __KerberosInternal_krb5int_buf_init_dynamic
#define krb5int_buf_add                      __KerberosInternal_krb5int_buf_add
#define krb5int_buf_add_len                  __KerberosInternal_krb5int_buf_add_len
#define krb5int_buf_add_fmt                  __KerberosInternal_krb5int_buf_add_fmt
#define krb5int_buf_truncate                 __KerberosInternal_krb5int_buf_truncate
#define krb5int_buf_data                     __KerberosInternal_krb5int_buf_data
#define krb5int_buf_len                      __KerberosInternal_krb5int_buf_len
#define krb5int_free_buf                     __KerberosInternal_krb5int_free_buf

#if defined(KFM_TARGET_CONFIGURE)

#elif defined(KFM_TARGET_COM_ERR)

#elif defined(KFM_TARGET_CCAPI)

#elif defined(KFM_TARGET_CCACHESERVER)

#elif defined(KFM_TARGET_KIM)
#    define LIBDIR                   "/usr/lib"

#elif defined(KFM_TARGET_SUPPORT)

#elif defined(KFM_TARGET_SS)

#elif defined(KFM_TARGET_PROFILE)

#elif defined(KFM_TARGET_K5CRYPTO)

#elif defined(KFM_TARGET_KRB5)
#    define LIBDIR                   "/usr/lib"

#elif defined(KFM_TARGET_DES425)

#elif defined(KFM_TARGET_KRB4)
#    define KRB4_USE_KEYTAB          1

#elif defined(KFM_TARGET_GSSAPI)

#elif defined(KFM_TARGET_GSSRPC)
#    define GSSAPI_KRB5              1
#    define DEBUG_GSSAPI             0
#    define GSSRPC__IMPL             1

#elif defined(KFM_TARGET_KDB5)
#    define KDB5_USE_LIB_KDB_DB2     1

#elif defined(KFM_TARGET_KDB_LDAP)
#define krb5_dbe_lookup_last_pwd_change kdb_ldap_dbe_lookup_last_pwd_change
#define krb5_dbe_lookup_mod_princ_data  kdb_ldap_dbe_lookup_mod_princ_data
#define krb5_dbe_lookup_tl_data         kdb_ldap_dbe_lookup_tl_data
#define krb5_dbe_update_last_pwd_change kdb_ldap_dbe_update_last_pwd_change
#define krb5_dbe_update_mod_princ_data  kdb_ldap_dbe_update_mod_princ_data
#define krb5_dbe_update_tl_data         kdb_ldap_dbe_update_tl_data

#elif defined(KFM_TARGET_KADM5CLNT)

#elif defined(KFM_TARGET_KADM5SRV)

#elif defined(KFM_TARGET_APPUTILS)

#elif defined(KFM_TARGET_KRB524D)
#    define USE_MASTER               1
#    define KRB524_PRIVATE           1

#elif defined(KFM_TARGET_KRB5KDC)
#    define LIBDIR                   "/usr/lib"

#elif defined(KFM_TARGET_KADMIN)

#elif defined(KFM_TARGET_KADMIN_LOCAL)

#elif defined(KFM_TARGET_KDB5_UTIL)
#    define KDB4_DISABLE             1

#elif defined(KFM_TARGET_KDB5_LDAP_UTIL)
#    define KDB4_DISABLE             1

#elif defined(KFM_TARGET_KTUTIL)

#elif defined(KFM_TARGET_KADMIND)

#elif defined(KFM_TARGET_KPROP)

#elif defined(KFM_TARGET_KPROPD)

#elif defined(KFM_TARGET_KPROPLOG)

#elif defined(KFM_TARGET_DB2)
#define PLUGIN                       1

#elif defined(KFM_TARGET_KLDAP)
#define PLUGIN                       1

#elif defined(KFM_TARGET_TEST_PLUGIN)

#elif defined(KFM_TARGET_TEST_CCAPI)

#elif defined(KFM_TARGET_TEST_KIM)

#elif defined(KFM_TARGET_TEST_UI_PLUGIN)

#else
#    error "KfM target macro not defined"
#endif
