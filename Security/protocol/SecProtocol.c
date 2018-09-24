//
//  SecProtocol.c
//  Security
//

#include <Security/SecProtocolOptions.h>
#include <Security/SecProtocolMetadata.h>
#include <Security/SecProtocolPriv.h>

#include <Security/SecureTransportPriv.h>

#include <xpc/xpc.h>
#include <os/log.h>
#include <dlfcn.h>
#include <sys/param.h>

#define CFReleaseSafe(value) \
    if (value != NULL) { \
        CFRelease(value); \
    }

typedef bool (^sec_access_block_t)(void *handle);

static bool
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

static bool
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

#define SEC_PROTOCOL_OPTIONS_VALIDATE(o,r)                                    \
if (o == NULL) {                                                        \
return r;                                                            \
}

#define SEC_PROTOCOL_METADATA_VALIDATE(m,r)                                    \
if (((void *)m == NULL) || ((size_t)m == 0)) {                            \
return r;                                                            \
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
sec_protocol_options_add_tls_ciphersuite(sec_protocol_options_t options, SSLCipherSuite ciphersuite)
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
sec_protocol_options_add_tls_ciphersuite_group(sec_protocol_options_t options, SSLCiphersuiteGroup group)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->ciphersuites == NULL) {
            content->ciphersuites = xpc_array_create(NULL, 0);
        }

        // Fetch the list of ciphersuites associated with the ciphersuite group
        size_t ciphersuite_count = 0;
        const SSLCipherSuite *list = SSLCiphersuiteGroupToCiphersuiteList(group, &ciphersuite_count);
        if (list != NULL) {
            for (size_t i = 0; i < ciphersuite_count; i++) {
                SSLCipherSuite ciphersuite = list[i];
                xpc_array_set_uint64(content->ciphersuites, XPC_ARRAY_APPEND, (uint64_t)ciphersuite);
            }
        }

        return true;
    });
}

void
sec_protocol_options_set_tls_min_version(sec_protocol_options_t options, SSLProtocol version)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->min_version = version;
        return true;
    });
}

void
sec_protocol_options_set_tls_max_version(sec_protocol_options_t options, SSLProtocol version)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->max_version = version;
        return true;
    });
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
sec_protocol_options_set_tls_server_name(sec_protocol_options_t options, const char *server_name)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(server_name,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->server_name != NULL) {
            free(content->server_name);
        }
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
sec_protocol_options_set_tls_is_fallback_attempt(sec_protocol_options_t options, bool fallback_attempt)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->enable_fallback_attempt = fallback_attempt;
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
sec_protocol_options_add_tls_extension(sec_protocol_options_t options, sec_tls_extension_t extension)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(extension,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->custom_extensions == NULL) {
            content->custom_extensions = sec_array_create();
        }

        sec_array_append(content->custom_extensions, (sec_object_t)extension);
        return true;
    });
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

SSLCipherSuite
sec_protocol_metadata_get_negotiated_ciphersuite(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, SSL_NO_SUCH_CIPHERSUITE);

    __block SSLCipherSuite negotiated_ciphersuite = SSL_NO_SUCH_CIPHERSUITE;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        negotiated_ciphersuite = content->negotiated_ciphersuite;
        return true;
    });

    return negotiated_ciphersuite;
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
