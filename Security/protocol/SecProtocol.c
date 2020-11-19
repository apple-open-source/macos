//
//  SecProtocol.c
//  Security
//

#include <Security/SecProtocolOptions.h>
#include <Security/SecProtocolMetadata.h>
#include <Security/SecProtocolPriv.h>
#include <Security/SecProtocolTypesPriv.h>
#include "SecProtocolInternal.h"

#include <Security/SecureTransportPriv.h>

#include <Security/SecFramework.h>

#include <xpc/xpc.h>
#include <os/log.h>
#include <dlfcn.h>
#include <sys/param.h>

#define MAX_SEC_PROTOCOL_OPTIONS_KEY_LEN 128

// Options keys
#define SEC_PROTOCOL_OPTIONS_KEY_min_version "min_version"
#define SEC_PROTOCOL_OPTIONS_KEY_max_version "max_version"
#define SEC_PROTOCOL_OPTIONS_KEY_minimum_rsa_key_size "minimum_rsa_key_size"
#define SEC_PROTOCOL_OPTIONS_KEY_minimum_ecdsa_key_size "minimum_ecdsa_key_size"
#define SEC_PROTOCOL_OPTIONS_KEY_minimum_signature_algorithm "minimum_signature_algorithm"
#define SEC_PROTOCOL_OPTIONS_KEY_ats_required "ats_required"
#define SEC_PROTOCOL_OPTIONS_KEY_ats_minimum_tls_version_allowed "ats_minimum_tls_version_allowed"
#define SEC_PROTOCOL_OPTIONS_KEY_ats_non_pfs_ciphersuite_allowed "ats_non_pfs_ciphersuite_allowed"
#define SEC_PROTOCOL_OPTIONS_KEY_trusted_peer_certificate "trusted_peer_certificate"
#define SEC_PROTOCOL_OPTIONS_KEY_disable_sni "disable_sni"
#define SEC_PROTOCOL_OPTIONS_KEY_enable_fallback_attempt "enable_fallback_attempt"
#define SEC_PROTOCOL_OPTIONS_KEY_enable_false_start "enable_false_start"
#define SEC_PROTOCOL_OPTIONS_KEY_enable_tickets "enable_tickets"
#define SEC_PROTOCOL_OPTIONS_KEY_enable_sct "enable_sct"
#define SEC_PROTOCOL_OPTIONS_KEY_enable_ocsp "enable_ocsp"
#define SEC_PROTOCOL_OPTIONS_KEY_enforce_ev "enforce_ev"
#define SEC_PROTOCOL_OPTIONS_KEY_enable_resumption "enable_resumption"
#define SEC_PROTOCOL_OPTIONS_KEY_enable_renegotiation "enable_renegotiation"
#define SEC_PROTOCOL_OPTIONS_KEY_enable_early_data "enable_early_data"
#define SEC_PROTOCOL_OPTIONS_KEY_peer_authentication_required "peer_authentication_required"
#define SEC_PROTOCOL_OPTIONS_KEY_peer_authentication_optional "peer_authentication_optional"
#define SEC_PROTOCOL_OPTIONS_KEY_certificate_compression_enabled "certificate_compression_enabled"
#define SEC_PROTOCOL_OPTIONS_KEY_eddsa_enabled "eddsa_enabled"
#define SEC_PROTOCOL_OPTIONS_KEY_tls_delegated_credentials_enabled "tls_delegated_credentials_enabled"
#define SEC_PROTOCOL_OPTIONS_KEY_tls_grease_enabled "tls_grease_enabled"
#define SEC_PROTOCOL_OPTIONS_KEY_tls_ticket_request_count "tls_ticket_request_count"
#define SEC_PROTOCOL_OPTIONS_KEY_ciphersuites "ciphersuites"

// Metadata keys
#define SEC_PROTOCOL_METADATA_KEY_PROCESS_IDENTIFIER "process"
#define SEC_PROTOCOL_METADATA_KEY_CIPHERSUITE "cipher_name"
#define SEC_PROTOCOL_METADATA_KEY_FALLBACK_ENABLED "fallback"
#define SEC_PROTOCOL_METADATA_KEY_DH_GROUP_SIZE "dhe_size"
#define SEC_PROTOCOL_METADATA_KEY_NEGOTIATED_CURVE "neg_curve"
#define SEC_PROTOCOL_METADATA_KEY_PEER_CERTIFICATE_REQUEST_TYPE "cert_request_type"
#define SEC_PROTOCOL_METADATA_KEY_LOCAL_PRIVATE_KEY_TYPE "private_key_type"
#define SEC_PROTOCOL_METADATA_KEY_PEER_PUBLIC_KEY_TYPE "peer_public_key_type"
#define SEC_PROTOCOL_METADATA_KEY_NEGOTIATED_PROTOCOL "negotiated_protocol"
#define SEC_PROTOCOL_METADATA_KEY_ALPN_USED "alpn_used"
#define SEC_PROTOCOL_METADATA_KEY_NPN_USED "npn_used"
#define SEC_PROTOCOL_METADATA_KEY_PROTOCOL_VERSION "version"
#define SEC_PROTOCOL_METADATA_KEY_FALSE_START_ENABLED "false_start_enabled"
#define SEC_PROTOCOL_METADATA_KEY_FALSE_START_USED "false_start_used"
#define SEC_PROTOCOL_METADATA_KEY_TICKET_OFFERED "ticket_offered"
#define SEC_PROTOCOL_METADATA_KEY_TICKET_RECEIVED "ticket_received"
#define SEC_PROTOCOL_METADATA_KEY_SESSION_RESUMED "session_resumed"
#define SEC_PROTOCOL_METADATA_KEY_SESSION_RENEWED "session_renewed"
#define SEC_PROTOCOL_METADATA_KEY_RESUMPTION_ATTEMPTED "resumption_attempted"
#define SEC_PROTOCOL_METADATA_KEY_TICKET_LIFETIME "ticket_lifetime"
#define SEC_PROTOCOL_METADATA_KEY_MAX_EARLY_DATA_SUPPORTED "max_early_data_supported"
#define SEC_PROTOCOL_METADATA_KEY_OCSP_ENABLED "ocsp_enabled"
#define SEC_PROTOCOL_METADATA_KEY_OCSP_RECEIVED "ocsp_received"
#define SEC_PROTOCOL_METADATA_KEY_SCT_ENABLED "sct_enabled"
#define SEC_PROTOCOL_METADATA_KEY_SCT_RECEIVED "sct_received"
#define SEC_PROTOCOL_METADATA_KEY_RSA_SIGNATURE_REQUESTED "client_rsa_requested"
#define SEC_PROTOCOL_METADATA_KEY_ECDSA_SIGNATURE_REQUESTED "client_ecdsa_requested"
#define SEC_PROTOCOL_METADATA_KEY_FAILURE_ALERT_TYPE "alert_type"
#define SEC_PROTOCOL_METADATA_KEY_FAILURE_ALERT_CODE "alert_code"
#define SEC_PROTOCOL_METADATA_KEY_FAILURE_HANDSHAKE_STATE "handshake_state"
#define SEC_PROTOCOL_METADATA_KEY_FAILURE_STACK_ERROR "stack_error"
#define SEC_PROTOCOL_METADATA_KEY_DEFAULT_EMPTY_STRING "none"

#define CFReleaseSafe(value) \
    if (value != NULL) { \
        CFRelease(value); \
    }

bool
sec_protocol_options_access_handle(sec_protocol_options_t options,
                                   sec_access_block_t access_block)
{
    static void *libnetworkImage = NULL;
    static dispatch_once_t onceToken;
    static bool (*_nw_protocol_options_access_handle)(void *, sec_access_block_t) = NULL;

    dispatch_once(&onceToken, ^{
        libnetworkImage = dlopen("/usr/lib/libnetwork.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (NULL != libnetworkImage) {
            _nw_protocol_options_access_handle = (__typeof(_nw_protocol_options_access_handle))dlsym(libnetworkImage,
                                                                                                     "nw_protocol_options_access_handle");
            if (NULL == _nw_protocol_options_access_handle) {
                os_log_error(OS_LOG_DEFAULT, "dlsym libnetwork nw_protocol_options_access_handle");
            }
        } else {
            os_log_error(OS_LOG_DEFAULT, "dlopen libnetwork");
        }
    });

    if (_nw_protocol_options_access_handle == NULL) {
        return false;
    }

    return _nw_protocol_options_access_handle(options, access_block);
}

bool
sec_protocol_metadata_access_handle(sec_protocol_metadata_t options,
                                    sec_access_block_t access_block)
{
    static void *libnetworkImage = NULL;
    static dispatch_once_t onceToken;
    static bool (*_nw_protocol_metadata_access_handle)(void *, sec_access_block_t) = NULL;

    dispatch_once(&onceToken, ^{
        libnetworkImage = dlopen("/usr/lib/libnetwork.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (NULL != libnetworkImage) {
            _nw_protocol_metadata_access_handle = (__typeof(_nw_protocol_metadata_access_handle))dlsym(libnetworkImage,
                                                                                                       "nw_protocol_metadata_access_handle");
            if (NULL == _nw_protocol_metadata_access_handle) {
                os_log_error(OS_LOG_DEFAULT, "dlsym libnetwork _nw_protocol_metadata_access_handle");
            }
        } else {
            os_log_error(OS_LOG_DEFAULT, "dlopen libnetwork");
        }
    });

    if (_nw_protocol_metadata_access_handle == NULL) {
        return false;
    }

    return _nw_protocol_metadata_access_handle(options, access_block);
}

#define SEC_PROTOCOL_OPTIONS_VALIDATE(o,r)                                   \
    if (o == NULL) {                                                         \
        return r;                                                            \
    }

#define SEC_PROTOCOL_METADATA_VALIDATE(m,r)                                  \
    if (((void *)m == NULL) || ((size_t)m == 0)) {                           \
        return r;                                                            \
    }

bool
sec_protocol_options_contents_are_equal(sec_protocol_options_content_t contentA, sec_protocol_options_content_t contentB)
{
    if (contentA == contentB) {
        return true;
    }
    if (contentA == NULL || contentB == NULL) {
        return false;
    }

    sec_protocol_options_content_t optionsA = (sec_protocol_options_content_t)contentA;
    sec_protocol_options_content_t optionsB = (sec_protocol_options_content_t)contentB;

    // Check boolean and primitive field types first
#define CHECK_FIELD(field) \
    if (optionsA->field != optionsB->field) { \
        return false; \
    }

    CHECK_FIELD(min_version);
    CHECK_FIELD(max_version);
    CHECK_FIELD(minimum_rsa_key_size);
    CHECK_FIELD(minimum_ecdsa_key_size);
    CHECK_FIELD(minimum_signature_algorithm);
    CHECK_FIELD(tls_ticket_request_count);
    CHECK_FIELD(ats_required);
    CHECK_FIELD(ats_minimum_tls_version_allowed);
    CHECK_FIELD(ats_non_pfs_ciphersuite_allowed);
    CHECK_FIELD(trusted_peer_certificate);
    CHECK_FIELD(disable_sni);
    CHECK_FIELD(enable_fallback_attempt);
    CHECK_FIELD(enable_false_start);
    CHECK_FIELD(enable_tickets);
    CHECK_FIELD(enable_sct);
    CHECK_FIELD(enable_ocsp);
    CHECK_FIELD(enforce_ev);
    CHECK_FIELD(enable_resumption);
    CHECK_FIELD(enable_renegotiation);
    CHECK_FIELD(enable_early_data);
    CHECK_FIELD(peer_authentication_required);
    CHECK_FIELD(peer_authentication_optional);
    CHECK_FIELD(certificate_compression_enabled);
    CHECK_FIELD(eddsa_enabled);
    CHECK_FIELD(tls_delegated_credentials_enabled);
    CHECK_FIELD(tls_grease_enabled);
    CHECK_FIELD(allow_unknown_alpn_protos);

#undef CHECK_FIELD

    // Check callback block and queue pairs next
#define CHECK_BLOCK_QUEUE(block, queue) \
    if (optionsA->block && optionsB->block) { \
        if (optionsA->block != optionsB->block) { \
            return false; \
        } \
        if (optionsA->queue != optionsB->queue) { \
            return false; \
        } \
    } else if (optionsA->block || optionsB->block) { \
        return false; \
    }

    CHECK_BLOCK_QUEUE(key_update_block, key_update_queue);
    CHECK_BLOCK_QUEUE(psk_selection_block, psk_selection_queue);
    CHECK_BLOCK_QUEUE(challenge_block, challenge_queue);
    CHECK_BLOCK_QUEUE(verify_block, verify_queue);
    CHECK_BLOCK_QUEUE(tls_secret_update_block, tls_secret_update_queue);
    CHECK_BLOCK_QUEUE(tls_encryption_level_update_block, tls_encryption_level_update_queue);

#undef CHECK_BLOCK_QUEUE

    // Deep compare dispatch data fields
#define CHECK_DISPATCH_DATA(data) \
    if (optionsA->data && optionsB->data) { \
        if (false == sec_protocol_helper_dispatch_data_equal(optionsA->data, optionsB->data)) { \
            return false; \
        } \
    } else if (optionsA->data || optionsB->data) { \
        return false; \
    }

    CHECK_DISPATCH_DATA(dh_params);
    CHECK_DISPATCH_DATA(quic_transport_parameters);
    CHECK_DISPATCH_DATA(psk_identity_hint);

#undef CHECK_DISPATCH_DATA

    // Deep compare XPC objects
#define CHECK_XPC_OBJECT(xpc) \
    if (optionsA->xpc && optionsB->xpc) { \
        if (false == xpc_equal(optionsA->xpc, optionsB->xpc)) { \
            return false; \
        } \
    } else if (optionsA->xpc || optionsB->xpc) { \
        return false; \
    }

    CHECK_XPC_OBJECT(application_protocols);
    CHECK_XPC_OBJECT(ciphersuites);
    CHECK_XPC_OBJECT(key_exchange_groups);
    CHECK_XPC_OBJECT(pre_shared_keys);

#undef CHECK_XPC_OBJECT

    // Deep compare all other fields
    if (optionsA->server_name && optionsB->server_name) {
        if (0 != strcmp(optionsA->server_name, optionsB->server_name)) {
            return false;
        }
    } else if (optionsA->server_name || optionsB->server_name) {
        return false;
    }

    if (optionsA->identity && optionsB->identity) {
        SecIdentityRef identityA = sec_identity_copy_ref((sec_identity_t)optionsA->identity);
        SecIdentityRef identityB = sec_identity_copy_ref((sec_identity_t)optionsB->identity);

        if (false == CFEqual(identityA, identityB)) {
            return false;
        }

        CFRelease(identityA);
        CFRelease(identityB);
    } else if (optionsA->identity || optionsB->identity) {
        return false;
    }

    if (optionsA->output_handler_access_block && optionsB->output_handler_access_block) {
        if (optionsA->output_handler_access_block != optionsB->output_handler_access_block) {
            return false;
        }
    } else if (optionsA->output_handler_access_block || optionsB->output_handler_access_block) {
        return false;
    }

    return true;
}

bool
sec_protocol_options_are_equal(sec_protocol_options_t handleA, sec_protocol_options_t handleB)
{
    if (handleA == handleB) {
        return true;
    }
    if (handleA == NULL || handleB == NULL) {
        return false;
    }

    return sec_protocol_options_access_handle(handleA, ^bool(void *innerA) {
        sec_protocol_options_content_t optionsA = (sec_protocol_options_content_t)innerA;
        return sec_protocol_options_access_handle(handleB, ^bool(void *innerB) {
            sec_protocol_options_content_t optionsB = (sec_protocol_options_content_t)innerB;
            return sec_protocol_options_contents_are_equal(optionsA, optionsB);
        });
    });
}

void
sec_protocol_options_set_local_identity(sec_protocol_options_t options, sec_identity_t identity)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->identity != NULL) {
            sec_release(content->identity);
        }
        content->identity = sec_retain(identity);
        return true;
    });
}

void
sec_protocol_options_append_tls_ciphersuite(sec_protocol_options_t options, tls_ciphersuite_t ciphersuite)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->ciphersuites == NULL) {
            content->ciphersuites = xpc_array_create(NULL, 0);
        }
        xpc_array_set_uint64(content->ciphersuites, XPC_ARRAY_APPEND, (uint64_t)ciphersuite);
        return true;
    });
}

void
sec_protocol_options_add_tls_ciphersuite(sec_protocol_options_t options, SSLCipherSuite ciphersuite)
{
    sec_protocol_options_append_tls_ciphersuite(options, (tls_ciphersuite_t)ciphersuite);
}

void
sec_protocol_options_append_tls_ciphersuite_group(sec_protocol_options_t options, tls_ciphersuite_group_t group)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        // Fetch the list of ciphersuites associated with the ciphersuite group
        size_t ciphersuite_count = 0;
        const tls_ciphersuite_t *list = sec_protocol_helper_ciphersuite_group_to_ciphersuite_list(group, &ciphersuite_count);
        if (list != NULL) {
            if (content->ciphersuites == NULL) {
                content->ciphersuites = xpc_array_create(NULL, 0);
            }

            for (size_t i = 0; i < ciphersuite_count; i++) {
                tls_ciphersuite_t ciphersuite = list[i];
                xpc_array_set_uint64(content->ciphersuites, XPC_ARRAY_APPEND, (uint64_t)ciphersuite);
            }
        }

        return true;
    });
}

void
sec_protocol_options_add_tls_ciphersuite_group(sec_protocol_options_t options, SSLCiphersuiteGroup group)
{
    switch (group) {
        case kSSLCiphersuiteGroupDefault:
            return sec_protocol_options_append_tls_ciphersuite_group(options, tls_ciphersuite_group_default);
        case kSSLCiphersuiteGroupCompatibility:
            return sec_protocol_options_append_tls_ciphersuite_group(options, tls_ciphersuite_group_compatibility);
        case kSSLCiphersuiteGroupLegacy:
            return sec_protocol_options_append_tls_ciphersuite_group(options, tls_ciphersuite_group_legacy);
        case kSSLCiphersuiteGroupATS:
            return sec_protocol_options_append_tls_ciphersuite_group(options, tls_ciphersuite_group_ats);
        case kSSLCiphersuiteGroupATSCompatibility:
            return sec_protocol_options_append_tls_ciphersuite_group(options, tls_ciphersuite_group_ats_compatibility);
    }
}

void
sec_protocol_options_clear_tls_ciphersuites(sec_protocol_options_t options)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->ciphersuites != NULL) {
            xpc_release(content->ciphersuites);
            content->ciphersuites = NULL;
        }
        return true;
    });
}

void
sec_protocol_options_set_tls_min_version(sec_protocol_options_t options, SSLProtocol version)
{
    tls_protocol_version_t protocol_version = (tls_protocol_version_t)SSLProtocolGetVersionCodepoint(version);
    if (protocol_version != 0) {
        sec_protocol_options_set_min_tls_protocol_version(options, protocol_version);
    }
}

void
sec_protocol_options_set_min_tls_protocol_version(sec_protocol_options_t options, tls_protocol_version_t version)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        SSLProtocol converted_protocol = SSLProtocolFromVersionCodepoint(version);
        content->min_version = converted_protocol;
        return true;
    });
}

tls_protocol_version_t
sec_protocol_options_get_default_min_tls_protocol_version(void)
{
    return tls_protocol_version_TLSv10;
}

tls_protocol_version_t
sec_protocol_options_get_default_min_dtls_protocol_version(void)
{
    return tls_protocol_version_DTLSv10;
}

void
sec_protocol_options_set_tls_max_version(sec_protocol_options_t options, SSLProtocol version)
{
    tls_protocol_version_t protocol_version = (tls_protocol_version_t)SSLProtocolGetVersionCodepoint(version);
    if (protocol_version != 0) {
        sec_protocol_options_set_max_tls_protocol_version(options, protocol_version);
    }
}

void
sec_protocol_options_set_max_tls_protocol_version(sec_protocol_options_t options, tls_protocol_version_t version)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        SSLProtocol converted_protocol = SSLProtocolFromVersionCodepoint(version);
        content->max_version = converted_protocol;
        return true;
    });
}

tls_protocol_version_t
sec_protocol_options_get_default_max_tls_protocol_version(void)
{
    return tls_protocol_version_TLSv13;
}

tls_protocol_version_t
sec_protocol_options_get_default_max_dtls_protocol_version(void)
{
    return tls_protocol_version_DTLSv12;
}

void
sec_protocol_options_add_tls_application_protocol(sec_protocol_options_t options, const char *application_protocol)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(application_protocol,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->application_protocols == NULL) {
            content->application_protocols = xpc_array_create(NULL, 0);
        }
        xpc_array_set_string(content->application_protocols, XPC_ARRAY_APPEND, application_protocol);
        return true;
    });
}

void
sec_protocol_options_add_transport_specific_application_protocol(sec_protocol_options_t options, const char *application_protocol, sec_protocol_transport_t specific_transport)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(application_protocol,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->application_protocols == NULL) {
            content->application_protocols = xpc_array_create(NULL, 0);
        }
		xpc_object_t tuple = xpc_array_create(NULL, 0);
        if (tuple != NULL) {
            xpc_array_set_string(tuple, XPC_ARRAY_APPEND, application_protocol);
            xpc_array_set_uint64(tuple, XPC_ARRAY_APPEND, (uint64_t)specific_transport);

            xpc_array_append_value(content->application_protocols, tuple);
            xpc_release(tuple);
        }
        return true;
    });
}

xpc_object_t
sec_protocol_options_copy_transport_specific_application_protocol(sec_protocol_options_t options, sec_protocol_transport_t specific_transport)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options, NULL);

    xpc_object_t filtered_application_protocols = xpc_array_create(NULL, 0);

    bool success = sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        xpc_object_t application_protocols = content->application_protocols;
        if (application_protocols == NULL) {
            return false;
        }

        size_t application_protocol_count = xpc_array_get_count(application_protocols);
        for (size_t i = 0; i < application_protocol_count; i++) {
            xpc_object_t application_protocol = xpc_array_get_value(application_protocols, i);

            if (xpc_get_type(application_protocol) == XPC_TYPE_STRING) {
                xpc_array_set_string(filtered_application_protocols, XPC_ARRAY_APPEND, xpc_string_get_string_ptr(application_protocol));
                continue;
            }

            if (xpc_get_type(application_protocol) == XPC_TYPE_ARRAY) {
                uint64_t application_protocol_transport = xpc_array_get_uint64(application_protocol, 1);
                if (application_protocol_transport != (uint64_t)specific_transport && specific_transport != sec_protocol_transport_any) {
                    continue;
                }

                xpc_array_set_string(filtered_application_protocols, XPC_ARRAY_APPEND, xpc_array_get_string(application_protocol, 0));
                continue;
            }
        }

        return xpc_array_get_count(filtered_application_protocols) != 0;
    });

    if (!success) {
        xpc_release(filtered_application_protocols);
        filtered_application_protocols = NULL;
    }

    return filtered_application_protocols;
}

void
sec_protocol_options_set_tls_server_name(sec_protocol_options_t options, const char *server_name)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(server_name,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        free(content->server_name);
        content->server_name = strdup(server_name);
        return true;
    });
}

void
sec_protocol_options_set_tls_diffie_hellman_parameters(sec_protocol_options_t options, dispatch_data_t params)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(params,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->dh_params) {
            dispatch_release(content->dh_params);
        }
        content->dh_params = params;
        dispatch_retain(params);
        return true;
    });
}

void
sec_protocol_options_add_pre_shared_key(sec_protocol_options_t options, dispatch_data_t psk, dispatch_data_t psk_identity)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(psk,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(psk_identity,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->pre_shared_keys == NULL) {
            content->pre_shared_keys = xpc_array_create(NULL, 0);
        }

        xpc_object_t psk_data = xpc_data_create_with_dispatch_data(psk);
        xpc_object_t psk_identity_data = xpc_data_create_with_dispatch_data(psk_identity);

        xpc_object_t tuple = xpc_array_create(NULL, 0);
        xpc_array_set_value(tuple, XPC_ARRAY_APPEND, psk_data);
        xpc_array_set_value(tuple, XPC_ARRAY_APPEND, psk_identity_data);
        xpc_release(psk_data);
        xpc_release(psk_identity_data);

        xpc_array_set_value(content->pre_shared_keys, XPC_ARRAY_APPEND, tuple);
        xpc_release(tuple);
        return true;
    });
}

void
sec_protocol_options_set_tls_pre_shared_key_identity_hint(sec_protocol_options_t options, dispatch_data_t psk_identity_hint)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(psk_identity_hint,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->psk_identity_hint != NULL) {
            dispatch_release(content->psk_identity_hint);
        }

        content->psk_identity_hint = psk_identity_hint;
        dispatch_retain(psk_identity_hint);
        return true;
    });
}

void
sec_protocol_options_set_pre_shared_key_selection_block(sec_protocol_options_t options, sec_protocol_pre_shared_key_selection_t psk_selection_block, dispatch_queue_t psk_selection_queue)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(psk_selection_block,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(psk_selection_queue,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->psk_selection_block != NULL) {
            Block_release(content->psk_selection_block);
        }
        if (content->psk_selection_queue != NULL) {
            dispatch_release(content->psk_selection_queue);
        }

        content->psk_selection_block = Block_copy(psk_selection_block);
        content->psk_selection_queue = psk_selection_queue;
        dispatch_retain(content->psk_selection_queue);
        return true;
    });
}

void
sec_protocol_options_set_tls_is_fallback_attempt(sec_protocol_options_t options, bool fallback_attempt)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->enable_fallback_attempt = fallback_attempt;
        content->enable_fallback_attempt_override = true;
        return true;
    });
}

void
sec_protocol_options_set_tls_tickets_enabled(sec_protocol_options_t options, bool tickets_enabled)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->enable_tickets = tickets_enabled;
        content->enable_tickets_override = true;
        return true;
    });
}

void
sec_protocol_options_set_tls_resumption_enabled(sec_protocol_options_t options, bool resumption_enabled)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->enable_resumption = resumption_enabled;
        content->enable_resumption_override = true;
        return true;
    });
}

void
sec_protocol_options_set_tls_false_start_enabled(sec_protocol_options_t options, bool false_start_enabled)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->enable_false_start = false_start_enabled;
        content->enable_false_start_override = true;
        return true;
    });
}

void
sec_protocol_options_set_tls_early_data_enabled(sec_protocol_options_t options, bool early_data_enabled)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->enable_early_data = early_data_enabled;
        content->enable_early_data_override = true;
        return true;
    });
}

void
sec_protocol_options_set_tls_sni_disabled(sec_protocol_options_t options, bool sni_disabled)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->disable_sni = sni_disabled;
        content->disable_sni_override = true;
        return true;
    });
}

void
sec_protocol_options_set_enforce_ev(sec_protocol_options_t options, bool enforce_ev)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->enforce_ev = enforce_ev;
        content->enforce_ev_override = true;
        return true;
    });
}

void
sec_protocol_options_set_tls_ocsp_enabled(sec_protocol_options_t options, bool ocsp_enabled)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->enable_ocsp = ocsp_enabled;
        content->enable_ocsp_override = true;
        return true;
    });
}

void
sec_protocol_options_set_tls_sct_enabled(sec_protocol_options_t options, bool sct_enabled)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->enable_sct = sct_enabled;
        content->enable_sct_override = true;
        return true;
    });
}

void
sec_protocol_options_set_tls_renegotiation_enabled(sec_protocol_options_t options, bool renegotiation_enabled)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->enable_renegotiation = renegotiation_enabled;
        content->enable_renegotiation_override = true;
        return true;
    });
}

void
sec_protocol_options_set_peer_authentication_required(sec_protocol_options_t options, bool peer_authentication_required)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->peer_authentication_required = peer_authentication_required;
        content->peer_authentication_override = true;
        return true;
    });
}

void
sec_protocol_options_set_peer_authentication_optional(sec_protocol_options_t options, bool peer_authentication_optional) {
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->peer_authentication_optional = peer_authentication_optional;
        content->peer_authentication_override = true;
        return true;
    });
}

void
sec_protocol_options_set_key_update_block(sec_protocol_options_t options, sec_protocol_key_update_t update_block, dispatch_queue_t update_queue)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(update_queue,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->key_update_block != NULL) {
            Block_release(content->key_update_block);
        }
        if (content->key_update_queue != NULL) {
            dispatch_release(content->key_update_queue);
        }

        content->key_update_block = Block_copy(update_block);
        content->key_update_queue = Block_copy(update_queue);
        dispatch_retain(content->key_update_queue);
        return true;
    });
}

void
sec_protocol_options_set_challenge_block(sec_protocol_options_t options, sec_protocol_challenge_t challenge_block, dispatch_queue_t challenge_queue)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(challenge_queue,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->challenge_block != NULL) {
            Block_release(content->challenge_block);
        }
        if (content->challenge_queue != NULL) {
            dispatch_release(content->challenge_queue);
        }

        content->challenge_block = Block_copy(challenge_block);
        content->challenge_queue = challenge_queue;
        dispatch_retain(content->challenge_queue);
        return true;
    });
}

void
sec_protocol_options_set_verify_block(sec_protocol_options_t options, sec_protocol_verify_t verify_block, dispatch_queue_t verify_queue)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(verify_queue,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->verify_block != NULL) {
            Block_release(content->verify_block);
        }
        if (content->verify_queue != NULL) {
            dispatch_release(content->verify_queue);
        }

        content->verify_block = Block_copy(verify_block);
        content->verify_queue = verify_queue;
        dispatch_retain(content->verify_queue);
        return true;
    });
}

void
sec_protocol_options_set_session_update_block(sec_protocol_options_t options, sec_protocol_session_update_t update_block, dispatch_queue_t update_queue)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(update_block,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(update_queue,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->session_update_block != NULL) {
            Block_release(content->session_update_block);
        }
        if (content->session_update_queue != NULL) {
            dispatch_release(content->session_update_queue);
        }
        
        content->session_update_block = Block_copy(update_block);
        content->session_update_queue = update_queue;
        dispatch_retain(content->session_update_queue);
        return true;
    });
}

void
sec_protocol_options_set_tls_encryption_secret_update_block(sec_protocol_options_t options, sec_protocol_tls_encryption_secret_update_t update_block, dispatch_queue_t update_queue)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(update_block,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(update_queue,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->tls_secret_update_block != NULL) {
            Block_release(content->tls_secret_update_block);
        }
        if (content->tls_secret_update_queue != NULL) {
            dispatch_release(content->tls_secret_update_queue);
        }

        content->tls_secret_update_block = Block_copy(update_block);
        content->tls_secret_update_queue = update_queue;
        dispatch_retain(content->tls_secret_update_queue);
        return true;
    });
}

void
sec_protocol_options_set_tls_encryption_level_update_block(sec_protocol_options_t options, sec_protocol_tls_encryption_level_update_t update_block, dispatch_queue_t update_queue)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(update_block,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(update_queue,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->tls_encryption_level_update_block != NULL) {
            Block_release(content->tls_encryption_level_update_block);
        }
        if (content->tls_encryption_level_update_queue != NULL) {
            dispatch_release(content->tls_encryption_level_update_queue);
        }

        content->tls_encryption_level_update_block = Block_copy(update_block);
        content->tls_encryption_level_update_queue = update_queue;
        dispatch_retain(content->tls_encryption_level_update_queue);
        return true;
    });
}

void
sec_protocol_options_set_session_state(sec_protocol_options_t options, dispatch_data_t session_state)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(session_state,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->session_state != NULL) {
            dispatch_release(content->session_state);
        }

        content->session_state = session_state;
        dispatch_retain(session_state);
        return true;
    });
}

void
sec_protocol_options_set_quic_transport_parameters(sec_protocol_options_t options, dispatch_data_t transport_parameters)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(transport_parameters,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->quic_transport_parameters != NULL) {
            dispatch_release(content->quic_transport_parameters);
        }

        content->quic_transport_parameters = transport_parameters;
        dispatch_retain(transport_parameters);
        return true;
    });
}

void
sec_protocol_options_set_ats_required(sec_protocol_options_t options, bool required)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->ats_required = required;
        return true;
    });
}

void
sec_protocol_options_set_minimum_rsa_key_size(sec_protocol_options_t options, size_t minimum_key_size)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->minimum_rsa_key_size = minimum_key_size;
        return true;
    });
}

void
sec_protocol_options_set_minimum_ecdsa_key_size(sec_protocol_options_t options, size_t minimum_key_size)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->minimum_ecdsa_key_size = minimum_key_size;
        return true;
    });
}

void
sec_protocol_options_set_minimum_signature_algorithm(sec_protocol_options_t options, SecSignatureHashAlgorithm algorithm)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->minimum_signature_algorithm = algorithm;
        return true;
    });
}

void
sec_protocol_options_set_trusted_peer_certificate(sec_protocol_options_t options, bool trusted_peer_certificate)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->trusted_peer_certificate = trusted_peer_certificate;
        content->trusted_peer_certificate_override = true;
        return true;
    });
}

void
sec_protocol_options_set_private_key_blocks(sec_protocol_options_t options,
                                            sec_protocol_private_key_sign_t sign_block,
                                            sec_protocol_private_key_decrypt_t decrypt_block,
                                            dispatch_queue_t operation_queue)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(sign_block,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(decrypt_block,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(operation_queue,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->private_key_sign_block != NULL) {
            Block_release(content->private_key_sign_block);
        }
        if (content->private_key_decrypt_block != NULL) {
            Block_release(content->private_key_decrypt_block);
        }
        if (content->private_key_queue != NULL) {
            dispatch_release(content->private_key_queue);
        }

        content->private_key_sign_block = Block_copy(sign_block);
        content->private_key_decrypt_block = Block_copy(decrypt_block);
        content->private_key_queue = operation_queue;
        dispatch_retain(content->private_key_queue);

        return true;
    });
}

void
sec_protocol_options_set_local_certificates(sec_protocol_options_t options, sec_array_t certificates)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(certificates,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->certificates != NULL) {
            sec_release(content->certificates);
        }

        content->certificates = certificates;
        sec_retain(content->certificates);
        return true;
    });
}

void
sec_protocol_options_set_tls_certificate_compression_enabled(sec_protocol_options_t options, bool certificate_compression_enabled)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->certificate_compression_enabled = certificate_compression_enabled;
        return true;
    });
}

void
sec_protocol_options_set_output_handler_access_block(sec_protocol_options_t options,
                                                     sec_protocol_output_handler_access_block_t access_block)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(access_block,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->output_handler_access_block = Block_copy(access_block);
        return true;
    });
}

void
sec_protocol_options_tls_handshake_message_callback(sec_protocol_options_t options, sec_protocol_tls_handshake_message_handler_t handler, dispatch_queue_t queue)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(handler,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(queue,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->handshake_message_callback != NULL) {
            Block_release(content->handshake_message_callback);
        }
        if (content->handshake_message_callback_queue != NULL) {
            dispatch_release(content->handshake_message_callback_queue);
        }

        content->handshake_message_callback = Block_copy(handler);
        content->handshake_message_callback_queue = queue;
        dispatch_retain(content->handshake_message_callback_queue);

        return true;
   });
}

void
sec_protocol_options_set_eddsa_enabled(sec_protocol_options_t options, bool eddsa_enabled)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->eddsa_enabled = eddsa_enabled;
        return true;
    });
}

void
sec_protocol_options_set_tls_delegated_credentials_enabled(sec_protocol_options_t options, bool tls_delegated_credentials_enabled)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->tls_delegated_credentials_enabled = tls_delegated_credentials_enabled;
        return true;
    });
}

void
sec_protocol_options_set_tls_grease_enabled(sec_protocol_options_t options, bool tls_grease_enabled)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->tls_grease_enabled = tls_grease_enabled;
        return true;
    });
}

void
sec_protocol_options_set_allow_unknown_alpn_protos(sec_protocol_options_t options, bool allow_unknown_alpn_protos)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->allow_unknown_alpn_protos = allow_unknown_alpn_protos;
        content->allow_unknown_alpn_protos_override = true;
        return true;
    });
}

void
sec_protocol_options_set_experiment_identifier(sec_protocol_options_t options, const char *experiment_identifier)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(experiment_identifier,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->experiment_identifier != NULL) {
            free(content->experiment_identifier);
        }
        if (experiment_identifier != NULL) {
            content->experiment_identifier = strdup(experiment_identifier);
        }
        return true;
    });
}

void
sec_protocol_options_set_connection_id(sec_protocol_options_t options, uuid_t _Nonnull connection_id)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(connection_id,);

    sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        memcpy(content->connection_id, connection_id, sizeof(content->connection_id));
        return true;
    });
}

void
sec_protocol_options_set_tls_ticket_request_count(sec_protocol_options_t options, uint8_t tls_ticket_request_count)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->tls_ticket_request_count = tls_ticket_request_count;
        return true;
    });
}

void
sec_protocol_options_set_ats_non_pfs_ciphersuite_allowed(sec_protocol_options_t options, bool ats_non_pfs_ciphersuite_allowed)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->ats_non_pfs_ciphersuite_allowed = ats_non_pfs_ciphersuite_allowed;
        return true;
    });
}

void
sec_protocol_options_set_ats_minimum_tls_version_allowed(sec_protocol_options_t options, bool ats_minimum_tls_version_allowed)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->ats_minimum_tls_version_allowed = ats_minimum_tls_version_allowed;
        return true;
    });
}

void
sec_protocol_options_append_tls_key_exchange_group(sec_protocol_options_t options, tls_key_exchange_group_t group)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->key_exchange_groups == NULL) {
            content->key_exchange_groups = xpc_array_create(NULL, 0);
        }
        xpc_array_set_uint64(content->key_exchange_groups, XPC_ARRAY_APPEND, (uint64_t)group);
        return true;
    });
}

void
sec_protocol_options_add_tls_key_exchange_group(sec_protocol_options_t options, SSLKeyExchangeGroup group)
{
    return sec_protocol_options_append_tls_key_exchange_group(options, (tls_key_exchange_group_t)group);
}

void
sec_protocol_options_append_tls_key_exchange_group_set(sec_protocol_options_t options, tls_key_exchange_group_set_t set)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->key_exchange_groups == NULL) {
            content->key_exchange_groups = xpc_array_create(NULL, 0);
        }

        // Fetch the list of ciphersuites associated with the ciphersuite group
        size_t group_set_count = 0;
        const tls_key_exchange_group_t *group_set = sec_protocol_helper_tls_key_exchange_group_set_to_key_exchange_group_list(set, &group_set_count);
        if (group_set != NULL) {
            for (size_t i = 0; i < group_set_count; i++) {
                tls_key_exchange_group_t group = group_set[i];
                xpc_array_set_uint64(content->key_exchange_groups, XPC_ARRAY_APPEND, (uint64_t)group);
            }
        }

        return true;
    });
}

void
sec_protocol_options_add_tls_key_exchange_group_set(sec_protocol_options_t options, SSLKeyExchangeGroupSet set)
{
    switch (set) {
        case kSSLKeyExchangeGroupSetDefault:
            sec_protocol_options_append_tls_key_exchange_group_set(options, tls_key_exchange_group_set_default);
            break;
        case kSSLKeyExchangeGroupSetCompatibility:
            sec_protocol_options_append_tls_key_exchange_group_set(options, tls_key_exchange_group_set_compatibility);
            break;
        case kSSLKeyExchangeGroupSetLegacy:
            sec_protocol_options_append_tls_key_exchange_group_set(options, tls_key_exchange_group_set_legacy);
            break;
    }
}

const char *
sec_protocol_metadata_get_negotiated_protocol(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, NULL);

    __block const char *negotiated_protocol = NULL;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        negotiated_protocol = content->negotiated_protocol;
        return true;
    });

    return negotiated_protocol;
}

const char *
sec_protocol_metadata_get_server_name(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, NULL);

    __block const char *server_name = NULL;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        server_name = content->server_name;
        return true;
    });

    return server_name;
}

uint64_t
sec_protocol_metadata_get_handshake_time_ms(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0);

    __block uint64_t time = 0;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);
        time = metadata_content->handshake_time;
        return true;
    });

    return time;
}

uint64_t
sec_protocol_metadata_get_handshake_byte_count(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0);

    __block uint64_t count = 0;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);
        count = metadata_content->total_byte_count;
        return true;
    });

    return count;
}

uint64_t
sec_protocol_metadata_get_handshake_sent_byte_count(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0);

    __block uint64_t count = 0;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);
        count = metadata_content->sent_byte_count;
        return true;
    });

    return count;
}

uint64_t
sec_protocol_metadata_get_handshake_received_byte_count(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0);

    __block uint64_t count = 0;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);
        count = metadata_content->received_byte_count;
        return true;
    });

    return count;
}

size_t
sec_protocol_metadata_get_handshake_read_stall_count(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0);

    __block size_t count = 0;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);
        count = metadata_content->read_stall_count;
        return true;
    });

    return count;
}

size_t
sec_protocol_metadata_get_handshake_write_stall_count(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0);

    __block size_t count = 0;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);
        count = metadata_content->write_stall_count;
        return true;
    });

    return count;
}

size_t
sec_protocol_metadata_get_handshake_async_call_count(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0);

    __block size_t count = 0;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);
        count = metadata_content->async_call_count;
        return true;
    });

    return count;
}

bool
sec_protocol_metadata_access_peer_certificate_chain(sec_protocol_metadata_t metadata,
                                                    void (^handler)(sec_certificate_t certficate))
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, false);
    SEC_PROTOCOL_METADATA_VALIDATE(handler, false);

    return sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        if (content->peer_certificate_chain == NULL) {
            return false;
        }
        sec_array_t array = content->peer_certificate_chain;
        sec_array_apply(array, ^bool(__unused size_t index, sec_object_t object) {
            handler((sec_certificate_t)object);
            return true;
        });
        return true;
    });
}

dispatch_data_t
sec_protocol_metadata_copy_peer_public_key(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, NULL);

    __block dispatch_data_t peer_public_key = NULL;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        peer_public_key = ((dispatch_data_t)content->peer_public_key);
        if (peer_public_key) {
            dispatch_retain(peer_public_key);
        }
        return true;
    });

    return peer_public_key;
}

tls_protocol_version_t
sec_protocol_metadata_get_negotiated_tls_protocol_version(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0x0000);

    __block tls_protocol_version_t protocol_version = 0x0000;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        protocol_version = SSLProtocolGetVersionCodepoint(content->negotiated_protocol_version);
        return true;
    });

    return protocol_version;
}

SSLProtocol
sec_protocol_metadata_get_negotiated_protocol_version(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, kSSLProtocolUnknown);

    __block SSLProtocol protocol_version = kSSLProtocolUnknown;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        protocol_version = content->negotiated_protocol_version;
        return true;
    });

    return protocol_version;
}

tls_ciphersuite_t
sec_protocol_metadata_get_negotiated_tls_ciphersuite(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0xFFFF);

    __block tls_ciphersuite_t negotiated_ciphersuite = SSL_NO_SUCH_CIPHERSUITE;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        negotiated_ciphersuite = content->negotiated_ciphersuite;
        return true;
    });

    return negotiated_ciphersuite;
}

SSLCipherSuite
sec_protocol_metadata_get_negotiated_ciphersuite(sec_protocol_metadata_t metadata)
{
    return (SSLCipherSuite)sec_protocol_metadata_get_negotiated_tls_ciphersuite(metadata);
}

bool
sec_protocol_metadata_get_early_data_accepted(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, false);

    return sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        return content->early_data_accepted;
    });
}

bool
sec_protocol_metadata_access_supported_signature_algorithms(sec_protocol_metadata_t metadata,
                                                            void (^handler)(uint16_t signature_algorithm))
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, false);
    SEC_PROTOCOL_METADATA_VALIDATE(handler, false);

    return sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        if (content->supported_signature_algorithms == NULL) {
            return false;
        }
        xpc_object_t array = content->supported_signature_algorithms;
        xpc_array_apply(array, ^bool(__unused size_t index, xpc_object_t _Nonnull value) {
            handler((uint16_t)xpc_uint64_get_value(value));
            return true;
        });
        return true;
    });
}

bool
sec_protocol_metadata_access_ocsp_response(sec_protocol_metadata_t metadata,
                                           void (^handler)(dispatch_data_t ocsp_data))
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, false);
    SEC_PROTOCOL_METADATA_VALIDATE(handler, false);

    return sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        if (content->ocsp_response == NULL) {
            return false;
        }
        sec_array_t array = content->ocsp_response;
        sec_array_apply(array, ^bool(__unused size_t index, sec_object_t object) {
            handler((dispatch_data_t)object);
            return true;
        });
        return true;
    });
}

bool
sec_protocol_metadata_access_distinguished_names(sec_protocol_metadata_t metadata,
                                                 void (^handler)(dispatch_data_t distinguished_name))
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, false);
    SEC_PROTOCOL_METADATA_VALIDATE(handler, false);

    return sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        if (content->distinguished_names == NULL) {
            return false;
        }
        sec_array_t array = content->distinguished_names;
        sec_array_apply(array, ^bool(__unused size_t index, sec_object_t object) {
            handler((dispatch_data_t)object);
            return true;
        });
        return true;
    });
}

static dispatch_data_t
create_dispatch_data_from_xpc_data(xpc_object_t xpc_data)
{
    if (!xpc_data) {
        return nil;
    }

    size_t data_len = xpc_data_get_length(xpc_data);
    if (data_len == 0) {
        return nil;
    }

    uint8_t *data_buffer = malloc(data_len);
    if (!data_buffer) {
        return nil;
    }

    size_t copied_count = xpc_data_get_bytes(xpc_data, data_buffer, 0, data_len);
    if (copied_count != data_len) {
        free(data_buffer);
        return nil;
    }

    dispatch_data_t data = dispatch_data_create(data_buffer, data_len, NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    free(data_buffer);

    return data;
}

bool
sec_protocol_metadata_access_pre_shared_keys(sec_protocol_metadata_t metadata,
                                             void (^handler)(dispatch_data_t psk, dispatch_data_t _Nullable psk_identity))
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, false);
    SEC_PROTOCOL_METADATA_VALIDATE(handler, false);

    return sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        if (content->pre_shared_keys == NULL) {
            return false;
        }
        xpc_array_apply(content->pre_shared_keys, ^bool(size_t index, xpc_object_t _Nonnull tuple) {
            if (xpc_array_get_count(tuple) == 2) {
                xpc_object_t xpc_psk_data = xpc_array_get_value(tuple, 0);
                xpc_object_t xpc_psk_identity_data = xpc_array_get_value(tuple, 1);

                dispatch_data_t psk_data = create_dispatch_data_from_xpc_data(xpc_psk_data);
                dispatch_data_t psk_identity_data = create_dispatch_data_from_xpc_data(xpc_psk_identity_data);
                if (!psk_data || !psk_identity_data) {
                    // Skip and return early if we can't create a PSK or identity from the provided data. Something's wrong.
                    return false;
                }

                handler(psk_data, psk_identity_data);
            }
            return true;
        });
        return true;
    });
}

static bool
sec_protocol_dispatch_data_are_equal(dispatch_data_t left, dispatch_data_t right)
{
    if (!left || !right || left == right) {
        return left == right;
    }
    if (dispatch_data_get_size(left) != dispatch_data_get_size(right)) {
        return false;
    }

    __block bool equal = true;
    dispatch_data_apply(left, ^bool(__unused dispatch_data_t  _Nonnull lregion, size_t loffset, const void * _Nonnull lbuffer, size_t lsize) {
        dispatch_data_apply(right, ^bool(__unused dispatch_data_t  _Nonnull rregion, size_t roffset, const void * _Nonnull rbuffer, size_t rsize) {
            // There is some overlap
            const size_t start = MAX(loffset, roffset);
            const size_t end = MIN(loffset + lsize, roffset + rsize);
            if (start < end) {
                equal = memcmp(&((const uint8_t*)rbuffer)[start - roffset], &((const uint8_t*)lbuffer)[start - loffset], end - start) == 0;
            } else {
                if (roffset > loffset + lsize) {
                    // Iteration of right has gone past where we're at on left, bail out of inner apply
                    // left |---|
                    // right      |---|
                    return false;
                } else if (roffset + rsize < loffset) {
                    // Iteration of right has not yet reached where we're at on left, keep going
                    // left        |---|
                    // right  |--|
                    return true;
                }
            }
            return equal;
        });
        return equal;
    });
    return equal;
}

static bool
sec_protocol_sec_array_of_dispatch_data_are_equal(sec_array_t arrayA, sec_array_t arrayB)
{
    if (sec_array_get_count(arrayA) != sec_array_get_count(arrayB)) {
        return false;
    }

    __block bool equal = true;
    (void)sec_array_apply(arrayA, ^bool(size_t indexA, sec_object_t objectA) {
        return sec_array_apply(arrayB, ^bool(size_t indexB, sec_object_t objectB) {
            if (indexA == indexB) {
                dispatch_data_t dataA = (dispatch_data_t)objectA;
                dispatch_data_t dataB = (dispatch_data_t)objectB;
                equal &= sec_protocol_dispatch_data_are_equal(dataA, dataB);
                return equal;
            }
            return true;
        });
    });

    return equal;
}

static bool
sec_protocol_sec_array_of_sec_certificate_are_equal(sec_array_t arrayA, sec_array_t arrayB)
{
    if (sec_array_get_count(arrayA) != sec_array_get_count(arrayB)) {
        return false;
    }

    __block bool equal = true;
    (void)sec_array_apply(arrayA, ^bool(size_t indexA, sec_object_t objectA) {
        return sec_array_apply(arrayB, ^bool(size_t indexB, sec_object_t objectB) {
            if (indexA == indexB) {
                sec_certificate_t certA = (sec_certificate_t)objectA;
                sec_certificate_t certB = (sec_certificate_t)objectB;

                SecCertificateRef certRefA = sec_certificate_copy_ref(certA);
                SecCertificateRef certRefB = sec_certificate_copy_ref(certB);

                if (certRefA == NULL && certRefB != NULL) {
                    equal = false;
                } else if (certRefA != NULL && certRefB == NULL) {
                    equal = false;
                } else if (certRefA == NULL && certRefB == NULL) {
                    // pass
                } else {
                    equal &= CFEqual(certRefA, certRefB);
                }

                CFReleaseSafe(certRefA);
                CFReleaseSafe(certRefB);

                return equal;
            }
            return true;
        });
    });

    return equal;
}

static bool
sec_protocol_xpc_object_are_equal(xpc_object_t objectA, xpc_object_t objectB)
{
    if (objectA == NULL && objectB != NULL) {
        return false;
    } else if (objectA != NULL && objectB == NULL) {
        return false;
    } else if (objectA == NULL && objectB == NULL) {
        return true;
    } else {
        return xpc_equal(objectA, objectB);
    }
}

bool
sec_protocol_metadata_peers_are_equal(sec_protocol_metadata_t metadataA, sec_protocol_metadata_t metadataB)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadataA, false);
    SEC_PROTOCOL_METADATA_VALIDATE(metadataB, false);

    return sec_protocol_metadata_access_handle(metadataA, ^bool(void *handleA) {
        sec_protocol_metadata_content_t contentA = (sec_protocol_metadata_content_t)handleA;
        SEC_PROTOCOL_METADATA_VALIDATE(contentA, false);

        return sec_protocol_metadata_access_handle(metadataB, ^bool(void *handleB) {
            sec_protocol_metadata_content_t contentB = (sec_protocol_metadata_content_t)handleB;
            SEC_PROTOCOL_METADATA_VALIDATE(contentB, false);

            // Relevant peer information includes: Certificate chain, public key, support signature algorithms, OCSP response, and distinguished names
            if (!sec_protocol_sec_array_of_sec_certificate_are_equal(contentA->peer_certificate_chain, contentB->peer_certificate_chain)) {
                return false;
            }
            if (!sec_protocol_dispatch_data_are_equal((dispatch_data_t)contentA->peer_public_key, (dispatch_data_t)contentB->peer_public_key)) {
                return false;
            }
            if (!sec_protocol_xpc_object_are_equal((xpc_object_t)contentA->supported_signature_algorithms, (xpc_object_t)contentB->supported_signature_algorithms)) {
                return false;
            }
            if (!sec_protocol_sec_array_of_dispatch_data_are_equal(contentA->ocsp_response, contentB->ocsp_response)) {
                return false;
            }
            if (!sec_protocol_sec_array_of_dispatch_data_are_equal(contentA->distinguished_names, contentB->distinguished_names)) {
                return false;
            }

            return true;
        });
    });
}

bool
sec_protocol_metadata_challenge_parameters_are_equal(sec_protocol_metadata_t metadataA, sec_protocol_metadata_t metadataB)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadataA, false);
    SEC_PROTOCOL_METADATA_VALIDATE(metadataB, false);

    return sec_protocol_metadata_access_handle(metadataA, ^bool(void *handleA) {
        sec_protocol_metadata_content_t contentA = (sec_protocol_metadata_content_t)handleA;
        SEC_PROTOCOL_METADATA_VALIDATE(contentA, false);

        return sec_protocol_metadata_access_handle(metadataB, ^bool(void *handleB) {
            sec_protocol_metadata_content_t contentB = (sec_protocol_metadata_content_t)handleB;
            SEC_PROTOCOL_METADATA_VALIDATE(contentB, false);

            if (!sec_protocol_xpc_object_are_equal((xpc_object_t)contentA->supported_signature_algorithms, (xpc_object_t)contentB->supported_signature_algorithms)) {
                return false;
            }
            if (!sec_protocol_sec_array_of_dispatch_data_are_equal(contentA->distinguished_names, contentB->distinguished_names)) {
                return false;
            }
            if (!sec_protocol_dispatch_data_are_equal(contentA->request_certificate_types, contentB->request_certificate_types)) {
                return false;
            }

            return true;
        });
    });
}

dispatch_data_t
sec_protocol_metadata_create_secret(sec_protocol_metadata_t metadata, size_t label_len,
                                    const char *label, size_t exporter_length)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, NULL);
    SEC_PROTOCOL_METADATA_VALIDATE(label_len, NULL);
    SEC_PROTOCOL_METADATA_VALIDATE(label, NULL);
    SEC_PROTOCOL_METADATA_VALIDATE(exporter_length, NULL);

    __block dispatch_data_t secret = NULL;
    sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        if (content->exporter_function && content->exporter_context) {
            sec_protocol_metadata_exporter exporter = (sec_protocol_metadata_exporter)content->exporter_function;
            secret = exporter(content->exporter_context, label_len, label, 0, NULL, exporter_length);
        }
        return true;
    });
    return secret;
}

dispatch_data_t
sec_protocol_metadata_create_secret_with_context(sec_protocol_metadata_t metadata, size_t label_len,
                                                 const char *label, size_t context_len,
                                                 const uint8_t *context, size_t exporter_length)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, NULL);
    SEC_PROTOCOL_METADATA_VALIDATE(label_len, NULL);
    SEC_PROTOCOL_METADATA_VALIDATE(label, NULL);
    SEC_PROTOCOL_METADATA_VALIDATE(context_len, NULL);
    SEC_PROTOCOL_METADATA_VALIDATE(context, NULL);
    SEC_PROTOCOL_METADATA_VALIDATE(exporter_length, NULL);

    __block dispatch_data_t secret = NULL;
    sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        if (content->exporter_function && content->exporter_context) {
            sec_protocol_metadata_exporter exporter = (sec_protocol_metadata_exporter)content->exporter_function;
            secret = exporter(content->exporter_context, label_len, label, context_len, context, exporter_length);
        }
        return true;
    });
    return secret;
}

bool
sec_protocol_metadata_get_tls_false_start_used(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, false);

    return sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        return content->false_start_used;
    });
}

bool
sec_protocol_metadata_get_ticket_offered(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, false);

    return sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        return content->ticket_offered;
    });
}

bool
sec_protocol_metadata_get_ticket_received(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, false);

    return sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        return content->ticket_received;
    });
}

bool
sec_protocol_metadata_get_session_resumed(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, false);

    return sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        return content->session_resumed;
    });
}

bool
sec_protocol_metadata_get_session_renewed(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, false);

    return sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        return content->session_renewed;
    });
}

SSLConnectionStrength
sec_protocol_metadata_get_connection_strength(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, SSLConnectionStrengthNonsecure);

    __block SSLConnectionStrength strength = SSLConnectionStrengthNonsecure;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        // TLSv1.2 and higher are considered strong. Anything less than TLSv1.2 is considered weak at best.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        SSLProtocol version = content->negotiated_protocol_version;
        if (version >= kTLSProtocol12) {
            strength = SSLConnectionStrengthStrong;
        } else if (version == kTLSProtocol11 || version == kTLSProtocol1) {
            strength = SSLConnectionStrengthWeak;
        } else {
            strength = SSLConnectionStrengthNonsecure;
        }

        // Legacy ciphersuites make the connection weak, for now. We may consider changing this to nonsecure.
        SSLCipherSuite ciphersuite = content->negotiated_ciphersuite;
        if (strength != SSLConnectionStrengthNonsecure &&
                SSLCiphersuiteGroupContainsCiphersuite(kSSLCiphersuiteGroupLegacy, ciphersuite)) {
            strength = SSLConnectionStrengthWeak;
        }
#pragma clang diagnostic pop

        return true;
    });

    return strength;
}

dispatch_data_t
sec_protocol_metadata_copy_serialized_session(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, NULL);

    __block dispatch_data_t session = NULL;
    sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        if (content->session_exporter_function && content->session_exporter_context) {
            sec_protocol_metadata_session_exporter exporter = (sec_protocol_metadata_session_exporter)content->session_exporter_function;
            session = exporter(content->session_exporter_context);
        }
        return true;
    });
    return session;
}

const char * __nullable
sec_protocol_metadata_get_experiment_identifier(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, NULL);

    __block const char *experiment_identifer = NULL;
    sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        experiment_identifer = content->experiment_identifier;
        return true;
    });
    return experiment_identifer;
}

void
sec_protocol_metadata_copy_connection_id(sec_protocol_metadata_t metadata, uuid_t _Nonnull output_uuid)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata,);
    SEC_PROTOCOL_METADATA_VALIDATE(output_uuid,);

    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        memcpy(output_uuid, content->connection_id, sizeof(content->connection_id));
        return true;
    });
}

static const char *_options_uint64_keys[] = {
    SEC_PROTOCOL_OPTIONS_KEY_min_version,
    SEC_PROTOCOL_OPTIONS_KEY_max_version,
    SEC_PROTOCOL_OPTIONS_KEY_minimum_rsa_key_size,
    SEC_PROTOCOL_OPTIONS_KEY_minimum_ecdsa_key_size,
    SEC_PROTOCOL_OPTIONS_KEY_minimum_signature_algorithm,
    SEC_PROTOCOL_OPTIONS_KEY_tls_ticket_request_count,
};
static const size_t _options_uint64_keys_len = sizeof(_options_uint64_keys) / sizeof(_options_uint64_keys[0]);

static const char *_options_bool_keys[] = {
    SEC_PROTOCOL_OPTIONS_KEY_ats_required,
    SEC_PROTOCOL_OPTIONS_KEY_ats_minimum_tls_version_allowed,
    SEC_PROTOCOL_OPTIONS_KEY_ats_non_pfs_ciphersuite_allowed,
    SEC_PROTOCOL_OPTIONS_KEY_trusted_peer_certificate,
    SEC_PROTOCOL_OPTIONS_KEY_disable_sni,
    SEC_PROTOCOL_OPTIONS_KEY_enable_fallback_attempt,
    SEC_PROTOCOL_OPTIONS_KEY_enable_false_start,
    SEC_PROTOCOL_OPTIONS_KEY_enable_tickets,
    SEC_PROTOCOL_OPTIONS_KEY_enable_sct,
    SEC_PROTOCOL_OPTIONS_KEY_enable_ocsp,
    SEC_PROTOCOL_OPTIONS_KEY_enforce_ev,
    SEC_PROTOCOL_OPTIONS_KEY_enable_resumption,
    SEC_PROTOCOL_OPTIONS_KEY_enable_renegotiation,
    SEC_PROTOCOL_OPTIONS_KEY_enable_early_data,
    SEC_PROTOCOL_OPTIONS_KEY_peer_authentication_required,
    SEC_PROTOCOL_OPTIONS_KEY_peer_authentication_optional,
    SEC_PROTOCOL_OPTIONS_KEY_certificate_compression_enabled,
    SEC_PROTOCOL_OPTIONS_KEY_eddsa_enabled,
    SEC_PROTOCOL_OPTIONS_KEY_tls_delegated_credentials_enabled,
    SEC_PROTOCOL_OPTIONS_KEY_tls_grease_enabled,

};
static const size_t _options_bool_keys_len = sizeof(_options_bool_keys) / sizeof(_options_bool_keys[0]);

static bool
_dictionary_has_key(xpc_object_t dict, const char *target_key)
{
    if (xpc_get_type(dict) != XPC_TYPE_DICTIONARY) {
        return false;
    }

    return !xpc_dictionary_apply(dict, ^bool(const char * _Nonnull key, xpc_object_t  _Nonnull value) {
        if (strncmp(key, target_key, strlen(target_key)) == 0) {
            return false;
        }
        return true;
    });
}

static bool
_options_config_matches_partial_config(xpc_object_t full, xpc_object_t partial)
{
    SEC_PROTOCOL_METADATA_VALIDATE(full, false);
    SEC_PROTOCOL_METADATA_VALIDATE(partial, false);

    return xpc_dictionary_apply(partial, ^bool(const char * _Nonnull entry_key, xpc_object_t _Nonnull value) {
        size_t entry_key_len = strnlen(entry_key, MAX_SEC_PROTOCOL_OPTIONS_KEY_LEN);

        for (size_t i = 0; i < _options_uint64_keys_len; i++) {
            const char *key = _options_uint64_keys[i];
            if (strncmp(entry_key, key, MAX(entry_key_len, strlen(key))) == 0) {
                if (_dictionary_has_key(full, key)) {
                    if (xpc_dictionary_get_uint64(full, key) != xpc_dictionary_get_uint64(partial, key)) {
                        return false;
                    }
                } else {
                    return false;
                }
            }
        }

        for (size_t i = 0; i < _options_bool_keys_len; i++) {
            const char *key = _options_bool_keys[i];
            if (strncmp(entry_key, key, MAX(entry_key_len, strlen(key))) == 0) {
                if (_dictionary_has_key(full, key)) {
                    if (xpc_dictionary_get_bool(full, key) != xpc_dictionary_get_bool(partial, key)) {
                        return false;
                    }
                } else {
                    return false;
                }
            }
        }

        return true;
    });
}

static bool
_serialize_options(xpc_object_t dictionary, sec_protocol_options_content_t options_content)
{
#define EXPAND_PARAMETER(field) \
    SEC_PROTOCOL_OPTIONS_KEY_##field , options_content->field

    xpc_dictionary_set_uint64(dictionary, EXPAND_PARAMETER(min_version));
    xpc_dictionary_set_uint64(dictionary, EXPAND_PARAMETER(max_version));
    xpc_dictionary_set_uint64(dictionary, EXPAND_PARAMETER(minimum_rsa_key_size));
    xpc_dictionary_set_uint64(dictionary, EXPAND_PARAMETER(minimum_ecdsa_key_size));
    xpc_dictionary_set_uint64(dictionary, EXPAND_PARAMETER(minimum_signature_algorithm));
    xpc_dictionary_set_uint64(dictionary, EXPAND_PARAMETER(tls_ticket_request_count));

    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(ats_required));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(ats_minimum_tls_version_allowed));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(ats_non_pfs_ciphersuite_allowed));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(trusted_peer_certificate));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(disable_sni));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(enable_fallback_attempt));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(enable_false_start));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(enable_tickets));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(enable_sct));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(enable_ocsp));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(enforce_ev));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(enable_resumption));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(enable_renegotiation));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(enable_early_data));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(peer_authentication_required));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(peer_authentication_optional));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(certificate_compression_enabled));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(eddsa_enabled));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(tls_delegated_credentials_enabled));
    xpc_dictionary_set_bool(dictionary, EXPAND_PARAMETER(tls_grease_enabled));

#undef EXPAND_PARAMETER

    return true;
}

static struct _options_bool_key_setter {
    const char *key;
    void (*setter_pointer)(sec_protocol_options_t, bool);
} _options_bool_key_setters[] = {
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_ats_required,
        .setter_pointer = sec_protocol_options_set_ats_required,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_ats_minimum_tls_version_allowed,
        .setter_pointer = sec_protocol_options_set_ats_minimum_tls_version_allowed,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_ats_non_pfs_ciphersuite_allowed,
        .setter_pointer = sec_protocol_options_set_ats_non_pfs_ciphersuite_allowed,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_trusted_peer_certificate,
        .setter_pointer = sec_protocol_options_set_trusted_peer_certificate,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_disable_sni,
        .setter_pointer = sec_protocol_options_set_tls_sni_disabled
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_enable_fallback_attempt,
        .setter_pointer = sec_protocol_options_set_tls_is_fallback_attempt,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_enable_false_start,
        .setter_pointer = sec_protocol_options_set_tls_false_start_enabled,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_enable_tickets,
        .setter_pointer = sec_protocol_options_set_tls_tickets_enabled,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_enable_sct,
        .setter_pointer = sec_protocol_options_set_tls_sct_enabled
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_enable_ocsp,
        .setter_pointer = sec_protocol_options_set_tls_ocsp_enabled,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_enforce_ev,
        .setter_pointer = sec_protocol_options_set_enforce_ev,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_enable_resumption,
        .setter_pointer = sec_protocol_options_set_tls_resumption_enabled,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_enable_renegotiation,
        .setter_pointer = sec_protocol_options_set_tls_renegotiation_enabled,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_enable_early_data,
        .setter_pointer = sec_protocol_options_set_tls_early_data_enabled,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_peer_authentication_required,
        .setter_pointer = sec_protocol_options_set_peer_authentication_required,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_peer_authentication_optional,
        .setter_pointer = sec_protocol_options_set_peer_authentication_optional,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_certificate_compression_enabled,
        .setter_pointer = sec_protocol_options_set_tls_certificate_compression_enabled,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_eddsa_enabled,
        .setter_pointer = sec_protocol_options_set_eddsa_enabled,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_tls_delegated_credentials_enabled,
        .setter_pointer = sec_protocol_options_set_tls_delegated_credentials_enabled,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_tls_grease_enabled,
        .setter_pointer = sec_protocol_options_set_tls_grease_enabled,
    },
};
static const size_t _options_bool_key_setters_len = sizeof(_options_bool_key_setters) / sizeof(_options_bool_key_setters[0]);

static struct _options_uint64_key_setter {
    const char *key;
    void (*setter_pointer)(sec_protocol_options_t, uint64_t);
} _options_uint64_key_setters[] = {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_min_version,
        .setter_pointer = (void (*)(sec_protocol_options_t, uint64_t))sec_protocol_options_set_tls_min_version
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_max_version,
        .setter_pointer = (void (*)(sec_protocol_options_t, uint64_t))sec_protocol_options_set_tls_max_version
    },
#pragma clang diagnostic pop
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_minimum_rsa_key_size,
        .setter_pointer = (void (*)(sec_protocol_options_t, uint64_t))sec_protocol_options_set_minimum_rsa_key_size,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_minimum_ecdsa_key_size,
        .setter_pointer = (void (*)(sec_protocol_options_t, uint64_t))sec_protocol_options_set_minimum_ecdsa_key_size,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_minimum_signature_algorithm,
        .setter_pointer = (void (*)(sec_protocol_options_t, uint64_t))sec_protocol_options_set_minimum_signature_algorithm,
    },
    {
        .key = SEC_PROTOCOL_OPTIONS_KEY_tls_ticket_request_count,
        .setter_pointer = (void (*)(sec_protocol_options_t, uint64_t))sec_protocol_options_set_tls_ticket_request_count,
    },
};
static const size_t _options_uint64_key_setters_len = sizeof(_options_uint64_key_setters) / sizeof(_options_uint64_key_setters[0]);

static bool
_apply_config_options(sec_protocol_options_t options, xpc_object_t config)
{
    return sec_protocol_options_access_handle(options, ^bool(void *options_handle) {
        sec_protocol_options_content_t options_content = (sec_protocol_options_content_t)options_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(options_content, false);
        return xpc_dictionary_apply(config, ^bool(const char * _Nonnull key, xpc_object_t  _Nonnull value) {

            size_t key_len = strnlen(key, MAX_SEC_PROTOCOL_OPTIONS_KEY_LEN);
            for (size_t i = 0; i < _options_bool_key_setters_len; i++) {
                const char *setter_key = _options_bool_key_setters[i].key;
                size_t setter_key_len = strnlen(setter_key, MAX_SEC_PROTOCOL_OPTIONS_KEY_LEN);
                if (strncmp(setter_key, key, MAX(key_len, setter_key_len)) == 0) {
                    _options_bool_key_setters[i].setter_pointer(options, xpc_dictionary_get_bool(config, key));
                }
            }

            for (size_t i = 0; i < _options_uint64_key_setters_len; i++) {
                const char *setter_key = _options_uint64_key_setters[i].key;
                size_t setter_key_len = strnlen(setter_key, MAX_SEC_PROTOCOL_OPTIONS_KEY_LEN);
                if (strncmp(setter_key, key, MAX(key_len, setter_key_len)) == 0) {
                    _options_uint64_key_setters[i].setter_pointer(options, xpc_dictionary_get_uint64(config, key));
                }
            }

            // Now check for ciphersuite options, as these are not expressed via serialized configs
            if (strncmp(key, SEC_PROTOCOL_OPTIONS_KEY_ciphersuites, key_len) == 0) {
                if (xpc_get_type(value) == XPC_TYPE_ARRAY) {
                    xpc_array_apply(value, ^bool(size_t index, xpc_object_t  _Nonnull ciphersuite_value) {
                        SSLCipherSuite ciphersuite = (SSLCipherSuite)xpc_array_get_uint64(value, index);
                        sec_protocol_options_append_tls_ciphersuite(options, ciphersuite);
                        return true;
                    });
                }
            }

            return true;
        });
    });
}

static bool
_serialize_metadata(xpc_object_t dictionary, sec_protocol_metadata_content_t metadata_content)
{
#define xpc_dictionary_set_string_default(d, key, value, default) \
    do { \
        if (value != NULL) { \
            xpc_dictionary_set_string(d, key, value); \
        } else { \
            xpc_dictionary_set_string(d, key, default); \
        } \
    } while (0);

    xpc_dictionary_set_uint64(dictionary, SEC_PROTOCOL_METADATA_KEY_CIPHERSUITE, metadata_content->negotiated_ciphersuite);
    xpc_dictionary_set_uint64(dictionary, SEC_PROTOCOL_METADATA_KEY_PROTOCOL_VERSION, metadata_content->negotiated_protocol_version);
    xpc_dictionary_set_uint64(dictionary, SEC_PROTOCOL_METADATA_KEY_TICKET_LIFETIME, metadata_content->ticket_lifetime);

    xpc_dictionary_set_string_default(dictionary, SEC_PROTOCOL_METADATA_KEY_PEER_PUBLIC_KEY_TYPE,
                                      metadata_content->peer_public_key_type, SEC_PROTOCOL_METADATA_KEY_DEFAULT_EMPTY_STRING);
    xpc_dictionary_set_string_default(dictionary, SEC_PROTOCOL_METADATA_KEY_NEGOTIATED_CURVE,
                                      metadata_content->negotiated_curve, SEC_PROTOCOL_METADATA_KEY_DEFAULT_EMPTY_STRING);
    xpc_dictionary_set_string_default(dictionary, SEC_PROTOCOL_METADATA_KEY_PEER_CERTIFICATE_REQUEST_TYPE,
                                      metadata_content->certificate_request_type, SEC_PROTOCOL_METADATA_KEY_DEFAULT_EMPTY_STRING);
    xpc_dictionary_set_string_default(dictionary, SEC_PROTOCOL_METADATA_KEY_NEGOTIATED_PROTOCOL,
                                      metadata_content->negotiated_protocol, SEC_PROTOCOL_METADATA_KEY_DEFAULT_EMPTY_STRING);

    xpc_dictionary_set_bool(dictionary, SEC_PROTOCOL_METADATA_KEY_FALSE_START_USED, metadata_content->false_start_used);
    xpc_dictionary_set_bool(dictionary, SEC_PROTOCOL_METADATA_KEY_SESSION_RESUMED, metadata_content->session_resumed);
    xpc_dictionary_set_bool(dictionary, SEC_PROTOCOL_METADATA_KEY_TICKET_OFFERED, metadata_content->ticket_offered);
    xpc_dictionary_set_bool(dictionary, SEC_PROTOCOL_METADATA_KEY_TICKET_RECEIVED, metadata_content->ticket_received);
    xpc_dictionary_set_bool(dictionary, SEC_PROTOCOL_METADATA_KEY_SESSION_RENEWED, metadata_content->session_renewed);
    xpc_dictionary_set_bool(dictionary, SEC_PROTOCOL_METADATA_KEY_RESUMPTION_ATTEMPTED, metadata_content->resumption_attempted);
    xpc_dictionary_set_bool(dictionary, SEC_PROTOCOL_METADATA_KEY_ALPN_USED, metadata_content->alpn_used);
    xpc_dictionary_set_bool(dictionary, SEC_PROTOCOL_METADATA_KEY_NPN_USED, metadata_content->npn_used);
    xpc_dictionary_set_bool(dictionary, SEC_PROTOCOL_METADATA_KEY_OCSP_ENABLED, metadata_content->ocsp_enabled);
    xpc_dictionary_set_bool(dictionary, SEC_PROTOCOL_METADATA_KEY_OCSP_RECEIVED, metadata_content->ocsp_response != NULL);
    xpc_dictionary_set_bool(dictionary, SEC_PROTOCOL_METADATA_KEY_SCT_ENABLED, metadata_content->sct_enabled);
    xpc_dictionary_set_bool(dictionary, SEC_PROTOCOL_METADATA_KEY_SCT_RECEIVED, metadata_content->signed_certificate_timestamps != NULL);

#undef xpc_dictionary_set_string_default

    return true;
}

static bool
_serialize_success_with_options(xpc_object_t dictionary, sec_protocol_metadata_content_t metadata_content, sec_protocol_options_content_t options_content)
{
    if (!_serialize_options(dictionary, options_content)) {
        return false;
    }
    if (!_serialize_metadata(dictionary, metadata_content)) {
        return false;
    }
    return true;
}

static bool
_serialize_failure_with_options(xpc_object_t dictionary, sec_protocol_metadata_content_t metadata_content, sec_protocol_options_content_t options_content)
{
    xpc_dictionary_set_uint64(dictionary, SEC_PROTOCOL_METADATA_KEY_FAILURE_ALERT_TYPE, metadata_content->alert_type);
    xpc_dictionary_set_uint64(dictionary, SEC_PROTOCOL_METADATA_KEY_FAILURE_ALERT_CODE, metadata_content->alert_code);
    xpc_dictionary_set_uint64(dictionary, SEC_PROTOCOL_METADATA_KEY_FAILURE_HANDSHAKE_STATE, metadata_content->handshake_state);
    xpc_dictionary_set_uint64(dictionary, SEC_PROTOCOL_METADATA_KEY_FAILURE_STACK_ERROR, metadata_content->stack_error);

    return true;
}

xpc_object_t
sec_protocol_metadata_serialize_with_options(sec_protocol_metadata_t metadata, sec_protocol_options_t options)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, NULL);
    SEC_PROTOCOL_METADATA_VALIDATE(options, NULL);

    __block xpc_object_t dictionary = xpc_dictionary_create(NULL, NULL, 0);
    if (dictionary == NULL) {
        return NULL;
    }

    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);

        return sec_protocol_options_access_handle(options, ^bool(void *options_handle) {
            sec_protocol_options_content_t options_content = (sec_protocol_options_content_t)options_handle;
            SEC_PROTOCOL_METADATA_VALIDATE(options_content, false);

            if (metadata_content->failure) {
                return _serialize_failure_with_options(dictionary, metadata_content, options_content);
            } else {
                return _serialize_success_with_options(dictionary, metadata_content, options_content);
            }
        });
    });

    return dictionary;
}

dispatch_data_t
sec_protocol_metadata_copy_quic_transport_parameters(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, NULL);

    __block dispatch_data_t copied_parameters = NULL;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);
        if (metadata_content->quic_transport_parameters) {
            copied_parameters = metadata_content->quic_transport_parameters;
            dispatch_retain(copied_parameters);
        }
        return true;
    });

    return copied_parameters;
}

bool
sec_protocol_metadata_get_tls_certificate_compression_used(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, false);

    __block bool certificate_compression_used = false;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);
        certificate_compression_used = metadata_content->certificate_compression_used;
        return true;
    });

    return certificate_compression_used;
}

uint16_t
sec_protocol_metadata_get_tls_certificate_compression_algorithm(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0);

    __block uint16_t certificate_compression_algorithm = 0;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);
        certificate_compression_algorithm = metadata_content->certificate_compression_algorithm;
        return true;
    });

    return certificate_compression_algorithm;
}

uint64_t
sec_protocol_metadata_get_handshake_rtt(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0);

    __block uint64_t rtt = 0;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);
        rtt = metadata_content->handshake_rtt;
        return true;
    });

    return rtt; 
}

sec_trust_t
sec_protocol_metadata_copy_sec_trust(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, nil);

    __block sec_trust_t trust = nil;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);
        if (metadata_content->trust_ref != nil) {
            trust = metadata_content->trust_ref;
            sec_retain(trust);
        }
        return true;
    });

    return trust;
}

sec_identity_t
sec_protocol_metadata_copy_sec_identity(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, nil);

    __block sec_identity_t identity = nil;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);
        if (metadata_content->identity != nil) {
            identity = metadata_content->identity;
            sec_retain(identity);
        }
        return true;
    });

    return identity;
}

bool
sec_protocol_metadata_access_sent_certificates(sec_protocol_metadata_t metadata,
                                               void (^handler)(sec_certificate_t certificate))
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, false);
    SEC_PROTOCOL_METADATA_VALIDATE(handler, false);

    return sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        if (content->identity != nil && sec_identity_has_certificates(content->identity)) {
            return sec_identity_access_certificates(content->identity, handler);
        }

        if (content->sent_certificate_chain != NULL) {
            return sec_array_apply(content->sent_certificate_chain, ^bool(__unused size_t index, sec_object_t object) {
                handler((sec_certificate_t)object);
                return true;
            });
        }

        return false;
    });
}

const char *
sec_protocol_metadata_get_tls_negotiated_group(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, NULL);

    __block const char *negotiated_curve = NULL;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        negotiated_curve = content->negotiated_curve;
        return true;
    });

    return negotiated_curve;
}

void *
sec_retain(void *obj)
{
    if (obj != NULL) {
        return os_retain(obj);
    } else {
        return NULL;
    }
}

void
sec_release(void *obj)
{
    if (obj != NULL) {
        os_release(obj);
    }
}

xpc_object_t
sec_protocol_options_create_config(sec_protocol_options_t options)
{
    SEC_PROTOCOL_METADATA_VALIDATE(options, NULL);

    __block xpc_object_t dictionary = xpc_dictionary_create(NULL, NULL, 0);
    if (dictionary == NULL) {
        return NULL;
    }

    bool serialized = sec_protocol_options_access_handle(options, ^bool(void *options_handle) {
        sec_protocol_options_content_t options_content = (sec_protocol_options_content_t)options_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(options_content, false);

        return _serialize_options(dictionary, options_content);
    });

    if (serialized) {
        return dictionary; // retained reference
    } else {
        xpc_release(dictionary);
        return NULL;
    }
}

bool
sec_protocol_options_matches_config(sec_protocol_options_t options, xpc_object_t config)
{
    SEC_PROTOCOL_METADATA_VALIDATE(options, false);
    SEC_PROTOCOL_METADATA_VALIDATE(config, false);

    if (xpc_get_type(config) != XPC_TYPE_DICTIONARY) {
        return false;
    }

    xpc_object_t options_config = sec_protocol_options_create_config(options);
    if (options_config == NULL) {
        return false;
    }

    bool match = _options_config_matches_partial_config(options_config, config);
    xpc_release(options_config);

    return match;
}

bool
sec_protocol_options_apply_config(sec_protocol_options_t options, xpc_object_t config)
{
    SEC_PROTOCOL_METADATA_VALIDATE(options, false);
    SEC_PROTOCOL_METADATA_VALIDATE(config, false);

    if (xpc_get_type(config) != XPC_TYPE_DICTIONARY) {
        return false;
    }

    return _apply_config_options(options, config);
}

bool
sec_protocol_options_set_tls_block_length_padding(sec_protocol_options_t options, sec_protocol_block_length_padding_t block_length_padding)
{
    SEC_PROTOCOL_METADATA_VALIDATE(options, false);

    return sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->tls_block_length_padding = block_length_padding;
        return true;
    });
}
