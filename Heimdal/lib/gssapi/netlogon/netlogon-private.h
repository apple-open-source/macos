/* This is a generated file */
#ifndef __netlogon_private_h__
#define __netlogon_private_h__

#include <stdarg.h>

gssapi_mech_interface
__gss_netlogon_initialize (void);

OM_uint32
_netlogon_accept_sec_context (
	OM_uint32 * /*minor_status*/,
	gss_ctx_id_t * /*context_handle*/,
	const gss_cred_id_t /*acceptor_cred_handle*/,
	const gss_buffer_t /*input_token_buffer*/,
	const gss_channel_bindings_t /*input_chan_bindings*/,
	gss_name_t * /*src_name*/,
	gss_OID * /*mech_type*/,
	gss_buffer_t /*output_token*/,
	OM_uint32 * /*ret_flags*/,
	OM_uint32 * /*time_rec*/,
	gss_cred_id_t * delegated_cred_handle );

OM_uint32
_netlogon_acquire_cred (
	OM_uint32 * /*min_stat*/,
	const gss_name_t /*desired_name*/,
	OM_uint32 /*time_req*/,
	const gss_OID_set /*desired_mechs*/,
	gss_cred_usage_t /*cred_usage*/,
	gss_cred_id_t * /*output_cred_handle*/,
	gss_OID_set * /*actual_mechs*/,
	OM_uint32 * /*time_rec*/);

OM_uint32
_netlogon_acquire_cred_ex (
	gss_status_id_t /*status*/,
	const gss_name_t /*desired_name*/,
	OM_uint32 /*flags*/,
	OM_uint32 /*time_req*/,
	gss_cred_usage_t /*cred_usage*/,
	gss_auth_identity_t /*identity*/,
	void */*ctx*/,
	void (*/*complete*/)(void *, OM_uint32, gss_status_id_t, gss_cred_id_t, OM_uint32));

OM_uint32
_netlogon_add_cred (
	 OM_uint32 */*minor_status*/,
	const gss_cred_id_t /*input_cred_handle*/,
	const gss_name_t /*desired_name*/,
	const gss_OID /*desired_mech*/,
	gss_cred_usage_t /*cred_usage*/,
	OM_uint32 /*initiator_time_req*/,
	OM_uint32 /*acceptor_time_req*/,
	gss_cred_id_t */*output_cred_handle*/,
	gss_OID_set */*actual_mechs*/,
	OM_uint32 */*initiator_time_rec*/,
	OM_uint32 */*acceptor_time_rec*/);

OM_uint32
_netlogon_canonicalize_name (
	 OM_uint32 * /*minor_status*/,
	const gss_name_t /*input_name*/,
	const gss_OID /*mech_type*/,
	gss_name_t * output_name );

OM_uint32
_netlogon_compare_name (
	OM_uint32 * /*minor_status*/,
	const gss_name_t /*name1*/,
	const gss_name_t /*name2*/,
	int * name_equal );

OM_uint32
_netlogon_context_time (
	OM_uint32 * /*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	OM_uint32 * time_rec );

void
_netlogon_decode_be_uint32 (
	const void */*ptr*/,
	uint32_t */*n*/);

void
_netlogon_decode_le_uint32 (
	const void */*ptr*/,
	uint32_t */*n*/);

OM_uint32
_netlogon_delete_sec_context (
	OM_uint32 * /*minor_status*/,
	gss_ctx_id_t * /*context_handle*/,
	gss_buffer_t /*output_token*/);

OM_uint32
_netlogon_display_name (
	OM_uint32 * /*minor_status*/,
	const gss_name_t /*input_name*/,
	gss_buffer_t /*output_name_buffer*/,
	gss_OID * output_name_type );

OM_uint32
_netlogon_display_status (
	OM_uint32 */*minor_status*/,
	OM_uint32 /*status_value*/,
	int /*status_type*/,
	const gss_OID /*mech_type*/,
	OM_uint32 */*message_context*/,
	gss_buffer_t /*status_string*/);

OM_uint32
_netlogon_duplicate_name (
	 OM_uint32 * /*minor_status*/,
	const gss_name_t /*src_name*/,
	gss_name_t * dest_name );

void
_netlogon_encode_be_uint32 (
	uint32_t /*n*/,
	uint8_t */*p*/);

void
_netlogon_encode_le_uint32 (
	uint32_t /*n*/,
	uint8_t */*p*/);

OM_uint32
_netlogon_export_name (
	OM_uint32 * /*minor_status*/,
	const gss_name_t /*input_name*/,
	gss_buffer_t exported_name );

OM_uint32
_netlogon_export_sec_context (
	 OM_uint32 * /*minor_status*/,
	gss_ctx_id_t * /*context_handle*/,
	gss_buffer_t interprocess_token );

OM_uint32
_netlogon_get_mic (
	OM_uint32 * /*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	gss_qop_t /*qop_req*/,
	const gss_buffer_t /*message_buffer*/,
	gss_buffer_t message_token );

OM_uint32
_netlogon_import_name (
	OM_uint32 * /*minor_status*/,
	const gss_buffer_t /*input_name_buffer*/,
	gss_const_OID /*input_name_type*/,
	gss_name_t * output_name );

OM_uint32
_netlogon_import_sec_context (
	 OM_uint32 * /*minor_status*/,
	const gss_buffer_t /*interprocess_token*/,
	gss_ctx_id_t * context_handle );

OM_uint32
_netlogon_indicate_mechs (
	OM_uint32 * /*minor_status*/,
	gss_OID_set * mech_set );

OM_uint32
_netlogon_init_sec_context (
	OM_uint32 * /*minor_status*/,
	const gss_cred_id_t /*initiator_cred_handle*/,
	gss_ctx_id_t * /*context_handle*/,
	const gss_name_t /*target_name*/,
	const gss_OID /*mech_type*/,
	OM_uint32 /*req_flags*/,
	OM_uint32 /*time_req*/,
	const gss_channel_bindings_t /*input_chan_bindings*/,
	const gss_buffer_t /*input_token*/,
	gss_OID * /*actual_mech_type*/,
	gss_buffer_t /*output_token*/,
	OM_uint32 * /*ret_flags*/,
	OM_uint32 * /*time_rec*/);

OM_uint32
_netlogon_inquire_context (
	 OM_uint32 * /*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	gss_name_t * /*src_name*/,
	gss_name_t * /*targ_name*/,
	OM_uint32 * /*lifetime_rec*/,
	gss_OID * /*mech_type*/,
	OM_uint32 * /*ctx_flags*/,
	int * /*locally_initiated*/,
	int * open_context );

OM_uint32
_netlogon_inquire_cred (
	OM_uint32 * /*minor_status*/,
	const gss_cred_id_t /*cred_handle*/,
	gss_name_t * /*name*/,
	OM_uint32 * /*lifetime*/,
	gss_cred_usage_t * /*cred_usage*/,
	gss_OID_set * mechanisms );

OM_uint32
_netlogon_inquire_cred_by_mech (
	 OM_uint32 * /*minor_status*/,
	const gss_cred_id_t /*cred_handle*/,
	const gss_OID /*mech_type*/,
	gss_name_t * /*name*/,
	OM_uint32 * /*initiator_lifetime*/,
	OM_uint32 * /*acceptor_lifetime*/,
	gss_cred_usage_t * cred_usage );

OM_uint32
_netlogon_inquire_mechs_for_name (
	 OM_uint32 * /*minor_status*/,
	const gss_name_t /*input_name*/,
	gss_OID_set * mech_types );

OM_uint32
_netlogon_inquire_names_for_mech (
	 OM_uint32 * /*minor_status*/,
	gss_const_OID /*mechanism*/,
	gss_OID_set * name_types );

void
_netlogon_iter_creds_f (
	OM_uint32 /*flags*/,
	void *userctx ,
	void (*/*cred_iter*/)(void *, gss_OID, gss_cred_id_t));

OM_uint32
_netlogon_process_context_token (
	 OM_uint32 */*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	const gss_buffer_t token_buffer );

OM_uint32
_netlogon_release_cred (
	OM_uint32 * /*minor_status*/,
	gss_cred_id_t * cred_handle );

OM_uint32
_netlogon_release_name (
	OM_uint32 * /*minor_status*/,
	gss_name_t * input_name );

OM_uint32
_netlogon_set_cred_option (
	OM_uint32 */*minor_status*/,
	gss_cred_id_t */*cred_handle*/,
	const gss_OID /*desired_object*/,
	const gss_buffer_t /*value*/);

OM_uint32
_netlogon_unwrap_iov (
	OM_uint32 */*minor_status*/,
	gss_ctx_id_t /*context_handle*/,
	int */*conf_state*/,
	gss_qop_t */*qop_state*/,
	gss_iov_buffer_desc */*iov*/,
	int /*iov_count*/);

OM_uint32
_netlogon_verify_mic (
	OM_uint32 * /*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	const gss_buffer_t /*message_buffer*/,
	const gss_buffer_t /*token_buffer*/,
	gss_qop_t * qop_state );

OM_uint32
_netlogon_wrap_iov (
	OM_uint32 * /*minor_status*/,
	gss_ctx_id_t /*context_handle*/,
	int /*conf_req_flag*/,
	gss_qop_t /*qop_req*/,
	int * /*conf_state*/,
	gss_iov_buffer_desc */*iov*/,
	int /*iov_count*/);

OM_uint32
_netlogon_wrap_iov_length (
	OM_uint32 * /*minor_status*/,
	gss_ctx_id_t /*context_handle*/,
	int /*conf_req_flag*/,
	gss_qop_t /*qop_req*/,
	int */*conf_state*/,
	gss_iov_buffer_desc */*iov*/,
	int /*iov_count*/);

OM_uint32
_netlogon_wrap_size_limit (
	 OM_uint32 * /*minor_status*/,
	const gss_ctx_id_t /*context_handle*/,
	int /*conf_req_flag*/,
	gss_qop_t /*qop_req*/,
	OM_uint32 /*req_output_size*/,
	OM_uint32 *max_input_size );

#endif /* __netlogon_private_h__ */
