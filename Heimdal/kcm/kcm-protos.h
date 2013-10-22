/* This is a generated file */
#ifndef __kcm_protos_h__
#define __kcm_protos_h__

#include <stdarg.h>

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

#ifdef __cplusplus
extern "C" {
#endif

krb5_error_code
kcm_access (
	krb5_context /*context*/,
	kcm_client */*client*/,
	kcm_operation /*opcode*/,
	kcm_ccache /*ccache*/);

void
kcm_cache_remove_session (pid_t /*session*/);

krb5_error_code
kcm_ccache_acquire (
	krb5_context /*context*/,
	kcm_ccache /*ccache*/,
	time_t */*expire*/);

krb5_error_code
kcm_ccache_destroy (
	krb5_context /*context*/,
	const char */*name*/);

krb5_error_code
kcm_ccache_destroy_client (
	krb5_context /*context*/,
	kcm_client */*client*/,
	const char */*name*/);

krb5_error_code
kcm_ccache_enqueue_default (
	krb5_context /*context*/,
	kcm_ccache /*cache*/,
	krb5_creds */*newcred*/);

struct kcm_creds *
kcm_ccache_find_cred_uuid (
	krb5_context /*context*/,
	kcm_ccache /*ccache*/,
	kcmuuid_t /*uuid*/);

char *
kcm_ccache_first_name (kcm_client */*client*/);

const char *
kcm_client_get_execpath(kcm_client *client);

krb5_error_code
kcm_ccache_get_uuids (
	krb5_context /*context*/,
	kcm_client */*client*/,
	kcm_operation /*opcode*/,
	krb5_storage */*sp*/);

krb5_error_code
kcm_ccache_new (
	krb5_context /*context*/,
	const char */*name*/,
	kcm_ccache */*ccache*/);

krb5_error_code
kcm_ccache_new_client (
	krb5_context /*context*/,
	kcm_client */*client*/,
	const char */*name*/,
	kcm_ccache */*ccache_p*/);

char *kcm_ccache_nextid (
	pid_t /*pid*/,
	uid_t /*uid*/);

krb5_error_code
kcm_ccache_refresh (
	krb5_context /*context*/,
	kcm_ccache /*ccache*/,
	time_t */*expire*/);

krb5_error_code
kcm_ccache_remove_cred (
	krb5_context /*context*/,
	kcm_ccache /*ccache*/,
	krb5_flags /*whichfields*/,
	const krb5_creds */*mcreds*/);

krb5_error_code
kcm_ccache_remove_cred_internal (
	krb5_context /*context*/,
	kcm_ccache /*ccache*/,
	krb5_flags /*whichfields*/,
	const krb5_creds */*mcreds*/);

krb5_error_code
kcm_ccache_remove_creds (
	krb5_context /*context*/,
	kcm_ccache /*ccache*/);

krb5_error_code
kcm_ccache_remove_creds_internal (
	krb5_context /*context*/,
	kcm_ccache /*ccache*/);

krb5_error_code
kcm_ccache_resolve_by_name (
	krb5_context /*context*/,
	const char */*name*/,
	kcm_ccache */*ccache*/);

krb5_error_code
kcm_ccache_resolve_by_uuid (
	krb5_context /*context*/,
	kcmuuid_t /*uuid*/,
	kcm_ccache */*ccache*/);

krb5_error_code
kcm_ccache_resolve_client (
	krb5_context /*context*/,
	kcm_client */*client*/,
	kcm_operation /*opcode*/,
	const char */*name*/,
	kcm_ccache */*ccache*/);

krb5_error_code
kcm_ccache_retrieve_cred (
	krb5_context /*context*/,
	kcm_ccache /*ccache*/,
	krb5_flags /*whichfields*/,
	const krb5_creds */*mcreds*/,
	krb5_creds **/*credp*/);

krb5_error_code
kcm_ccache_retrieve_cred_internal (
	krb5_context /*context*/,
	kcm_ccache /*ccache*/,
	krb5_flags /*whichfields*/,
	const krb5_creds */*mcreds*/,
	krb5_creds **/*creds*/);

krb5_error_code
kcm_ccache_store_cred (
	krb5_context /*context*/,
	kcm_ccache /*ccache*/,
	krb5_creds */*creds*/,
	int /*copy*/);

krb5_error_code
kcm_ccache_store_cred_internal (
	krb5_context /*context*/,
	kcm_ccache /*ccache*/,
	krb5_creds */*creds*/,
	kcmuuid_t /* uuid */,
	int /*copy*/);

krb5_error_code
kcm_chmod (
	krb5_context /*context*/,
	kcm_client */*client*/,
	kcm_ccache /*ccache*/,
	uint16_t /*mode*/);

krb5_error_code
kcm_chown (
	krb5_context /*context*/,
	kcm_client */*client*/,
	kcm_ccache /*ccache*/,
	uid_t /*uid*/);

void
kcm_configure (
	int /*argc*/,
	char **/*argv*/);

krb5_error_code
kcm_debug_ccache (krb5_context /*context*/);

krb5_error_code
kcm_debug_events (krb5_context /*context*/);

krb5_error_code
kcm_dispatch (
	krb5_context /*context*/,
	kcm_client */*client*/,
	krb5_data */*req_data*/,
	krb5_data */*resp_data*/);

krb5_error_code
kcm_internal_ccache (
	krb5_context /*context*/,
	kcm_ccache /*c*/,
	krb5_ccache /*id*/);

int
kcm_is_same_session (
	kcm_client */*client*/,
	uid_t /*uid*/,
	pid_t /*session*/);

void
kcm_log (
	int /*level*/,
	const char */*fmt*/,
	...)
     __attribute__ ((format (printf, 2, 3)));

char*
kcm_log_msg (
	int /*level*/,
	const char */*fmt*/,
	...)
     __attribute__ ((format (printf, 2, 3)));

char*
kcm_log_msg_va (
	int /*level*/,
	const char */*fmt*/,
	va_list /*ap*/)
     __attribute__ ((format (printf, 2, 0)));

const char *
kcm_op2string (kcm_operation /*opcode*/);

void
kcm_openlog (void);

krb5_error_code
kcm_release_ccache (
	krb5_context /*context*/,
	kcm_ccache /*c*/);

krb5_error_code
kcm_retain_ccache (
	krb5_context /*context*/,
	kcm_ccache /*ccache*/);

void
kcm_service (
	void */*ctx*/,
	const heim_idata */*req*/,
	const heim_icred /*cred*/,
	heim_ipc_complete /*complete*/,
	heim_sipc_call /*cctx*/);

void
kcm_session_add (pid_t /*session_id*/);

void
kcm_session_setup_handler (void);

void
kcm_update_renew_time (kcm_ccache /*ccache*/);

void
kcm_update_expire_time(kcm_ccache, time_t);

krb5_error_code
kcm_zero_ccache_data (
	krb5_context /*context*/,
	kcm_ccache /*cache*/);

krb5_error_code
kcm_zero_ccache_data_internal (
	krb5_context /*context*/,
	kcm_ccache /*cache*/);

void
kcm_unparse_cache_data(krb5_context context, krb5_data *data);
    
krb5_error_code
kcm_store_io(krb5_context context,
	     krb5_uuid uuid,
	     void *ptr,
	     size_t length,
	     krb5_data *data,
	     bool encrypt);
    
krb5_error_code
kcm_create_key(krb5_uuid uuid);

void kcm_read_dump(krb5_context context);
void kcm_write_dump(krb5_context context);

krb5_error_code
kcm_unparse_digest_all(krb5_context context, krb5_storage *sp);

krb5_error_code
kcm_parse_digest_one(krb5_context context, krb5_storage *sp);

krb5_error_code
kcm_unparse_wrap(krb5_storage *sp, char *name, int32_t session, int (^wrapped)(krb5_storage *inner));

krb5_error_code
kcm_unparse_challenge_all(krb5_context context, krb5_storage *sp);

krb5_error_code
kcm_parse_ntlm_challenge_one(krb5_context context, krb5_storage *sp);


krb5_error_code
kcm_ccache_update_acquire_status(krb5_context context,
				 kcm_ccache ccache,
				 int status, krb5_error_code ret);

#ifdef __cplusplus
}
#endif

#endif /* __kcm_protos_h__ */
