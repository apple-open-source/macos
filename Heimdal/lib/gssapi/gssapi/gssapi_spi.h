
#ifndef GSSAPI_GSSAPI_SPI_H_
#define GSSAPI_GSSAPI_SPI_H_

#include <gssapi.h>
#include <gssapi_rewrite.h>

/* binary compat glue, these are missing _oid_desc */
extern gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_ntlm_v1;
extern gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_ntlm_v2;
extern gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_ntlm_session_key;
extern gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_ntlm_force_v1;
extern gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_ntlm_support_channelbindings;
extern gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_ntlm_support_lm2;
extern gss_OID_desc GSSAPI_LIB_VARIABLE __gss_appl_lkdc_supported_desc;
extern gss_OID_desc GSSAPI_LIB_VARIABLE __gss_c_nt_uuid_desc;


extern int __gss_ntlm_is_digest_service;

struct gssapi_mech_interface_desc;
struct _gss_mechanism_name;
struct _gss_mechanism_cred;
struct _gss_name;
struct _gss_name_type;
struct gss_mo_desc;

#if defined(__APPLE__) && (defined(__ppc__) || defined(__ppc64__) || defined(__i386__) || defined(__x86_64__))
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

/*
 * Kerberos SPI
 */

struct krb5_keytab_data;
struct krb5_ccache_data;
struct Principal;

struct gsskrb5_send_to_kdc {
    void *func;
    void *ptr;
};

struct EncryptionKey;

struct gsskrb5_krb5_plugin {
    int type;
    char *name;
    void *symbol;
};

GSSAPI_CPP_START

#ifdef __BLOCKS__
typedef void (^gss_acquire_cred_complete)(gss_status_id_t, gss_cred_id_t, gss_OID_set, OM_uint32);
#endif


#include <gssapi_private.h>

GSSAPI_CPP_END

#if defined(__APPLE__) && (defined(__ppc__) || defined(__ppc64__) || defined(__i386__) || defined(__x86_64__))
#pragma pack(pop)
#endif

#endif /* GSSAPI_GSSAPI_H_ */
