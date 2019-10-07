//
//  SecProtocolConfigurationTest.m
//  SecureTransportTests
//

#import <XCTest/XCTest.h>

#include <os/log.h>
#include <dlfcn.h>
#include <sys/param.h>

#import "SecProtocolConfiguration.h"
#import "SecProtocolPriv.h"
#import "SecProtocolInternal.h"

#import <nw/private.h> // Needed for the mock protocol

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

    struct nw_protocol_callbacks *callbacks = (struct nw_protocol_callbacks *)malloc(sizeof(struct nw_protocol_callbacks));
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
    static struct nw_protocol_identifier mock_identifier = {};
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        memset(&mock_identifier, 0, sizeof(mock_identifier));

        strlcpy((char *)mock_identifier.name, name, name_len);

        mock_identifier.level = nw_protocol_level_application;
        mock_identifier.mapping = nw_protocol_mapping_one_to_one;

        mock_protocol_register_extended(&mock_identifier, _mock_protocol_create_extended);
    });

    return &mock_identifier;
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
        const char *mock_protocol_name = "SecProtocolConfigTestMock";
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static SSLProtocol
protocol_string_to_version(const char *protocol)
{
    if (protocol == NULL) {
        return kSSLProtocolUnknown;
    }

    const char *tlsv10 = "TLSv1.0";
    const char *tlsv11 = "TLSv1.1";
    const char *tlsv12 = "TLSv1.2";
    const char *tlsv13 = "TLSv1.3";

    if (strlen(protocol) == strlen(tlsv10) && strncmp(protocol, tlsv10, strlen(protocol)) == 0) {
        return kTLSProtocol1;
    } else if (strlen(protocol) == strlen(tlsv11) && strncmp(protocol, tlsv11, strlen(protocol)) == 0) {
        return kTLSProtocol11;
    } else if (strlen(protocol) == strlen(tlsv12) && strncmp(protocol, tlsv12, strlen(protocol)) == 0) {
        return kTLSProtocol12;
    } else if (strlen(protocol) == strlen(tlsv13) && strncmp(protocol, tlsv13, strlen(protocol)) == 0) {
        return kTLSProtocol13;
    }

    return kSSLProtocolUnknown;
}
#pragma clang diagnostic pop

@interface SecProtocolConfigurationTest : XCTestCase
@end

@implementation SecProtocolConfigurationTest

- (void)setUp {
}

- (void)tearDown {
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

- (void)testExampleFile:(NSURL *)path
{
    NSDictionary *dictionary = [[NSDictionary alloc] init];
    sec_protocol_configuration_builder_t builder = sec_protocol_configuration_builder_create((__bridge CFDictionaryRef)dictionary, true);
    sec_protocol_configuration_t configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }

    NSData *exampleData = [[NSData alloc] initWithContentsOfURL:path];
    NSDictionary *exampleATS = [NSJSONSerialization JSONObjectWithData:exampleData options:kNilOptions error:nil];
    XCTAssertNotNil(exampleATS, @"Loading %@ failed", path);
    if (!exampleATS) {
        return;
    }

    [exampleATS enumerateKeysAndObjectsUsingBlock:^(id _key, id _obj, BOOL *stop) {
        NSString *key = (NSString *)_key;
        if ([key isEqualToString:@"NSExceptionDomains"]) {
            NSDictionary *domain_map = (NSDictionary *)_obj;
            [domain_map enumerateKeysAndObjectsUsingBlock:^(id _domain, id _domain_entry, BOOL *_domain_stop) {
                NSString *domain = (NSString *)_domain;
                NSDictionary *entry = (NSDictionary *)_domain_entry;

#define BOOLEAN_FOR_KEY(key, value, default) \
    bool value = default; \
    { \
        NSNumber *nsValue = [entry valueForKey:key]; \
        if (nsValue) { \
            value = [nsValue boolValue]; \
        } \
    }
#define STRING_FOR_KEY(key, value, default) \
    NSString *value = default; \
    { \
        NSString *nsValue = [entry valueForKey:key]; \
        if (nsValue) { \
            value = nsValue; \
        } \
    }
                BOOLEAN_FOR_KEY(@"NSExceptionAllowsInsecureHTTPLoads", allows_http, false);
                BOOLEAN_FOR_KEY(@"NSIncludesSubdomains", includes_subdomains, false);
                BOOLEAN_FOR_KEY(@"NSExceptionRequiresForwardSecrecy", requires_pfs, false);
                STRING_FOR_KEY(@"NSExceptionMinimumTLSVersion", minimum_tls, @"TLSv1.2");
#undef STRING_FOR_KEY
#undef BOOLEAN_FOR_KEY

                SSLProtocol minimum_protocol_version = protocol_string_to_version([minimum_tls cStringUsingEncoding:NSUTF8StringEncoding]);

                sec_protocol_options_t options = [self create_sec_protocol_options];
                sec_protocol_options_t transformed = sec_protocol_configuration_copy_transformed_options_for_host(configuration, options, [domain cStringUsingEncoding:NSUTF8StringEncoding]);
                sec_protocol_options_access_handle(transformed, ^bool(void *handle) {
                    sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
                    SEC_PROTOCOL_METADATA_VALIDATE(content, false);

                    XCTAssertTrue(content->ats_required == true);
                    XCTAssertTrue(content->min_version == minimum_protocol_version);
                    if (requires_pfs) {
                        XCTAssertTrue(content->ciphersuites != nil);
                    } else {
                        XCTAssertTrue(content->ciphersuites == nil);
                    }
                });

                XCTAssertTrue(allows_http != sec_protocol_configuration_tls_required_for_host(configuration, [domain cStringUsingEncoding:NSUTF8StringEncoding]));
            }];
        }
    }];
}

- (void)testExampleATSDictionaries {
    NSArray <NSURL *>* testFiles = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".json" subdirectory:@"."];
    [testFiles enumerateObjectsUsingBlock:^(NSURL*  _Nonnull path, __unused NSUInteger idx, BOOL * _Nonnull stop) {
        [self testExampleFile:path];
    }];
}

@end
