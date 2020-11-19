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

static void
mock_protocol_deallocate_metadata(__unused nw_protocol_definition_t definition, void *metadata)
{
    sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)metadata;
    if (content) {
        // pass
    }
    free(content);
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
    
    return (sec_protocol_metadata_t)_nw_protocol_metadata_create(mock_protocol_copy_definition(), identifier);
}

- (void)test_sec_protocol_metadata_get_connection_strength_tls12 {
    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];

    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        content->negotiated_ciphersuite = TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        content->negotiated_protocol_version = kTLSProtocol12;
#pragma clang diagnostic pop

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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            content->negotiated_protocol_version = kTLSProtocol12;
#pragma clang diagnostic pop
            
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
            content->negotiated_protocol_version = kTLSProtocol11;
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
            content->negotiated_protocol_version = kTLSProtocol1;
#pragma clang diagnostic pop
            
            return true;
        });
        
        XCTAssertTrue(SSLConnectionStrengthWeak == sec_protocol_metadata_get_connection_strength(metadata), 
            "Expected SSLConnectionStrengthWeak for TLS 1.0, got %d", (int)sec_protocol_metadata_get_connection_strength(metadata));
    }
}

- (void)test_sec_protocol_metadata_get_connection_strength_sslv3_strong_ciphersuite {
    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];
    if (metadata) {
        (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
            sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
            SEC_PROTOCOL_METADATA_VALIDATE(content, false);
            
            content->negotiated_ciphersuite = TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256; // This can be anything -- we downgrade based on the version here.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            content->negotiated_protocol_version = kSSLProtocol3;
#pragma clang diagnostic pop
            
            return true;
        });
        
        XCTAssertTrue(SSLConnectionStrengthNonsecure == sec_protocol_metadata_get_connection_strength(metadata), 
            "Expected SSLConnectionStrengthNonsecure for SSL 3.0, got %d", (int)sec_protocol_metadata_get_connection_strength(metadata));
    }
}

- (void)test_sec_protocol_metadata_get_connection_strength_sslv3_weak_ciphersuite {
    sec_protocol_metadata_t metadata = [self create_sec_protocol_metadata];
    if (metadata) {
        (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
            sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
            SEC_PROTOCOL_METADATA_VALIDATE(content, false);
            
            content->negotiated_ciphersuite = SSL_RSA_WITH_3DES_EDE_CBC_SHA;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            content->negotiated_protocol_version = kSSLProtocol3;
#pragma clang diagnostic pop
            
            return true;
        });
        
        XCTAssertTrue(SSLConnectionStrengthNonsecure == sec_protocol_metadata_get_connection_strength(metadata), 
            "Expected SSLConnectionStrengthNonsecure for SSL 3.0, got %d", (int)sec_protocol_metadata_get_connection_strength(metadata));
    }
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

    sec_protocol_options_set_tls_certificate_compression_enabled(options, true);
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        XCTAssertTrue(content->certificate_compression_enabled);
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    sec_protocol_options_set_tls_min_version(optionsA, kTLSProtocol13);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_min_version(optionsB, kTLSProtocol13);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));

    sec_protocol_options_set_tls_max_version(optionsA, kTLSProtocol13);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
    sec_protocol_options_set_tls_max_version(optionsB, kTLSProtocol13);
    XCTAssertTrue(sec_protocol_options_are_equal(optionsA, optionsB));
#pragma clang diagnostic pop

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


    sec_protocol_options_append_tls_ciphersuite(optionsB, 7331);
    XCTAssertFalse(sec_protocol_options_are_equal(optionsA, optionsB));
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
    XCTAssertTrue(sec_protocol_options_get_default_max_tls_protocol_version() == tls_protocol_version_TLSv13);
    XCTAssertTrue(sec_protocol_options_get_default_min_tls_protocol_version() == tls_protocol_version_TLSv10);
    XCTAssertTrue(sec_protocol_options_get_default_max_dtls_protocol_version() == tls_protocol_version_DTLSv12);
    XCTAssertTrue(sec_protocol_options_get_default_min_dtls_protocol_version() == tls_protocol_version_DTLSv10);
}

- (void)test_sec_protocol_options_set_psk_hint {
    __block dispatch_data_t hint = [self create_random_dispatch_data];
    sec_protocol_options_t options = [self create_sec_protocol_options];

    (void)sec_protocol_options_access_handle(options, ^bool(void * _Nonnull handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        XCTAssertNil(content->psk_identity_hint, @"PSK identity initialized incorrectly");
    });

    sec_protocol_options_set_tls_pre_shared_key_identity_hint(options, hint);

    (void)sec_protocol_options_access_handle(options, ^bool(void * _Nonnull handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        XCTAssertTrue(sec_protocol_helper_dispatch_data_equal(content->psk_identity_hint, hint), @"PSK identity mistmatch");
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
    XCTAssertTrue(sec_protocol_metadata_get_experiment_identifier(metadata) == NULL);

    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        content->experiment_identifier = strdup(identifier);
        return true;
    });

    XCTAssertTrue(strncmp(identifier, sec_protocol_metadata_get_experiment_identifier(metadata), strlen(identifier)) == 0);
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

@end
