/* This is a generated file */
#ifndef __gssapi_private_h__
#define __gssapi_private_h__

#include <stdarg.h>

#ifndef HEIMDAL_PRINTF_ATTRIBUTE
#if defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1 )))
#define HEIMDAL_PRINTF_ATTRIBUTE(x) __attribute__((format x))
#else
#define HEIMDAL_PRINTF_ATTRIBUTE(x)
#endif
#endif

struct gssapi_mech_interface_desc *
__gss_get_mechanism (gss_const_OID mech);

OM_uint32
_gss_acquire_mech_cred (
	OM_uint32 *minor_status,
	struct gssapi_mech_interface_desc *m,
	const struct _gss_mechanism_name *mn,
	gss_const_OID credential_type,
	const void *credential_data,
	OM_uint32 time_req,
	gss_const_OID desired_mech,
	gss_cred_usage_t cred_usage,
	struct _gss_mechanism_cred **output_cred_handle);

OM_uint32
_gss_copy_buffer (
	OM_uint32 *minor_status,
	const gss_buffer_t from_buf,
	gss_buffer_t to_buf);

struct _gss_mechanism_cred *
_gss_copy_cred (struct _gss_mechanism_cred *mc);

OM_uint32
_gss_copy_oid (
	OM_uint32 *minor_status,
	gss_const_OID from_oid,
	gss_OID to_oid);

struct _gss_name *
_gss_create_name (
	gss_name_t new_mn,
	struct gssapi_mech_interface_desc *m);

gss_name_t
_gss_cred_copy_name (
	OM_uint32 *minor_status,
	gss_cred_id_t credential,
	gss_const_OID mech);

OM_uint32
_gss_find_mn (
	OM_uint32 *minor_status,
	struct _gss_name *name,
	gss_const_OID mech,
	struct _gss_mechanism_name **output_mn);

OM_uint32
_gss_free_oid (
	OM_uint32 *minor_status,
	gss_OID oid);

void
_gss_load_mech (void);

void
_gss_load_plugins (void);

OM_uint32
_gss_mech_import_name (
	OM_uint32 * minor_status,
	gss_const_OID mech,
	struct _gss_name_type *names,
	const gss_buffer_t input_name_buffer,
	gss_const_OID input_name_type,
	gss_name_t * output_name);

OM_uint32
_gss_mech_inquire_names_for_mech (
	OM_uint32 * minor_status,
	struct _gss_name_type *names,
	gss_OID_set * name_types);

struct _gss_cred *
_gss_mg_alloc_cred (void);

OM_uint32
_gss_mg_allocate_buffer (
	OM_uint32 *minor_status,
	gss_iov_buffer_desc *buffer,
	size_t size);

void
_gss_mg_check_credential (gss_cred_id_t credential);

void
_gss_mg_check_name (gss_name_t name);

CFTypeRef
_gss_mg_copy_key (
	CFStringRef domain,
	CFStringRef key);

CFErrorRef
_gss_mg_create_cferror (
	OM_uint32 major_status,
	OM_uint32 minor_status,
	gss_const_OID mech);

void
_gss_mg_decode_be_uint32 (
	const void *ptr,
	uint32_t *n);

void
_gss_mg_decode_le_uint32 (
	const void *ptr,
	uint32_t *n);

void
_gss_mg_encode_be_uint32 (
	uint32_t n,
	uint8_t *p);

void
_gss_mg_encode_le_uint32 (
	uint32_t n,
	uint8_t *p);

void
_gss_mg_error (
	struct gssapi_mech_interface_desc *m,
	OM_uint32 min);

gss_iov_buffer_desc *
_gss_mg_find_buffer (
	gss_iov_buffer_desc *iov,
	int iov_count,
	OM_uint32 type);

OM_uint32
_gss_mg_get_error (
	const gss_OID mech,
	OM_uint32 value,
	gss_buffer_t string);

void
_gss_mg_log (
	int level,
	const char *fmt,
	...)
     HEIMDAL_PRINTF_ATTRIBUTE((printf, 2, 3));

void
_gss_mg_log_cred (
	int level,
	struct _gss_cred *cred,
	const char *fmt,
	...)
     HEIMDAL_PRINTF_ATTRIBUTE((printf, 3, 4));

int
_gss_mg_log_level (int level);

void
_gss_mg_log_name (
	int level,
	struct _gss_name *name,
	gss_OID mech_type,
	const char *fmt,
	...)
     HEIMDAL_PRINTF_ATTRIBUTE((printf, 4, 5));

void
_gss_mg_release_cred (struct _gss_cred *cred);

void
_gss_mg_release_name (struct _gss_name *name);

gss_OID
_gss_mg_support_mechanism (gss_const_OID mech);

int
_gss_mo_get_ctx_as_string (
	gss_const_OID mech,
	struct gss_mo_desc *mo,
	gss_buffer_t value);

int
_gss_mo_get_option_0 (
	gss_const_OID mech,
	struct gss_mo_desc *mo,
	gss_buffer_t value);

int
_gss_mo_get_option_1 (
	gss_const_OID mech,
	struct gss_mo_desc *mo,
	gss_buffer_t value);

OM_uint32 GSSAPI_LIB_FUNCTION
gss_acquire_cred_ex (
	const gss_name_t desired_name,
	OM_uint32 flags,
	OM_uint32 time_req,
	gss_const_OID desired_mech,
	gss_cred_usage_t cred_usage,
	gss_auth_identity_t identity,
	gss_acquire_cred_complete complete);

OM_uint32 GSSAPI_LIB_FUNCTION
gss_acquire_cred_ex_f (
	gss_status_id_t status,
	gss_name_t desired_name,
	OM_uint32 flags,
	OM_uint32 time_req,
	gss_const_OID desired_mech,
	gss_cred_usage_t cred_usage,
	gss_auth_identity_t identity,
	void * userctx,
	void (*usercomplete)(void *, OM_uint32, gss_status_id_t, gss_cred_id_t, gss_OID_set, OM_uint32));

OM_uint32
gss_acquire_cred_ext (
	OM_uint32 *minor_status,
	const gss_name_t desired_name,
	gss_const_OID credential_type,
	const void *credential_data,
	OM_uint32 time_req,
	gss_const_OID desired_mech,
	gss_cred_usage_t cred_usage,
	gss_cred_id_t *output_cred_handle);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_add_cred_with_password (
	OM_uint32 *minor_status,
	const gss_cred_id_t input_cred_handle,
	const gss_name_t desired_name,
	const gss_OID desired_mech,
	const gss_buffer_t password,
	gss_cred_usage_t cred_usage,
	OM_uint32 initiator_time_req,
	OM_uint32 acceptor_time_req,
	gss_cred_id_t *output_cred_handle,
	gss_OID_set *actual_mechs,
	OM_uint32 *initiator_time_rec,
	OM_uint32 *acceptor_time_rec);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_authorize_localname (
	OM_uint32 *minor_status,
	const gss_name_t gss_name,
	const gss_name_t gss_user);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_context_query_attributes (
	OM_uint32 *minor_status,
	const gss_ctx_id_t context_handle,
	const gss_OID attribute,
	void *data,
	size_t len);

OM_uint32
gss_cred_hold (
	OM_uint32 *min_stat,
	gss_cred_id_t cred_handle);

OM_uint32
gss_cred_label_get (
	OM_uint32 * min_stat,
	gss_cred_id_t cred_handle,
	const char * label,
	gss_buffer_t value);

OM_uint32
gss_cred_label_set (
	OM_uint32 * min_stat,
	gss_cred_id_t cred_handle,
	const char * label,
	gss_buffer_t value);

OM_uint32
gss_cred_unhold (
	OM_uint32 *min_stat,
	gss_cred_id_t cred_handle);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_delete_name_attribute (
	OM_uint32 *minor_status,
	gss_name_t input_name,
	gss_buffer_t attr);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_display_name_ext (
	OM_uint32 *minor_status,
	gss_name_t input_name,
	gss_OID display_as_name_type,
	gss_buffer_t display_name);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_export_name_composite (
	OM_uint32 *minor_status,
	gss_name_t input_name,
	gss_buffer_t exp_composite_name);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_get_name_attribute (
	OM_uint32 *minor_status,
	gss_name_t input_name,
	gss_buffer_t attr,
	int *authenticated,
	int *complete,
	gss_buffer_t value,
	gss_buffer_t display_value,
	int *more);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_krb5_get_tkt_flags (
	OM_uint32 *minor_status,
	gss_ctx_id_t context_handle,
	OM_uint32 *tkt_flags);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_krb5_import_cred (
	OM_uint32 *minor_status,
	struct krb5_ccache_data *id,
	struct Principal *keytab_principal,
	struct krb5_keytab_data *keytab,
	gss_cred_id_t *cred);

void
gss_mg_collect_error (
	gss_OID mech,
	OM_uint32 maj,
	OM_uint32 min);

OM_uint32
gss_mg_export_name (
	OM_uint32 *minor_status,
	const gss_const_OID mech,
	const void *name,
	size_t length,
	gss_buffer_t exported_name);

OM_uint32
gss_mg_gen_cb (
	OM_uint32 *minor_status,
	const gss_channel_bindings_t b,
	uint8_t p[16],
	gss_buffer_t buffer);

OM_uint32
gss_mg_set_error_string (
	gss_OID mech,
	OM_uint32 maj,
	OM_uint32 min,
	const char *fmt,
	...)
     HEIMDAL_PRINTF_ATTRIBUTE((printf, 4, 5));

OM_uint32
gss_mg_validate_cb (
	OM_uint32 *minor_status,
	const gss_channel_bindings_t b,
	const uint8_t p[16],
	gss_buffer_t buffer);

GSSAPI_LIB_FUNCTION int GSSAPI_LIB_CALL
gss_mo_get (
	gss_const_OID mech,
	gss_const_OID option,
	gss_buffer_t value);

GSSAPI_LIB_FUNCTION void GSSAPI_LIB_CALL
gss_mo_list (
	gss_const_OID mech,
	gss_OID_set *options);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_mo_name (
	gss_const_OID mech,
	gss_const_OID option,
	gss_buffer_t name);

GSSAPI_LIB_FUNCTION int GSSAPI_LIB_CALL
gss_mo_set (
	gss_const_OID mech,
	gss_const_OID option,
	int enable,
	gss_buffer_t value);

GSSAPI_LIB_FUNCTION gss_const_OID GSSAPI_LIB_CALL
gss_name_to_oid (const char *name);

GSSAPI_LIB_FUNCTION const char * GSSAPI_LIB_CALL
gss_oid_to_name (gss_const_OID oid);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_pname_to_uid (
	OM_uint32 *minor_status,
	const gss_name_t pname,
	const gss_OID mech_type,
	uid_t *uidp);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_release_iov_buffer (
	OM_uint32 *minor_status,
	gss_iov_buffer_desc *iov,
	int iov_count);

void
gss_set_log_function (
	void *ctx,
	void (*func)(void *ctx, int level, const char *fmt, va_list));

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_set_name_attribute (
	OM_uint32 *minor_status,
	gss_name_t input_name,
	int complete,
	gss_buffer_t attr,
	gss_buffer_t value);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_store_cred (
	OM_uint32 *minor_status,
	gss_cred_id_t input_cred_handle,
	gss_cred_usage_t cred_usage,
	const gss_OID desired_mech,
	OM_uint32 overwrite_cred,
	OM_uint32 default_cred,
	gss_OID_set *elements_stored,
	gss_cred_usage_t *cred_usage_stored);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_unwrap_iov (
	OM_uint32 *minor_status,
	gss_ctx_id_t context_handle,
	int *conf_state,
	gss_qop_t *qop_state,
	gss_iov_buffer_desc *iov,
	int iov_count);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_wrap_iov (
	OM_uint32 * minor_status,
	gss_ctx_id_t context_handle,
	int conf_req_flag,
	gss_qop_t qop_req,
	int * conf_state,
	gss_iov_buffer_desc *iov,
	int iov_count);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_wrap_iov_length (
	OM_uint32 * minor_status,
	gss_ctx_id_t context_handle,
	int conf_req_flag,
	gss_qop_t qop_req,
	int *conf_state,
	gss_iov_buffer_desc *iov,
	int iov_count);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gsskrb5_extract_authtime_from_sec_context (
	OM_uint32 *minor_status,
	gss_ctx_id_t context_handle,
	time_t *authtime);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gsskrb5_extract_service_keyblock (
	OM_uint32 *minor_status,
	gss_ctx_id_t context_handle,
	struct EncryptionKey **keyblock);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gsskrb5_get_initiator_subkey (
	OM_uint32 *minor_status,
	gss_ctx_id_t context_handle,
	struct EncryptionKey **keyblock);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gsskrb5_get_subkey (
	OM_uint32 *minor_status,
	gss_ctx_id_t context_handle,
	struct EncryptionKey **keyblock);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gsskrb5_get_time_offset (int *offset);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gsskrb5_plugin_register (struct gsskrb5_krb5_plugin *c);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gsskrb5_set_default_realm (const char *realm);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gsskrb5_set_dns_canonicalize (int flag);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gsskrb5_set_send_to_kdc (struct gsskrb5_send_to_kdc *c);

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gsskrb5_set_time_offset (int offset);

#endif /* __gssapi_private_h__ */
