//
//  SecProtocolTest.m
//  SecureTransportTests
//

#import <AssertMacros.h>
#import <Foundation/Foundation.h>

#include <os/log.h>
#include <dlfcn.h>
#include <sys/param.h>

#import <XCTest/XCTest.h>

#import "SecProtocolConfiguration.h"
#import "SecProtocolPriv.h"
#import "SecProtocolTypesPriv.h"
#import "SecProtocolInternal.h"

#import <nw/private.h> // Needed for the mock protocol

#define SEC_PROTOCOL_OPTIONS_VALIDATE(m, r) \
    if (((void *)(m) == NULL) || ((size_t)(m) == 0)) { \
        return (r); \
    }

#define SEC_PROTOCOL_METADATA_VALIDATE(m, r) \
    if (((void *)(m) == NULL) || ((size_t)(m) == 0)) { \
        return (r); \
    }

typedef struct mock_protocol {
    struct nw_protocol protocol;
    char *name;
} *mock_protocol_t;

static nw_protocol_t
_mock_protocol_create_extended(nw_protocol_identifier_const_t identifier,
                               nw_endpoint_t endpoint,
                               nw_parameters_t parameters)
{
    mock_protocol_t handle = (mock_protocol_t)calloc(1, sizeof(struct mock_protocol));
    if (handle == NULL) {
        return NULL;
    }

    struct nw_protocol_callbacks *callbacks = (struct nw_protocol_callbacks *) malloc(sizeof(struct nw_protocol_callbacks));
    memset(callbacks, 0, sizeof(struct nw_protocol_callbacks));

    handle->protocol.callbacks = callbacks;
    handle->protocol.handle = (void *)handle;

    return &handle->protocol;
}

static bool
mock_protocol_register_extended(nw_protocol_identifier_const_t identifier,
                                nw_protocol_create_extended_f create_extended_function)
{
    static void *libnetworkImage = NULL;
    static dispatch_once_t onceToken;
    static bool (*_nw_protocol_register_extended)(nw_protocol_identifier_const_t, nw_protocol_create_extended_f) = NULL;
    
    dispatch_once(&onceToken, ^{
        libnetworkImage = dlopen("/usr/lib/libnetwork.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (NULL != libnetworkImage) {
            _nw_protocol_register_extended = (__typeof(_nw_protocol_register_extended))dlsym(libnetworkImage, "nw_protocol_register_extended");
            if (NULL == _nw_protocol_register_extended) {
                os_log_error(OS_LOG_DEFAULT, "dlsym libnetwork nw_protocol_register_extended");
            }
        } else {
            os_log_error(OS_LOG_DEFAULT, "dlopen libnetwork");
        }
    });
    
    if (_nw_protocol_register_extended == NULL) {
        return false;
    }
    
    return _nw_protocol_register_extended(identifier, create_extended_function);
}

static nw_protocol_identifier_t
_mock_protocol_identifier(const char *name, size_t name_len)
{
    static struct nw_protocol_identifier mock_identifer = {};
    static dispatch_once_t onceToken = 0;
    dispatch_once(&onceToken, ^{
        memset(&mock_identifer, 0, sizeof(mock_identifer));

        strlcpy((char *)mock_identifer.name, name, name_len);

        mock_identifer.level = nw_protocol_level_application;
        mock_identifer.mapping = nw_protocol_mapping_one_to_one;

        mock_protocol_register_extended(&mock_identifer, _mock_protocol_create_extended);
    });

    return &mock_identifer;
}

static void * _Nullable
mock_protocol_allocate_metadata(__unused nw_protocol_definition_t definition)
{
    return calloc(1, sizeof(struct sec_protocol_metadata_content));
}

#define mock_protocol_safe_free(pointer)                                                                                 \
    if ((pointer) != NULL) {                                                                                           \
        free((void *)(pointer));                                                                                       \
        (pointer) = NULL;                                                                                              \
    }

static void mock_protocol_returned_raw_string_pointer_deallocate(const void* value, __unused void *context) {
    mock_protocol_safe_free(value);
}

static void
mock_protocol_deallocate_metadata(__unused nw_protocol_definition_t definition, void *metadata)
{
    sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)metadata;
    if (content) {
        mock_protocol_safe_free(content->negotiated_protocol);
        mock_protocol_safe_free(content->negotiated_curve);
        mock_protocol_safe_free(content->server_name);
        mock_protocol_safe_free(content->experiment_identifier);
        mock_protocol_safe_free(content->eap_key_material);
        if (content->returned_raw_string_pointers != NULL) {
            CFSetApplyFunction(content->returned_raw_string_pointers, mock_protocol_returned_raw_string_pointer_deallocate, NULL);
            CFRelease(content->returned_raw_string_pointers);
        }
        content->sent_certificate_chain = nil;
        content->peer_certificate_chain = nil;
        content->pre_shared_keys = nil;
        content->peer_public_key = nil;
        content->supported_signature_algorithms = nil;
        content->request_certificate_types = nil;
        content->signed_certificate_timestamps = nil;
        content->ocsp_response = nil;
        content->distinguished_names = nil;
        content->quic_transport_parameters = nil;
        content->identity = nil;
        content->trust_ref = nil;
    }
    mock_protocol_safe_free(metadata);
}

static void
mock_protocol_set_metadata_allocator(nw_protocol_definition_t definition, nw_protocol_definition_allocate_f allocator, nw_protocol_definition_deallocate_f deallocator)
{
    static void *libnetworkImage = NULL;
    static dispatch_once_t onceToken;
    static void (*_nw_protocol_definition_set_metadata_allocator)(nw_protocol_definition_t, nw_protocol_definition_allocate_f, nw_protocol_definition_deallocate_f) = NULL;
    
    dispatch_once(&onceToken, ^{
        libnetworkImage = dlopen("/usr/lib/libnetwork.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (NULL != libnetworkImage) {
            _nw_protocol_definition_set_metadata_allocator = (__typeof(_nw_protocol_definition_set_metadata_allocator))dlsym(libnetworkImage, "nw_protocol_definition_set_metadata_allocator");
            if (NULL == _nw_protocol_definition_set_metadata_allocator) {
                os_log_error(OS_LOG_DEFAULT, "dlsym libnetwork nw_protocol_definition_set_metadata_allocator");
            }
        } else {
            os_log_error(OS_LOG_DEFAULT, "dlopen libnetwork");
        }
    });
    
    if (_nw_protocol_definition_set_metadata_allocator == NULL) {
        return;
    }
    
    _nw_protocol_definition_set_metadata_allocator(definition, allocator, deallocator);
}

static void * _Nullable
mock_protocol_copy_options(__unused nw_protocol_definition_t definition, void *options)
{
    void *new_options = calloc(1, sizeof(struct sec_protocol_options_content));

    sec_protocol_options_content_t copy = (sec_protocol_options_content_t)new_options;
    sec_protocol_options_content_t original = (sec_protocol_options_content_t)options;

    copy->min_version = original->min_version;
    copy->max_version = original->max_version;
    copy->disable_sni = original->disable_sni;
    copy->enable_fallback_attempt = original->enable_fallback_attempt;
    copy->enable_false_start = original->enable_false_start;
    copy->enable_tickets = original->enable_tickets;
    copy->enable_sct = original->enable_sct;
    copy->enable_ocsp = original->enable_ocsp;
    copy->enable_resumption = original->enable_resumption;
    copy->enable_renegotiation = original->enable_renegotiation;
    copy->enable_early_data = original->enable_early_data;

    if (original->server_name) {
        copy->server_name = strdup(original->server_name);
    }
    if (original->identity) {
        copy->identity = original->identity;
    }
    if (original->application_protocols) {
        copy->application_protocols = xpc_copy(original->application_protocols);
    }
    if (original->ciphersuites) {
        copy->ciphersuites = xpc_copy(original->ciphersuites);
    }
    if (original->dh_params) {
        copy->dh_params = original->dh_params;
    }
    if (original->key_update_block) {
        copy->key_update_block = original->key_update_block;
        copy->key_update_queue = original->key_update_queue;
    }
    if (original->challenge_block) {
        copy->challenge_block = original->challenge_block;
        copy->challenge_queue = original->challenge_queue;
    }
    if (original->verify_block) {
        copy->verify_block = original->verify_block;
        copy->verify_queue = original->verify_queue;
    }
    if (original->session_state) {
        copy->session_state = original->session_state;
    }
    if (original->session_update_block) {
        copy->session_update_block = original->session_update_block;
        copy->session_update_queue = original->session_update_queue;
    }
    if (original->pre_shared_keys) {
        copy->pre_shared_keys = xpc_copy(original->pre_shared_keys);
    }

    return new_options;
}

static void * _Nullable
mock_protocol_allocate_options(__unused nw_protocol_definition_t definition)
{
    return calloc(1, sizeof(struct sec_protocol_options_content));
}

static void
mock_protocol_deallocate_options(__unused nw_protocol_definition_t definition, void *options)
{
    sec_protocol_options_content_t content = (sec_protocol_options_content_t)options;
    if (content) {
        // pass
    }
    free(content);
}

static void
mock_protocol_set_options_allocator(nw_protocol_definition_t definition,
                                    nw_protocol_definition_allocate_f allocate_function,
                                    nw_protocol_definition_copy_f copy_function,
                                    nw_protocol_definition_deallocate_f deallocate_function)
{
    static void *libnetworkImage = NULL;
    static dispatch_once_t onceToken;
    static void (*_nw_protocol_definition_set_options_allocator)(nw_protocol_definition_t, nw_protocol_definition_allocate_f, nw_protocol_definition_copy_f, nw_protocol_definition_deallocate_f) = NULL;

    dispatch_once(&onceToken, ^{
        libnetworkImage = dlopen("/usr/lib/libnetwork.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (NULL != libnetworkImage) {
            _nw_protocol_definition_set_options_allocator = (__typeof(_nw_protocol_definition_set_options_allocator))dlsym(libnetworkImage, "nw_protocol_definition_set_options_allocator");
            if (NULL == _nw_protocol_definition_set_options_allocator) {
                os_log_error(OS_LOG_DEFAULT, "dlsym libnetwork nw_protocol_definition_set_options_allocator");
            }
        } else {
            os_log_error(OS_LOG_DEFAULT, "dlopen libnetwork");
        }
    });

    if (_nw_protocol_definition_set_options_allocator == NULL) {
        return;
    }

    _nw_protocol_definition_set_options_allocator(definition, allocate_function, copy_function, deallocate_function);
}

static nw_protocol_definition_t
mock_protocol_definition_create_with_identifier(nw_protocol_identifier_const_t identifier)
{
    static void *libnetworkImage = NULL;
    static dispatch_once_t onceToken;
    static nw_protocol_definition_t (*_nw_protocol_definition_create_with_identifier)(nw_protocol_identifier_const_t) = NULL;
    
    dispatch_once(&onceToken, ^{
        libnetworkImage = dlopen("/usr/lib/libnetwork.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (NULL != libnetworkImage) {
            _nw_protocol_definition_create_with_identifier = (__typeof(_nw_protocol_definition_create_with_identifier))dlsym(libnetworkImage, "nw_protocol_definition_create_with_identifier");
            if (NULL == _nw_protocol_definition_create_with_identifier) {
                os_log_error(OS_LOG_DEFAULT, "dlsym libnetwork nw_protocol_definition_create_with_identifier");
            }
        } else {
            os_log_error(OS_LOG_DEFAULT, "dlopen libnetwork");
        }
    });
    
    if (_nw_protocol_definition_create_with_identifier == NULL) {
        return NULL;
    }
    
    return _nw_protocol_definition_create_with_identifier(identifier);
}

static nw_protocol_definition_t
mock_protocol_copy_definition(void)
{
    static nw_protocol_definition_t definition = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        const char *mock_protocol_name = "secProtocolTestMockProtocol";
        definition = mock_protocol_definition_create_with_identifier(_mock_protocol_identifier(mock_protocol_name, strlen(mock_protocol_name)));
        mock_protocol_set_options_allocator(definition,
                                            mock_protocol_allocate_options,
                                            mock_protocol_copy_options,
                                            mock_protocol_deallocate_options);
        mock_protocol_set_metadata_allocator(definition,
                                             mock_protocol_allocate_metadata,
                                             mock_protocol_deallocate_metadata);
                                                      
    });

    return definition;
}

@interface SecProtocolTest : XCTestCase
@property nw_protocol_t mock_protocol;
@end

@implementation SecProtocolTest

- (void)setUp {
    [super setUp];
}

- (void)tearDown {
    [super tearDown];
}

- (sec_protocol_options_t)create_sec_protocol_options {
    static void *libnetworkImage = NULL;
    static dispatch_once_t onceToken;

    static sec_protocol_options_t (*_nw_protocol_create_options)(nw_protocol_definition_t) = NULL;

    dispatch_once(&onceToken, ^{
        libnetworkImage = dlopen("/usr/lib/libnetwork.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (NULL != libnetworkImage) {
            _nw_protocol_create_options = (__typeof(_nw_protocol_create_options))dlsym(libnetworkImage, "nw_protocol_create_options");
            if (NULL == _nw_protocol_create_options) {
                os_log_error(OS_LOG_DEFAULT, "dlsym libnetwork _nw_protocol_create_options");
            }
        } else {
            os_log_error(OS_LOG_DEFAULT, "dlopen libnetwork");
        }
    });

    if (_nw_protocol_create_options == NULL) {
        return nil;
    }

    return (sec_protocol_options_t)_nw_protocol_create_options(mock_protocol_copy_definition());
}

- (sec_protocol_metadata_t)create_sec_protocol_metadata {
    uuid_t identifier;
    uuid_generate(identifier);

    static void *libnetworkImage = NULL;
    static dispatch_once_t onceToken;
    static sec_protocol_metadata_t (*_nw_protocol_metadata_create)(nw_protocol_definition_t, _Nonnull uuid_t) = NULL;

    dispatch_once(&onceToken, ^{
        libnetworkImage = dlopen("/usr/lib/libnetwork.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (NULL != libnetworkImage) {
            _nw_protocol_metadata_create = (__typeof(_nw_protocol_metadata_create))dlsym(libnetworkImage, "nw_protocol_metadata_create");
            if (NULL == _nw_protocol_metadata_create) {
                os_log_error(OS_LOG_DEFAULT, "dlsym libnetwork nw_protocol_metadata_create");
            }
        } else {
            os_log_error(OS_LOG_DEFAULT, "dlopen libnetwork");
        }
    });

    if (_nw_protocol_metadata_create == NULL) {
        return nil;
    }

    sec_protocol_metadata_t metadata = (sec_protocol_metadata_t)_nw_protocol_metadata_create(mock_protocol_copy_definition(), identifier);
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        return true;
    });

    return metadata;
}

- (void)test_sec_protocol_metadata_get_connection_strength_tls12 {
    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];

    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        content->negotiated_ciphersuite = TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256;
        content->negotiated_protocol_version = tls_protocol_version_TLSv12;

        return true;
    });

    XCTAssertTrue(SSLConnectionStrengthStrong == sec_protocol_metadata_get_connection_strength(metadata),
                  "Expected SSLConnectionStrengthStrong for TLS 1.2 with a strong ciphersuite, got %d", (int)sec_protocol_metadata_get_connection_strength(metadata));
}

- (void)test_sec_protocol_metadata_get_connection_strength_tls12_weak_ciphersuite {
    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];
    if (metadata) {
        (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
            sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
            SEC_PROTOCOL_METADATA_VALIDATE(content, false);

            content->negotiated_ciphersuite = TLS_DHE_RSA_WITH_AES_256_GCM_SHA384;
            content->negotiated_protocol_version = tls_protocol_version_TLSv12;

            return true;
        });

        XCTAssertTrue(SSLConnectionStrengthWeak == sec_protocol_metadata_get_connection_strength(metadata),
                      "Expected SSLConnectionStrengthWeak for TLS 1.2 with a weak ciphersuite, got %d", (int)sec_protocol_metadata_get_connection_strength(metadata));
    }
}

- (void)test_sec_protocol_metadata_get_connection_strength_tls11 {
    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];
    if (metadata) {
        (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
            sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
            SEC_PROTOCOL_METADATA_VALIDATE(content, false);

            content->negotiated_ciphersuite = TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            content->negotiated_protocol_version = tls_protocol_version_TLSv11;
#pragma clang diagnostic pop

            return true;
        });

        XCTAssertTrue(SSLConnectionStrengthWeak == sec_protocol_metadata_get_connection_strength(metadata),
                      "Expected SSLConnectionStrengthWeak for TLS 1.1, got %d", (int)sec_protocol_metadata_get_connection_strength(metadata));
    }
}

- (void)test_sec_protocol_metadata_get_connection_strength_tls10 {
    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];
    if (metadata) {
        (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
            sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
            SEC_PROTOCOL_METADATA_VALIDATE(content, false);

            content->negotiated_ciphersuite = TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            content->negotiated_protocol_version = tls_protocol_version_TLSv10;
#pragma clang diagnostic pop

            return true;
        });

        XCTAssertTrue(SSLConnectionStrengthWeak == sec_protocol_metadata_get_connection_strength(metadata),
                      "Expected SSLConnectionStrengthWeak for TLS 1.0, got %d", (int)sec_protocol_metadata_get_connection_strength(metadata));
    }
}

- (void)test_sec_protocol_metadata_returned_raw_string_pointers_and_copy_apis {
    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];

    const char *expected_negotiated_protocol = "protocolA";
    const char *expected_server_name = "serverName";
    const char *expected_experiment_identifier = "experimentID";
    const char *expected_negotiated_curve = "curve";

    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
            sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
            SEC_PROTOCOL_METADATA_VALIDATE(content, false);

            content->negotiated_protocol = expected_negotiated_protocol;
            content->server_name = expected_server_name;
            content->experiment_identifier = expected_experiment_identifier;
            content->negotiated_curve = expected_negotiated_curve;

            return true;
        });

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // should return a pointer to a string "protocolA" and that string should be stored in set returned_raw_string_pointers
    const char *negotiated_protocol = sec_protocol_metadata_get_negotiated_protocol(metadata);
    const char *negotiated_curve = sec_protocol_metadata_get_tls_negotiated_group(metadata);
    const char *experiment_identifier = sec_protocol_metadata_get_experiment_identifier(metadata);
    const char *server_name = sec_protocol_metadata_get_server_name(metadata);
#pragma clang diagnostic pop
    XCTAssert(strcmp(negotiated_protocol, expected_negotiated_protocol) == 0);
    XCTAssert(strcmp(negotiated_curve, expected_negotiated_curve) == 0);
    XCTAssert(strcmp(experiment_identifier, expected_experiment_identifier) == 0);
    XCTAssert(strcmp(server_name, expected_server_name) == 0);

    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
            sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
            SEC_PROTOCOL_METADATA_VALIDATE(content, false);
            XCTAssert(CFSetContainsValue(content->returned_raw_string_pointers, expected_negotiated_protocol));
            XCTAssert(CFSetContainsValue(content->returned_raw_string_pointers, expected_server_name));
            XCTAssert(CFSetContainsValue(content->returned_raw_string_pointers, expected_experiment_identifier));
            XCTAssert(CFSetContainsValue(content->returned_raw_string_pointers, expected_negotiated_curve));
            return true;
    });

    // Check newer copy style APIs
    negotiated_protocol = sec_protocol_metadata_copy_negotiated_protocol(metadata);
    negotiated_curve = sec_protocol_metadata_copy_tls_negotiated_group(metadata);
    experiment_identifier = sec_protocol_metadata_copy_experiment_identifier(metadata);
    server_name = sec_protocol_metadata_copy_server_name(metadata);

    XCTAssert(strcmp(negotiated_protocol, expected_negotiated_protocol) == 0);
    XCTAssert(strcmp(negotiated_curve, expected_negotiated_curve) == 0);
    XCTAssert(strcmp(experiment_identifier, expected_experiment_identifier) == 0);
    XCTAssert(strcmp(server_name, expected_server_name) == 0);

    // Caller responsible for freeing returned objects from new APIs
    mock_protocol_safe_free(negotiated_protocol);
    mock_protocol_safe_free(negotiated_curve);
    mock_protocol_safe_free(experiment_identifier);
    mock_protocol_safe_free(server_name);
}

static size_t
_sec_protocol_dispatch_data_copyout(dispatch_data_t data, void *destination, size_t maxlen)
{
    __block size_t copied = 0;
    __block uint8_t *buffer = (uint8_t *)destination;

    if (data) {
        dispatch_data_apply(data, ^bool(__unused dispatch_data_t region, __unused size_t offset, const void *dbuffer, size_t size) {
            size_t consumed = MIN(maxlen - copied, size);
            if (consumed) {
                memcpy(&buffer[copied], dbuffer, consumed);
                copied += consumed;
            }

            return copied < maxlen;
        });
    }

    return copied;
}

static dispatch_data_t
_sec_protocol_test_metadata_session_exporter(void *handle)
{
    if (handle == NULL) {
        return nil;
    }

    const char *received_handle = (const char *)handle;
    dispatch_data_t serialized_session = dispatch_data_create(received_handle, strlen(received_handle), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    return serialized_session;
}

- (void)test_sec_protocol_register_session_update {
    sec_protocol_options_t options = [self create_sec_protocol_options];
    dispatch_queue_t test_queue = dispatch_queue_create("test_sec_protocol_register_session_update", NULL);
    __block bool session_updated = false;

    __block dispatch_data_t serialized_session_copy = nil;
    sec_protocol_session_update_t update_block = ^(sec_protocol_metadata_t metadata) {
        session_updated = true;
        serialized_session_copy = sec_protocol_metadata_copy_serialized_session(metadata);
    };

    sec_protocol_options_set_session_update_block(options, update_block, test_queue);

    const char *metadata_context_handle = "context handle";

    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        content->session_exporter_context = (void *)metadata_context_handle;
        content->session_exporter_function = _sec_protocol_test_metadata_session_exporter;

        return true;
    });

    update_block(metadata);

    XCTAssertTrue(session_updated, "Expected session update callback block to fire");
    XCTAssertNotNil(serialized_session_copy, "Expected non-nil serialized session");

    if (serialized_session_copy) {
        size_t data_size = dispatch_data_get_size(serialized_session_copy);
        uint8_t *session_copy_buffer = (uint8_t *)malloc(data_size);

        (void)_sec_protocol_dispatch_data_copyout(serialized_session_copy, session_copy_buffer, data_size);
        XCTAssertTrue(data_size == strlen(metadata_context_handle));
        XCTAssertTrue(memcmp(session_copy_buffer, metadata_context_handle, data_size) == 0);

        free(session_copy_buffer);
    }
}

#define SEC_PROTOCOL_METADATA_KEY_FAILURE_STACK_ERROR "stack_error"
#define SEC_PROTOCOL_METADATA_KEY_CIPHERSUITE "cipher_name"

- (void)test_sec_protocol_metadata_serialize_success {
    sec_protocol_options_t options = [self create_sec_protocol_options];
    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        content->failure = false;
        content->stack_error = 0xDEAD;
        content->negotiated_ciphersuite = TLS_AES_256_GCM_SHA384;
        return true;
    });

    xpc_object_t dictionary = sec_protocol_metadata_serialize_with_options(metadata, options);
    XCTAssertTrue(dictionary != NULL);
    XCTAssertTrue(xpc_dictionary_get_uint64(dictionary, SEC_PROTOCOL_METADATA_KEY_FAILURE_STACK_ERROR) == 0x00,
                  "Expected 0x%x, got 0x%llx", 0x00, xpc_dictionary_get_int64(dictionary, SEC_PROTOCOL_METADATA_KEY_FAILURE_STACK_ERROR));
    XCTAssertTrue(xpc_dictionary_get_uint64(dictionary, SEC_PROTOCOL_METADATA_KEY_CIPHERSUITE) == TLS_AES_256_GCM_SHA384,
                  "Expected 0x%x, got 0x%llx", TLS_AES_256_GCM_SHA384, xpc_dictionary_get_int64(dictionary, SEC_PROTOCOL_METADATA_KEY_CIPHERSUITE));
}

- (void)test_sec_protocol_metadata_serialize_failure {
    sec_protocol_options_t options = [self create_sec_protocol_options];
    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        content->failure = true;
        content->stack_error = 0xDEAD;
        content->negotiated_ciphersuite = TLS_AES_256_GCM_SHA384;
        return true;
    });

    xpc_object_t dictionary = sec_protocol_metadata_serialize_with_options(metadata, options);
    XCTAssertTrue(dictionary != NULL);
    XCTAssertTrue(xpc_dictionary_get_uint64(dictionary, SEC_PROTOCOL_METADATA_KEY_FAILURE_STACK_ERROR) == 0xDEAD,
                  "Expected 0x%x, got 0x%llx", 0xDEAD, xpc_dictionary_get_int64(dictionary, SEC_PROTOCOL_METADATA_KEY_FAILURE_STACK_ERROR));
    XCTAssertTrue(xpc_dictionary_get_uint64(dictionary, SEC_PROTOCOL_METADATA_KEY_CIPHERSUITE) == 0x00,
                  "Expected 0x%x, got 0x%llx", 0x00, xpc_dictionary_get_int64(dictionary, SEC_PROTOCOL_METADATA_KEY_CIPHERSUITE));
}

- (void)test_sec_protocol_options_set_quic_transport_parameters {
    uint8_t parameters_buffer[] = {0x00, 0x01, 0x02, 0x03};
    uint8_t expected_parameters_buffer[sizeof(parameters_buffer)] = {0};

    __block size_t parameters_len = sizeof(parameters_buffer);
    __block uint8_t *parameters = parameters_buffer;
    __block uint8_t *expected_parameters = expected_parameters_buffer;
    __block dispatch_data_t parameters_data = dispatch_data_create(parameters, sizeof(parameters_buffer), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    sec_protocol_options_t options = [self create_sec_protocol_options];
    sec_protocol_options_set_quic_transport_parameters(options, parameters_data);

    bool result = sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->quic_transport_parameters) {
            dispatch_data_t actual_parameters = content->quic_transport_parameters;
            size_t data_len = _sec_protocol_dispatch_data_copyout(actual_parameters, expected_parameters, parameters_len);

            if (data_len == parameters_len) {
                return 0 == memcmp(parameters, expected_parameters, parameters_len);
            }
        }

        return false;
    });

    XCTAssertTrue(result);
}

- (void)test_sec_protocol_metadata_copy_quic_transport_parameters {
    uint8_t parameters_buffer[] = {0x00, 0x01, 0x02, 0x03};
    uint8_t expected_parameters_buffer[sizeof(parameters_buffer)] = {0};

    __block size_t parameters_len = sizeof(parameters_buffer);
    __block uint8_t *parameters = parameters_buffer;
    __block uint8_t *expected_parameters = expected_parameters_buffer;
    __block dispatch_data_t parameters_data = dispatch_data_create(parameters, sizeof(parameters_buffer), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        content->quic_transport_parameters = parameters_data;
        return true;
    });

    dispatch_data_t actual_parameters = sec_protocol_metadata_copy_quic_transport_parameters(metadata);
    size_t data_len = _sec_protocol_dispatch_data_copyout(actual_parameters, expected_parameters, parameters_len);

    bool result = false;
    if (data_len == parameters_len) {
        result = 0 == memcmp(parameters, expected_parameters, parameters_len);
    }
    XCTAssertTrue(result);
}

- (void)test_sec_protocol_options_set_tls_encryption_secret_update_block {
    void (^update_block)(sec_protocol_tls_encryption_level_t, bool, dispatch_data_t) = ^(__unused sec_protocol_tls_encryption_level_t level, __unused bool is_write, __unused dispatch_data_t secret) {
        // pass
    };

    dispatch_queue_t update_queue = dispatch_queue_create("test_sec_protocol_options_set_tls_encryption_secret_update_block_queue", DISPATCH_QUEUE_SERIAL);

    sec_protocol_options_t options = [self create_sec_protocol_options];
    sec_protocol_options_set_tls_encryption_secret_update_block(options, update_block, update_queue);
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertTrue(content->tls_secret_update_block == update_block);
        XCTAssertTrue(content->tls_secret_update_queue != nil);
        return false;
    });
}

- (void)test_sec_protocol_options_set_tls_encryption_level_update_block {
    void (^update_block)(sec_protocol_tls_encryption_level_t, bool) = ^(__unused sec_protocol_tls_encryption_level_t level, __unused bool is_write) {
        // pass
    };

    dispatch_queue_t update_queue = dispatch_queue_create("test_sec_protocol_options_set_tls_encryption_level_update_block_queue", DISPATCH_QUEUE_SERIAL);

    sec_protocol_options_t options = [self create_sec_protocol_options];
    sec_protocol_options_set_tls_encryption_level_update_block(options, update_block, update_queue);
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertTrue(content->tls_encryption_level_update_block == update_block);
        XCTAssertTrue(content->tls_encryption_level_update_queue != nil);
        return false;
    });
}

- (void)test_sec_protocol_options_set_local_certificates {
    sec_protocol_options_t options = [self create_sec_protocol_options];

    sec_array_t certificates = sec_array_create();
    sec_protocol_options_set_local_certificates(options, certificates);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertTrue(content->certificates == certificates);
        return true;
    });
}

- (void)test_sec_protocol_options_set_private_key_blocks {
    sec_protocol_options_t options = [self create_sec_protocol_options];

    void (^sign_block)(uint16_t algorithm, dispatch_data_t, sec_protocol_private_key_complete_t) = ^(__unused uint16_t algorithm, __unused dispatch_data_t input, __unused sec_protocol_private_key_complete_t complete) {
        // pass
    };
    void (^decrypt_block)(dispatch_data_t, sec_protocol_private_key_complete_t) = ^(__unused dispatch_data_t input, __unused sec_protocol_private_key_complete_t complete) {
        // pass
    };
    dispatch_queue_t queue = dispatch_queue_create("private_key_operation_queue", DISPATCH_QUEUE_SERIAL);

    sec_protocol_options_set_private_key_blocks(options, sign_block, decrypt_block, queue);
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertTrue(content->private_key_sign_block == sign_block);
        XCTAssertTrue(content->private_key_decrypt_block == decrypt_block);
        XCTAssertTrue(content->private_key_queue == queue);
        return true;
    });
}

- (void)test_sec_protocol_options_set_tls_certificate_compression_enabled {
    sec_protocol_options_t options = [self create_sec_protocol_options];
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertFalse(content->certificate_compression_enabled);
        XCTAssertFalse(content->certificate_compression_enabled_override);
        return true;
    });
    sec_protocol_options_set_tls_certificate_compression_enabled(options, false);
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertFalse(content->certificate_compression_enabled);
        XCTAssertTrue(content->certificate_compression_enabled_override);
        return true;
    });
    sec_protocol_options_set_tls_certificate_compression_enabled(options, true);
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertTrue(content->certificate_compression_enabled);
        XCTAssertTrue(content->certificate_compression_enabled_override);
        return true;
    });
}

- (void)test_sec_protocol_options_set_peer_authentication_required {
    sec_protocol_options_t options = [self create_sec_protocol_options];

    sec_protocol_options_set_peer_authentication_required(options, true);
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertTrue(content->peer_authentication_required);
        return true;
    });
}

- (void)test_sec_protocol_options_set_peer_authentication_optional {
    sec_protocol_options_t options = [self create_sec_protocol_options];

    sec_protocol_options_set_peer_authentication_optional(options, true);
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertTrue(content->peer_authentication_optional);
        return true;
    });
}

- (void)test_sec_protocol_options_are_equal {
    sec_protocol_options_t optionsA = [self create_sec_protocol_options];
    sec_protocol_options_t optionsB = [self create_sec_protocol_options];

    sec_protocol_options_set_min_tls_protocol_version(optionsA, tls_protocol_version_TLSv13);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_min_tls_protocol_version(optionsB, tls_protocol_version_TLSv13);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_max_tls_protocol_version(optionsA, tls_protocol_version_TLSv13);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_max_tls_protocol_version(optionsB, tls_protocol_version_TLSv13);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_tls_sni_disabled(optionsA, true);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_sni_disabled(optionsB, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_sni_disabled(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_tls_is_fallback_attempt(optionsA, true);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_is_fallback_attempt(optionsB, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_is_fallback_attempt(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_tls_false_start_enabled(optionsA, true);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_false_start_enabled(optionsB, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_false_start_enabled(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_tls_tickets_enabled(optionsA, true);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_tickets_enabled(optionsB, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_tickets_enabled(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_tls_sct_enabled(optionsA, true);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_sct_enabled(optionsB, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_sct_enabled(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_tls_ocsp_enabled(optionsA, true);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_ocsp_enabled(optionsB, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_ocsp_enabled(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_tls_resumption_enabled(optionsA, true);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_resumption_enabled(optionsB, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_resumption_enabled(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_tls_renegotiation_enabled(optionsA, true);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_renegotiation_enabled(optionsB, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_renegotiation_enabled(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_tls_grease_enabled(optionsA, true);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_grease_enabled(optionsB, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_grease_enabled(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_pqtls_mode(optionsA, no_pqtls);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_pqtls_mode(optionsA, try_pqtls);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_pqtls_mode(optionsA, force_pqtls);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_pqtls_mode(optionsB, force_pqtls);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_tls_delegated_credentials_enabled(optionsA, true);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_delegated_credentials_enabled(optionsB, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_delegated_credentials_enabled(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_eddsa_enabled(optionsA, true);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_eddsa_enabled(optionsB, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_eddsa_enabled(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_tls_early_data_enabled(optionsA, true);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_early_data_enabled(optionsB, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_early_data_enabled(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_tls_certificate_compression_enabled(optionsA, true);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_certificate_compression_enabled(optionsB, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_certificate_compression_enabled(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    const char *server_nameA = "localhost";
    const char *server_nameB = "apple.com";
    sec_protocol_options_set_tls_server_name(optionsA, server_nameA);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_server_name(optionsB, server_nameB);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_server_name(optionsB, server_nameA);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    uint8_t quic_parameters_buffer[] = {0x00, 0x01, 0x02, 0x03};
    dispatch_data_t quic_parameters = dispatch_data_create(quic_parameters_buffer, sizeof(quic_parameters_buffer), nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    sec_protocol_options_set_quic_transport_parameters(optionsA, quic_parameters);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_quic_transport_parameters(optionsB, quic_parameters);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_append_tls_ciphersuite(optionsA, 1337);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_append_tls_ciphersuite(optionsB, 1337);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    const char *application_protocolA = "h2";
    sec_protocol_options_add_tls_application_protocol(optionsA, application_protocolA);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_add_tls_application_protocol(optionsB, application_protocolA);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    const char *application_protocolB = "h3";
    sec_protocol_options_add_transport_specific_application_protocol(optionsA, application_protocolB,
                                                                     sec_protocol_transport_quic);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_add_transport_specific_application_protocol(optionsB, application_protocolB,
                                                                     sec_protocol_transport_quic);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_quic_use_legacy_codepoint(optionsA, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_quic_use_legacy_codepoint(optionsA, false);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_quic_use_legacy_codepoint(optionsB, false);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_quic_use_legacy_codepoint(optionsA, true);
    sec_protocol_options_set_quic_use_legacy_codepoint(optionsB, true);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_append_tls_ciphersuite(optionsB, 7331);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
}

- (void)test_sec_protocol_options_tls_application_protocols {
    sec_protocol_options_t options = [self create_sec_protocol_options];
    XCTAssertNotNil(options);

    // We should be able to add an application protocol
    sec_protocol_options_add_tls_application_protocol(options, "h2");

    (void)sec_protocol_options_access_handle(options, ^bool(void * _Nonnull handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        XCTAssertNotNil(content->application_protocols);
        XCTAssertTrue(xpc_get_type(content->application_protocols) == XPC_TYPE_ARRAY);
        XCTAssertTrue(xpc_array_get_count(content->application_protocols) == 1);

        xpc_array_apply(content->application_protocols, ^bool(size_t index, xpc_object_t  _Nonnull value) {
            XCTAssertTrue(index == 0);
            XCTAssertNotNil(value);
            XCTAssertTrue(xpc_get_type(value) == XPC_TYPE_STRING);

            const char *alpn_string = xpc_string_get_string_ptr(value);
            XCTAssertTrue(strncmp("h2", alpn_string, sizeof("h2")) == 0, @"ALPN string is not correct");
            return true;
        });
        return true;
    });

    // Adding a second value should give us two values, in the correct order
    sec_protocol_options_add_tls_application_protocol(options, "h3");

    (void)sec_protocol_options_access_handle(options, ^bool(void * _Nonnull handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        XCTAssertNotNil(content->application_protocols);
        XCTAssertTrue(xpc_get_type(content->application_protocols) == XPC_TYPE_ARRAY);
        XCTAssertTrue(xpc_array_get_count(content->application_protocols) == 2);

        xpc_array_apply(content->application_protocols, ^bool(size_t index, xpc_object_t  _Nonnull value) {
            XCTAssertTrue(index == 0 || index == 1);
            XCTAssertNotNil(value);
            XCTAssertTrue(xpc_get_type(value) == XPC_TYPE_STRING);

            const char *alpn_string = xpc_string_get_string_ptr(value);

            if (index == 0) {
                XCTAssertTrue(strncmp("h2", alpn_string, sizeof("h2")) == 0, @"ALPN string for h2 is not correct");
            } else if (index == 1) {
                XCTAssertTrue(strncmp("h3", alpn_string, sizeof("h3")) == 0, @"ALPN string for h3 is not correct");
            }

            return true;
        });
        return true;
    });

    // There should be no array present at all after we clear it
    sec_protocol_options_clear_tls_application_protocols(options);

    (void)sec_protocol_options_access_handle(options, ^bool(void * _Nonnull handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        XCTAssertNil(content->application_protocols);
        return true;
    });

    // And we should be able to add another one after clearing it
    sec_protocol_options_add_tls_application_protocol(options, "h4");

    (void)sec_protocol_options_access_handle(options, ^bool(void * _Nonnull handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        XCTAssertNotNil(content->application_protocols);
        XCTAssertTrue(xpc_get_type(content->application_protocols) == XPC_TYPE_ARRAY);
        XCTAssertTrue(xpc_array_get_count(content->application_protocols) == 1);

        xpc_array_apply(content->application_protocols, ^bool(size_t index, xpc_object_t  _Nonnull value) {
            XCTAssertTrue(index == 0);
            XCTAssertNotNil(value);
            XCTAssertTrue(xpc_get_type(value) == XPC_TYPE_STRING);

            const char *alpn_string = xpc_string_get_string_ptr(value);
            XCTAssertTrue(strncmp("h4", alpn_string, sizeof("h4")) == 0, @"ALPN string for h4 is not correct");
            return true;
        });
        return true;
    });
}

- (void)test_sec_protocol_options_copy_transport_specific_application_protocol {
    sec_protocol_options_t options = [self create_sec_protocol_options];

    const char *application_protocol_dummy = "dummy";
    const char *application_protocol_h2 = "h2";
    const char *application_protocol_h3 = "h3";

    sec_protocol_options_add_transport_specific_application_protocol(options, application_protocol_h2, sec_protocol_transport_tcp);
    xpc_object_t protocols = sec_protocol_options_copy_transport_specific_application_protocol(options, sec_protocol_transport_quic);
    XCTAssertFalse(protocols != NULL);
    if (protocols != NULL) {
        return;
    }

    sec_protocol_options_add_tls_application_protocol(options, application_protocol_dummy);
    sec_protocol_options_add_transport_specific_application_protocol(options, application_protocol_h3, sec_protocol_transport_quic);

    for (sec_protocol_transport_t t = sec_protocol_transport_any; t <= sec_protocol_transport_quic; t++) {
        protocols = sec_protocol_options_copy_transport_specific_application_protocol(options, t);
        XCTAssertFalse(protocols == NULL);
        if (protocols == NULL) {
            return;
        }

        const char *application_protocols_for_any[]  = { application_protocol_h2, application_protocol_dummy, application_protocol_h3, };
        // application_protocols_for_tcp includes application_protocol_dummy because "dummy" isn't tied to any transport.
        const char *application_protocols_for_tcp[]  = { application_protocol_h2, application_protocol_dummy, };
        const char *application_protocols_for_quic[] = { application_protocol_dummy, application_protocol_h3, };

        size_t count_of_application_protocols_for_transport[] = {
            [sec_protocol_transport_any]  = sizeof(application_protocols_for_any)/sizeof(application_protocols_for_any[0]),
            [sec_protocol_transport_tcp]  = sizeof(application_protocols_for_tcp)/sizeof(application_protocols_for_tcp[0]),
            [sec_protocol_transport_quic] = sizeof(application_protocols_for_quic)/sizeof(application_protocols_for_quic[0]),
        };

        XCTAssertFalse(xpc_get_type(protocols) != XPC_TYPE_ARRAY);
        if (xpc_get_type(protocols) != XPC_TYPE_ARRAY) {
            return;
        }

        size_t protocols_count = xpc_array_get_count(protocols);
        XCTAssertFalse(protocols_count != count_of_application_protocols_for_transport[t]);
        if (protocols_count != count_of_application_protocols_for_transport[t]) {
            return;
        }

        const char **application_protocols_for_transport[] = {
            [sec_protocol_transport_any]  = application_protocols_for_any,
            [sec_protocol_transport_tcp]  = application_protocols_for_tcp,
            [sec_protocol_transport_quic] = application_protocols_for_quic,
        };

        for (size_t i = 0; i < protocols_count; i++) {
            const char *protocol_name = xpc_array_get_string(protocols, i);
            const char *expected_protocol_name = application_protocols_for_transport[t][i];
            bool protocol_match = (strcmp(protocol_name, expected_protocol_name) == 0);

            XCTAssertFalse(protocol_match == false);
            if (protocol_match == false) {
                return;
            }
        }
    }
}

- (void)test_sec_protocol_options_set_tls_server_name {
    sec_protocol_options_t optionsA = [self create_sec_protocol_options];
    sec_protocol_options_t optionsB = [self create_sec_protocol_options];

    const char *server_nameA = "apple.com";
    const char *server_nameB = "example.com";

    /*
     * Empty options should be equal.
     */
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    /*
     * Set the name in optionsA.
     * Options A, B should now be different.
     */
    sec_protocol_options_set_tls_server_name(optionsA, server_nameA);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));

    /*
     * Set the name to nameA in optionsB.
     * Options A, B should now be equal.
     */
    sec_protocol_options_set_tls_server_name(optionsB, server_nameA);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    /*
     * Change the current name in B.
     * Comparison should fail.
     */
    sec_protocol_options_set_tls_server_name(optionsB, server_nameB);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
}

- (void)test_sec_protocol_options_create_and_import_config {
    sec_protocol_options_t options = [self create_sec_protocol_options];
    sec_protocol_options_t imported_options = [self create_sec_protocol_options];

    sec_protocol_options_set_min_tls_protocol_version(options, tls_protocol_version_TLSv13);
    sec_protocol_options_set_tls_early_data_enabled(options, true);
    sec_protocol_options_set_quic_use_legacy_codepoint(options, false);
    sec_protocol_options_set_pqtls_mode(options, force_pqtls);
    xpc_object_t config = sec_protocol_options_create_config(options);
    XCTAssertTrue(config != NULL);
    if (config != NULL) {
        sec_protocol_options_apply_config(imported_options, config);
        XCTAssertTrue(sec_protocol_options_are_equal(options, imported_options));
    }
}

- (void)test_sec_protocol_options_matches_full_config {
    sec_protocol_options_t options = [self create_sec_protocol_options];

    sec_protocol_options_set_min_tls_protocol_version(options, tls_protocol_version_TLSv13);
    sec_protocol_options_set_tls_early_data_enabled(options, true);
    xpc_object_t config = sec_protocol_options_create_config(options);
    XCTAssertTrue(config != NULL);
    if (config != NULL) {
        XCTAssertTrue(sec_protocol_options_matches_config(options, config));
    }
}

- (void)test_sec_protocol_options_matches_partial_config {
    sec_protocol_options_t options = [self create_sec_protocol_options];
    sec_protocol_options_set_tls_resumption_enabled(options, true);

    xpc_object_t config = sec_protocol_options_create_config(options);
    XCTAssertTrue(config != NULL);
    if (config != NULL) {
        // Drop one key from the config, and make sure that the result still matches
        __block const char *enable_resumption_key = "enable_resumption";
        xpc_object_t trimmed_config = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_apply(config, ^bool(const char * _Nonnull key, xpc_object_t  _Nonnull value) {
            if (strncmp(key, enable_resumption_key, strlen(enable_resumption_key)) != 0) {
                xpc_dictionary_set_value(trimmed_config, key, value);
            }
            return true;
        });
        XCTAssertTrue(sec_protocol_options_matches_config(options, trimmed_config));
    }
}

- (void)test_sec_protocol_options_matches_config_with_mismatch {
    sec_protocol_options_t options = [self create_sec_protocol_options];

    __block bool enable_resumption = true;
    sec_protocol_options_set_tls_resumption_enabled(options, enable_resumption);

    xpc_object_t config = sec_protocol_options_create_config(options);
    XCTAssertTrue(config != NULL);
    if (config != NULL) {
        // Flip a value in the config, and expect the match to fail
        __block const char *enable_resumption_key = "enable_resumption";
        xpc_object_t mismatched_config = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_apply(config, ^bool(const char * _Nonnull key, xpc_object_t  _Nonnull value) {
            if (strncmp(key, enable_resumption_key, strlen(enable_resumption_key)) != 0) {
                xpc_dictionary_set_value(mismatched_config, key, value);
            } else {
                xpc_dictionary_set_bool(mismatched_config, key, !enable_resumption);
            }
            return true;
        });
        XCTAssertFalse(sec_protocol_options_matches_config(options, mismatched_config));
    }
}

- (void)test_protocol_version_map {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    XCTAssertTrue(tls_protocol_version_TLSv10 == SSLProtocolGetVersionCodepoint(kTLSProtocol1));
    XCTAssertTrue(tls_protocol_version_TLSv11 == SSLProtocolGetVersionCodepoint(kTLSProtocol11));
    XCTAssertTrue(tls_protocol_version_TLSv12 == SSLProtocolGetVersionCodepoint(kTLSProtocol12));
    XCTAssertTrue(tls_protocol_version_TLSv13 == SSLProtocolGetVersionCodepoint(kTLSProtocol13));
    XCTAssertTrue(tls_protocol_version_DTLSv12 == SSLProtocolGetVersionCodepoint(kDTLSProtocol12));
    XCTAssertTrue(tls_protocol_version_DTLSv10 == SSLProtocolGetVersionCodepoint(kDTLSProtocol1));

    XCTAssertTrue(kTLSProtocol1 == SSLProtocolFromVersionCodepoint(tls_protocol_version_TLSv10));
    XCTAssertTrue(kTLSProtocol11 == SSLProtocolFromVersionCodepoint(tls_protocol_version_TLSv11));
    XCTAssertTrue(kTLSProtocol12 == SSLProtocolFromVersionCodepoint(tls_protocol_version_TLSv12));
    XCTAssertTrue(kTLSProtocol13 == SSLProtocolFromVersionCodepoint(tls_protocol_version_TLSv13));
    XCTAssertTrue(kDTLSProtocol12 == SSLProtocolFromVersionCodepoint(tls_protocol_version_DTLSv12));
    XCTAssertTrue(kDTLSProtocol1 == SSLProtocolFromVersionCodepoint(tls_protocol_version_DTLSv10));
#pragma clang diagnostic pop
}

- (void)test_default_protocol_versions {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    XCTAssertTrue(sec_protocol_options_get_default_max_tls_protocol_version() == tls_protocol_version_TLSv13);
    XCTAssertTrue(sec_protocol_options_get_default_min_tls_protocol_version() == tls_protocol_version_TLSv10);
    XCTAssertTrue(sec_protocol_options_get_default_max_dtls_protocol_version() == tls_protocol_version_DTLSv12);
    XCTAssertTrue(sec_protocol_options_get_default_min_dtls_protocol_version() == tls_protocol_version_DTLSv10);
#pragma clang diagnostic pop
}

- (void)test_enable_ech {
    sec_protocol_options_t options = [self create_sec_protocol_options];
    XCTAssertFalse(sec_protocol_options_get_enable_encrypted_client_hello(options), "enable_ech initialized to true");
    sec_protocol_options_set_enable_encrypted_client_hello(options, true);
    XCTAssertTrue(sec_protocol_options_get_enable_encrypted_client_hello(options), "ECH still disabled after set to true");
    sec_protocol_options_set_enable_encrypted_client_hello(options, false);
    XCTAssertFalse(sec_protocol_options_get_enable_encrypted_client_hello(options), "ECH still enabled after changed back to false");
}

- (void)test_pqtls_mode {
    sec_protocol_options_t options = [self create_sec_protocol_options];
    XCTAssertTrue(sec_protocol_options_get_pqtls_mode(options) == no_pqtls, "pqtls_mode initially not equal to no_pqtls");
    sec_protocol_options_set_pqtls_mode(options, try_pqtls);
    XCTAssertTrue(sec_protocol_options_get_pqtls_mode(options) == try_pqtls, "pqtls_mode not equal to try_pqtls after set");
    sec_protocol_options_set_pqtls_mode(options, force_pqtls);
    XCTAssertTrue(sec_protocol_options_get_pqtls_mode(options) == force_pqtls, "pqtls_mode not equal to force_pqtls after set");
    sec_protocol_options_set_pqtls_mode(options, no_pqtls);
    XCTAssertTrue(sec_protocol_options_get_pqtls_mode(options) == no_pqtls, "pqtls_mode not equal to no_pqtls after set");
}

- (void)test_quic_use_legacy_codepoint {
    sec_protocol_options_t options = [self create_sec_protocol_options];
    XCTAssertTrue(sec_protocol_options_get_quic_use_legacy_codepoint(options), "quic_use_legacy_codepoint default set to false");
    sec_protocol_options_set_quic_use_legacy_codepoint(options, false);
    XCTAssertFalse(sec_protocol_options_get_quic_use_legacy_codepoint(options), "quic_use_legacy_codepoint still true after changed to false");
    sec_protocol_options_set_quic_use_legacy_codepoint(options, true);
    XCTAssertTrue(sec_protocol_options_get_quic_use_legacy_codepoint(options), "quic_use_legacy_codepoint still false after set to true");

}

- (void)test_sec_protocol_options_set_psk_hint {
    __block dispatch_data_t hint = [self create_random_dispatch_data];
    sec_protocol_options_t options = [self create_sec_protocol_options];

    (void)sec_protocol_options_access_handle(options, ^bool(void * _Nonnull handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        XCTAssertNil(content->psk_identity_hint, @"PSK identity initialized incorrectly");
        return true;
    });

    sec_protocol_options_set_tls_pre_shared_key_identity_hint(options, hint);

    (void)sec_protocol_options_access_handle(options, ^bool(void * _Nonnull handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(content->psk_identity_hint, hint), @"PSK identity mistmatch");
        return true;
    });
}

- (void)test_sec_protocol_options_set_psk_selection_block {
    void (^selection_block)(sec_protocol_metadata_t, dispatch_data_t, sec_protocol_pre_shared_key_selection_complete_t) = ^(__unused sec_protocol_metadata_t metadata, __unused dispatch_data_t psk_identity_hint, __unused sec_protocol_pre_shared_key_selection_complete_t complete) {
        // pass
    };
    dispatch_queue_t selection_queue = dispatch_queue_create("test_sec_protocol_options_set_psk_selection_block_queue", DISPATCH_QUEUE_SERIAL);

    sec_protocol_options_t options = [self create_sec_protocol_options];
    sec_protocol_options_set_pre_shared_key_selection_block(options, selection_block, selection_queue);
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertTrue(content->psk_selection_block == selection_block);
        XCTAssertTrue(content->psk_selection_queue != nil);
        return false;
    });
}

- (dispatch_data_t)create_random_dispatch_data {
    uint8_t random[32];
    (void)SecRandomCopyBytes(NULL, sizeof(random), random);
    return dispatch_data_create(random, sizeof(random), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
}

- (void)test_sec_protocol_metadata_access_psks {
    __block dispatch_data_t psk_data = [self create_random_dispatch_data];
    __block dispatch_data_t psk_identity_data = [self create_random_dispatch_data];

    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        content->pre_shared_keys = xpc_array_create(NULL, 0);

        xpc_object_t xpc_psk_data = xpc_data_create_with_dispatch_data(psk_data);
        xpc_object_t xpc_psk_identity_data = xpc_data_create_with_dispatch_data(psk_identity_data);

        xpc_object_t tuple = xpc_array_create(NULL, 0);
        xpc_array_set_value(tuple, XPC_ARRAY_APPEND, xpc_psk_data);
        xpc_array_set_value(tuple, XPC_ARRAY_APPEND, xpc_psk_identity_data);

        xpc_array_set_value(content->pre_shared_keys, XPC_ARRAY_APPEND, tuple);
        return true;
    });

    BOOL accessed = sec_protocol_metadata_access_pre_shared_keys(metadata, ^(dispatch_data_t psk, dispatch_data_t identity) {
        XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(psk, psk_data), @"Expected PSK data match");
        XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(identity, psk_identity_data), @"Expected PSK identity data match");
    });
    XCTAssertTrue(accessed, @"Expected sec_protocol_metadata_access_pre_shared_keys to traverse PSK list");
}

- (void)test_sec_protocol_options_set_tls_block_length_padding {
    sec_protocol_options_t options = [self create_sec_protocol_options];

    sec_protocol_block_length_padding_t expected_block_length_padding = SEC_PROTOCOL_BLOCK_LENGTH_PADDING_DEFAULT;
    sec_protocol_options_set_tls_block_length_padding(options, expected_block_length_padding);

    __block sec_protocol_block_length_padding_t current_block_length_padding = SEC_PROTOCOL_BLOCK_LENGTH_PADDING_NONE;
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        current_block_length_padding = content->tls_block_length_padding;
        return true;
    });

    XCTAssertTrue(current_block_length_padding == expected_block_length_padding);
}

- (void)test_sec_protocol_experiment_identifier {
    sec_protocol_options_t options = [self create_sec_protocol_options];

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertTrue(content->experiment_identifier == NULL);
        return true;
    });

    const char *identifier = "first_experiment";
    sec_protocol_options_set_experiment_identifier(options, identifier);
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertTrue(content->experiment_identifier != NULL);
        XCTAssertTrue(strncmp(identifier, content->experiment_identifier, strlen(identifier)) == 0);
        return true;
    });

    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];
    XCTAssertTrue(sec_protocol_metadata_copy_experiment_identifier(metadata) == NULL);

    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        content->experiment_identifier = strdup(identifier);
        return true;
    });

    const char *experiment_id = sec_protocol_metadata_copy_experiment_identifier(metadata);
    XCTAssertTrue(strncmp(identifier, experiment_id, strlen(identifier)) == 0);
    mock_protocol_safe_free(experiment_id);
}

- (void)test_sec_protocol_connection_id {
    sec_protocol_options_t options = [self create_sec_protocol_options];

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        uuid_t zeroes = {};
        XCTAssertTrue(memcmp(zeroes, content->connection_id, sizeof(zeroes)) == 0);
        return true;
    });

    uuid_t uuid = {};
    __block uint8_t *uuid_ptr = uuid;
    __block size_t uuid_len = sizeof(uuid);
    (void)SecRandomCopyBytes(NULL, sizeof(uuid), uuid);
    sec_protocol_options_set_connection_id(options, uuid);
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertTrue(memcmp(content->connection_id, uuid_ptr, uuid_len) == 0);
        return true;
    });

    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        memcpy(content->connection_id, uuid_ptr, uuid_len);
        return true;
    });

    uuid_t copied_metadata = {};
    sec_protocol_metadata_copy_connection_id(metadata, copied_metadata);

    XCTAssertTrue(memcmp(uuid, copied_metadata, sizeof(copied_metadata)) == 0);
}

- (void)test_sec_protocol_options_set_allow_unknown_alpn_protos {
    sec_protocol_options_t options = [self create_sec_protocol_options];

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertFalse(content->allow_unknown_alpn_protos_override);
        return true;
    });

    sec_protocol_options_set_allow_unknown_alpn_protos(options, true);
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertTrue(content->allow_unknown_alpn_protos);
        XCTAssertTrue(content->allow_unknown_alpn_protos_override);
        return true;
    });
}

// Assigns NULL to CF. Releases the value stored at CF unless it was NULL.  Always returns NULL, for your convenience
#define CFReleaseNull(CF) ({ __typeof__(CF) *const _pcf = &(CF), _cf = *_pcf; (_cf ? (*_pcf) = ((__typeof__(CF))0), (CFRelease(_cf), ((__typeof__(CF))0)) : _cf); })

#if TARGET_OS_OSX
static NSString *letters = @"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static NSString *
generate_random_string(size_t length)
{
    NSMutableString *randomString = [NSMutableString stringWithCapacity:length];
    for (size_t i = 0; i < length; i++) {
        [randomString appendFormat:@"%C", [letters characterAtIndex:arc4random_uniform([letters length])]];
    }
    return randomString;
}

static SecKeychainRef
createKeychain(void)
{
    CFArrayRef cfSearchList = NULL;
    NSMutableArray *oldSearchList = NULL, *newSearchList = NULL;
    SecKeychainRef localKeychain = NULL;

    // Create path for keychain in our sandbox
    NSString *prefix = [[NSProcessInfo processInfo] globallyUniqueString];
    NSString *keychain_name =
        [NSString stringWithFormat:@"%@_%@_boringssl_tmp.keychain", prefix, generate_random_string(32)];
    NSString *base_path = NSTemporaryDirectory();
    NSURL *fileURL = [[NSURL fileURLWithPath:base_path isDirectory:YES] URLByAppendingPathComponent:keychain_name];

    // Create keychain and add to search list (for automatic lookup)
    if (SecKeychainCopySearchList(&cfSearchList) != errSecSuccess) {
        fprintf(stderr, "SecKeychainCopySearchList failed");
        return NULL;
    }

    oldSearchList = CFBridgingRelease(cfSearchList);
    newSearchList = [NSMutableArray arrayWithArray:oldSearchList];

    if (SecKeychainCreate([fileURL fileSystemRepresentation], 8, "password", false, NULL,
            &localKeychain) != errSecSuccess) {
        fprintf(stderr, "SecKeychainCreate failed");
        return NULL;
    }

    if (localKeychain != NULL) {
        [newSearchList addObject:(__bridge id)localKeychain];
        if (SecKeychainSetSearchList((__bridge CFArrayRef)newSearchList) != errSecSuccess) {
            fprintf(stderr, "SecKeychainSetSearchList failed");
            return NULL;
        }
    }

    return localKeychain;
}
#endif // TARGET_OS_OSX

/*!
 * The keychain reference is static so that unit tests finish in a reasonable amount of time. Otherwise,
 * the change+compile+test loop takes too long to be useful.
 */
#if TARGET_OS_OSX
static SecKeychainRef keychain;
#endif

static SecIdentityRef parse_sec_identity_from_pkcs12(NSData *pkcs12data)
{
  NSDictionary *options = @{
          (__bridge NSString *)kSecImportExportPassphrase : @("password"),
#ifndef TARGET_OS_BRIDGE
          (__bridge NSString *)kSecImportToMemoryOnly: @(TRUE),
#endif // TARGET_OS_BRIDGE
      };
    if (options == nil) {
        fprintf(stderr, "Failed to create the SecImport options");
        return NULL;
    }

    CFArrayRef legacyImportedItems = NULL;
#if TARGET_OS_OSX
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        keychain = createKeychain();
    });

    SecExternalFormat sef = kSecFormatPKCS12;
    SecItemImportExportKeyParameters keyParams = {
        .passphrase = CFSTR("password"),
        .flags = kSecKeyNoAccessControl,
    };

    OSStatus securityError =
        SecItemImport((__bridge CFDataRef)pkcs12data, NULL, &sef, NULL, 0, &keyParams, keychain, &legacyImportedItems);
    if (securityError != errSecSuccess || legacyImportedItems == NULL) {
        fprintf(stderr, "[OSX ONLY] SecItemImport failed: %d %p %p", (int)securityError, pkcs12data, keychain);
        return NULL;
    }

    if (CFGetTypeID(CFArrayGetValueAtIndex(legacyImportedItems, 0)) != SecIdentityGetTypeID()) {
        CFShow(CFArrayGetValueAtIndex(legacyImportedItems, 0));
        fprintf(stderr, "[OSX ONLY] Failed to load a SecIdentityRef from the local PKCS12 data\n");
        return NULL;
    }

    SecIdentityRef identity = (SecIdentityRef)CFArrayGetValueAtIndex(legacyImportedItems, 0);
    CFRetain(identity);
    CFReleaseNull(legacyImportedItems);
    return identity;
#else // !TARGET_OS_OSX
    OSStatus securityError =
        SecPKCS12Import((__bridge CFDataRef)pkcs12data, (__bridge CFDictionaryRef)options, &legacyImportedItems);
    if (securityError != errSecSuccess || legacyImportedItems == NULL) {
        fprintf(stderr, "SecPKCS12Import failed: %d", (int)securityError);
        return NULL;
    }

    NSArray *importedItems = (__bridge_transfer NSArray *)legacyImportedItems;
    legacyImportedItems = NULL;
    if (importedItems == nil || ![importedItems isKindOfClass:[NSArray class]] || importedItems.count == 0) {
        fprintf(stderr, "importedItems is NULL");
        return NULL;
    }

    NSDictionary *dict = importedItems[0];
    SecIdentityRef local_identity = (__bridge SecIdentityRef)dict[(__bridge NSString *)kSecImportItemIdentity];

    CFRetain(local_identity);
    CFReleaseNull(legacyImportedItems);
    return local_identity;
#endif // !TARGET_OS_OSX
}

- (void)test_sec_identity_type {
    unsigned char prime256v1_server_leaf_p12[] = {
      0x30, 0x82, 0x04, 0xa2, 0x02, 0x01, 0x03, 0x30, 0x82, 0x04, 0x68, 0x06, 0x09, 0x2a, 0x86, 0x48,
      0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0xa0, 0x82, 0x04, 0x59, 0x04, 0x82, 0x04, 0x55, 0x30, 0x82,
      0x04, 0x51, 0x30, 0x82, 0x03, 0x47, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07,
      0x06, 0xa0, 0x82, 0x03, 0x38, 0x30, 0x82, 0x03, 0x34, 0x02, 0x01, 0x00, 0x30, 0x82, 0x03, 0x2d,
      0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0x30, 0x1c, 0x06, 0x0a, 0x2a,
      0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x01, 0x06, 0x30, 0x0e, 0x04, 0x08, 0x5c, 0xd3, 0x24,
      0x0e, 0xea, 0x91, 0xb2, 0xfb, 0x02, 0x02, 0x08, 0x00, 0x80, 0x82, 0x03, 0x00, 0x3c, 0xbc, 0x54,
      0xa7, 0x77, 0xab, 0x55, 0x9d, 0x4f, 0x27, 0xae, 0x64, 0xc7, 0x23, 0x8c, 0x06, 0x44, 0xfe, 0xf7,
      0x6e, 0xe7, 0x5b, 0xc4, 0x1e, 0x0f, 0xee, 0x7c, 0x3f, 0x41, 0x73, 0x6a, 0x5e, 0xfa, 0xc1, 0x03,
      0xe3, 0x81, 0xcc, 0x9f, 0x20, 0x6c, 0x3f, 0x2b, 0xb9, 0x69, 0x8e, 0x50, 0xb2, 0xe6, 0xf9, 0x23,
      0xcd, 0x74, 0xd9, 0x2a, 0x87, 0xc6, 0xda, 0xb7, 0x9e, 0x3f, 0x72, 0xa0, 0xf0, 0xe8, 0xc9, 0x1b,
      0xeb, 0xb1, 0xd9, 0xd3, 0x8f, 0x9d, 0xf0, 0xd2, 0xd5, 0x42, 0x3d, 0x5b, 0x52, 0x57, 0x03, 0xd5,
      0xeb, 0x76, 0x7b, 0x37, 0xa7, 0xe2, 0xfb, 0x3c, 0xcf, 0x93, 0x17, 0x4f, 0x90, 0x0e, 0x8e, 0x84,
      0xdb, 0x81, 0xa5, 0x5f, 0x79, 0xd0, 0x16, 0x75, 0x1b, 0x08, 0x54, 0xc4, 0x14, 0xfc, 0x04, 0xec,
      0xd4, 0x25, 0xed, 0x8b, 0x3f, 0xef, 0x88, 0x85, 0x04, 0x20, 0xba, 0xd1, 0x66, 0x3f, 0x01, 0xb7,
      0x12, 0x25, 0xf7, 0xc8, 0x85, 0x76, 0xae, 0xa3, 0x48, 0x35, 0x26, 0xad, 0x29, 0x79, 0x86, 0xdb,
      0x22, 0xe4, 0x4a, 0xe3, 0xad, 0xd3, 0xdd, 0xb9, 0x09, 0xd6, 0xa2, 0x04, 0xc0, 0xf9, 0xfd, 0x0c,
      0x30, 0xec, 0x7d, 0xf3, 0x29, 0x94, 0xde, 0xac, 0x56, 0x03, 0xd4, 0xac, 0x2f, 0x2b, 0x72, 0x4c,
      0x89, 0xe5, 0x92, 0x48, 0x24, 0xaa, 0xb6, 0x59, 0xa4, 0x28, 0xe0, 0x4b, 0x78, 0x2b, 0xbb, 0x3b,
      0x59, 0xc3, 0x96, 0x7d, 0x40, 0xac, 0xc8, 0x13, 0xe5, 0x47, 0x9a, 0xd0, 0x5b, 0xad, 0x9b, 0x39,
      0xe2, 0xa7, 0xe4, 0x15, 0xeb, 0x24, 0xbc, 0x30, 0x85, 0xaf, 0x92, 0xbd, 0x78, 0x0f, 0x47, 0xd5,
      0x9f, 0x94, 0x10, 0xce, 0x4c, 0xf0, 0x6c, 0x10, 0x40, 0x24, 0xa3, 0xac, 0x55, 0x3e, 0xa8, 0x95,
      0xcf, 0x2c, 0x90, 0xaa, 0x43, 0xe7, 0x03, 0xf1, 0xcd, 0x07, 0x63, 0x9d, 0x2d, 0xeb, 0x82, 0x74,
      0x18, 0x43, 0x8f, 0x9d, 0xcc, 0x2c, 0xa2, 0xa5, 0xb1, 0x57, 0x4f, 0x9e, 0x33, 0xe7, 0x20, 0x5f,
      0x7e, 0xb7, 0xb2, 0x32, 0x60, 0xab, 0x62, 0x77, 0x8b, 0xe4, 0xe2, 0x99, 0xa6, 0xd9, 0xb2, 0x3b,
      0x6c, 0xbb, 0x0d, 0x63, 0xec, 0x14, 0x17, 0xe9, 0xc8, 0x58, 0xa6, 0x75, 0xf4, 0xfa, 0xeb, 0x9b,
      0x9f, 0x10, 0x12, 0x67, 0x7e, 0xbe, 0x43, 0xfb, 0xe6, 0x01, 0xa3, 0x67, 0xc9, 0xe7, 0xc1, 0xf4,
      0x86, 0x1a, 0x21, 0x2e, 0x38, 0xd3, 0xa4, 0xfa, 0x15, 0xfb, 0x1a, 0xf0, 0xaf, 0x7e, 0x35, 0xe2,
      0xc2, 0x96, 0x6d, 0xad, 0x25, 0x81, 0xd0, 0x2d, 0xd0, 0xf8, 0x8b, 0x43, 0x28, 0xc7, 0x46, 0x23,
      0x20, 0x0f, 0xb0, 0xd9, 0xd0, 0x73, 0xe5, 0xa4, 0xf9, 0x04, 0x01, 0x9e, 0x9b, 0x38, 0x8b, 0x04,
      0x20, 0x7b, 0x81, 0xf0, 0x0d, 0x61, 0x00, 0x39, 0x4c, 0xb6, 0x53, 0x67, 0x5e, 0xa4, 0xee, 0x49,
      0x82, 0xff, 0x23, 0x18, 0xae, 0x37, 0x22, 0x36, 0xe7, 0xfa, 0x16, 0x3d, 0x46, 0x74, 0x99, 0xda,
      0x73, 0x4f, 0xab, 0x76, 0xbb, 0x32, 0x32, 0xa7, 0x41, 0x08, 0x9e, 0xbe, 0x43, 0x98, 0xef, 0xd4,
      0xc6, 0xc9, 0x72, 0x1b, 0xa8, 0x5f, 0x74, 0x48, 0x95, 0xa2, 0x8b, 0xf7, 0xd0, 0x6a, 0x38, 0xc1,
      0xf2, 0xdb, 0xe0, 0xb1, 0xe4, 0xb6, 0xf4, 0xc3, 0xe4, 0x05, 0x4c, 0xb8, 0xc8, 0xfd, 0x61, 0x72,
      0xa6, 0x39, 0x11, 0xe9, 0x5e, 0x71, 0x5f, 0xac, 0x39, 0x23, 0xea, 0x95, 0xa8, 0x61, 0x6d, 0x9c,
      0x42, 0xab, 0x0d, 0x93, 0x69, 0xd1, 0x22, 0x1a, 0x42, 0x5c, 0xf8, 0xbe, 0x44, 0xcf, 0x93, 0x98,
      0x31, 0x75, 0x40, 0xf7, 0xab, 0x1b, 0x26, 0xef, 0x68, 0x00, 0x93, 0x42, 0x42, 0x56, 0x75, 0x0a,
      0x7d, 0x2c, 0xbb, 0x1f, 0xc4, 0xd0, 0xe9, 0x2f, 0x20, 0xe9, 0xe0, 0xed, 0x58, 0xc9, 0x8c, 0xba,
      0x04, 0x03, 0x83, 0xd4, 0xfd, 0xf9, 0x68, 0xda, 0x67, 0x79, 0x92, 0x0c, 0xce, 0xdb, 0x4e, 0x1c,
      0x62, 0x2d, 0x8f, 0x9c, 0xe9, 0xd7, 0x48, 0xd3, 0xec, 0xd4, 0x72, 0x26, 0x56, 0xa0, 0xfc, 0x48,
      0x56, 0xb1, 0xd1, 0xb4, 0x94, 0xc1, 0x80, 0x98, 0x5e, 0x8d, 0x58, 0x61, 0x74, 0x12, 0xa8, 0x97,
      0x7e, 0x42, 0x19, 0x65, 0xc0, 0x2d, 0xde, 0x04, 0x78, 0x9e, 0x2d, 0xfb, 0xdd, 0xe8, 0xc9, 0x5b,
      0xa2, 0xc3, 0x3b, 0x33, 0xec, 0x57, 0x8d, 0x12, 0x88, 0x9f, 0x30, 0x3a, 0x78, 0x5c, 0x4d, 0x62,
      0x85, 0x44, 0x47, 0xf6, 0x34, 0xb8, 0x19, 0x6f, 0x53, 0x44, 0x52, 0x1e, 0x5f, 0x12, 0xcc, 0x81,
      0x32, 0x82, 0xb7, 0x5e, 0x45, 0x33, 0x0e, 0x80, 0x13, 0x24, 0x73, 0x9a, 0xb7, 0x59, 0x14, 0x1a,
      0xa5, 0xdc, 0x37, 0x89, 0x35, 0x34, 0x02, 0x3c, 0x49, 0x81, 0x8e, 0xd2, 0x73, 0x31, 0x75, 0x8e,
      0x2c, 0x7c, 0x55, 0x90, 0xa1, 0x7d, 0x19, 0x65, 0xd0, 0xc7, 0xbe, 0x5d, 0x43, 0xe4, 0x60, 0x1f,
      0x91, 0xf5, 0x79, 0xfe, 0x7e, 0x37, 0x8c, 0xa5, 0x1e, 0x0b, 0x2b, 0x17, 0x52, 0x1b, 0x92, 0x24,
      0xd6, 0xac, 0x4e, 0x9d, 0x56, 0x5a, 0xd9, 0x88, 0x47, 0x0b, 0x82, 0xd4, 0x16, 0xc2, 0x90, 0x29,
      0xf4, 0xc1, 0x02, 0x75, 0xdf, 0x7f, 0x92, 0xac, 0xa7, 0xde, 0x47, 0x4c, 0x79, 0xfb, 0x33, 0xa4,
      0xbc, 0x8d, 0xfd, 0x8e, 0xce, 0xae, 0x55, 0xac, 0x97, 0xd6, 0x6b, 0xec, 0x98, 0x35, 0x01, 0x7a,
      0x33, 0xd0, 0xa0, 0x11, 0xe1, 0x6c, 0x0a, 0x96, 0x9d, 0x4c, 0x2e, 0xbb, 0xcf, 0xf3, 0x0b, 0x06,
      0x41, 0xdb, 0x5f, 0x82, 0x5e, 0x1c, 0x52, 0xfc, 0xc5, 0x11, 0xd7, 0x54, 0x3a, 0xb1, 0x7c, 0x10,
      0x87, 0x92, 0x68, 0xab, 0x67, 0x14, 0x77, 0x7b, 0x33, 0x08, 0x84, 0x4a, 0x24, 0x30, 0x82, 0x01,
      0x02, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0xa0, 0x81, 0xf4, 0x04,
      0x81, 0xf1, 0x30, 0x81, 0xee, 0x30, 0x81, 0xeb, 0x06, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
      0x01, 0x0c, 0x0a, 0x01, 0x02, 0xa0, 0x81, 0xb4, 0x30, 0x81, 0xb1, 0x30, 0x1c, 0x06, 0x0a, 0x2a,
      0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x01, 0x03, 0x30, 0x0e, 0x04, 0x08, 0x2c, 0xe3, 0xa1,
      0x25, 0x5b, 0x67, 0x91, 0xc4, 0x02, 0x02, 0x08, 0x00, 0x04, 0x81, 0x90, 0x34, 0x8a, 0xec, 0x22,
      0xe4, 0xa4, 0x3b, 0xd0, 0xc8, 0x35, 0x65, 0x74, 0xb2, 0xe6, 0x54, 0x2b, 0xb1, 0xdf, 0xf3, 0x52,
      0x13, 0x4f, 0xa5, 0xde, 0xbd, 0x2a, 0xe4, 0x67, 0x7a, 0x09, 0xb1, 0x09, 0x28, 0x17, 0x0c, 0xf0,
      0x79, 0xdf, 0x03, 0x23, 0x54, 0xe2, 0xc1, 0x7a, 0xef, 0x01, 0x08, 0xc7, 0xe5, 0xf2, 0xc6, 0xd0,
      0xd3, 0xc5, 0x78, 0xa2, 0xcb, 0x0a, 0x43, 0x9f, 0x9b, 0x90, 0xd5, 0xdf, 0x11, 0xed, 0x0c, 0x7e,
      0xf3, 0x4e, 0x81, 0x03, 0x7f, 0x54, 0xea, 0xab, 0x74, 0xb2, 0xc9, 0x26, 0xcd, 0x41, 0xaf, 0x91,
      0xc3, 0x4f, 0x71, 0xe6, 0x54, 0x71, 0x48, 0xe4, 0xb1, 0xe0, 0x6b, 0x20, 0xb5, 0x3c, 0x40, 0x1f,
      0x83, 0xc7, 0xe5, 0xba, 0x4a, 0xa6, 0x4b, 0x50, 0x97, 0x52, 0xa6, 0xc6, 0xa8, 0xa3, 0xab, 0x72,
      0xdb, 0x17, 0x1a, 0x7f, 0x55, 0x65, 0x8f, 0x5a, 0x16, 0xe6, 0x86, 0x43, 0x9b, 0xb7, 0xc0, 0xcf,
      0x11, 0x44, 0x24, 0xcb, 0x55, 0x62, 0x9b, 0x08, 0x6f, 0x9d, 0x35, 0x45, 0x31, 0x25, 0x30, 0x23,
      0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x15, 0x31, 0x16, 0x04, 0x14, 0x85,
      0xd8, 0xba, 0xad, 0xc3, 0x15, 0xde, 0x7d, 0x82, 0x0b, 0xa5, 0xe7, 0xb0, 0x68, 0xaa, 0x3d, 0x40,
      0xd6, 0x1e, 0x57, 0x30, 0x31, 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a,
      0x05, 0x00, 0x04, 0x14, 0x28, 0x7f, 0x97, 0xe6, 0x1f, 0x31, 0x71, 0xb5, 0x59, 0x35, 0x38, 0xa0,
      0xbf, 0x6a, 0xa3, 0x78, 0xd3, 0x3d, 0xf6, 0xf5, 0x04, 0x08, 0x0c, 0x88, 0x7c, 0xca, 0xcf, 0x50,
      0xff, 0x53, 0x02, 0x02, 0x08, 0x00
    };

    SecIdentityRef sec_identity = parse_sec_identity_from_pkcs12([[NSData alloc] initWithBytes:prime256v1_server_leaf_p12 length:sizeof(prime256v1_server_leaf_p12)]);
    sec_identity_t cert_identity = sec_identity_create(sec_identity);
    XCTAssertNotNil(cert_identity);
    XCTAssertEqual(sec_identity_copy_type(cert_identity), SEC_PROTOCOL_IDENTITY_TYPE_CERTIFICATE);

    uint8_t context[32];
    dispatch_data_t context_data = dispatch_data_create(context, sizeof(context), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    uint8_t client_identity[32];
    dispatch_data_t client_identity_data = dispatch_data_create(client_identity, sizeof(client_identity), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    uint8_t server_identity[32];
    dispatch_data_t server_identity_data = dispatch_data_create(server_identity, sizeof(server_identity), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    uint8_t client_verifier[SEC_PROTOCOL_SPAKE2PLUSV1_INPUT_PASSWORD_VERIFIER_NBYTES];
    dispatch_data_t client_verifier_data = dispatch_data_create(client_verifier, sizeof(client_verifier), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    sec_identity_t pake_identity = sec_identity_create_client_SPAKE2PLUSV1_identity_internal(context_data, client_identity_data, server_identity_data, client_verifier_data);
    XCTAssertNotNil(pake_identity);
    XCTAssertEqual(sec_identity_copy_type(pake_identity), SEC_PROTOCOL_IDENTITY_TYPE_SPAKE2PLUSV1);
}

- (void)test_sec_identity_pake_verifier_creation {
    uint8_t context[32];
    dispatch_data_t context_data = dispatch_data_create(context, sizeof(context), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    uint8_t client_identity[32];
    dispatch_data_t client_identity_data = dispatch_data_create(client_identity, sizeof(client_identity), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    uint8_t server_identity[32];
    dispatch_data_t server_identity_data = dispatch_data_create(server_identity, sizeof(server_identity), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    // rdar://147480365 (ccspake_reduce_w performs incorrect reduction when converting w0s to w0)
    //    w0s = 0x9b42cf8eaa1945fcb4092c5faff2ce7171ec66ed74ade384e3263361124a917b517cc23fe298bab9
    //    w1s = 0xf7a7e9a9421959df10bf8e90ac316dba25bde15025ec4bc0f32fb2bee0a81b4641c47a0a95bf81e0
    uint8_t input_password_verifier[80] = {
        0x9b, 0x42, 0xcf, 0x8e, 0xaa, 0x19, 0x45, 0xfc, 0xb4, 0x09, 0x2c, 0x5f,
        0xaf, 0xf2, 0xce, 0x71, 0x71, 0xec, 0x66, 0xed, 0x74, 0xad, 0xe3, 0x84,
        0xe3, 0x26, 0x33, 0x61, 0x12, 0x4a, 0x91, 0x7b, 0x51, 0x7c, 0xc2, 0x3f,
        0xe2, 0x98, 0xba, 0xb9, 0xf7, 0xa7, 0xe9, 0xa9, 0x42, 0x19, 0x59, 0xdf,
        0x10, 0xbf, 0x8e, 0x90, 0xac, 0x31, 0x6d, 0xba, 0x25, 0xbd, 0xe1, 0x50,
        0x25, 0xec, 0x4b, 0xc0, 0xf3, 0x2f, 0xb2, 0xbe, 0xe0, 0xa8, 0x1b, 0x46,
        0x41, 0xc4, 0x7a, 0x0a, 0x95, 0xbf, 0x81, 0xe0
    };
    dispatch_data_t input_password_verifier_data = dispatch_data_create(input_password_verifier, sizeof(input_password_verifier), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    sec_identity_t identity = sec_identity_create_client_SPAKE2PLUSV1_identity_internal(context_data, client_identity_data, server_identity_data, input_password_verifier_data);

    uint8_t buffer[SEC_PROTOCOL_SPAKE2PLUSV1_INPUT_PASSWORD_VERIFIER_NBYTES];
    dispatch_data_t input_verifier = dispatch_data_create(buffer, sizeof(buffer) - 1, NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    dispatch_data_t registration_record = sec_identity_create_SPAKE2PLUSV1_registration_record(input_verifier);
    XCTAssertNil(registration_record);

    dispatch_data_t client_verifier = sec_identity_create_SPAKE2PLUSV1_client_password_verifier(input_verifier);
    XCTAssertNil(client_verifier);

    dispatch_data_t server_verifier = sec_identity_create_SPAKE2PLUSV1_server_password_verifier(input_verifier);
    XCTAssertNil(server_verifier);

    registration_record = sec_identity_copy_SPAKE2PLUSV1_registration_record(identity);
    XCTAssertNotNil(registration_record);
    XCTAssertEqual(dispatch_data_get_size(registration_record), SEC_PROTOCOL_SPAKE2PLUSV1_REGISTRATION_RECORD_NBYTES);
    uint8_t expected_registration_record[] = {
        0x04, 0x5a, 0xd6, 0x1d, 0x45, 0x3d, 0x5b, 0x80, 0x03, 0x90, 0x5c, 0xb1,
        0xb7, 0xc1, 0x16, 0x37, 0x30, 0x18, 0xd3, 0x2a, 0x68, 0x42, 0x68, 0xd4,
        0x87, 0xd2, 0x68, 0x03, 0xf4, 0xeb, 0xe9, 0xff, 0x33, 0xbf, 0x56, 0x0f,
        0xf2, 0x7d, 0x21, 0x53, 0xee, 0x3f, 0x13, 0xdf, 0x0c, 0x54, 0x4c, 0x86,
        0xe4, 0xc7, 0x32, 0xcd, 0xcd, 0x1e, 0x1d, 0x13, 0x17, 0x7b, 0xbc, 0x42,
        0x4c, 0x90, 0x92, 0x04, 0xad
    };
    dispatch_data_t expected_registration_record_data = dispatch_data_create(expected_registration_record, sizeof(expected_registration_record), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(registration_record, expected_registration_record_data));

    client_verifier = sec_identity_copy_SPAKE2PLUSV1_client_password_verifier(identity);
    XCTAssertNotNil(client_verifier);
    XCTAssertEqual(dispatch_data_get_size(client_verifier), SEC_PROTOCOL_SPAKE2PLUSV1_CLIENT_PASSWORD_VERIFIER_NBYTES);
    uint8_t expected_client_verifier[] = {
        0x5e, 0x22, 0x72, 0x5b, 0x6a, 0x96, 0xb8, 0xe6, 0x9a, 0x9e, 0x10, 0x00,
        0x78, 0x32, 0xf2, 0x69, 0xad, 0x83, 0x0d, 0x29, 0xe6, 0x2c, 0xe6, 0xba,
        0x81, 0x6f, 0xe7, 0xbd, 0x78, 0x97, 0xd2, 0xbe, 0x52, 0xd8, 0xe8, 0x6e,
        0x72, 0x70, 0x2a, 0x32, 0x66, 0xa7, 0x08, 0x03, 0x76, 0x05, 0x36, 0xc0,
        0x58, 0xe6, 0x51, 0xb5, 0x77, 0x7c, 0x90, 0xc3, 0x8a, 0x28, 0xdf, 0x8e,
        0x63, 0x3e, 0x7b, 0xd8
    };
    dispatch_data_t expected_client_verifier_data = dispatch_data_create(expected_client_verifier, sizeof(expected_client_verifier), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(client_verifier, expected_client_verifier_data));

    server_verifier = sec_identity_copy_SPAKE2PLUSV1_server_password_verifier(identity);
    XCTAssertNotNil(server_verifier);
    XCTAssertEqual(dispatch_data_get_size(server_verifier), SEC_PROTOCOL_SPAKE2PLUSV1_SERVER_PASSWORD_VERIFIER_NBYTES);
    uint8_t expected_server_verifier[] = {
        0x5e, 0x22, 0x72, 0x5b, 0x6a, 0x96, 0xb8, 0xe6, 0x9a, 0x9e, 0x10, 0x00,
        0x78, 0x32, 0xf2, 0x69, 0xad, 0x83, 0x0d, 0x29, 0xe6, 0x2c, 0xe6, 0xba,
        0x81, 0x6f, 0xe7, 0xbd, 0x78, 0x97, 0xd2, 0xbe
    };
    dispatch_data_t expected_server_verifier_data = dispatch_data_create(expected_server_verifier, sizeof(expected_server_verifier), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(server_verifier, expected_server_verifier_data));
}

- (void)test_sec_identity_pake_creation_internal {
    uint8_t context[32];
    dispatch_data_t context_data = dispatch_data_create(context, sizeof(context), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    uint8_t client_identity[32];
    dispatch_data_t client_identity_data = dispatch_data_create(client_identity, sizeof(client_identity), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    uint8_t server_identity[32];
    dispatch_data_t server_identity_data = dispatch_data_create(server_identity, sizeof(server_identity), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    uint8_t buffer[SEC_PROTOCOL_SPAKE2PLUSV1_INPUT_PASSWORD_VERIFIER_NBYTES];
    dispatch_data_t input_verifier = dispatch_data_create(buffer, sizeof(buffer), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    dispatch_data_t registration_record = sec_identity_create_SPAKE2PLUSV1_registration_record(input_verifier);
    XCTAssertNotNil(registration_record);

    dispatch_data_t client_verifier_data = sec_identity_create_SPAKE2PLUSV1_client_password_verifier(input_verifier);
    XCTAssertNotNil(client_verifier_data);

    dispatch_data_t server_verifier_data = sec_identity_create_SPAKE2PLUSV1_server_password_verifier(input_verifier);
    XCTAssertNotNil(server_verifier_data);

    sec_identity_t client_pake_identity = sec_identity_create_client_SPAKE2PLUSV1_identity_internal(context_data, client_identity_data, server_identity_data, input_verifier);
    XCTAssertNotNil(client_pake_identity);
    XCTAssertEqual(sec_identity_copy_type(client_pake_identity), SEC_PROTOCOL_IDENTITY_TYPE_SPAKE2PLUSV1);

    dispatch_data_t other_client_verifier_data = sec_identity_copy_SPAKE2PLUSV1_client_password_verifier(client_pake_identity);
    XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(client_verifier_data, other_client_verifier_data));
    dispatch_data_t other_registration_record = sec_identity_copy_SPAKE2PLUSV1_registration_record(client_pake_identity);
    XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(registration_record, other_registration_record));
    dispatch_data_t other_server_verifier_data = sec_identity_copy_SPAKE2PLUSV1_server_password_verifier(client_pake_identity);
    XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(server_verifier_data, other_server_verifier_data));

    sec_identity_t server_pake_identity = sec_identity_create_server_SPAKE2PLUSV1_identity(context_data, client_identity_data, server_identity_data, server_verifier_data, registration_record);
    XCTAssertNotNil(server_pake_identity);
    XCTAssertEqual(sec_identity_copy_type(server_pake_identity), SEC_PROTOCOL_IDENTITY_TYPE_SPAKE2PLUSV1);

    other_registration_record = sec_identity_copy_SPAKE2PLUSV1_registration_record(server_pake_identity);
    XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(registration_record, other_registration_record));
    other_server_verifier_data = sec_identity_copy_SPAKE2PLUSV1_server_password_verifier(server_pake_identity);
    XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(server_verifier_data, other_server_verifier_data));
    other_client_verifier_data = sec_identity_copy_SPAKE2PLUSV1_client_password_verifier(server_pake_identity);
    XCTAssertNil(other_client_verifier_data);
}

- (void)test_sec_identity_pake_creation {
    uint8_t context[32];
    dispatch_data_t context_data = dispatch_data_create(context, sizeof(context), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    uint8_t client_identity[32];
    dispatch_data_t client_identity_data = dispatch_data_create(client_identity, sizeof(client_identity), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    uint8_t server_identity[32];
    dispatch_data_t server_identity_data = dispatch_data_create(server_identity, sizeof(server_identity), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    uint8_t password[32];
    dispatch_data_t password_data = dispatch_data_create(password, sizeof(password), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    sec_identity_t client_pake_identity = sec_identity_create_client_SPAKE2PLUSV1_identity(context_data, client_identity_data, server_identity_data, password_data, PAKE_PBKDF_PARAMS_SCRYPT_DEFAULT);
    XCTAssertNotNil(client_pake_identity);
    XCTAssertEqual(sec_identity_copy_type(client_pake_identity), SEC_PROTOCOL_IDENTITY_TYPE_SPAKE2PLUSV1);

    dispatch_data_t client_verifier_data = sec_identity_copy_SPAKE2PLUSV1_client_password_verifier(client_pake_identity);
    XCTAssertNotNil(client_verifier_data);

    dispatch_data_t registration_record = sec_identity_copy_SPAKE2PLUSV1_registration_record(client_pake_identity);
    XCTAssertNotNil(registration_record);

    dispatch_data_t server_verifier_data = sec_identity_copy_SPAKE2PLUSV1_server_password_verifier(client_pake_identity);
    XCTAssertNotNil(server_verifier_data);

    sec_identity_t server_pake_identity = sec_identity_create_server_SPAKE2PLUSV1_identity(context_data, client_identity_data, server_identity_data, server_verifier_data, registration_record);
    XCTAssertNotNil(server_pake_identity);
    XCTAssertEqual(sec_identity_copy_type(server_pake_identity), SEC_PROTOCOL_IDENTITY_TYPE_SPAKE2PLUSV1);

    dispatch_data_t other_client_verifier_data = sec_identity_copy_SPAKE2PLUSV1_client_password_verifier(server_pake_identity);
    XCTAssertNil(other_client_verifier_data);
    dispatch_data_t other_registration_record = sec_identity_copy_SPAKE2PLUSV1_registration_record(server_pake_identity);
    XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(registration_record, other_registration_record));
    dispatch_data_t other_server_verifier_data = sec_identity_copy_SPAKE2PLUSV1_server_password_verifier(server_pake_identity);
    XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(server_verifier_data, other_server_verifier_data));
}

- (void)test_sec_identity_pake_option_equality {
    uint8_t context[] = "test context";
    dispatch_data_t context_data = dispatch_data_create(context, sizeof(context), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    uint8_t client_identity[] = "client_identity";
    dispatch_data_t client_identity_data = dispatch_data_create(client_identity, sizeof(client_identity), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    uint8_t server_identity[] = "server_identity";
    dispatch_data_t server_identity_data = dispatch_data_create(server_identity, sizeof(server_identity), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    uint8_t password[] = "password";
    dispatch_data_t password_data = dispatch_data_create(password, sizeof(password), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    sec_identity_t client_pake_identity = sec_identity_create_client_SPAKE2PLUSV1_identity(context_data, client_identity_data, server_identity_data, password_data, PAKE_PBKDF_PARAMS_SCRYPT_DEFAULT);
    XCTAssertNotNil(client_pake_identity);

    dispatch_data_t registration_record = sec_identity_copy_SPAKE2PLUSV1_registration_record(client_pake_identity);
    XCTAssertNotNil(registration_record);

    dispatch_data_t server_verifier_data = sec_identity_copy_SPAKE2PLUSV1_server_password_verifier(client_pake_identity);
    XCTAssertNotNil(server_verifier_data);

    sec_protocol_options_t client_optionsA = [self create_sec_protocol_options];
    sec_protocol_options_t client_optionsB = [self create_sec_protocol_options];
    sec_protocol_options_set_local_identity(client_optionsA, client_pake_identity);
    XCTAssertFalse(sec_protocol_options_are_equal(client_optionsA, client_optionsB));
    sec_protocol_options_set_local_identity(client_optionsB, client_pake_identity);
    XCTAssertTrue(sec_protocol_options_are_equal(client_optionsA, client_optionsB));

    sec_identity_t server_pake_identity = sec_identity_create_server_SPAKE2PLUSV1_identity(context_data, client_identity_data, server_identity_data, server_verifier_data, registration_record);
    XCTAssertNotNil(server_pake_identity);

    // Initialize a different PAKE identity with different context
    sec_identity_t other_server_pake_identity = sec_identity_create_server_SPAKE2PLUSV1_identity(client_identity_data, client_identity_data, server_identity_data, server_verifier_data, registration_record);
    XCTAssertNotNil(other_server_pake_identity);

    sec_protocol_options_t server_optionsA = [self create_sec_protocol_options];
    sec_protocol_options_t server_optionsB = [self create_sec_protocol_options];
    sec_protocol_options_set_local_identity(server_optionsA, server_pake_identity);
    XCTAssertFalse(sec_protocol_options_are_equal(server_optionsA, server_optionsB));
    sec_protocol_options_set_local_identity(server_optionsB, client_pake_identity);
    XCTAssertFalse(sec_protocol_options_are_equal(server_optionsA, server_optionsB));
    sec_protocol_options_set_local_identity(server_optionsB, other_server_pake_identity);
    XCTAssertFalse(sec_protocol_options_are_equal(server_optionsA, server_optionsB));
    sec_protocol_options_set_local_identity(server_optionsB, server_pake_identity);
    XCTAssertTrue(sec_protocol_options_are_equal(server_optionsA, server_optionsB));
}

@end
