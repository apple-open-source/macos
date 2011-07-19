
#ifndef GSSAPI_GSSAPI_SPI_H_
#define GSSAPI_GSSAPI_SPI_H_

#include <gssapi.h>
#include <gssapi_rewrite.h>

extern gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_nt_uuid_desc;
#define GSS_C_NT_UUID	(&__gss_c_nt_uuid_desc)

extern gss_OID_desc GSSAPI_LIB_VARIABLE __gss_appl_lkdc_supported_desc;
#define GSS_APPL_LKDC_SUPPORTED	(&__gss_appl_lkdc_supported_desc)

#ifdef __APPLE__ /* Compatiblity with MIT Kerberos */
#pragma pack(push,2)
#endif

typedef struct gss_auth_identity {
    uint32_t type;
#define GSS_AUTH_IDENTITY_TYPE_1	1
    uint32_t flags;
    char *username;
    char *realm;
    char *password;
    gss_buffer_t *credentialsRef;
} gss_auth_identity_desc;


GSSAPI_CPP_START

/**
 *
 */

OM_uint32 GSSAPI_LIB_FUNCTION
gss_encapsulate_token(const gss_buffer_t /* input_token */,
		      const gss_OID /* oid */,
		      gss_buffer_t /* output_token */);

OM_uint32 GSSAPI_LIB_FUNCTION
gss_decapsulate_token(const gss_buffer_t /* input_token */,
		      const gss_OID /* oid */,
		      gss_buffer_t /* output_token */);



/*
 * Query functions
 */

typedef struct {
    size_t header; /**< size of header */
    size_t trailer; /**< size of trailer */
    size_t max_msg_size; /**< maximum message size */
    size_t buffers; /**< extra GSS_IOV_BUFFER_TYPE_EMPTY buffer to pass */
    size_t blocksize; /**< Specificed optimal size of messages, also
			 is the maximum padding size
			 (GSS_IOV_BUFFER_TYPE_PADDING) */
} gss_context_stream_sizes; 

extern gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_attr_stream_sizes_oid_desc;
#define GSS_C_ATTR_STREAM_SIZES (&__gss_c_attr_stream_sizes_oid_desc)


OM_uint32 GSSAPI_LIB_FUNCTION
gss_context_query_attributes(OM_uint32 * /* minor_status */,
			     const gss_ctx_id_t /* context_handle */,
			     const gss_OID /* attribute */,
			     void * /*data*/,
			     size_t /* len */);

/*
 * AEAD support
 */

/*
 * GSS_IOV
 */

OM_uint32 GSSAPI_LIB_FUNCTION
gss_wrap_iov(OM_uint32 *, gss_ctx_id_t, int, gss_qop_t, int *,
	     gss_iov_buffer_desc *, int);


OM_uint32 GSSAPI_LIB_FUNCTION
gss_unwrap_iov(OM_uint32 *, gss_ctx_id_t, int *, gss_qop_t *,
	       gss_iov_buffer_desc *, int);

OM_uint32 GSSAPI_LIB_FUNCTION
gss_wrap_iov_length(OM_uint32 *, gss_ctx_id_t, int, gss_qop_t, int *,
		    gss_iov_buffer_desc *, int);

OM_uint32 GSSAPI_LIB_FUNCTION
gss_release_iov_buffer(OM_uint32 *, gss_iov_buffer_desc *, int);

OM_uint32 GSSAPI_LIB_FUNCTION
gss_export_cred(OM_uint32 * /* minor_status */,
		gss_cred_id_t /* cred_handle */,
		gss_buffer_t /* cred_token */);

OM_uint32 GSSAPI_LIB_FUNCTION
gss_import_cred(OM_uint32 * /* minor_status */,
		gss_buffer_t /* cred_token */,
		gss_cred_id_t * /* cred_handle */);


/*
 *
 */

OM_uint32 GSSAPI_LIB_FUNCTION
gss_create_status(gss_status_id_t *status);

void GSSAPI_LIB_FUNCTION
gss_delete_status(gss_status_id_t *status);

OM_uint32 GSSAPI_LIB_FUNCTION
gss_status_get_message(gss_status_id_t status, gss_buffer_t message);

GSSAPI_LIB_FUNCTION OM_uint32
gss_retain_status(gss_status_id_t status);

OM_uint32 GSSAPI_LIB_FUNCTION
gss_acquire_cred_ex_f(gss_status_id_t /* status */,
		      const gss_name_t /* desired_name */,
		      OM_uint32 /* flags */,
		      OM_uint32 /* time_req */,
		      const gss_OID /*desired_mech */,
		      gss_cred_usage_t /* cred_usage */,
		      gss_auth_identity_t /* identity */,
		      void * /* ctx */,
		      void (* /* complete */)(void *, OM_uint32, gss_status_id_t, gss_cred_id_t, gss_OID_set, OM_uint32));

#ifdef __BLOCKS__
typedef void (^gss_acquire_cred_complete)(gss_status_id_t, 
					 gss_cred_id_t, gss_OID_set, OM_uint32);

OM_uint32 GSSAPI_LIB_FUNCTION
gss_acquire_cred_ex(const gss_name_t /* desired_name */,
		    OM_uint32 /* flags */,
		    OM_uint32 /* time_req */,
		    const gss_OID /*desired_mech */,
		    gss_cred_usage_t /* cred_usage */,
		    gss_auth_identity_t /* identity */,
		    gss_acquire_cred_complete /* complete */);

#endif


GSSAPI_LIB_FUNCTION OM_uint32
gss_create_sec_context(OM_uint32 /* initiator */,
		       const gss_OID /*mech_type*/,
		       OM_uint32 /*req_flags*/,
		       const gss_cred_id_t /*cred_handle*/,
		       OM_uint32 /*time_req*/,
		       gss_ctx_id_t */*context_handle*/);

GSSAPI_LIB_FUNCTION OM_uint32
gss_ctx_set_channel_bindings(gss_ctx_id_t /*context_handle*/,
			     const gss_channel_bindings_t /*input_chan_bindings*/);

GSSAPI_LIB_FUNCTION OM_uint32
gss_ctx_set_target_name(gss_ctx_id_t /*context_handle*/,
			const gss_name_t /*target_name*/);

GSSAPI_LIB_FUNCTION OM_uint32
gss_init_sec_context_f(gss_ctx_id_t /*context_handle*/,
		       const gss_buffer_t /*input_token*/,
		       void * /* userctx */,
		       void (* /* complete */)(void * /*userctx */,
					       OM_uint32 /* major */,
					       gss_status_id_t /* status */,
					       gss_buffer_t /* out */,
					       gss_uint32 /* ret_flags */)
		       );

GSSAPI_LIB_FUNCTION OM_uint32
gss_accept_sec_context_f(gss_ctx_id_t /*context_handle*/,
			 const gss_buffer_t /*input_token*/,
			 void * /* userctx */,
			 void (* /* complete */)(void * /*userctx */,
						 OM_uint32 /* major */,
						 gss_status_id_t /* status */,
						 gss_buffer_t /* out */,
						 gss_name_t /* src_name*/,
						 gss_cred_id_t * /*delegated_cred_handle*/,
						 gss_uint32 /* ret_flags */)
			 );


GSSAPI_LIB_FUNCTION OM_uint32
gss_ctx_cancel(gss_ctx_id_t /* context_handle */);

GSSAPI_LIB_FUNCTION const char *
gss_oid_to_name(gss_OID /* oid */);

GSSAPI_LIB_FUNCTION gss_OID
gss_name_to_oid(const char * /* name */);


int
gss_mo_set(gss_OID mech, gss_OID option, int enable, gss_buffer_t value);

int
gss_mo_get(gss_OID mech, gss_OID option, gss_buffer_t value);

void
gss_mo_list(gss_OID mech, gss_OID_set *options);

OM_uint32
gss_mo_name(gss_OID mech, gss_OID options, gss_buffer_t name);

OM_uint32
gss_cred_hold(OM_uint32 *min_stat, gss_cred_id_t cred_handle);

OM_uint32
gss_cred_unhold(OM_uint32 *min_stat, gss_cred_id_t cred_handle);

OM_uint32
gss_cred_label_get(OM_uint32 * min_stat, gss_cred_id_t cred_handle,
		   const char * label, gss_buffer_t value);

OM_uint32
gss_cred_label_set(OM_uint32 * min_stat, gss_cred_id_t cred_handle,
		   const char * label, gss_buffer_t value);


/*
 * Kerberos SPI
 */

struct krb5_keytab_data;
struct krb5_ccache_data;
struct Principal;

OM_uint32 GSSAPI_LIB_FUNCTION
gss_krb5_import_cred(OM_uint32 * /*minor*/,
		     struct krb5_ccache_data * /*in*/,
		     struct Principal * /*keytab_principal*/,
		     struct krb5_keytab_data * /*keytab*/,
		     gss_cred_id_t * /*out*/);

OM_uint32 GSSAPI_LIB_FUNCTION gss_krb5_get_tkt_flags
	(OM_uint32 * /*minor*/,
	 gss_ctx_id_t /*context_handle*/,
	 OM_uint32 * /*tkt_flags*/);

OM_uint32 GSSAPI_LIB_FUNCTION
gsskrb5_set_dns_canonicalize(int);

struct gsskrb5_send_to_kdc {
    void *func;
    void *ptr;
};

OM_uint32 GSSAPI_LIB_FUNCTION
gsskrb5_set_default_realm(const char *);

OM_uint32 GSSAPI_LIB_FUNCTION
gsskrb5_extract_authtime_from_sec_context(OM_uint32 *, gss_ctx_id_t, time_t *);

struct EncryptionKey;

OM_uint32 GSSAPI_LIB_FUNCTION
gsskrb5_extract_service_keyblock(OM_uint32 *minor_status,
				 gss_ctx_id_t context_handle,
				 struct EncryptionKey **out);
OM_uint32 GSSAPI_LIB_FUNCTION
gsskrb5_get_initiator_subkey(OM_uint32 *minor_status,
				 gss_ctx_id_t context_handle,
				 struct EncryptionKey **out);
OM_uint32 GSSAPI_LIB_FUNCTION
gsskrb5_get_subkey(OM_uint32 *minor_status,
		   gss_ctx_id_t context_handle,
		   struct EncryptionKey **out);

OM_uint32 GSSAPI_LIB_FUNCTION
gsskrb5_set_time_offset(int);

OM_uint32 GSSAPI_LIB_FUNCTION
gsskrb5_get_time_offset(int *);

struct gsskrb5_krb5_plugin {
    int type;
    char *name;
    void *symbol;
};

OM_uint32 GSSAPI_LIB_FUNCTION
gsskrb5_plugin_register(struct gsskrb5_krb5_plugin *);




GSSAPI_CPP_END

#ifdef __APPLE__ /* Compatiblity with MIT Kerberos */
#pragma pack(pop)
#endif

#endif /* GSSAPI_GSSAPI_H_ */
